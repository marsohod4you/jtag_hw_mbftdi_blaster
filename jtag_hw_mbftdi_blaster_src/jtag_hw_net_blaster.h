#pragma once

#include <thread>
#include <list>
#include <mutex>
#include "jtag_hw_blaster.h"

using namespace std;

//-----------------------------
//USB Blaster related definitions
//-----------------------------
#define DEV_NAME "MBUSB-0"
#define PROGRAMER_NAME "NW-JTAG-v1.9b"

class net_blaster : public jblaster {
public:
	net_blaster( int idx );
	~net_blaster();
	unsigned int write_flags_read_status(unsigned int flags, unsigned int* pstatus);
	int configure();
private:
	unsigned int write_read_as_buffer(unsigned int wr_len, unsigned int rd_len, unsigned int bitsidx, unsigned int need_read);
	unsigned int write_jtag_stream_as(unsigned int start_idx, unsigned int count, unsigned int need_read);
	unsigned int write_jtag_stream( struct jtag_task* jt );
	unsigned int flush_passive_serial();

	unsigned char last_bits_flags_{ 0 };
	unsigned char last_bits_flags_org_{ 0 };
	int mode_as_{ 0 }; //mean Active Serial mode
	int curr_idx_{ 0 };
	int num_rbytes_{ 0 };
	unsigned char sbuf_[RW_BUF_SIZE];
	unsigned char jbuf_[RW_BUF_SIZE];
	unsigned char rbuf_[RW_BUF_SIZE];
	unsigned char rbufn_[RW_BUF_SIZE];
	char* tdi_{ nullptr };
	char* tms_{ nullptr };
	CSocket* tcp_sock{ nullptr };
};
