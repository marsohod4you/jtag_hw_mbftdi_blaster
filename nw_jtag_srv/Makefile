
PRJ0PATH=$(PWD)/../jtag_hw_mbftdi_blaster_src
LIBPATH=$(PWD)/../arm_ftdi/build
INCPATH=$(PWD)/../arm_ftdi

CXFLAGS+=-std=c++11 -O3 -fPIC -Wall -DX32
#CXFLAGS+=-DDBGPRINT
LDFLAGS+=-lpthread -ldl $(LIBPATH)/libftd2xx.a -Wl,-no-whole-archive
LDFLAGS+=-lpthread -Wl,-no-whole-archive

INCLUDES+=-I./ -I$(INCPATH) -I$(PRJ0PATH)

PHONY: nw_jtag_srv

nw_jtag_srv: clean
	g++ -o nw_jtag_srv $(CXFLAGS) $(INCLUDES) nw_jtag_srv.cpp rpi_gpio.cpp $(PRJ0PATH)/../common/debug.cpp $(PRJ0PATH)/jtag_hw_blaster.cpp $(PRJ0PATH)/jtag_hw_mbftdi_blaster.cpp $(LDFLAGS) 

clean:
	rm -f nw_jtag_srv



