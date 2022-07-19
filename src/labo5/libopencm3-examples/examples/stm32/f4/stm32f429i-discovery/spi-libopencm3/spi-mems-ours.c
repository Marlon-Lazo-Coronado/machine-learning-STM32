/*
 * This file is part of the libopencm3 project.
 *
 * Copyright (C) 2013 Chuck McManis <cmcmanis@mcmanis.com>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * SPI Port example
 */

#include <stdint.h>
#include <libopencm3/stm32/usart.h> 
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>
#include "clock.h"
#include "console.h"

/*
 * Functions defined for accessing the SPI port 8 bits at a time
 */
uint16_t read_reg(int reg);
void write_reg(uint8_t reg, uint8_t value);
uint8_t read_xyz(int16_t vecs[3]);
void spi_init(void);

/*
 * Chart of the various SPI ports (1 - 6) and where their pins can be:
 *
 *	 NSS		  SCK			MISO		MOSI
 *	 --------------   -------------------   -------------   ---------------
 * SPI1  PA4, PA15	  PA5, PB3		PA6, PB4	PA7, PB5
 * SPI2  PB9, PB12, PI0   PB10, PB13, PD3, PI1  PB14, PC2, PI2  PB15, PC3, PI3
 * SPI3  PA15*, PA4*	  PB3*, PC10*		PB4*, PC11*	PB5*, PD6, PC12*
 * SPI4  PE4,PE11	  PE2, PE12		PE5, PE13	PE6, PE14
 * SPI5  PF6, PH5	  PF7, PH6		PF8		PF9, PF11, PH7
 * SPI6  PG8		  PG13			PG12		PG14
 *
 * Pin name with * is alternate function 6 otherwise use alternate function 5.
 *
 * MEMS uses SPI5 - SCK (PF7), MISO (PF8), MOSI (PF9),
 * MEMS CS* (PC1)  -- GPIO
 * MEMS INT1 = PA1, MEMS INT2 = PA2
 */

void spi_init(void)
{
	rcc_periph_clock_enable(RCC_GPIOF);
	rcc_periph_clock_enable(RCC_SPI5);
    rcc_periph_clock_enable(RCC_GPIOC);

	gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO1);
    gpio_set(GPIOC, GPIO1);

    gpio_mode_setup(GPIOF, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO8 | GPIO7 | GPIO9);   
	gpio_set_af(GPIOF, GPIO_AF5, GPIO8 | GPIO7 | GPIO9);

	spi_set_clock_polarity_0(SPI5);
	spi_set_clock_phase_0(SPI5);
    spi_set_master_mode(SPI5);
	spi_set_unidirectional_mode(SPI5);
	spi_set_baudrate_prescaler(SPI5, SPI_CR1_BR_FPCLK_DIV_64);
	spi_set_full_duplex_mode(SPI5);

	spi_send_msb_first(SPI5);
	spi_enable_software_slave_management(SPI5);
	spi_set_nss_high(SPI5);

	spi_enable(SPI5);
    SPI_I2SCFGR(SPI5) &= ~SPI_I2SCFGR_I2SMOD;    
}


static void my_usart_print_int(uint32_t usart, int32_t value)
{
	int8_t cont;
	int8_t get_nums = 0;
	char queue[25];

	usart_send_blocking(usart, '\n\n');

	while (value > 0) {
		if (value < 0) {
			usart_send_blocking(usart, '-');
			value = value * -1;
		}

		queue[get_nums++] = (int32_t)(value % 10);
		value = value*0.1;
	}

	cont = get_nums-1; 
	while (cont >= 0) {
		usart_send_blocking(usart, queue[cont]);
		cont--;
	}
}


int print_decimal(int);

/*
 * int len = print_decimal(int value)
 *
 * Very simple routine to print an integer as a decimal
 * number on the console.
 */
int 
print_decimal(int num)
{
	int		ndx = 0;
	char	buf[10];
	int		len = 0;
	char	is_signed = 0;

	if (num < 0) {
		is_signed++;
		num = 0 - num;
	}
	buf[ndx++] = '\000';
	do {
		buf[ndx++] = (num % 10) + '0';
		num = num / 10;
	} while (num != 0);
	ndx--;
	if (is_signed != 0) {
		console_putc('-');
		len++;
	}
	while (buf[ndx] != '\000') {
		console_putc(buf[ndx--]);
		len++;
	}
	return len; /* number of characters printed */
}

char *axes[] = { "X: ", "Y: ", "Z: " };

/*
 * This then is the actual bit of example. It initializes the
 * SPI port, and then shows a continuous display of values on
 * the console once you start it. Typing ^C will reset it.
 */
int main(void)
{
	int16_t vecs[3];
	
	clock_setup();
	console_setup(115200);
    spi_init();

    gpio_clear(GPIOC, GPIO1);         
	spi_send(SPI5, 32);    
	spi_read(SPI5);                  
	spi_send(SPI5, 8 | 2 |   
			1 | 4 |
			(3 << 4));
	spi_read(SPI5);                    
	gpio_set(GPIOC, GPIO1);            


    gpio_clear(GPIOC, GPIO1);
	spi_send(SPI5, 35);
	spi_read(SPI5);
	spi_send(SPI5, (1 << 4));  
	spi_read(SPI5);
	gpio_set(GPIOC, GPIO1);


    console_puts("X\tY\tZ\n");
    
	while (1) {
        uint8_t temp;
        uint8_t who;
        int16_t gyr_x;
        int16_t gyr_y;
        int16_t gyr_z;

		gpio_clear(GPIOC, GPIO1);                   
		spi_send(SPI5, 15 | 0x80);  
		spi_read(SPI5);               
		spi_send(SPI5, 0);             
		who=spi_read(SPI5);          
		gpio_set(GPIOC, GPIO1);      

		gpio_clear(GPIOC, GPIO1);
		spi_send(SPI5, 39 | 128);
		spi_read(SPI5);
		spi_send(SPI5, 0);
		temp=spi_read(SPI5);
		gpio_set(GPIOC, GPIO1);

		gpio_clear(GPIOC, GPIO1);
		spi_send(SPI5, 38 | 128);
		spi_read(SPI5);
		spi_send(SPI5, 0);
		temp=spi_read(SPI5);
		gpio_set(GPIOC, GPIO1);  

		gpio_clear(GPIOC, GPIO1);
		spi_send(SPI5, 40 | 128);
		spi_read(SPI5);
		spi_send(SPI5, 0);
		gyr_x=spi_read(SPI5);
		gpio_set(GPIOC, GPIO1);

		gpio_clear(GPIOC, GPIO1);
		spi_send(SPI5, 41 | 128);
		spi_read(SPI5);
		spi_send(SPI5, 0);
		gyr_x|=spi_read(SPI5) << 8;
		gpio_set(GPIOC, GPIO1);

		gpio_clear(GPIOC, GPIO1);
		spi_send(SPI5, 42 | 128);
		spi_read(SPI5);
		spi_send(SPI5, 0);
		gyr_y=spi_read(SPI5);
		gpio_set(GPIOC, GPIO1);

		gpio_clear(GPIOC, GPIO1);
		spi_send(SPI5, 43 | 128);
		spi_read(SPI5);
		spi_send(SPI5, 0);
		gyr_y|=spi_read(SPI5) << 8;
		gpio_set(GPIOC, GPIO1);

		gpio_clear(GPIOC, GPIO1);
		spi_send(SPI5, 44 | 128);
		spi_read(SPI5);
		spi_send(SPI5, 0);
		gyr_z=spi_read(SPI5);
		gpio_set(GPIOC, GPIO1);

		gpio_clear(GPIOC, GPIO1);
		spi_send(SPI5, 45 | 128);
		spi_read(SPI5);
		spi_send(SPI5, 0);
		gyr_z|=spi_read(SPI5) << 8;
		gpio_set(GPIOC, GPIO1);

        gyr_x = gyr_x*(0.0175F);
        gyr_y = gyr_y*(0.0175F);
        gyr_z = gyr_z*(0.0175F);

	    print_decimal(gyr_x); console_puts("\t");
        print_decimal(gyr_y); console_puts("\t");
        print_decimal(gyr_z); console_puts("\n");

		int i;
		for (i = 0; i < 80000; i++)    
			__asm__("nop");
	}

	return 0;
}
