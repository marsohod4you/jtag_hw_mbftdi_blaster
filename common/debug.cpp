#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cstdarg>
#include <cctype>
#include <iostream>
#include <mutex>

#include "debug.h"

#ifdef _WINDOWS
#define LOG_FILE_NAME "d:\\common\\jtag\\jtag_hw_logfile.txt"
#else
#define LOG_FILE_NAME "/home/nick/jtag_logfile.txt"
#endif

#if DBGPRINT

int slen(char* msg)
{
	int i = 0;
	while (msg[i]) i++;
	return i;
}

std::mutex g_dbg_mutex;

void __cdecl printd_(const char *format, ...)
{
	char    buf[4096], *p = buf;
	va_list args;
	int     n;

	std::lock_guard<std::mutex> lock(g_dbg_mutex);

	va_start(args, format);
#ifdef _WINDOWS
	n = _vsnprintf_s(p, sizeof(buf) - 3, sizeof(buf) - 3, format, args); // buf-3 is room for CR/LF/NUL
#else
	n = vsnprintf(p, sizeof(buf) - 3, format, args); // buf-3 is room for CR/LF/NUL
#endif
	va_end(args);

	p += (n < 0) ? sizeof buf - 3 : n;

	while (p > buf  &&  isspace(p[-1]))
		*--p = '\0';

	*p++ = '\r';
	*p++ = '\n';
	*p = '\0';
#ifdef _WINDOWS
	FILE * pLogFile = nullptr;
	errno_t err = fopen_s( &pLogFile,LOG_FILE_NAME, "ab");
	fwrite(buf, sizeof(char), slen(buf), pLogFile);
	fclose(pLogFile);
#else
	FILE * pLogFile = nullptr;
	pLogFile = fopen( LOG_FILE_NAME, "ab");
	fwrite(buf, sizeof(char), slen(buf), pLogFile);
	fclose(pLogFile);
	//std::cout << buf;
#endif
}

#endif

