///////////////////////////////////////
// Module:	nw_jtag_srv
// Author:	Nick Kovach
// Copyright (c)  2017 InproPlus Ltd
// Remarks:
//	Network Jtag Server for Linux, including RPI3 devices
//	TCP server which receives network commands for JTAG
///////////////////////////////////////

#include <stdio.h>
#include <fcntl.h>
#include <string.h>    //strlen
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h> //inet_addr
#include <netinet/tcp.h> //inet_addr
#include <unistd.h>    //write
#include <iostream>
#include <thread>
#include <mutex>

#include <stddef.h>
#include "rpi_gpio.h"

#include "../arm_ftdi/ftd2xx.h"
#include <jtag_hw_blaster.h>
#include <jtag_hw_mbftdi_blaster.h>
#include "../common/jtagsrv.h"

#include "nw_jtag_srv.h"

using namespace std;

mutex cout_mtx;
int udp_discover_cnt = 0;
int udp_ack_cnt = 0;
int conn_cnt = 0;
int jtag_wr_cnt = 0;
int jtag_rd_cnt = 0;

void print_srv_stat()
{
    lock_guard<mutex> lock( cout_mtx );
    cout << "Stat Srv Discover/Ack: " << udp_discover_cnt << "/" << udp_ack_cnt 
	 << ", Conn: " << conn_cnt
	 << ", JTAG Wr/Rd: " << jtag_wr_cnt << "/" << jtag_rd_cnt << "\r" << flush;
}

int doTcpRecv( int sock, char* recv_buffer, int length )
{
    //cout << "need recv: "<<length<<"\n";
    while( length ) {
	int received = recv( sock, recv_buffer, length, 0 );
	//cout << "got: "<<received<<"\n";
	if( received<=0)
	    return -1; //error
	length-=received;
	recv_buffer+=received;
    }
    return 0; //ok
}

int doTcpSend( int sock, char* send_buffer, int length )
{
    while( length ) {
	int sent = send( sock, send_buffer, length, 0 );
	if( sent<0)
	    return -1; //error
	length-=sent;
	send_buffer+=sent;
    }
    return 0; //ok
}

bool check_tag(char* tag0, const char* tag1)
{
    if(tag0[0]==tag1[0] && tag0[1]==tag1[1] && tag0[2]==tag1[2] && tag0[3]==tag1[3] )
	return true;
    return false;
}

void udpListenThread( int socket )
{
    cout << "UDP Thread listens discovery..\n";
    while(1) {
	char rbuf[2048];
	struct sockaddr addr;
	int addr_len = sizeof(addr);
	memset(rbuf,0,sizeof(rbuf));
	int n = recvfrom( socket, rbuf, sizeof(rbuf), 0, &addr, (socklen_t*)&addr_len );
	if( n ) {
	    //received
	    //cout << "UDP dgram\n";
	    struct udp_cmd* pucmd = (struct udp_cmd*)rbuf;
	    if( check_tag(pucmd->tag0,UDP_TAG0_STR) && check_tag(pucmd->tag1,UDP_TAG1_STR) ) {
		//cout << "Nw Jtag Discover: "<< pucmd->cmd <<"\n";
		udp_discover_cnt++;
		if( pucmd->cmd==UDP_CMD_DISCOVER ) {
		    pucmd->cmd = UDP_CMD_ACK;
		    pucmd->ver = VERSION;
		    //int r = 
			sendto( socket, rbuf, sizeof(struct udp_cmd), 0, &addr, (socklen_t)addr_len);
		    udp_ack_cnt++;
		    //cout << "UDP ack\n";
		}
		print_srv_stat();
	    }
	}
	else {
	    //error?
	    usleep(1000000);
	}
    }
}

//struct jtagsrv_interface jtagsrv_intf;
extern struct jtagsrv_interface jtagsrvi;
void*  mbftdi_ctx = nullptr;
int g_client_tcp_socket = 0;
unsigned int jtagsrv_pass_data(void* ctx ,unsigned char* buffer ,unsigned int sz )
{
    //cout << "pass data " << sz << "\n";

	int data_len = (sz+7)/8; //convert number of bits into number of bytes
	buffer[data_len+0]=0;
	buffer[data_len+1]=0;
	buffer[data_len+2]=0;
	buffer[data_len+3]=0;

    struct nw_cmd nwcmd;
    data_len += 4; //add more 4 bytes for nothing
    nwcmd.setLength(data_len);
    nwcmd.cmd=CMD_JTAG_WR_TDI_TMS_BUF_R;
    nwcmd.param1 = sz; //number of bits;
    nwcmd.param2 = 0;
    //cout << "jtagsrv_pass_data " << sz << " " << send_len;
    //cout << hex;
    //cout << " " << (int)buffer[0] << " " << (int)buffer[1] << " " << (int)buffer[2] << " " << (int)buffer[3] <<"\n";
    //cout << dec;
    doTcpSend( g_client_tcp_socket, (char*)&nwcmd, nwcmd.size() );
    doTcpSend( g_client_tcp_socket, (char*)buffer, data_len );
    return 0;
}

#ifdef USE_MBFTDI
ftdi_blaster* my_blaster = nullptr;
void write_jtag_stream_(int sock, char* stream, int num_bytes)
{
    //cout << "wr_jtag_stream " << num_bytes << "\n";
    //unsigned char* ps = (unsigned char*)stream;
    //printf(">>> %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X \n",ps[0],ps[1],ps[2],ps[3],ps[4],ps[5],ps[6],ps[7],ps[8],ps[9]);
    int mem_sz = num_bytes+sizeof(struct jtag_task)+1024;
    struct jtag_task* jt = (struct jtag_task*)new unsigned char[mem_sz];
    jt->wr_idx = num_bytes;
    jt->need_tdo = sock ? 1:0;
    memcpy(&jt->data[0],stream,num_bytes);
    my_blaster->write_jtag_stream(jt);
    delete[](unsigned char*)jt;
    //cout << "wr done\n";
}
#endif

#define PIN_DELAY_CNT_SMALL 3
#define PIN_DELAY_CNT 3

int pause(int p)
{
	volatile int x=0;
	for(int i=0; i<p; i++)
		x = GET_GPIO(TDO_RPI_PIN);
	return x;
}

void pulse_tck()
{
	pause(PIN_DELAY_CNT_SMALL);
	GPIO_CLR = (1<<TCK_RPI_PIN);
	pause(PIN_DELAY_CNT);
	GPIO_SET = (1<<TCK_RPI_PIN);
	pause(PIN_DELAY_CNT);
}

int pulse_tck_r()
{
	int v;
	pause(PIN_DELAY_CNT_SMALL);
	GPIO_CLR = (1<<TCK_RPI_PIN);
	v = pause(PIN_DELAY_CNT);
	GPIO_SET = (1<<TCK_RPI_PIN);
	pause(PIN_DELAY_CNT);
	return v ? 1:0;
}

void write_jtag_stream(int sock, char* stream, int num_bits)
{
	int return_buf_size = (num_bits+7)/8+4;
	unsigned char* sbuf = nullptr;
	if(sock) {
		sbuf = new unsigned char[return_buf_size];
		memset(sbuf,0,return_buf_size);
	}
	for(int i=0; i<num_bits; i++) {
		int tdi = stream[i]&1;
		int tms = stream[i]&2;
		if(tms) {
			GPIO_SET = (1<<TMS_RPI_PIN);
		} else {
			GPIO_CLR = (1<<TMS_RPI_PIN);
		}
		if(tdi) {
			GPIO_SET = (1<<TDI_RPI_PIN);
		} else {
			GPIO_CLR = (1<<TDI_RPI_PIN);
		}

		if(num_bits==34) {
			    cout << (tms? "1":"0")<< (tdi? "1":"0");
		}
		if(sock) {
			int wr_idx = i/8;
			int v = pulse_tck_r();
			if(v)
				sbuf[wr_idx] = sbuf[wr_idx] | (0x01<<(i%8));
			else
				sbuf[wr_idx] = sbuf[wr_idx] & ((0x01<<(i%8))^0xFF);
		}
		else {
		    pulse_tck();
		}
	}
	
	if(sock) {
		struct nw_cmd nwcmd;
		nwcmd.setLength( return_buf_size );
		nwcmd.cmd = CMD_JTAG_WR_TDI_TMS_BUF_R;
		nwcmd.param1 = num_bits;
		nwcmd.param2 = 0;
		doTcpSend( sock, (char*)&nwcmd, nwcmd.size() );
		doTcpSend( sock, (char*)sbuf, return_buf_size );
/*
		cout << "\nR "<< num_bits << " * " << std::hex 
			<< (unsigned int)sbuf[0] << " " 
			<< (unsigned int)sbuf[1] << " " 
			<< (unsigned int)sbuf[2] << " " 
			<< (unsigned int)sbuf[3] << " " 
			<< (unsigned int)sbuf[4] << " " 
			<< (unsigned int)sbuf[5] << " " 
			<< std::dec <<"\n";
*/
		delete[] sbuf;
	}
}

void write_flags_read_status(int sock, unsigned char flags)
{
	if(sock) {
		struct nw_cmd nwcmd;
		nwcmd.setLength( 0 );
		nwcmd.cmd = CMD_JTAG_WR_FLAGS_RD_STATUS;
		nwcmd.param1 = 1; //status
		nwcmd.param2 = 0;
		doTcpSend( sock, (char*)&nwcmd, nwcmd.size() );
	}
}

int main(int argc , char *argv[])
{
    int socket_tcp, socket_tcp_client, c;
    int socket_udp;
    struct sockaddr_in server_udp_addr, server_tcp_addr, client_tcp_addr;

    cout << "Hello RPI3 Network JTAG Server!\n";
    int ver_ma = VERSION / 100;
    int ver_mi = VERSION - (ver_ma*100);
    cout << "Version " << ver_ma << "." << ver_mi << "\n";
	
	if( setup_rpi_gpio() ) {
		cout << "Cannot map GPIO memory, probably use <sudo>\n";
		return -1;
	}

#ifdef USE_MBFTDI
    //------------------------------
    char bl_name[128] = "";
    unsigned int  listdev_r =  SearchBlasters(0, bl_name, sizeof(bl_name));
    if(listdev_r==0){
	cout << "MBFTDI blaster was not found\n";
	return -1;
    }

    cout << "MBFTDI blaster found!\n";

    memset(&jtagsrvi,0,sizeof(jtagsrvi));
    jtagsrvi.size = sizeof(jtagsrvi);
    jtagsrvi.jtagsrv_pass_data = &jtagsrv_pass_data;
    my_blaster = (ftdi_blaster*)CreateBlaster(0);
    my_blaster->configure();
    my_blaster->jtagsrv_context_ = &jtagsrvi;

    if( my_blaster==nullptr ){
	cout << "MBFTDI blaster was not created\n";
	return -1;
    }

    cout << "MBFTDI blaster created!\n";
    //------------------------------
#endif

    //Create socket
    socket_tcp = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_tcp == -1) {
        cout << "Could not create TCP server socket\n";
	return -1;
    }
    cout << "TCP Server Socket created\n";

    //Prepare the sockaddr_in structure
    server_tcp_addr.sin_family = AF_INET;
    server_tcp_addr.sin_addr.s_addr = INADDR_ANY;
    server_tcp_addr.sin_port = htons( NW_JTAG_TCP_PORT );

    //Bind
    if( bind(socket_tcp,(struct sockaddr *)&server_tcp_addr, sizeof(server_tcp_addr)) < 0) {
        cout << "Bind Port Failed. Error\n";
        return -1;
    }
    cout << "TCP Port Bind Done Success (" << NW_JTAG_TCP_PORT << ")\n";

    //Create UDP socket
    socket_udp = socket(AF_INET , SOCK_DGRAM , 0);
    if (socket_udp == -1) {
        cout << "Could not create UDP server socket\n";
	return -1;
    }
    cout << "UDP Server Socket created\n";

    //Prepare the sockaddr_in structure
    server_udp_addr.sin_family = AF_INET;
    server_udp_addr.sin_addr.s_addr = INADDR_ANY;
    server_udp_addr.sin_port = htons( NW_JTAG_UDP_PORT );

    //Bind
    if( bind(socket_udp,(struct sockaddr *)&server_udp_addr, sizeof(server_udp_addr)) < 0) {
        cout << "Bind Udp Port Failed. Error\n";
        return -1;
    }
    cout << "UDP Port Bind Done Success (" << NW_JTAG_UDP_PORT << ")\n";

    std::thread t = std::thread( [=]{ udpListenThread( socket_udp ); } );

    //Listen
    listen(socket_tcp, 3);

    //Accept and incoming connection
    cout << "Waiting for incoming connections...\n";
    print_srv_stat();

    c = sizeof(struct sockaddr_in);

    while(1) {
	//accept connection from an incoming client
	socket_tcp_client = accept(socket_tcp, (struct sockaddr *)&client_tcp_addr, (socklen_t*)&c);
	if (socket_tcp_client < 0) {
	    cout << "TCP accept failed\n";
	    return -1;
	}
	//cout << "Incoming TCP Connection accepted\n";
	conn_cnt++;
	print_srv_stat();
	g_client_tcp_socket = socket_tcp_client;

	int flag=1;
	setsockopt(socket_tcp_client,IPPROTO_TCP,TCP_NODELAY,&flag,sizeof(flag));

	int recv_buffer_size = 4*1024*1024;
	char* recv_buffer = new char[ recv_buffer_size ];
	struct nw_cmd* pnwcmd = (struct nw_cmd*)recv_buffer;
	int result;

	//Receive a message from client
	while( 1 ) {
	    result = doTcpRecv( socket_tcp_client, (char*)&pnwcmd->len, sizeof(pnwcmd->len) );
	    if( result<0 ) { /*cout << "Disconnect?\n";*/ break; }
	    int pkt_size = pnwcmd->len;
	    //cout << "cmd data_size" << data_size << "\n";

	    if( (pkt_size+1024)>recv_buffer_size ) {
		//realloc buffer
		recv_buffer_size = pkt_size+1024;
		//cout << "Realloc rbuf " << pkt_size << " " << recv_buffer_size << "\n";

		delete[] recv_buffer;
		recv_buffer = new char[ recv_buffer_size ];
		pnwcmd = (struct nw_cmd*)recv_buffer;
		pnwcmd->len = pkt_size;
	    }
	    result = doTcpRecv( socket_tcp_client, (char*)&pnwcmd->cmd, pkt_size);
	    if( result<0 ) { /*cout << "Disconnect?\n";*/ break; }
	    int cmd = pnwcmd->cmd;
	    //cout << "cmd " << cmd << " " << pkt_size << "\n";

	    if( cmd == CMD_JTAG_WR_FLAGS_RD_STATUS) {
			//cout << "CMD_JTAG_WR_FLAGS_RD_STATUS " << (unsigned int)pnwcmd->param1 << "\n";
			//cout << "Jtag status: " << (unsigned int)pnwcmd->param1 << "\n";
			write_flags_read_status( socket_tcp_client, pnwcmd->param1 );
	    }
	    else if( cmd == CMD_JTAG_WR_TDI_TMS_BUF_R ) {
			int count = pnwcmd->dataSize();
			int num_bits = pnwcmd->param1;
			if( (num_bits+7)/8>count ) {
			    cout << "ERR CMD_JTAG_WR_TDI_TMS_BUF_R! " << count << " " << num_bits << "\n";
			    break;
			}
			//cout << "Jtag wr/rd: " << num_bits << "\n";
			jtag_wr_cnt+=num_bits;
			jtag_rd_cnt+=num_bits;
			write_jtag_stream(socket_tcp_client,&pnwcmd->data[0],num_bits);
			print_srv_stat();
	    }
	    else if( cmd == CMD_JTAG_WR_TDI_TMS_BUF ) {
			int count = pnwcmd->dataSize();
			int num_bits = pnwcmd->param1;
			if( (num_bits+7)/8>count ) {
			    cout << "ERR CMD_JTAG_WR_TDI_TMS_BUF! " << count << " " << num_bits << "\n";
			    break;
			}
			//cout << "CMD_JTAG_WR_TDI_TMS_BUF! " << count << " " << num_bits << "\n";
			//cout << "Jtag wr: " << num_bits << "\n";
			jtag_wr_cnt+=num_bits;
			write_jtag_stream(0,&pnwcmd->data[0],num_bits);
			print_srv_stat();
	    }
	    else {
		cout << "Unknown cmd: " << cmd << "\n";
		break;
	    }
	}
	//lock_guard<mutex> lock( cout_mtx );
	//cout << "\nJtagSrv TCP Client disconnected\n";
	conn_cnt--;
	print_srv_stat();
	delete []recv_buffer;
    }

    cout << "TCP Server stopped\n";
    return 0;
}
