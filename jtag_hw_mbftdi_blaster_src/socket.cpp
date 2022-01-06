
#include <cstdint>
#include <iostream>
#include <cstring>
#include "../common/debug.h"
#include "socket.h"

#ifdef _WINDOWS
#pragma comment(lib, "IPHLPAPI.lib")
#pragma comment(lib, "ws2_32.lib")

bool CSocket::WSAStarted = false;
#else
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <linux/if_link.h>
#define INVALID_SOCKET (-1)
#endif

CSocket::CSocket( bool Udp )
{
#ifdef _WINDOWS
	if (!WSAStarted) {
		WSADATA MyWSADATA;
		if (!WSAStartup(0x0101, &MyWSADATA) == 0)
			throw("can't start WINSOCK");
		WSAStarted = true;
	}
#endif
	udp_ = Udp;
	if( Udp )
		socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	else
		socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (socket_ == INVALID_SOCKET)
		throw("can't create socket");
}

CSocket::~CSocket()
{
#ifdef _WINDOWS
	closesocket( socket_ );
#else
	close( socket_ );
#endif
}

bool CSocket::connectTcp(sockaddr* addr)
{
	int nodelay = 1;
	setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(nodelay));
	int r = connect(socket_, addr, sizeof(sockaddr_in));
	return r == 0;
}

void CSocket::disconnectTcp()
{
#ifdef _WINDOWS
	shutdown( socket_, SD_BOTH );
	closesocket( socket_ );
#else
	shutdown( socket_, SHUT_RDWR );
	close( socket_ );
#endif
}

#ifdef NET_DEBUG
void CSocket::dump( char* str, unsigned char* buf, int len )
{
	std::lock_guard<std::mutex> lock(mutex_net_);
	FILE * pLogFile = nullptr;
	errno_t err = fopen_s(&pLogFile, "d:\\common\\jtag\\nwlog.txt", "ab");
	uint32_t* pcmd = (uint32_t*)buf;
	char tmp[1024];
	sprintf(tmp, "%s %d\n", str,len);
	fwrite(tmp, sizeof(char), strlen(tmp), pLogFile);
	tmp[0] = 0;
	int idx = 0;
	for (int i = 0; i < len; i++) {
		if (idx && ((i & 0xF) == 0)) {
			strcat(tmp, "\n");
			fwrite(tmp, sizeof(char), strlen(tmp), pLogFile);
			idx = 0;
		}
		sprintf(&tmp[idx], "%02X ", buf[i]);
		idx += 3;
	}
	if (idx) {
		tmp[idx++] = 0xD;
		tmp[idx++] = 0xA;
		tmp[idx++] = 0;
		fwrite(tmp, sizeof(char), strlen(tmp), pLogFile);
		idx = 0;
	}

	fclose(pLogFile);
}
#endif

int CSocket::recvData( char* recv_buffer, int length )
{
	int length_ = length;
	unsigned char* rbuf = (unsigned char*)recv_buffer;
	while (length) {
		int received = recv(socket_, recv_buffer, length, 0);
		if (received <= 0)
			return -1; //error
		length -= received;
		recv_buffer += received;
	}
	//dump("<<< RECV <<< ----------------",rbuf,length_);
	return 0; //ok
}

int CSocket::recvDgram(char* recv_buffer, int buffer_length, sockaddr* addr)
{
	socklen_t l = sizeof(sockaddr);
	int received = recvfrom(socket_, recv_buffer, buffer_length, 0, addr, &l);
	return received;
}

int CSocket::sendData( char* send_buffer, int length )
{
#ifdef NET_DEBUG
	dump(">>> SEND >>> ----------------", (unsigned char*)send_buffer, length);
#endif
	while (length) {
		int sent = send(socket_, send_buffer, length, 0);
		if (sent<0)
			return -1; //error
		length -= sent;
		send_buffer += sent;
	}
	return 0; //ok
}

unsigned long CSocket::hasReadData()
{
	unsigned long size = 0;
#ifdef _WINDOWS
	if (ioctlsocket(socket_, FIONREAD, &size) == 0)
		return size;
#else
	if (ioctl(socket_, FIONREAD, &size) == 0)
		return size;
#endif
	return 0; //ok
}

int CSocket::sendReq(list<string>ipservers, unsigned short port, char* send_buffer, int length)
{
	printd("SendReq %d", ipservers.size());
	if (!udp_)
		return -1;
	for (list<string>::iterator it = ipservers.begin(); it != ipservers.end(); ++it)
	{
		struct sockaddr_in sa;
		memset(&sa, 0, sizeof(sa));
		sa.sin_family = AF_INET;
		sa.sin_addr.s_addr = inet_addr(it->c_str());
		printd("SendReq addr %s %08X", it->c_str(), sa.sin_addr.s_addr);
		if (sa.sin_addr.s_addr == INADDR_NONE || sa.sin_addr.s_addr == INADDR_ANY)
			continue;
		sa.sin_port = htons(port);
		sendto(socket_, send_buffer, length, 0, (struct sockaddr*)&sa, sizeof(sa));
	}
	return 0;
}

#ifdef _WINDOWS
int CSocket::sendBroadcastReq(unsigned short port, char* send_buffer, int length)
{
	if (!udp_)
		return -1;

	bool bBroadcast = true;
	setsockopt(socket_, SOL_SOCKET, SO_BROADCAST, (char*)&bBroadcast, sizeof(bBroadcast));

	DWORD dwRetVal;
	ULONG family = AF_INET; // AF_UNSPEC;
	ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
	PIP_ADAPTER_ADDRESSES pAddresses = NULL;
	ULONG outBufLen = 16 * 1024;
	ULONG Iterations = 0;
	unsigned int MAX_TRIES = 8;
	do {
		pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(outBufLen);
		if (pAddresses == NULL) {
			//printf("Memory allocation failed for IP_ADAPTER_ADDRESSES struct\n");
			return -1;
		}

		dwRetVal = GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);

		if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
			free(pAddresses);
			pAddresses = NULL;
		}
		else {
			break;
		}

		Iterations++;
		outBufLen += 16 * 1024;

	} while ((dwRetVal == ERROR_BUFFER_OVERFLOW) && (Iterations < MAX_TRIES));

	if (dwRetVal == NO_ERROR) {
		int i;
		PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;
		PIP_ADAPTER_UNICAST_ADDRESS pUnicast = NULL;
		PIP_ADAPTER_ANYCAST_ADDRESS pAnycast = NULL;
		PIP_ADAPTER_MULTICAST_ADDRESS pMulticast = NULL;
		IP_ADAPTER_DNS_SERVER_ADDRESS *pDnServer = NULL;
		IP_ADAPTER_PREFIX *pPrefix = NULL;

		// If successful, output some information from the data we received
		pCurrAddresses = pAddresses;
		while (pCurrAddresses) {
			//printf("Adapter name: %s\n", pCurrAddresses->AdapterName);

			pUnicast = pCurrAddresses->FirstUnicastAddress;
			if (pUnicast != NULL) {
				for (i = 0; pUnicast != NULL; i++) {
					if (pUnicast->Address.lpSockaddr->sa_family == AF_INET)
					{
						sockaddr_in *sa_in = (sockaddr_in *)pUnicast->Address.lpSockaddr;
						ULONG Mask = 0;
						ConvertLengthToIpv4Mask(
							pUnicast->OnLinkPrefixLength, &Mask);
						char ip_addr_name[64];
						char ip_mask_name[64];
						inet_ntop(AF_INET, &(sa_in->sin_addr), ip_addr_name, sizeof(ip_addr_name));
						inet_ntop(AF_INET, &(Mask), ip_mask_name, sizeof(ip_mask_name));
						uint32_t* pmask = (uint32_t*)&Mask;
						uint32_t* paddr = (uint32_t*)&(sa_in->sin_addr);
						uint32_t  broadcast_addr = *paddr | (~(*pmask));
						//printf("\tIPV4: %s %s\n", ip_addr_name, ip_mask_name);
						struct sockaddr_in sa;
						memset(&sa, 0, sizeof(sa));
						sa.sin_family = AF_INET;
						sa.sin_addr.s_addr = broadcast_addr; // inet_addr("255.255.255.255");
						sa.sin_port = htons(port);
						sendto(socket_, send_buffer, length, 0, (struct sockaddr*)&sa, sizeof(sa));
					}
					pUnicast = pUnicast->Next;
				}
			}
			pCurrAddresses = pCurrAddresses->Next;
		}
		free(pAddresses);
	}
	return 0;
}
#else
int CSocket::sendBroadcastReq(unsigned short port, char* send_buffer, int length)
{
	struct ifaddrs *ifaddr, *ifa;
	int family, s, n;
	char host[NI_MAXHOST];
	char mask[NI_MAXHOST];
	char broadaddr[NI_MAXHOST];

	if( getifaddrs(&ifaddr) == -1 )
	{
		perror("getifaddrs");
		return -1;
	}

	/* Walk through linked list, maintaining head pointer so we can free list later */
	for( ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++)
	{
		if (ifa->ifa_addr == NULL)
			continue;

		family = ifa->ifa_addr->sa_family;

		// Display interface name and family (including symbolic form of the latter for the common families)
		/*
		printf( "%-8s %s (%d)\n",
			ifa->ifa_name,
			(family == AF_PACKET) ? "AF_PACKET" :
			(family == AF_INET) ? "AF_INET" :
			(family == AF_INET6) ? "AF_INET6" : "???",
			family );
		*/

		// For an AF_INET* interface address, display the address
		if( family == AF_INET /*|| family == AF_INET6*/ )
		{
			s = getnameinfo(ifa->ifa_addr,
					(family == AF_INET) ? sizeof(struct sockaddr_in) :
					sizeof(struct sockaddr_in6),
					host, NI_MAXHOST,
					NULL, 0, NI_NUMERICHOST);
			if( s != 0 )
			{
				printf("getnameinfo() failed: %s\n", gai_strerror(s));
				return -1;
			}

			s = getnameinfo(ifa->ifa_netmask,
					(family == AF_INET) ? sizeof(struct sockaddr_in) :
					sizeof(struct sockaddr_in6),
					mask, NI_MAXHOST,
					NULL, 0, NI_NUMERICHOST);
			if( s != 0 )
			{
				printf("getnameinfo() failed: %s\n", gai_strerror(s));
				return -1;
			}

			s = getnameinfo(ifa->ifa_ifu.ifu_broadaddr,
					(family == AF_INET) ? sizeof(struct sockaddr_in) :
					sizeof(struct sockaddr_in6),
					broadaddr, NI_MAXHOST,
					NULL, 0, NI_NUMERICHOST);
			if( s != 0 )
			{
				printf("getnameinfo() failed: %s\n", gai_strerror(s));
				return -1;
			}

			if( ifa->ifa_addr->sa_data[2]==127 && ifa->ifa_addr->sa_data[3]==0 && ifa->ifa_addr->sa_data[4]==0 && ifa->ifa_addr->sa_data[5]==1 )
			{
				//local address
			}
			else
			{
				//real address
				/*
				printf("address: %s %s %s\n", host, mask, broadaddr);
				printf("bytes: %d.%d.%d.%d\n",
					ifa->ifa_addr->sa_data[2],
					ifa->ifa_addr->sa_data[3],
					ifa->ifa_addr->sa_data[4],
					ifa->ifa_addr->sa_data[5]
					);
				*/
				ifa->ifa_ifu.ifu_broadaddr->sa_data[0] = (port>>8)>>8;
				ifa->ifa_ifu.ifu_broadaddr->sa_data[1] = (port&0xFF)>>8;
				sendto(socket_, send_buffer, length, 0, ifa->ifa_ifu.ifu_broadaddr, sizeof(struct sockaddr));
			}
		}
	}

	freeifaddrs(ifaddr);
	return 0;
}
#endif


