#pragma once

#include <thread>
#include <list>
#include <mutex>
#include "jtag_hw_blaster.h"

using namespace std;

/* define JTAG programmer properties */
#define PROGRAMER_NAME "MBFTDI-Blaster v2.1b"

//select neccessary channels A or B or both
#define USE_CHANNEL_A 1
#define USE_CHANNEL_B 0

//check for predefined vendor ID and product ID

//ft4232
#define VIDPID_FT4232 0x04036011

//ft2232
#define VIDPID_FT2232 0x04036010

//check for serial number range
#define CHECK_SERIAL 0
#define SERIAL_PREFIX "EPT"
#define SERIAL_MIN 8000000
#define SERIAL_MAX 8000200

//check for description string
#define CHECK_DESCRIPTION 0
#define NEED_DESCRIPTION "EPT USB <-> Serial&JTAG Cable"

//-----------------------------
//USB Blaster related definitions
//-----------------------------
#define DEV_NAME "MBUSB-0"

//-----------------------------
//FTDI related definitions, vars
//-----------------------------
#define DEV_NAME_SUFF_OFFSET 6
#define NUM_NODES 32
#define MAX_DEV_NUM 4
#define BLK_SIZE 16 
#define BUF_SIZE (1024*1024*16)
#define RW_BUF_SIZE (1024*128)

class ftdi_blaster : public jblaster {
public:
	ftdi_blaster( int idx );
	~ftdi_blaster();
	unsigned int write_flags_read_status(unsigned int flags, unsigned int* pstatus);
	unsigned int read_pass_jtagsrv(unsigned int num_bytes, unsigned char* rbufn);
	int configure();
private:
	FT_STATUS resetDevice();
	int configureMpsse();
	void set_freq(float freq);
	FT_STATUS wait_answer(unsigned int expect_num_bytes);

	unsigned int write_read_as_buffer(unsigned int wr_len, unsigned int rd_len, unsigned int bitsidx, unsigned int need_read);
	unsigned int write_jtag_stream_as( struct jtag_task* jt );
	unsigned int write_jtag_stream( struct jtag_task* jt );
	unsigned int flush_passive_serial();

	FT_HANDLE ftHandle_{ nullptr };
	//unsigned char last_bits_flags_{ 0 };
	//unsigned char last_bits_flags_org_{ 0 };
	int mode_as_{ 0 }; //mean Active Serial mode
	int curr_idx_{ 0 };
	int num_rbytes_{ 0 };
	unsigned char sbuf_[RW_BUF_SIZE];
	unsigned char jbuf_[RW_BUF_SIZE];
	unsigned char rbuf_[RW_BUF_SIZE];
	unsigned char rbufn_[RW_BUF_SIZE];
	char* tdi_{ nullptr };
	char* tms_{ nullptr };
};
