#ifndef _NW_JTAG_PROTO_H_
#define _NW_JTAG_PROTO_H_

#define NW_JTAG_UDP_PORT 8888
#define NW_JTAG_TCP_PORT 8889

#include <stdint.h>
#include <stddef.h>

#define CMD_JTAG_WR_FLAGS_RD_STATUS 0xA1
#define CMD_JTAG_WR_TDI_TMS_BUF 0xA2
#define CMD_JTAG_WR_TDI_TMS_BUF_R 0xA3
#define VERSION 102

struct nw_cmd {
	uint32_t len; //length of command after len field
    uint32_t cmd; //commands like CMD_JTAG_xxx
	uint32_t param1;
	uint32_t param2;
    char data[0];
	uint32_t size() { return offsetof(struct nw_cmd,data); };
	uint32_t dataSize() { return len-size()+sizeof(len); };
	void setLength(uint32_t data_length) { len = size() - sizeof(len) + data_length;  };
};

#define UDP_TAG0_STR "SRV_"
#define UDP_TAG1_STR "JTAG"
#define UDP_CMD_DISCOVER 0xAA01
#define UDP_CMD_ACK 0xAA02

struct udp_cmd {
    char tag0[4]; //SRV_
    char tag1[4]; //JTAG
    char tag2[4];
    char tag3[4];
    int	 ver;
    int	 cmd;
    char data[4]; //or more then 4
};

#endif //_NW_JTAG_PROTO_H_
