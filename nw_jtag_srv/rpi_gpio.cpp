///////////////////////////////////////
// Module:	rpi_gpio
// Author:	Nick Kovach
// Copyright (c)  2017 InproPlus Ltd
// Remarks:
//	Raspberry PI3 GPIO access functions
//	TCP server which receives network commands for JTAG
///////////////////////////////////////

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <iostream>
#include <fstream>
#include <string>

using namespace std;

#include "rpi_gpio.h"

// I/O access
volatile unsigned *gpio;
int  mem_fd;
void *gpio_map;

//
// Set up a memory regions to access GPIO
//
int setup_rpi_gpio()
{
	bool pi_zero = false;
	bool pi_4 = false;
	int num_cpus = 0;
	try {
		ifstream t;
		t.open("/proc/cpuinfo");
		if (!t.is_open())
		{
			cout << "Cannot identify CPU model\n";
		}
		else
		{
			string s;
			//int n = 0;
			while (getline(t, s))
			{
/*
				size_t pos = s.find("BCM2835");
				if (pos != string::npos)
				{
					//found 2835, Pi-zero W
					cout << "CPU model BCM2835\n";
					pi_zero = true;
					break;
				}
				pos = s.find("BCM2709");
				if (pos != string::npos)
				{
					//found 2709, RPI3
					cout << "CPU model BCM2709\n";
					pi_zero = false;
					break;
				}
				n++;
				if (n == 128)
					break; //too many lines
*/
				size_t pos = s.find("processor");
				if (pos != string::npos)
					num_cpus++;
				size_t pos2 = s.find("Pi 4");
				if (pos2 != string::npos)
					pi_4 = true;
			}
			if(num_cpus==1)
				pi_zero=true;
		}
	}
	catch (...) {
	}

	cout << "Number of processors: " << num_cpus << "\n";
	unsigned int gpio_base_addr = GPIO_BASE+ (pi_zero ? BCM2835_PERI_BASE : BCM2709_PERI_BASE);
	if( pi_zero )
	{
		gpio_base_addr = BCM2835_PERI_BASE + GPIO_BASE;
		cout << "Assume Pi-Zero, IO Base Addr: " << std::hex << gpio_base_addr << std::dec << "\n";
	}
	else
	if( pi_4 )
	{
		gpio_base_addr = RPI4_PERI_BASE + GPIO_BASE;
		cout << "Assume Pi4, IO Base Addr: " << std::hex << gpio_base_addr << std::dec << "\n";
	}
	else
	{
		gpio_base_addr = BCM2709_PERI_BASE + GPIO_BASE;
		cout << "Assume Pi2 or Pi3, IO Base Addr: " << std::hex << gpio_base_addr << std::dec << "\n";
	}


   /* open /dev/mem */
   if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
      printf("can't open /dev/mem \n");
      return -1;
   }
 
   /* mmap GPIO */
   gpio_map = mmap(
      NULL,             //Any adddress in our space will do
      BLOCK_SIZE,       //Map length
      PROT_READ|PROT_WRITE,// Enable reading & writting to mapped memory
      MAP_SHARED,       //Shared with other processes
      mem_fd,           //File to map
      gpio_base_addr    //Offset to GPIO peripheral
   );
 
   close(mem_fd); //No need to keep mem_fd open after mmap
 
   if (gpio_map == MAP_FAILED) {
      printf("mmap error %d\n", (int)gpio_map);//errno also set!
      return -1;
   }
 
   // Always use volatile pointer!
   gpio = (volatile unsigned *)gpio_map;
   
	INP_GPIO(TMS_RPI_PIN);
	OUT_GPIO(TMS_RPI_PIN);
	INP_GPIO(TDI_RPI_PIN);
	OUT_GPIO(TDI_RPI_PIN);
	INP_GPIO(TCK_RPI_PIN);
	OUT_GPIO(TCK_RPI_PIN);
	INP_GPIO(TDO_RPI_PIN);

   return 0;
}
