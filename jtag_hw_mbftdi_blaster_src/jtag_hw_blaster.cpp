//
// jtag_hw_blaster.cpp:  altera/intel jtagsrv blaster helper DLL.
//
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WINDOWS
#include <windows.h>
#else
#include <unistd.h>
#include <ctype.h>
#include <stdarg.h>
#endif

#include "../common/debug.h"
#include "../common/jtagsrv.h"
#include "config.h"
#include "CConfig.h"
#include "jtag_hw_blaster.h"

struct jtagsrv_interface jtagsrvi;
jblaster* g_pblaster[MAX_DEV_NUM];

jblaster::jblaster( int idx ):dev_idx_(idx)
{
};

jblaster::~jblaster()
{
};

void jblaster::allocJtagTask(unsigned int count, int need_tdo)
{
	int buf_size = max((int)count + MIN_TASK_BUF_SIZE/2, MIN_TASK_BUF_SIZE);
	int jtask_mem_size = sizeof(struct jtag_task) + buf_size;
	jtask_ = (struct jtag_task*)new char[jtask_mem_size];
	jtask_->buf_size = buf_size;
	jtask_->need_tdo = need_tdo;
	jtask_->wr_idx = 0;
	//printd("alloc jtask count %d need tdo %d sz %d", count, need_tdo, buf_size);
}

void jblaster::reallocJtagTask( unsigned int count )
{
	int new_buf_size = jtask_->buf_size + max((int)count + MIN_TASK_BUF_SIZE / 2, MIN_TASK_BUF_SIZE);
	int jtask_mem_size = sizeof(struct jtag_task) + new_buf_size;
	struct jtag_task* jt = (struct jtag_task*)new char[jtask_mem_size];
	jt->buf_size = new_buf_size;
	jt->need_tdo = jtask_->need_tdo;
	jt->wr_idx = jtask_->wr_idx;
	memcpy(jt->data, jtask_->data, jtask_->wr_idx);
	char* ptr = (char*)jtask_;
	delete[]ptr;
	jtask_ = jt;
	//printd("realloc jtask count %d need tdo %d sz %d", count, jt->need_tdo, jt->buf_size);
}

void jblaster::checkJtagTask( unsigned int count, unsigned int idx )
{
	int need_tdo = 0;
	if (jtagsrvi.jtagsrv_get_bits_attr) {
		uint32_t r = jtagsrvi.jtagsrv_get_bits_attr((void*)jtagsrv_context_, idx);
		if ((r & 0x80000000) == 0)
			need_tdo = 1;
		int sz = r & 0x7FFFFFFF;
		//printd("jsrv need_tdo %d sz %d\n", need_tdo, sz);
	}

	if (jtask_) {
		//jtask already started
		//is task of same type?
		if (jtask_->need_tdo == need_tdo) {
			//same type task, but has space enough?
			unsigned int space = jtask_->buf_size - jtask_->wr_idx;
			if (space < count) {
				//need more space
				reallocJtagTask(count);
			}
			else {
				//continue existing jtask..
			}
		}
		else {
			//another type of task, so put existing task into queue
			jtag_queue_.push_back(jtask_);
			//start new task of new current type
			allocJtagTask(count, need_tdo);
		}
	}
	else {
		//no jtask yet, so create it!
		allocJtagTask(count, need_tdo);
	}
}

void jblaster::write_pattern(char tms, char tdi, unsigned int count, unsigned int idx)
{
	//printd("\nwrite_pattern: %d %d %d\n", tms, count, idx);
	checkJtagTask( count, idx );
	char c=0;
	if (tms) c |= TMS_BIT;
	if (tdi) c |= TDI_BIT;
	for (unsigned int i = 0; i<count; i++) {
		jtask_->data[jtask_->wr_idx++] = c;
	}
	num_bits_in_queue_ += count;
}

unsigned int jblaster::write_masked(char tms, uint32_t* ptdibitarray, unsigned int count, unsigned int idx)
{
	printd("\nwrite_masked: %d %d %d\n", tms, count, idx);
	checkJtagTask(count, idx);
	for (unsigned int i = 0; i<count; i++)
	{
		uint32_t dw, mask;
		char tdi;
		dw = ptdibitarray[i / 32];
		mask = 1 << (i & 0x1f);
		if (dw & mask)
			tdi = 1;
		else
			tdi = 0;
		char c = 0;
		if (tms) c |= TMS_BIT;
		if (tdi) c |= TDI_BIT;
		jtask_->data[jtask_->wr_idx++] = c;
	}
	num_bits_in_queue_ += count;
	return 0;
}

unsigned int jblaster::send_recv(unsigned int need_rdata)
{
	int r = 1;
	//printd("send-recv\n");
	if (jtask_) {
		jtag_queue_.push_back(jtask_);
		jtask_ = { nullptr };
	}

	if (last_bits_flags_org_ == 0x13) {
		while (!jtag_queue_.empty()) {
			struct jtag_task* jt = jtag_queue_.front();
			//printd("send-recv: need tdo %d bits %d in list %d\n",jt->need_tdo,jt->wr_idx, jtag_queue_.size());
			//write_jtag_stream(jt); //jtag mode
			jt->need_tdo = 1;
			write_jtag_stream_as(jt); //active serial
			jtag_queue_.pop_front();
			num_bits_in_queue_ -= jt->wr_idx;
			delete[]jt;
		}
	}
	else {
		while (!jtag_queue_.empty()) {
			struct jtag_task* jt = jtag_queue_.front();
			//printd("send-recv: need tdo %d bits %d in list %d\n",jt->need_tdo,jt->wr_idx, jtag_queue_.size());
			//jt->need_tdo = 1;
			write_jtag_stream( jt ); //jtag mode
			jtag_queue_.pop_front();
			num_bits_in_queue_ -= jt->wr_idx;
			delete[]jt;
		}
	}

	curr_idx_ = 0;
	num_bits_in_queue_ = 0;
	return r;
}

unsigned int jblaster::printTdiTms(const unsigned char* buf, int num_bits)
{
	unsigned char dbg[16];
	unsigned char c;
	memset(dbg, 0, sizeof(dbg));
	int num = min(num_bits,64);
	for (int i = 0; i < num; i++) {
		if ((i & 3) == 0)
			c = buf[i];
		else
		if ((i & 3) == 1)
			c = c | (buf[i]<<2);
		else
		if ((i & 3) == 2)
			c = c | (buf[i] << 4);
		else
		if ((i & 3) == 3)
			c = c | (buf[i] << 6);
		dbg[i / 4] = c;
	}
	printd("tms-tdi (%d): %02X %02X %02X %02X %02X %02X\n", num_bits, dbg[0], dbg[1], dbg[2], dbg[3], dbg[4], dbg[5] );
	return 0;
}

unsigned int jblaster::checkSum(const unsigned char* buf, int num_bits )
{
	int k;
	int len = (num_bits + 7) / 8;
	unsigned int POLY = 0x82f63b78;
	unsigned int crc = 0;
	while (len--) {
		crc ^= *buf++;
		for (k = 0; k < 8; k++)
			crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
	}
	return ~crc;
}

//------- HWPROC -------

struct hw_descriptor hw_descriptor_my;

//jtag server enumerates attached blasters via this function
//port_num -> varies from 0 and up until function returns zero
//pblaster_name -> dest name ptr like "USB-0", should be filled by function with existed blaster name
//blaster_name_sz -> dest buffer size
unsigned int hwproc_listdev(int port_num, char* pblaster_name, int blaster_name_sz)
{
	//do not support more then 4 blasters
	if (port_num>MAX_DEV_NUM - 1)
		return 0;

	//do we have ftdi attached?
	int numchips = SearchBlasters(port_num, pblaster_name, blaster_name_sz);
	if (numchips <= 0)
		return 0;	//no FTDI found or error

	if (port_num >= numchips)
		return 0;	//no more FTDI chips

	return 1;
}

int g_num_open = 0;

//called by jtagsrv opening desired blaster
int hwproc_open_init(
	void** pmycontext,		//put my context pointer here
	char* pdevname,			//string like "USB-0"
	struct jtagsrv_interface* pjtagsrv_struct,	//jtagsrv struct has pointers to some usefull functions
	void* jtagsrv_context	//jtagsrv functions context
)
{
	//result OK
	int r = 0x56;
	int status;
	int dev_idx = PortName2Idx(pdevname);

	if (dev_idx < 0) {
		return 0;
	}

	printd("------------------ (%d) open jtag blaster %d %s --------------------\n", g_num_open, dev_idx, pdevname);

	jblaster* pblaster = g_pblaster[dev_idx] = CreateBlaster(dev_idx);
	if (pblaster == nullptr)
		return 0;
	
	status = pblaster->configure();
	if ( status==0 ) {
		printd("cannot configure MPSSE %d %s\n", dev_idx, pdevname);
		return 0;
	}

	//give them our context
	*pmycontext = (void*)pblaster;

	//save jtagsrv interface struct
	unsigned int jtagsrvi_sz = sizeof(jtagsrvi);
	if (jtagsrvi_sz>pjtagsrv_struct->size)
		jtagsrvi_sz = pjtagsrv_struct->size;
	memcpy(&jtagsrvi, pjtagsrv_struct, jtagsrvi_sz);

	//save jtagsrv context
	pblaster->jtagsrv_context_ = jtagsrv_context;
	return r;
}

unsigned int hwproc_close(unsigned int* pmycontext)
{
	jblaster* pblaster = (jblaster*)pmycontext;
	printd("hwproc_close %p\n", pmycontext);
	DeleteBlaster(pblaster);
	return 0;
}

unsigned int hwproc_unkn2(void* a, unsigned int b, unsigned int c, unsigned int d, unsigned int e)
{
	int r = 0;
	printd("hwproc_unkn2 %p %08X %08X %08X %08X\n", a, b, c, d, e);
	return r;
}

unsigned int hwproc_send_recv(void* context, unsigned int need_rdata)
{
	jblaster* pblaster = (jblaster*)context; 
	//printd("hwproc_send_recv %d %d\n", need_rdata);
	unsigned int r = pblaster->send_recv(need_rdata);
	return r;
}

//with UsbBlaster this fnction writes into programmer single byte command and reads one byte status
unsigned int hwproc_write_flags_read_status(void* context, unsigned int flags, unsigned int* pstatus)
{
	jblaster* pblaster = (jblaster*)context; 
	printd("hwproc_write_flags_read_status %p %08X %p\n",context,flags,pstatus);
	unsigned int ftStatus = pblaster->write_flags_read_status(flags, pstatus);
	printd("status: %08X\n", *pstatus);
	return 0;
}

unsigned int hwproc_unkn11(void* a, unsigned int b, unsigned int c, unsigned int d, unsigned int e)
{
	int r=0;
	printd("hwproc_unkn11 %p %08X %08X %08X %08X\n",a,b,c,d,e);
	return r;
}

unsigned int hwproc_write_pattern(void* context, unsigned int tms, unsigned int tdi, unsigned int count, unsigned int idx)
{
	jblaster* pblaster = (jblaster*)context; 
	//printd("hwproc_pattern %p %d %d %d %d\n",context,tms,tdi,count,idx);
	pblaster->write_pattern(tms,tdi,count,idx);
	return 0;
}

unsigned int hwproc_write_masked(void* context, unsigned int tms, unsigned int* ptdibitarray, unsigned int count, unsigned int idx)
{
	jblaster* pblaster = (jblaster*)context; 
	//printd("hwproc_write_masked %p %d %p %d\n",context,tms,ptdibitarray,count);
	pblaster->write_masked(tms, ptdibitarray, count, idx);
	return 0;
}

void init_my_hw_descr()
{
	memset((void*)&hw_descriptor_my,0,sizeof(struct hw_descriptor));
	hw_descriptor_my.size = sizeof(hw_descriptor_my);
#ifdef _WINDOWS
	strncpy_s((char*)&hw_descriptor_my.hw_name[0], sizeof(hw_descriptor_my.hw_name),GetBlasterName(),sizeof(hw_descriptor_my.hw_name));
#else
	strncpy  ((char*)&hw_descriptor_my.hw_name[0], GetBlasterName(),sizeof(hw_descriptor_my.hw_name));
#endif
	hw_descriptor_my.unknown = 0x3802; //0x800
	hw_descriptor_my._hwproc_listdev  = (pfunc_listdev)hwproc_listdev;
	hw_descriptor_my._hwproc_open_init = (pfunc_open_init)hwproc_open_init;
	hw_descriptor_my._hwproc_close = (pfunc_close)hwproc_close;
	hw_descriptor_my._hwproc_write_pattern = (pfunc_write_pattern)hwproc_write_pattern;
	hw_descriptor_my._hwproc_write_masked = (pfunc_write_masked)hwproc_write_masked;
	hw_descriptor_my._hwproc_send_recv = (pfunc_send_recv)hwproc_send_recv;
	hw_descriptor_my._hwproc_write_flags_read_status = (pfunc_write_flags_read_status)hwproc_write_flags_read_status;

	hw_descriptor_my._unkn2 = (FUNCx)hwproc_unkn2;
	//hw_descriptor_my.func11 = (FUNCx)hwproc_unkn11;
}

extern "C" 
#ifdef _WINDOWS
__declspec(dllexport)
#endif
struct hw_descriptor* get_supported_hardware(int hw_id)
{
	printd("Hello MBFTDI JTAG!\n");
#if _DEBUG
	printd("get_supported_hardware was called!!! %08X\n",hw_id);
#endif
	if(hw_id>0)
		return nullptr;

#ifdef _WINDOWS
	if(InitBlasterLibrary()<0)
		return nullptr;
#endif
	init_my_hw_descr();
	return &hw_descriptor_my;
}
