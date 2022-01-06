// jtag_hw_net_blaster.cpp:  altera/intel jtagsrv blaster helper DLL.
//
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "socket.h"

#ifdef _WINDOWS
#include <windows.h>
void do_sleep(int ms) { Sleep(ms);  }
#else
#include <unistd.h>
#include <ctype.h>
#include <stdarg.h>
void do_sleep(int ms) { usleep(ms*1000); }
#endif

#include "../common/debug.h"
#include "../common/jtagsrv.h"
#include "../nw_jtag_srv/nw_jtag_srv.h"
#include "CConfig.h"
#include "jtag_hw_net_blaster.h"

#define DEV_NAME_NET "NW-JTAG-"

#define NET_BLASTER_SRV_PORT 6123
CSocket* g_udp_sock = nullptr;

//answers from network jtag servers
struct nw_port {
	struct udp_cmd cmd;
	struct sockaddr addr;
	char addr_str[64];
};

int g_num_nw_ports = 0;
struct nw_port g_nw_ports[MAX_DEV_NUM];

net_blaster::net_blaster( int idx ):jblaster(idx)
{
	tcp_sock = new CSocket(false);
	sockaddr addr = g_nw_ports[idx].addr;
	sockaddr_in* paddr = (sockaddr_in*)&addr;
	paddr->sin_port = NW_JTAG_TCP_PORT;

	struct sockaddr_in nw_jtag_srv;
	nw_jtag_srv.sin_addr.s_addr = inet_addr(g_nw_ports[idx].addr_str);
	nw_jtag_srv.sin_family = AF_INET;
	nw_jtag_srv.sin_port = htons(NW_JTAG_TCP_PORT);

	if (!tcp_sock->connectTcp((sockaddr*)&nw_jtag_srv))
		throw;
};

net_blaster::~net_blaster()
{
	printd("net_blaster destructor, tcp: %p\n", tcp_sock);
	if (tcp_sock) {
		tcp_sock->disconnectTcp();
		delete tcp_sock;
	}
};

//-----------------------------
//Network related functions
//-----------------------------
int SearchBlasters(int port_num, char* pblaster_name, int blaster_name_sz)
{
	printd("hwproc_listdev %d %p %d\n", port_num, pblaster_name, blaster_name_sz);
	//do not support more then N blasters
	if (port_num >= MAX_DEV_NUM)
		return 0;

	char myblastername[] = DEV_NAME_NET;

	if (port_num == 0) {
		//first call initiates network discover
		g_num_nw_ports = 0;
		struct udp_cmd cmd;
		memset(&cmd, 0, sizeof(cmd));
		memcpy(&cmd.tag0, UDP_TAG0_STR, sizeof(cmd.tag0));
		memcpy(&cmd.tag1, UDP_TAG1_STR, sizeof(cmd.tag0));
		cmd.cmd = UDP_CMD_DISCOVER;
		printd("sending discover..\n");
		list<string> L = g_cfg.getIpServers();
		if(L.size())
			g_udp_sock->sendReq(L,NW_JTAG_UDP_PORT, (char*)&cmd, sizeof(cmd));
		else
			g_udp_sock->sendBroadcastReq(NW_JTAG_UDP_PORT, (char*)&cmd, sizeof(cmd));
		printd("sent!\n");
		//wait for small time, allow remote servers reply
		do_sleep(1000);
		//read UDP answers
		while (1) {
			unsigned long sz = g_udp_sock->hasReadData();
			if (sz == 0)
				break;
			//has some acks from NW JTAG servers
			g_udp_sock->recvDgram((char*)&g_nw_ports[g_num_nw_ports].cmd,
				sizeof(struct udp_cmd), &g_nw_ports[g_num_nw_ports].addr);
			g_num_nw_ports++;
			if (g_num_nw_ports == MAX_DEV_NUM)
				break;
		}
	}
	printd("hwproc_listdev found %d\n", g_num_nw_ports);
	if (port_num >= g_num_nw_ports)
		return 0; //no more devices

	sprintf(g_nw_ports[port_num].addr_str, "%d.%d.%d.%d",
		(unsigned char)g_nw_ports[port_num].addr.sa_data[2],
		(unsigned char)g_nw_ports[port_num].addr.sa_data[3],
		(unsigned char)g_nw_ports[port_num].addr.sa_data[4],
		(unsigned char)g_nw_ports[port_num].addr.sa_data[5]);
	strcpy(pblaster_name, g_nw_ports[port_num].addr_str); // , min(sizeof(g_nw_ports[port_num].addr_str), blaster_name_sz));
	//sprintf(pblaster_name,"ip%d=%s",port_num, g_nw_ports[port_num].addr_str);
	printd("hwproc_listdev return %s\n", pblaster_name);
	return g_num_nw_ports;
}

int PortName2Idx(const char* PortName) 
{
	int dev_idx = -1;

	printd("PortName2Idx %s\n", PortName);
	for (int i = 0; i < g_num_nw_ports; i++) {
		if (strcmp(PortName, g_nw_ports[i].addr_str) == 0) {
			dev_idx = i;
			break;
		}
	}
	if (dev_idx == -1) {
		printd("PortName2Idx err\n");
		return -1; //error, no such network port
	}
	return dev_idx;
}

char* GetBlasterName()
{
	return PROGRAMER_NAME PROG_NAME_SUFFIX;
}

int InitBlasterLibrary()
{
	if (g_udp_sock)
		delete g_udp_sock;
	g_udp_sock = new CSocket(true);
	return 0;
}

jblaster* CreateBlaster(int idx)
{
	net_blaster* pblaster = nullptr;
	try {
		pblaster = new net_blaster(idx);
	}
	catch (...) {
	}
	return (jblaster*)pblaster;
}

void DeleteBlaster( jblaster* jbl )
{
	delete static_cast<net_blaster*>(jbl);
}

int net_blaster::configure()
{
	int r = 1;
	return r;
}

//send all accumulated tdi data to chip using (passive) serial shift out
unsigned int net_blaster::flush_passive_serial()
{
	return 0;
}

unsigned int net_blaster::write_read_as_buffer(unsigned int wr_len, unsigned int rd_len, unsigned int bitsidx, unsigned int need_read)
{
	return 1;
}

unsigned int net_blaster::write_jtag_stream_as( unsigned int start_idx, unsigned int count, unsigned int need_read)
{
	return 1;
}

unsigned int net_blaster::write_jtag_stream( struct jtag_task* jt )
{
	printTdiTms(jt->data,jt->wr_idx);

	struct nw_cmd nwcmd;
	nwcmd.setLength(jt->wr_idx);
	nwcmd.cmd = jt->need_tdo ? CMD_JTAG_WR_TDI_TMS_BUF_R : CMD_JTAG_WR_TDI_TMS_BUF;
	nwcmd.param1 = jt->wr_idx;
	nwcmd.param2 = 0;
	int r1 = tcp_sock->sendData((char*)&nwcmd, nwcmd.size());
	printTdiTms(&jt->data[0], jt->wr_idx);
	int r2 = tcp_sock->sendData((char*)&jt->data[0], jt->wr_idx);
	//printd("nw_send_recv num bits %d (%d %d)\n", jt->wr_idx, r1, r2);
	if (r1==0 && r2==0) {
		//sent ok
		if (jt->need_tdo)
		{
			struct nw_cmd nw_rcv_cmd;
			if (tcp_sock->recvData((char*)&nw_rcv_cmd, nw_rcv_cmd.size()) == 0) {
				//header received OK
				int rdata_len = nw_rcv_cmd.dataSize();
				if (tcp_sock->recvData((char*)&jt->data[0], rdata_len) == 0) {
					if (jtagsrvi.jtagsrv_pass_data) {
						unsigned char* jptr = &jt->data[0];
						//jtagr = 
						jtagsrvi.jtagsrv_pass_data((void*)jtagsrv_context_, jptr, nw_rcv_cmd.param1);
						printd("jtagsrv_pass_data, numbits: %d Chk %08X Data: %02x %02x %02x %02x %02x %02x\n", nw_rcv_cmd.param1, checkSum(jptr, nw_rcv_cmd.param1),
							jptr[0], jptr[1], jptr[2], jptr[3], jptr[4], jptr[5]);

					}
				}
			}
		}
		else
		{
			if (jtagsrvi.jtagsrv_report) {
				printd("jtagsrv_report: bits %d bits in que %d\n", jt->wr_idx, num_bits_in_queue_);
				jtagsrvi.jtagsrv_report((void*)jtagsrv_context_, jt->wr_idx, num_bits_in_queue_);
				//printd("jtagsrv_report done\n");
			}
		}
	}
	return 1;
}

unsigned int net_blaster::write_flags_read_status(unsigned int flags, unsigned int* pstatus)
{
	struct nw_cmd nwcmd;
	nwcmd.setLength(0);
	nwcmd.cmd = CMD_JTAG_WR_FLAGS_RD_STATUS;
	nwcmd.param1 = flags;
	nwcmd.param2 = 0;
	int r = tcp_sock->sendData((char*)&nwcmd, nwcmd.size());
	if (r) {
		printd("write_flags_read_status: cannot send nwcmd\n");
		return -1; //error
	}
	memset(&nwcmd, 0, sizeof(nwcmd));
	r = tcp_sock->recvData((char*)&nwcmd, nwcmd.size());
	if (r) {
		printd("write_flags_read_status: cannot read nwcmd\n");
		return -1; //error
	}
	printd("write_flags_read_status: flags: %08X %08X\n",flags, nwcmd.param1);
	*pstatus = nwcmd.param1;
	return 0;
}

