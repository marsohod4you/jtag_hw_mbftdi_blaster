#pragma once

#include <thread>
#include <list>
#include <mutex>

using namespace std;

#ifdef X32
#define PROG_NAME_SUFFIX " (32)"
#else
#ifdef X64
#define PROG_NAME_SUFFIX " (64)"
#else
#error "platform not defined"
#endif
#endif

//-----------------------------
//FTDI related definitions, vars
//-----------------------------
#define DEV_NAME_SUFF_OFFSET 6
#define NUM_NODES 32
#define MAX_DEV_NUM 4
#define BLK_SIZE 16 
#define BUF_SIZE (1024*1024*16)
#define RW_BUF_SIZE (1024*128)

#define MIN_TASK_BUF_SIZE (1024*16)
#define TDI_BIT 1
#define TMS_BIT 2

struct jtag_task {
	unsigned int need_tdo;
	unsigned int buf_size;
	unsigned int wr_idx;
	unsigned char data[1];
};

class jblaster {
public:
	jblaster( int idx );
	~jblaster();
	void write_pattern(char tms, char tdi, unsigned int count, unsigned int idx);
	unsigned int write_masked(char tms, uint32_t* ptdibitarray, unsigned int count, unsigned int idx);
	unsigned int send_recv(unsigned int need_rdata);
	virtual unsigned int write_flags_read_status(unsigned int flags, unsigned int* pstatus) { return 0; };
	virtual int configure() { return 1; };
	void*  jtagsrv_context_{ nullptr };
	unsigned int checkSum(const unsigned char* buf, int num_bits);
	unsigned int printTdiTms(const unsigned char* buf, int num_bits);
protected:
	void allocJtagTask(unsigned int count, int need_tdo);
	void reallocJtagTask(unsigned int count);
	void checkJtagTask( unsigned int count, unsigned int idx );
	//unsigned int write_read_as_buffer(unsigned int wr_len, unsigned int rd_len, unsigned int bitsidx, unsigned int need_read);
	virtual unsigned int write_jtag_stream_as( struct jtag_task* jt ) { return 1; };
	virtual unsigned int write_jtag_stream( struct jtag_task* jt ) { return 1; };
	virtual unsigned int read_pass_jtagsrv(unsigned int num_bytes, unsigned char* rbufn) { return 0; };
	//unsigned int flush_passive_serial();

	int dev_idx_{ 0 };
	unsigned char last_bits_flags_{ 0 };
	unsigned char last_bits_flags_org_{ 0 };
	int mode_as_{ 0 }; //mean Active Serial mode
	int curr_idx_{ 0 };
	int num_rbytes_{ 0 };
	unsigned char sbuf_[RW_BUF_SIZE];
	unsigned char jbuf_[RW_BUF_SIZE];
	unsigned char rbuf_[RW_BUF_SIZE];
	unsigned char rbufn_[RW_BUF_SIZE];

	unsigned int num_bits_in_queue_{ 0 };
	list<struct jtag_task*> jtag_queue_;
	struct jtag_task* jtask_{ nullptr };
};

extern struct jtagsrv_interface jtagsrvi;
extern int InitBlasterLibrary();
extern char* GetBlasterName();
extern int PortName2Idx(const char* PortName);
extern jblaster* CreateBlaster(int idx);
extern void DeleteBlaster(jblaster* jbl);
extern int SearchBlasters(int port_num, char* pblaster_name, int blaster_name_sz);
