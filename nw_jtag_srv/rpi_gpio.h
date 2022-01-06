#ifndef _RPI_GPIO_H_
#define _RPI_GPIO_H_

// Access from ARM Running Linux

#define RPI4_PERI_BASE		0xFE000000
#define BCM2709_PERI_BASE	0x3F000000
#define BCM2835_PERI_BASE 	0x20000000
#define GPIO_BASE               0x200000 /* GPIO controller */

#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

// I/O access
extern volatile unsigned *gpio;

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))
 
#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0
 
#define GET_GPIO(g) (*(gpio+13)&(1<<g)) // 0 if LOW, (1<<g) if HIGH
 
#define GPIO_PULL *(gpio+37) // Pull up/pull down
#define GPIO_PULLCLK0 *(gpio+38) // Pull up/pull down clock

#define TMS_RPI_PIN 0
#define TDI_RPI_PIN 11
#define TCK_RPI_PIN 7
#define TDO_RPI_PIN 1

extern int setup_rpi_gpio();

#endif //_RPI_GPIO_H_
