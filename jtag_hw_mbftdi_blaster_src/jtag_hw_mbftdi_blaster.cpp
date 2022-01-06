// jtag_hw_mbftdi_blaster.cpp:  altera/intel jtagsrv blaster helper DLL.
//
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fstream>
#include <string>
#include <sstream>

#ifdef _WINDOWS
#include <windows.h>
#include "ftd2xx.h"
void do_sleep(int ms) { Sleep(ms);  }
#else
#include <unistd.h>
#include <ctype.h>
#include <stdarg.h>
#include "lftdi136/ftd2xx.h"
//#include "linux_ftdi/ftd2xx.h"
void do_sleep(int ms) { usleep(ms*1000); }
#endif

#include "../common/debug.h"
#include "../common/jtagsrv.h"
#include "config.h"
#include "jtag_hw_mbftdi_blaster.h"

int  g_cfg_channel = 0;
bool g_cfg_channel_set = false;
int  g_cfg_frequency = 10000000;
bool g_cfg_frequency_set = false;

#ifdef _WINDOWS

bool read_blaster_config()
{
	char cCurrentPath[FILENAME_MAX];
	DWORD ret = GetModuleFileNameA(NULL, cCurrentPath, sizeof(cCurrentPath));
	if (ret == 0 || GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
		return false;
	}
	//search latest slash in the path
	char* ptr = cCurrentPath;
	char* ptr2slash = nullptr;
	for (int i = 0; i < sizeof(cCurrentPath); i++) {
		if (*ptr == 0)
			break;
		if (*ptr == '\\')
			ptr2slash = ptr;
		ptr++;
	}
	if (ptr2slash) {
		//slash found
		ptr2slash++;
		*ptr2slash = 0;
		printd("Working directory: %s", cCurrentPath);
		strcat(cCurrentPath, "mbftdi.cfg"); //mbftdi.cfg string is shorter then jtagserver.exe, should fit

		try {
			ifstream fin(cCurrentPath);
			if (fin.is_open()) {
				string line, sleft, sright;

				while (getline(fin, line)) {
					size_t pos = line.find("=");
					if (pos == string::npos) {
						//"=" was not found, so skip this line
						continue;
					}
					sleft = line.substr(0, pos);
					sright = line.substr(pos + 1);
					printd("Cfg: %s %s", sleft.c_str(), sright.c_str());
					if (sleft == "channel") {
						g_cfg_channel = stoi(sright, 0, 10); g_cfg_channel_set = true;
					}
					if (sleft == "frequency") {
						g_cfg_frequency = stoi(sright, 0, 10); g_cfg_frequency_set = true;
					}
				}
			}
		}
		catch (...) {
			return false;
		}
	}

	return true;
}

//-----------------------------
//FTDI load DLL
//-----------------------------

typedef FT_STATUS(WINAPI *PtrToOpen)(unsigned int, FT_HANDLE *);
PtrToOpen g_pOpen = nullptr;

typedef FT_STATUS(WINAPI *PtrToOpenEx)(PVOID, unsigned int, FT_HANDLE *);
PtrToOpenEx g_pOpenEx = nullptr;

typedef FT_STATUS(WINAPI *PtrToListDevices)(PVOID, PVOID, unsigned int);
PtrToListDevices g_pListDevices = nullptr;

typedef FT_STATUS(WINAPI *PtrToClose)(FT_HANDLE);
PtrToClose g_pClose = nullptr;

typedef FT_STATUS(WINAPI *PtrToRead)(FT_HANDLE, LPVOID, unsigned int, unsigned int*);
PtrToRead g_pRead = nullptr;

typedef FT_STATUS(WINAPI *PtrToWrite)(FT_HANDLE, LPVOID, unsigned int, unsigned int*);
PtrToWrite g_pWrite = nullptr;

typedef FT_STATUS(WINAPI *PtrToResetDevice)(FT_HANDLE);
PtrToResetDevice g_pResetDevice = nullptr;

typedef FT_STATUS(WINAPI *PtrToPurge)(FT_HANDLE, ULONG);
PtrToPurge g_pPurge = nullptr;

typedef FT_STATUS(WINAPI *PtrToSetTimeouts)(FT_HANDLE, ULONG, ULONG);
PtrToSetTimeouts g_pSetTimeouts = nullptr;

typedef FT_STATUS(WINAPI *PtrToGetQueueStatus)(FT_HANDLE, unsigned int*);
PtrToGetQueueStatus g_pGetQueueStatus = nullptr;

typedef FT_STATUS(WINAPI *PtrToSetChars)(FT_HANDLE, UCHAR, UCHAR, UCHAR, UCHAR);
PtrToSetChars g_pSetChars = nullptr;

typedef FT_STATUS(WINAPI *PtrToCreateDeviceInfoList)(unsigned int*);
PtrToCreateDeviceInfoList g_pCreateDeviceInfoList = nullptr;

typedef FT_STATUS(WINAPI *PtrToGetDeviceInfoList)(PVOID, unsigned int*);
PtrToGetDeviceInfoList g_pGetDeviceInfoList = nullptr;

typedef FT_STATUS(WINAPI *PtrToSetBitMode)(FT_HANDLE, UCHAR, UCHAR);
PtrToSetBitMode g_pSetBitMode = nullptr;

typedef FT_STATUS(WINAPI *PtrToSetUSBParameters)(FT_HANDLE, ULONG, ULONG);
PtrToSetUSBParameters g_pSetUSBParameters = nullptr;

typedef FT_STATUS(WINAPI *PtrToSetLatencyTimer)(FT_HANDLE, UCHAR);
PtrToSetLatencyTimer g_pSetLatencyTimer = nullptr;

int InitBlasterLibrary()
{
	HMODULE hmodule = LoadLibraryA("Ftd2xx.dll");
	if (hmodule == nullptr) {
		printd("Error: Can't load FTDI Dll\n");
		return -1;
	}

	g_pWrite = (PtrToWrite)GetProcAddress(hmodule, "FT_Write");
	if (g_pWrite == nullptr) {
		printd("Error: Can't Find FT_Write\n");
		return -1;
	}

	g_pRead = (PtrToRead)GetProcAddress(hmodule, "FT_Read");
	if (g_pRead == nullptr) {
		printd("Error: Can't Find FT_Read\n");
		return -1;
	}

	g_pOpen = (PtrToOpen)GetProcAddress(hmodule, "FT_Open");
	if (g_pOpen == nullptr) {
		printd("Error: Can't Find FT_Open\n");
		return -1;
	}

	g_pOpenEx = (PtrToOpenEx)GetProcAddress(hmodule, "FT_OpenEx");
	if (g_pOpenEx == nullptr) {
		printd("Error: Can't Find FT_OpenEx\n");
		return -1;
	}

	g_pListDevices = (PtrToListDevices)GetProcAddress(hmodule, "FT_ListDevices");
	if (g_pListDevices == nullptr) {
		printd("Error: Can't Find FT_ListDevices\n");
		return -1;
	}

	g_pClose = (PtrToClose)GetProcAddress(hmodule, "FT_Close");
	if (g_pClose == nullptr) {
		printd("Error: Can't Find FT_Close\n");
		return -1;
	}

	g_pResetDevice = (PtrToResetDevice)GetProcAddress(hmodule, "FT_ResetDevice");
	if (g_pResetDevice == nullptr) {
		printd("Error: Can't Find FT_ResetDevice\n");
		return -1;
	}

	g_pPurge = (PtrToPurge)GetProcAddress(hmodule, "FT_Purge");
	if (g_pPurge == nullptr) {
		printd("Error: Can't Find FT_Purg\n");
		return -1;
	}

	g_pSetTimeouts = (PtrToSetTimeouts)GetProcAddress(hmodule, "FT_SetTimeouts");
	if (g_pSetTimeouts == nullptr) {
		printd("Error: Can't Find FT_SetTimeouts\n");
		return -1;
	}

	g_pGetQueueStatus = (PtrToGetQueueStatus)GetProcAddress(hmodule, "FT_GetQueueStatus");
	if (g_pGetQueueStatus == nullptr) {
		printd("Error: Can't Find FT_GetQueueStatus\n");
		return -1;
	}

	g_pSetChars = (PtrToSetChars)GetProcAddress(hmodule, "FT_SetChars");
	if (g_pSetChars == nullptr) {
		printd("Error: Can't Find FT_SetChars\n");
		return -1;
	}

	g_pCreateDeviceInfoList = (PtrToCreateDeviceInfoList)GetProcAddress(hmodule, "FT_CreateDeviceInfoList");
	if (g_pCreateDeviceInfoList == nullptr) {
		printd("Error: Can't Find FT_CreateDeviceInfoList\n");
		return -1;
	}

	g_pGetDeviceInfoList = (PtrToGetDeviceInfoList)GetProcAddress(hmodule, "FT_GetDeviceInfoList");
	if (g_pGetDeviceInfoList == nullptr) {
		printd("Error: Can't Find FT_GetDeviceInfoList\n");
		return -1;
	}

	g_pSetBitMode = (PtrToSetBitMode)GetProcAddress(hmodule, "FT_SetBitMode");
	if (g_pSetBitMode == nullptr) {
		printd("Error: Can't Find FT_SetBitMode\n");
		return -1;
	}

	g_pSetUSBParameters = (PtrToSetUSBParameters)GetProcAddress(hmodule, "FT_SetUSBParameters");
	if (g_pSetUSBParameters == nullptr) {
		printd("Error: Can't Find FT_SetUSBParameters\n");
		return -1;
	}

	g_pSetLatencyTimer = (PtrToSetLatencyTimer)GetProcAddress(hmodule, "FT_SetLatencyTimer");
	if (g_pSetLatencyTimer == nullptr) {
		printd("Error: Can't Find FT_SetLatencyTimer\n");
		return -1;
	}

	read_blaster_config();

	return 0;
}
#else
//use static linking in Linux
#define g_pOpen FT_Open
#define g_pOpenEx FT_OpenEx
#define g_pListDevices FT_ListDevices 
#define g_pClose FT_Close
#define g_pRead FT_Read
#define g_pWrite FT_Write
#define g_pResetDevice FT_ResetDevice
#define g_pPurge FT_Purge
#define g_pSetTimeouts FT_SetTimeouts
#define g_pGetQueueStatus FT_GetQueueStatus
#define g_pSetChars FT_SetChars
#define g_pCreateDeviceInfoList FT_CreateDeviceInfoList
#define g_pGetDeviceInfoList FT_GetDeviceInfoList
#define g_pSetBitMode FT_SetBitMode
#define g_pSetUSBParameters FT_SetUSBParameters
#define g_pSetLatencyTimer FT_SetLatencyTimer

int InitBlasterLibrary()
{
	return 0;
}

#endif

char* GetBlasterName()
{
	return PROGRAMER_NAME PROG_NAME_SUFFIX;
}

FT_DEVICE_LIST_INFO_NODE g_device_node[NUM_NODES];

//return nonzero if device has SerialNo in range
//FT_DEVICE_LIST_INFO_NODE field SerialNumber
int accept_serialno(char* pserialno, int maxlen)
{
	if (CHECK_SERIAL)
	{
		int i, j = 0;
		unsigned int iSerialNo = 0;
		char Prefix[32];
		Prefix[0] = 0;
		for (i = 0; i<maxlen; i++)
		{
			//check for end of serialno string
			if (pserialno[i] == 0)
				break; //end of string

					   //check for serialno digit
			if (pserialno[i] >= '0' && pserialno[i] <= '9')
			{
				//decimal digit
				iSerialNo = iSerialNo * 10 + (pserialno[i] - 0x30);
				continue;
			}
			else
			{
				//possibly prefix?
				Prefix[j] = pserialno[i];
				j++;
				Prefix[j] = 0;
				if (j == (sizeof(Prefix) - 1))
					break;
			}
		}

		//check range
		if (iSerialNo<SERIAL_MIN || iSerialNo >= SERIAL_MAX)
		{
			printd("Bad serial num %d\n", iSerialNo);
			return 0;
		}
		//check prefix
		Prefix[3] = 0;
		if (memcmp(Prefix, SERIAL_PREFIX, 3) == 0)
			return 1;
		printd("Bad serial prefix %s\n", Prefix);
		return 0;
	}
	else
		return 1; //always accept any serial
}

//return nonzero if this FTDI channel is accepted
//channel determined from struct FT_DEVICE_LIST_INFO_NODE field Description
int accept_channel(char* pdescription, int maxlen)
{
	int i;
	char channel = '-';
	//get channel letter at the end of description string
	for (i = 1; i<maxlen; i++)
	{
		if (pdescription[i] == 0)
		{
			channel = pdescription[i - 1];
			break;
		}
	}

	if (g_cfg_channel_set) {
		//use configuration for channel selection
		if (g_cfg_channel == 0 && channel == 'A')
			return 1;
		if (g_cfg_channel == 1 && channel == 'B')
			return 1;
		if (g_cfg_channel == 2 && channel == 'C')
			return 1;
		if (g_cfg_channel == 3 && channel == 'D')
			return 1;
	}
	else {
		//check channel
		if (USE_CHANNEL_A && channel == 'A')
			return 1;
		if (USE_CHANNEL_B && channel == 'B')
			return 1;
	}
	printd("Bad channel %c\n", channel);
	return 0;
}

//return nonzero if this FTDI description is accepted
//FT_DEVICE_LIST_INFO_NODE field Description
int accept_description(char* pdescription, int maxlen)
{
	if (CHECK_DESCRIPTION)
	{
		int i;
		char need_descr[] = { NEED_DESCRIPTION };

		for (i = 0; i<maxlen; i++)
		{
			if (need_descr[i] == 0)
				return 1;
			if (pdescription[i] != need_descr[i])
			{
				printd("Bad description %s\n", pdescription);
				return 0;
			}
		}

		printd("Bad description %s\n", pdescription);
		return 0;
	}
	else
		return 1;
}

int id2id(int ftdi_id)
{
	int i, j;
	j = 0;
	for (i = 0; i<NUM_NODES; i++)
	{
		if (
			((g_device_node[i].ID == VIDPID_FT2232) || (g_device_node[i].ID == VIDPID_FT4232)) &&
			accept_serialno(g_device_node[i].SerialNumber, sizeof(g_device_node[i].SerialNumber)) &&
			accept_description(g_device_node[i].Description, sizeof(g_device_node[i].Description)) &&
			accept_channel(g_device_node[i].Description, sizeof(g_device_node[i].Description))
			)
		{
			if (ftdi_id == j)
				return i;
			j++;
		}
	}
	return -1;
}

ftdi_blaster::ftdi_blaster( int idx ):jblaster(idx)
{
	// Open the port - For this application note, assume the first device is a FT2232H or FT4232H
	// Further checks can be made against the device descriptions, locations, serial numbers, etc.
	// before opening the port.
	int local_id = id2id(idx);
	printd("Try to open ftdi dev %d, local_id %d\n", idx, local_id);
	if (local_id<0)
	{
		printd("bad local_id %d\n", local_id);
		throw;
	}
	FT_STATUS ftStatus = g_pOpen(local_id, &ftHandle_);
	if (ftStatus != FT_OK)
	{
		printd("Open Failed with error %d\n", ftStatus);
		throw;
	}
};

ftdi_blaster::~ftdi_blaster()
{
	if (mode_as_)
	{
		//make all PIO "as inputs"? or better keep TMS as 1?

		// ADBUS0 TCK output 1 low 0
		// ADBUS1 TDI output 1 low 0
		// ADBUS2 TDO input 0 0
		// ADBUS3 TMS output 1 high 1
		// ADBUS4 GPIOL0 input 0 0
		// ADBUS5 nStatus input 0 0
		// ADBUS6 nCE output 1 0
		// ADBUS7 nCS output 1 0

		unsigned int dwNumBytesToSend = 0;
		unsigned int dwNumBytesSent = 0;
		unsigned char byOutputBuffer[16];
		// Set data bits low-byte of MPSSE port
		byOutputBuffer[dwNumBytesToSend++] = 0x80;
		byOutputBuffer[dwNumBytesToSend++] = 0x00;
		byOutputBuffer[dwNumBytesToSend++] = 0x00;
		g_pWrite(ftHandle_, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
	}

	//close ftdi handle 
	if(ftHandle_)
		g_pClose( ftHandle_ );
};

//-----------------------------
//FTDI related functions
//-----------------------------
int SearchBlasters(int port_num, char* pblaster_name, int blaster_name_sz)
{
unsigned int i;
unsigned int dwNumDevs;
unsigned int myNumDevs = 0;
FT_STATUS ftStatus;

// Does an FTDI device exist?
// Get the number of FTDI devices
printd("Checking for FTDI devices...\n");
ftStatus = g_pCreateDeviceInfoList(&dwNumDevs);

if (ftStatus != FT_OK) // Did the command execute OK?
{
	printd("Error in getting the number of devices\n");
	return -1; // Exit with error
}

if (dwNumDevs < 1) // Exit if we don't see any
{
	printd("There are no FTDI devices installed\n");
	return -1; // Exist with error
}

//limit number of devices
if(dwNumDevs > NUM_NODES-1)
	dwNumDevs = NUM_NODES-1;

memset(g_device_node,0,sizeof(g_device_node));
ftStatus = g_pGetDeviceInfoList(&g_device_node[0],&dwNumDevs);
for(i=0; i<dwNumDevs; i++)
{
	printd(">%d %08X %08X %08X **%s**%s**\n",
		g_device_node[i].Flags,
		g_device_node[i].Type,
		g_device_node[i].ID,
		g_device_node[i].LocId,
		g_device_node[i].SerialNumber,
		g_device_node[i].Description);

	if(
		((g_device_node[i].ID == VIDPID_FT2232) || (g_device_node[i].ID == VIDPID_FT4232)) &&
		accept_serialno(g_device_node[i].SerialNumber,sizeof(g_device_node[i].SerialNumber)) &&
		accept_description(g_device_node[i].Description,sizeof(g_device_node[i].Description)) &&
		accept_channel(g_device_node[i].Description,sizeof(g_device_node[i].Description))
		)
			myNumDevs++;

	char myblastername[] = DEV_NAME;
	memcpy(pblaster_name, myblastername, sizeof(myblastername));
	pblaster_name[DEV_NAME_SUFF_OFFSET] += (char)port_num;
}

printd("FTDI devices found: %d - the count includes individual ports on a single chip\n", dwNumDevs);
printd("FTDI 2232H devices found %d\n", myNumDevs);

if (myNumDevs == 0) // Exit if we don't see any
{
	printd("Cannot get my chips of FTDIs installed\n");
}

return myNumDevs;
}

void DeleteBlaster(jblaster* jbl)
{
	delete static_cast<ftdi_blaster*>(jbl);
}

int PortName2Idx(const char* PortName)
{
	int dev_idx = PortName[DEV_NAME_SUFF_OFFSET] - 0x30;
	return dev_idx;
}

FT_STATUS ftdi_blaster::resetDevice()
{
	BYTE byInputBuffer[1024];
	unsigned int dwNumBytesToRead = 0; // Number of bytes available to read in the driver's input buffer
	unsigned int dwNumBytesRead = 0; // Count of actual bytes read - used with FT_Read
									 //unsigned int dwClockDivisor = 29; // Value of clock divisor, SCL Frequency = 60/((1+vl)*2) (MHz)
	printd("resetDevice\n");

	//Reset USB device
	FT_STATUS ftStatus = 0;
	ftStatus |= g_pResetDevice(ftHandle_);

	//Purge USB receive buffer first by reading out all old data from FT2232H receive buffer

	// Get the number of bytes in the FT2232H receive buffer
	ftStatus |= g_pGetQueueStatus( ftHandle_, &dwNumBytesToRead);

	//Read out the data from FT2232H receive buffer
	if ((ftStatus == FT_OK) && (dwNumBytesToRead > 0))
		g_pRead(ftHandle_, &byInputBuffer, dwNumBytesToRead, &dwNumBytesRead);

	//Set USB request transfer sizes to 64K
	ftStatus |= g_pSetUSBParameters(ftHandle_, 65536, 65535);

	//Disable event and error characters
	ftStatus |= g_pSetChars(ftHandle_, false, 0, false, 0);

	//Sets the read and write timeouts in milliseconds
	ftStatus |= g_pSetTimeouts(ftHandle_, 0, 5000);

	//Set the latency timer (default is 16mS)
	ftStatus |= g_pSetLatencyTimer(ftHandle_, 16);

	//Reset controller
	ftStatus |= g_pSetBitMode(ftHandle_, 0x0, 0x00);
	ftStatus |= g_pSetBitMode(ftHandle_, 0x0, 0x02);
	return ftStatus;
}

void ftdi_blaster::set_freq( float freq )
{
	unsigned int clk_div;
	float freq_achived;
	unsigned int dwNumBytesToSend = 0;
	unsigned int dwNumBytesSent;
	unsigned char byOutputBuffer[32];
	//FT_STATUS ftStatus;

	//iterate to find proper clock divider
	for(clk_div=0; clk_div<0x10000; clk_div++)
	{
		freq_achived = (float)30000000/(float)(clk_div+1);
		if(freq_achived<=freq)
		{
			printd("Frequency is set to %eHz (FTDI clk divider %04X), requred %eHz\n",freq_achived,clk_div,freq);

			//Command to set clock divisor
			byOutputBuffer[dwNumBytesToSend++] = 0x86;
			//Set 0xValueL of clock divisor
			byOutputBuffer[dwNumBytesToSend++] = clk_div & 0xFF;
			//Set 0xValueH of clock divisor
			byOutputBuffer[dwNumBytesToSend++] = (clk_div >> 8) & 0xFF;
			// Send off the clock divisor commands
			//ftStatus = 
				g_pWrite(ftHandle_, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
			return;
		}
	}

	//use default
	printd("Oops\n");
	printd("WARNING: Frequency set was ignored, cannot recognize value!\n");
}

int ftdi_blaster::configureMpsse()
{
BYTE byOutputBuffer[1024];
BYTE byInputBuffer[1024];
BOOL bCommandEchod;
unsigned int dwCount = 0; // General loop index
unsigned int dwNumBytesToSend = 0; // Index to the output buffer
unsigned int dwNumBytesSent = 0; // Count of actual bytes sent - used with FT_Write
unsigned int dwNumBytesToRead = 0; // Number of bytes available to read in the driver's input buffer
unsigned int dwNumBytesRead = 0; // Count of actual bytes read - used with FT_Read
unsigned int dwClockDivisor = 29; // Value of clock divisor, SCL Frequency = 60/((1+vl)*2) (MHz)
FT_STATUS ftStatus;

// -----------------------------------------------------------
// Synchronize the MPSSE by sending a bogus opcode (0xAA),
// The MPSSE will respond with "Bad Command" (0xFA) followed by
// the bogus opcode itself.
// -----------------------------------------------------------
// Reset output buffer pointer
dwNumBytesToSend=0;
//Add bogus command ‘xAA’ to the queue
byOutputBuffer[dwNumBytesToSend++] = 0xAA;
// Send off the BAD commands
ftStatus = g_pWrite(ftHandle_, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
do
{
	// Get the number of bytes in the device input buffer
	ftStatus = g_pGetQueueStatus(ftHandle_, &dwNumBytesToRead);
} while ((dwNumBytesToRead == 0) && (ftStatus == FT_OK));

//Read out the data from input buffer
ftStatus = g_pRead(ftHandle_, &byInputBuffer, dwNumBytesToRead, &dwNumBytesRead);

//Check if Bad command and echo command received
bCommandEchod = false;
for (dwCount = 0; dwCount < dwNumBytesRead - 1; dwCount++)
{
	if ((byInputBuffer[dwCount] == 0xFA) && (byInputBuffer[dwCount+1] == 0xAA))
	{
		bCommandEchod = true;
		break;
	}
}

if (bCommandEchod == false)
{
	printd("Error in synchronizing the MPSSE\n");
	g_pClose(ftHandle_); ftHandle_ = nullptr;
	return -1; // Exit with error
}

// -----------------------------------------------------------
// Configure the MPSSE settings for JTAG
// Multiple commands can be sent to the MPSSE with one FT_Write
// -----------------------------------------------------------

// Set up the Hi-Speed specific commands for the FTx232H

// Start with a fresh index
dwNumBytesToSend = 0;

// Use 60MHz master clock (disable divide by 5)
byOutputBuffer[dwNumBytesToSend++] = 0x8A;
// Turn off adaptive clocking (may be needed for ARM)
byOutputBuffer[dwNumBytesToSend++] = 0x97;
// Disable three-phase clocking
byOutputBuffer[dwNumBytesToSend++] = 0x8D;
ftStatus = g_pWrite(ftHandle_, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);

// Send off the HS-specific commands

dwNumBytesToSend = 0;
// Set initial states of the MPSSE interface - low byte, both pin directions and output values
// Pin name Signal Direction Config Initial State Config
// ADBUS0 TCK output 1 low 0
// ADBUS1 TDI output 1 low 0
// ADBUS2 TDO input 0 0
// ADBUS3 TMS output 1 high 1
// ADBUS4 GPIOL0 input 0 0
// ADBUS5 GPIOL1 input 0 0
// ADBUS6 GPIOL2 input 0 0
// ADBUS7 GPIOL3 input 0 0
// Set data bits low-byte of MPSSE port
byOutputBuffer[dwNumBytesToSend++] = 0x80;
// Initial state config above
byOutputBuffer[dwNumBytesToSend++] = 0x08;
// Direction config above
byOutputBuffer[dwNumBytesToSend++] = 0x0B;
// Send off the low GPIO config commands
ftStatus = g_pWrite(ftHandle_, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);

dwNumBytesToSend = 0;
// Set initial states of the MPSSE interface - high byte, both pin directions and output values
// Pin name Signal Direction Config Initial State Config
// ACBUS0 GPIOH0 input 0 0
// ACBUS1 GPIOH1 input 0 0
// ACBUS2 GPIOH2 input 0 0
// ACBUS3 GPIOH3 input 0 0
// ACBUS4 GPIOH4 input 0 0
// ACBUS5 GPIOH5 input 0 0
// ACBUS6 GPIOH6 input 0 0
// ACBUS7 GPIOH7 input 0 0
// Set data bits low-byte of MPSSE port
byOutputBuffer[dwNumBytesToSend++] = 0x82;
// Initial state config above
byOutputBuffer[dwNumBytesToSend++] = 0x0;
// Direction config above
byOutputBuffer[dwNumBytesToSend++] = 0x0;
// Send off the high GPIO config commands
ftStatus = g_pWrite(ftHandle_, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);

dwNumBytesToSend = 0;
// Set TCK frequency
// TCK = 60MHz /((1 + [(1 +0xValueH*256) OR 0xValueL])*2)
//Command to set clock divisor
byOutputBuffer[dwNumBytesToSend++] = 0x86;
//Set 0xValueL of clock divisor
byOutputBuffer[dwNumBytesToSend++] = dwClockDivisor & 0xFF;
//Set 0xValueH of clock divisor
byOutputBuffer[dwNumBytesToSend++] = (dwClockDivisor >> 8) & 0xFF;
// Send off the clock divisor commands
ftStatus = g_pWrite(ftHandle_, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);

dwNumBytesToSend = 0;
// Disable internal loop-back
// Disable loopback
byOutputBuffer[dwNumBytesToSend++] = 0x85;
// Send off the loopback command
ftStatus = g_pWrite(ftHandle_, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);

return ftStatus;
}

//read/translate JTAG TDO stream and pass array to JTAGSRV
unsigned int ftdi_blaster::read_pass_jtagsrv(unsigned int num_bytes, unsigned char* rbufn)
{
	unsigned int ftStatus;
	unsigned int dwNumBytesToRead = 0;

	//Read out the data from input buffer
	ftStatus = g_pRead( ftHandle_, rbuf_, num_bytes, &dwNumBytesToRead );
	if (ftStatus != FT_OK)
		return ftStatus;

	//trasnform to received bit array
	unsigned int curr = 0;
	jbuf_[0] = 0;
	jbuf_[1] = 0;
	for (unsigned int i = 0; i<num_bytes; i++)
	{
		int nbits = rbufn[i];
		unsigned char b = rbuf_[i];
		b = b >> (8 - nbits);
		jbuf_[curr / 8] |= (b << (curr & 7)) & 0xFF;
		jbuf_[curr / 8 + 1] |= (b >> (8 - (curr & 7))) & 0xFF;
		jbuf_[curr / 8 + 2] = 0;
		curr += nbits;
	}

	if (jtagsrvi.jtagsrv_pass_data) {
		//jtagr = 
		jtagsrvi.jtagsrv_pass_data((void*)jtagsrv_context_, jbuf_, curr);
		printd("jtagsrv_pass_data, numbits: %d Chk %08X Data: %02x %02x %02x %02x %02x %02x\n", curr, checkSum(jbuf_,curr),
			jbuf_[0], jbuf_[1], jbuf_[2], jbuf_[3], jbuf_[4], jbuf_[5]);
	}
	return FT_OK;
}

jblaster* CreateBlaster(int idx)
{
	ftdi_blaster* pblaster = new ftdi_blaster(idx);
	return (jblaster*)pblaster;
}

int ftdi_blaster::configure()
{
	int r = 1;
	int status = resetDevice();
	do_sleep(10);
	status = configureMpsse();
	if(status!=FT_OK) {
		printd("cannot configure MPSSE\n");
		return 0;
	}
	float freq = 10000000;
	if (g_cfg_frequency_set)
	{
		if (g_cfg_frequency >= 1000 && g_cfg_frequency <= 30000000)
			freq = (float)g_cfg_frequency;
	}
	set_freq(freq);
	return r;
}

FT_STATUS ftdi_blaster::wait_answer( unsigned int expect_num_bytes )
{
	int k;
	FT_STATUS ftStatus;
	unsigned int dwNumBytesToRead;

	k=0;
	while(1)
	{
		k++;
		if(k>200)
		{
			//oops..
			printd("long wait for data %d %d\n",dwNumBytesToRead,expect_num_bytes);
			return FT_OTHER_ERROR;
		}

		// Get the number of bytes in the device input buffer
		dwNumBytesToRead = 0;
		ftStatus = g_pGetQueueStatus(ftHandle_, &dwNumBytesToRead);
		if (ftStatus != FT_OK) {
			printd("GetQueueStatus %d error\n", ftStatus);
			return ftStatus;
		}
		if (dwNumBytesToRead == expect_num_bytes) {
			printd("wait done %d\n", expect_num_bytes);
			return FT_OK;
		}
		do_sleep(10);
	}
	printd("Impossible error\n");
	return FT_OTHER_ERROR;
}

//send all accumulated tdi data to chip using (passive) serial shift out
unsigned int ftdi_blaster::flush_passive_serial()
{
	FT_STATUS ftStatus;
	unsigned int count, numbytes, idx, numbits, dwNumBytesSent;
	unsigned char b;

	//get number of bits to sbe sent
	count = curr_idx_;

	if( count == 0 )
		return 0; //nothing to send

	//make writable buffer
	numbytes = count/8;
	idx=0;
	unsigned int i=0;
	if(numbytes>0)
	{
		//need to send at least byte or more
		sbuf_[idx++] = 0x19;
		numbytes--; //ftdi counts numbytes from zero
		sbuf_[idx++] = (unsigned char)(numbytes&0xFF);
		sbuf_[idx++] = (unsigned char)((numbytes>>8)&0xFF);
		numbytes++;
		for(unsigned int i=0; i<numbytes; i++)
		{
			b =  tdi_[i*8+0]     |
				(tdi_[i*8+1]<<1) |
				(tdi_[i*8+2]<<2) |
				(tdi_[i*8+3]<<3) |
				(tdi_[i*8+4]<<4) |
				(tdi_[i*8+5]<<5) |
				(tdi_[i*8+6]<<6) |
				(tdi_[i*8+7]<<7);
			sbuf_[idx+i] = b;
		}
		idx+=numbytes;
	}

	numbits = count%8;
	if(numbits>0)
	{
		//need to send at least bit or more, but less then 8
		sbuf_[idx++] = 0x1b;
		numbits--; //ftdi counts numbits from zero
		sbuf_[idx++] = (unsigned char)(numbits);
		b =  tdi_[i*8+0]     |
			(tdi_[i*8+1]<<1) |
			(tdi_[i*8+2]<<2) |
			(tdi_[i*8+3]<<3) |
			(tdi_[i*8+4]<<4) |
			(tdi_[i*8+5]<<5) |
			(tdi_[i*8+6]<<6) |
			(tdi_[i*8+7]<<7);

		sbuf_[idx++] = b;
	}

	curr_idx_ = 0;

	ftStatus = g_pWrite(ftHandle_, sbuf_, idx, &dwNumBytesSent);
	if( ftStatus != FT_OK || dwNumBytesSent!=idx )
		return 1;

	return 0;
}

//active serial should use parallel bit set/poll
#define WR_CHUNK 2048

//active serial controls CLK and DATAOUT by setting/resetting PIO bits of FTDI
//wr_len gives number of FTDI commands being sent to FTDI chip
//rd_len gives expected number of receiving bytes for PIO port poll
//bitsidx - index of bit array where we keep received serial bits
unsigned int ftdi_blaster::write_read_as_buffer(unsigned int wr_len, unsigned int rd_len, unsigned int bitsidx, unsigned int need_read)
{
	FT_STATUS ftStatus;
	unsigned int m,dwNumBytesToRead,dwNumBytesSent;
	unsigned char blaster_status[WR_CHUNK],mask;

	ftStatus = g_pWrite(ftHandle_, sbuf_, wr_len, &dwNumBytesSent);
	if( ftStatus != FT_OK || dwNumBytesSent != wr_len )
		return 0; //error

	if(need_read)
	{
		//ftStatus = wait_answer(rd_len);
		//if( ftStatus != FT_OK )
			//return 0; //error

		//read status bits from programmer
		dwNumBytesToRead = 0;
		ftStatus = g_pRead(ftHandle_, &blaster_status, rd_len, &dwNumBytesToRead);
		if( ftStatus != FT_OK || dwNumBytesToRead != rd_len )
			return 0; //error

		for( m=bitsidx; m<bitsidx+rd_len; m++)
		{
			mask = (1<<(m&7))^0xFF;
			rbuf_[m/8] &= mask;
			mask = 0;
			if(blaster_status[m-bitsidx] & 0x20)
			{
				mask = 1<<(m&7);
				rbuf_[m/8] |= mask;
			}
		}
	}
	return 1;
}

unsigned int ftdi_blaster::write_jtag_stream_as( struct jtag_task* jt )
{
	unsigned int j,k,tdi,tms;
	unsigned char port_val;
	unsigned int dwNumBytesToSend;

	printd("write_jtag_stream_as ACTIVE SERIAL!!!!!!!!!!!!!!!\n");

	j=0;
	k=0;
	dwNumBytesToSend = 0;
	for( unsigned int i=0; i<jt->wr_idx; i++ )
	{
		//count same TDI seq in buffer
		tdi = jt->data[i] & TDI_BIT;
		tms = jt->data[i] & TMS_BIT;

		// ADBUS0 TCK output 1 low 0
		// ADBUS1 TDI output 1 low 0
		// ADBUS2 TDO input 0 0
		// ADBUS3 TMS output 1 high 1
		// ADBUS4 GPIOL0 input 0 0
		// ADBUS5 nStatus input 0 0
		// ADBUS6 nCE output 1 0
		// ADBUS7 nCS output 1 0
		port_val = last_bits_flags_ & 0xf0;
		if(tms)
			port_val |= 0x08;
		if(tdi)
			port_val |= 0x02;

		//each set/reset bit in PIO requires few FTDI commands
		sbuf_[dwNumBytesToSend++] = 0x80;
		sbuf_[dwNumBytesToSend++] = port_val;
		sbuf_[dwNumBytesToSend++] = 0xCB; //direction

		sbuf_[dwNumBytesToSend++] = 0x80;
		sbuf_[dwNumBytesToSend++] = port_val | 0x01; //clk pos edge
		sbuf_[dwNumBytesToSend++] = 0xCB; //direction

		if( jt->need_tdo )
			sbuf_[dwNumBytesToSend++] = 0x81; //read port bits

		sbuf_[dwNumBytesToSend++] = 0x80;
		sbuf_[dwNumBytesToSend++] = port_val; //clk neg edge
		sbuf_[dwNumBytesToSend++] = 0xCB; //direction

		k++;
		if(k<WR_CHUNK)
			continue;
		k=0;

		if( jt->need_tdo )
			sbuf_[dwNumBytesToSend++] = 0x87; //force fast reply

		write_read_as_buffer(dwNumBytesToSend,WR_CHUNK,j, jt->need_tdo );
		j+=WR_CHUNK;
		dwNumBytesToSend=0;
	}

	if(k)
	{
		if( jt->need_tdo )
			sbuf_[dwNumBytesToSend++] = 0x87; //force fast reply

		write_read_as_buffer(dwNumBytesToSend,k,j, jt->need_tdo );
		j+=k;
	}

	if( jt->need_tdo )
	{
		if( jtagsrvi.jtagsrv_pass_data )
			jtagsrvi.jtagsrv_pass_data(jtagsrv_context_,(rbuf_),j);
	}

	return 1;
}

unsigned int ftdi_blaster::write_jtag_stream( struct jtag_task* jt )
{
	printTdiTms(jt->data, jt->wr_idx);

	unsigned int j,k,wr_idx;
	char tdi;
	unsigned char sbyte,ftdi_cmd,ftdi_cmd_short;
	unsigned int dwNumBytesSent;
	unsigned char last_tms;
	unsigned char last_tms_known = 0;
	unsigned int num_bits_sent=0;
	FT_STATUS ftStatus;

	if( jt->need_tdo )
	{
		//need read jtag output
		ftdi_cmd=0x6b;
		ftdi_cmd_short=0x39;
	}
	else
	{
		ftdi_cmd=0x4b;
		ftdi_cmd_short=0x19;
	}

	wr_idx=0;

	for( unsigned int i=0; i<jt->wr_idx; )
	{
		//try more optimized pack for ftdi commands...
		int can_optimize=0;

		//have ahead block?
		if( (jt->wr_idx-i) > 8*BLK_SIZE )
		{
			if(last_tms_known)
			{
				can_optimize = 1;
				for(k=0; k<8*BLK_SIZE; k++)
				{
					if( jt->data[i+k]!=last_tms )
					{
						can_optimize = 0;
						break;
					}
				}
			}
		}
		//...........

		if( can_optimize )
		{
			sbuf_[wr_idx++] = ftdi_cmd_short;
			sbuf_[wr_idx++] = BLK_SIZE-1;
			sbuf_[wr_idx++] = 0;
			for(k=0; k<BLK_SIZE; k++)
			{
				unsigned char* ptr = (unsigned char*)&jt->data[i + k * 8];
				sbyte = ( 
					(((ptr[0] & TDI_BIT) ? 1 : 0)   ) |
					(((ptr[1] & TDI_BIT) ? 1 : 0) <<1) |
					(((ptr[2] & TDI_BIT) ? 1 : 0) <<2) |
					(((ptr[3] & TDI_BIT) ? 1 : 0) <<3) |
					(((ptr[4] & TDI_BIT) ? 1 : 0) <<4) |
					(((ptr[5] & TDI_BIT) ? 1 : 0) <<5) |
					(((ptr[6] & TDI_BIT) ? 1 : 0) <<6) |
					(((ptr[7] & TDI_BIT) ? 1 : 0) <<7)
					);

				sbuf_[wr_idx++] = sbyte;
				rbufn_[num_rbytes_]=8;
				num_bits_sent += 8;
				num_rbytes_++;
			}
			i += 8*BLK_SIZE;
		}
		else
		{
			//not optimized command path..
			//count same TDI seq in buffer
			tdi=(jt->data[i]&TDI_BIT) ? 1 : 0 ;
			sbyte = (jt->data[i] & TMS_BIT) ? 1 : 0;
			for(j=1; j<6; j++)
			{
				if(j+i==jt->wr_idx)
					break;
				char tdi2 = (jt->data[j+i] & TDI_BIT) ? 1 : 0;
				if(tdi!=tdi2)
					break;
				last_tms = (jt->data[j + i] & TMS_BIT) ? 1 : 0;
				last_tms_known = 1;
				sbyte |= last_tms << j;
			}

			sbuf_[wr_idx++] = ftdi_cmd;
			sbuf_[wr_idx++] = j-1;
			sbuf_[wr_idx++] = sbyte | (tdi<<7);
			i += j;
			rbufn_[num_rbytes_]=j;
			num_bits_sent += j;
			num_rbytes_++;
		}

		//maybe need to flush big buffer?
		if(wr_idx>32*1024)
		{
			if(jt->need_tdo)
			{
				//complete wr buffer with fast reply command
				sbuf_[wr_idx++] = 0x87;
			}

			//send to chip
			dwNumBytesSent=0;
			ftStatus = g_pWrite(ftHandle_, sbuf_, wr_idx, &dwNumBytesSent);
			if(ftStatus)
				{ printd("FT_Write err %08X\n",ftStatus); return 0; }

			if(jt->need_tdo)
			{
				ftStatus = read_pass_jtagsrv( num_rbytes_, rbufn_ );
				if(ftStatus)
					{ printd("Read answer err %08X\n",ftStatus); return 0; }
			}
			else
			{
				if (jtagsrvi.jtagsrv_report) {
					printd("jtagsrv_report: bits %d bits in que %d\n", num_bits_sent, num_bits_in_queue_);
					jtagsrvi.jtagsrv_report(( void*)jtagsrv_context_, num_bits_sent, num_bits_in_queue_ );
					//printd("jtagsrv_report done\n");
				}
			}

			num_rbytes_=0;
			num_bits_sent = 0;
			wr_idx=0;
		}
	}

	if(jt->need_tdo) {
		//complete wr buffer with fast reply command
		sbuf_[wr_idx++] = 0x87;
	}

	//send to chip
	dwNumBytesSent=0;
	ftStatus = g_pWrite(ftHandle_, sbuf_, wr_idx, &dwNumBytesSent);
	if(ftStatus)
		{ printd("FT_Write err %08X\n",ftStatus); return 0; }

	if(jt->need_tdo) {
		ftStatus = read_pass_jtagsrv( num_rbytes_, rbufn_ );
		if(ftStatus)
			{ printd("Read answer err %08X\n",ftStatus); return 0; }
	}
	else {
		if (jtagsrvi.jtagsrv_report) {
			printd("jtagsrv_report: bits %d bits in que %d\n", num_bits_sent, num_bits_in_queue_ );
			jtagsrvi.jtagsrv_report((void*)jtagsrv_context_, num_bits_sent, num_bits_in_queue_ );
			//printd("jtagsrv_report done.\n");
		}
	}

	num_rbytes_=0;
	wr_idx=0;
	return 1;
}

unsigned int ftdi_blaster::write_flags_read_status(unsigned int flags, unsigned int* pstatus)
{
	unsigned int r = 0;
	unsigned int dwNumBytesToSend, dwNumBytesToRead;
	unsigned int dwNumBytesSent;
	unsigned char byOutputBuffer[16];
	unsigned char wrflags, blaster_status, s;
	FT_STATUS ftStatus;

	/*
	if (curr_idx_) {
		if (last_bits_flags_org_ == 0x13) {
			mode_as_ = 1; //remember mode
			write_jtag_stream_as(0, curr_idx_, 1); //active serial
		}
		else {
			flush_passive_serial();
			//write_jtag_stream(pblaster, 0, curr_idx_, 0); //jtag mode but no jtagsrv reply
		}
		curr_idx_ = 0;
	}
	*/
	
	send_recv(0);

	//we see flags input parameter like 0x19, 0x1b, 0x1f
	//assume following bit-map
#define FLAG_BIT_TDI 0x01
#define FLAG_BIT_TMS 0x02
#define FLAG_BIT_TCK 0x04
#define FLAG_BIT_NCS 0x08
#define FLAG_BIT_NCE 0x10

	//remap incoming bits into our HW bits
	wrflags = 0x00;

	if (flags & FLAG_BIT_TDI)
		wrflags = 0x02;

	if (flags & FLAG_BIT_TMS)
		wrflags |= 0x08;

	if (flags & FLAG_BIT_TCK)
		wrflags |= 0x01;

	if (flags & FLAG_BIT_NCS)
		wrflags |= 0x80;

	if (flags & FLAG_BIT_NCE)
		wrflags |= 0x40;

	last_bits_flags_ = wrflags;
	last_bits_flags_org_ = flags;

	dwNumBytesToSend = 0;
	dwNumBytesSent = 0;
	// Set initial states of the MPSSE interface - low byte, both pin directions and output values
	// Pin name Signal Direction Config Initial State Config
	// ADBUS0 TCK output 1 low 0
	// ADBUS1 TDI output 1 low 0
	// ADBUS2 TDO input 0 0
	// ADBUS3 TMS output 1 high 1
	// ADBUS4 GPIOL0 input 0 0
	// ADBUS5 nStatus input 0 0
	// ADBUS6 nCE output 1 0
	// ADBUS7 nCS output 1 0
	// Set data bits low-byte of MPSSE port
	byOutputBuffer[dwNumBytesToSend++] = 0x80;
	// Initial state config above
	byOutputBuffer[dwNumBytesToSend++] = wrflags;
	// Direction config above
	byOutputBuffer[dwNumBytesToSend++] = 0xcB;
	// force read status
	byOutputBuffer[dwNumBytesToSend++] = 0x81;

	//complete wr buffer with fast reply command
	byOutputBuffer[dwNumBytesToSend++] = 0x87;

	ftStatus = g_pWrite(ftHandle_, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
	if ((ftStatus != FT_OK) || (dwNumBytesSent != dwNumBytesToSend)) {
		printd("hwproc_write_flags_read_status ft_write ERROR %08X %d %d\n", ftStatus, dwNumBytesToSend, dwNumBytesSent);
		return ftStatus; //error
	}

	//ftStatus = wait_answer(pblaster, 1);
	//if( ftStatus != FT_OK )
	//	return 1; //error

	//read status bits from programmer
	dwNumBytesToRead = 0;
	ftStatus = g_pRead(ftHandle_, &blaster_status, 1, &dwNumBytesToRead);
	if ((ftStatus != FT_OK) || (dwNumBytesToRead != 1)) {
		printd("hwproc_write_flags_read_status ft_read ERROR %08X %d\n", ftStatus, dwNumBytesToRead);
		return ftStatus; //error
	}

	//make status for jtagserver
	s = 0;
	//check ADBUS2 (TDO/CONFIG_DONE)
	if (blaster_status & 0x04)
		s |= 1;
	//check ADBUS5 GPIOL1 (nSTATUS)
	if (blaster_status & 0x20)
		s |= 2;

	*pstatus = s;
	return 0;
}

