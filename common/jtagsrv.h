#ifndef JTAGSRV_DEFS_H
#define JTAGSRV_DEFS_H

#include <cstdint>

#ifdef _WINDOWS
#else
#define __cdecl
#define _cdecl
#endif

//-----------------------------
//jtagsrv interface
typedef	unsigned int (_cdecl * pfunc_jtagsrv_pass_data) (void*,unsigned char*,unsigned int);
typedef	unsigned int(_cdecl * pfunc_jtagsrv_report) (void*, unsigned int, unsigned int);
typedef	unsigned int (_cdecl * pfunc_jtagsrv_get_bits_attr) (void*,unsigned int);
typedef	unsigned int(_cdecl * FUNC4) (void*, unsigned int, unsigned int, unsigned int, unsigned int);
typedef	unsigned int (_cdecl * FUNC1) (void*);

struct jtagsrv_interface{
	unsigned int size;
	pfunc_jtagsrv_pass_data	jtagsrv_pass_data;
	pfunc_jtagsrv_report	jtagsrv_report;
	FUNC4	jtagsrv_func2;
	pfunc_jtagsrv_get_bits_attr	jtagsrv_get_bits_attr;
	FUNC4	jtagsrv_func4;
	FUNC4	jtagsrv_func5;
	FUNC4	jtagsrv_func6;
	FUNC4	jtagsrv_func7;
};

typedef unsigned int(_cdecl * pfunc_write_masked) (void*, unsigned int, unsigned int*, unsigned int);
typedef unsigned int(_cdecl * pfunc_close) (unsigned int*);
typedef unsigned int(_cdecl * pfunc_open_init) (void**, char*, struct jtagsrv_interface*, void*);
typedef unsigned int(_cdecl * pfunc_send_recv) (void*, unsigned int);
typedef unsigned int(_cdecl * pfunc_listdev) (int, char*, int);
typedef unsigned int(_cdecl * pfunc_write_pattern) (void*, unsigned int, unsigned int, unsigned int, unsigned int);
typedef	unsigned int(_cdecl * FUNCx) (void*, unsigned int, unsigned int, unsigned int, unsigned int);
typedef	unsigned int(_cdecl * FUNC3) (void*, char*, unsigned int);
typedef unsigned int(_cdecl * pfunc_write_flags_read_status)(void*, unsigned int flags, unsigned int* pstatus);

//-----------------------------
//jtag blaster hw struct
//-----------------------------

struct hw_descriptor {
	unsigned int size;
	uint8_t  hw_name[32];
	unsigned int unknown;
	FUNCx func00;					//0
	FUNCx func01;					//1
	pfunc_listdev _hwproc_listdev;	//2
	FUNCx _unkn2;					//3
	pfunc_open_init _hwproc_open_init;	//4
	pfunc_close _hwproc_close;		//5
	FUNCx func06;					//6
	FUNCx func07;					//7
	pfunc_write_flags_read_status _hwproc_write_flags_read_status; //8
	pfunc_write_pattern _hwproc_write_pattern;	//9
	pfunc_write_masked _hwproc_write_masked;	//10
	FUNCx func11;					//11
	pfunc_send_recv _hwproc_send_recv;			//12
	FUNCx func13;					//13
	FUNCx func14;					//14
	FUNCx func15;					//15
	FUNCx func16;					//16
	FUNCx func17;					//17
	FUNCx func18;					//17
	FUNCx func19;					//17
	FUNCx func20;					//17
};

struct hw_descriptor_simple {
	unsigned int size;
	char  hw_name[32];
	unsigned int unknown;
	void* pfunc[32];
};

unsigned int hwproc_send_recv(void* context, unsigned int need_rdata);
unsigned int hwproc_write_flags_read_status(void* context, unsigned int flags, unsigned int* pstatus);
unsigned int hwproc_write_pattern(void* context, unsigned int tms, unsigned int tdi, unsigned int count, unsigned int idx);
unsigned int hwproc_listdev(int port_num, char* pblaster_name, int blaster_name_sz);
int hwproc_open_init(
	void** pmycontext,		//put my context pointer here
	char* pdevname,			//string like "USB-0"
	struct jtagsrv_interface* pjtagsrv_struct,	//jtagsrv struct has pointers to some usefull functions
	void* jtagsrv_context	//jtagsrv functions context
	);

#endif //JTAGSRV_DEFS_H
