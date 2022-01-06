#pragma once

#ifdef _WINDOWS
#include <WS2tcpip.h>
#include <iphlpapi.h>
#define SOCKET_TYPE SOCKET
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define SOCKET_TYPE int
#endif

#include <mutex>
#include <list>
#include <string>
using namespace std;

class CSocket
{
public:
	int recvData(char* recv_buffer, int length);
	int sendData(char* send_buffer, int length);
	unsigned long hasReadData();
	int sendReq(list<string>ipservers, unsigned short port, char* send_buffer, int length);
	int sendBroadcastReq(unsigned short port, char* send_buffer, int length);
	int recvDgram(char* recv_buffer, int buffer_length, struct sockaddr* addr);
	bool connectTcp(sockaddr* addr);
	void disconnectTcp();
	CSocket( bool Udp );
	virtual ~CSocket();
private:
	int error_ { 0 };
	bool udp_{ false };
#ifdef NET_DEBUG
	void dump(char* str, unsigned char* buf, int len);
#endif
	SOCKET_TYPE socket_;
	static bool WSAStarted;
	mutex mutex_net_;
};

