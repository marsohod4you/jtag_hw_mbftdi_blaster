#ifndef JTAG_DEBUG_H
#define JTAG_DEBUG_H

#ifdef _WINDOWS
#else
#define __cdecl
#endif

void __cdecl printd_(const char *format, ...);

#if DBGPRINT
#define printd printd_
#else
#define printd(...)
#endif

#endif //JTAG_DEBUG_H
