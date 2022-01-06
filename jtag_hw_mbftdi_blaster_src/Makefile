#linux Makefile

CC=gcc
CPP=g++

#LIBPATH = $(PWD)/linux_ftdi/x86_64
#INCPATH = $(PWD)/linux_ftdi
LIBPATH = $(PWD)/lftdi136/build
INCPATH = $(PWD)/lftdi136

CFLAGS += -I$(INCPATH)
CFLAGS += -DX64 -O3
CFLAGS += -fPIC -Wall

FTDI_APP = libjtag_hw_mbftdi-blaster.so
NET_APP = libjtag_hw_net-blaster.so
all: $(FTDI_APP) $(NET_APP)

ftdi: $(FTDI_APP)
net: $(NET_APP)

$(FTDI_APP): jtag_hw_mbftdi_blaster.cpp jtag_hw_blaster.cpp CConfig.cpp
	$(CPP) -shared -o $(FTDI_APP) -static-libstdc++ $(CFLAGS) jtag_hw_mbftdi_blaster.cpp  jtag_hw_blaster.cpp CConfig.cpp $(LIBPATH)/libftd2xx.a -Wl,-no-whole-archive

$(NET_APP): jtag_hw_net_blaster.cpp jtag_hw_blaster.cpp CConfig.cpp socket.cpp
	$(CPP) -shared -o $(NET_APP) -static-libstdc++ -rdynamic $(CFLAGS) -DDBGPRINT=1 jtag_hw_net_blaster.cpp  jtag_hw_blaster.cpp CConfig.cpp socket.cpp ../common/debug.cpp -Wl,-no-whole-archive

clean:
	rm *.so
