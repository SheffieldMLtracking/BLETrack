/**
 ****************************************************************************************
 *
 * @file main.c
 *
 * @brief Blinky example
 *
 * Copyright (C) 2012-2023 Renesas Electronics Corporation and/or its affiliates.
 * All rights reserved. Confidential Information.
 *
 * This software ("Software") is supplied by Renesas Electronics Corporation and/or its
 * affiliates ("Renesas"). Renesas grants you a personal, non-exclusive, non-transferable,
 * revocable, non-sub-licensable right and license to use the Software, solely if used in
 * or together with Renesas products. You may make copies of this Software, provided this
 * copyright notice and disclaimer ("Notice") is included in all such copies. Renesas
 * reserves the right to change or discontinue the Software at any time without notice.
 *
 * THE SOFTWARE IS PROVIDED "AS IS". RENESAS DISCLAIMS ALL WARRANTIES OF ANY KIND,
 * WHETHER EXPRESS, IMPLIED, OR STATUTORY, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. TO THE
 * MAXIMUM EXTENT PERMITTED UNDER LAW, IN NO EVENT SHALL RENESAS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE, EVEN IF RENESAS HAS BEEN ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES. USE OF THIS SOFTWARE MAY BE SUBJECT TO TERMS AND CONDITIONS CONTAINED IN
 * AN ADDITIONAL AGREEMENT BETWEEN YOU AND RENESAS. IN CASE OF CONFLICT BETWEEN THE TERMS
 * OF THIS NOTICE AND ANY SUCH ADDITIONAL LICENSE AGREEMENT, THE TERMS OF THE AGREEMENT
 * SHALL TAKE PRECEDENCE. BY CONTINUING TO USE THIS SOFTWARE, YOU AGREE TO THE TERMS OF
 * THIS NOTICE.IF YOU DO NOT AGREE TO THESE TERMS, YOU ARE NOT PERMITTED TO USE THIS
 * SOFTWARE.
 *
 ****************************************************************************************
 */
#include <stdio.h>
#include "arch_system.h"
#include "uart.h"
#include "uart_utils.h"
#include "user_periph_setup.h"
#include "gpio.h"
#include "i2c.h"

#include "arch_system.h"
#include "uart.h"
#include "uart_utils.h"
#include "gpio.h"
#include "i2c.h"

int32_t read_x = 0;
int32_t read_y = 0;
int32_t read_z = 0;
				
#define LIS2DU12_ADDR  0x19  // SA0 low : 0x18, SA0 high : 0x19
#define MAX_SAMPLES 128

static const i2c_cfg_t i2c_cfg = {
    .clock_cfg.ss_hcnt = I2C_SS_SCL_HCNT_REG_RESET,
    .clock_cfg.ss_lcnt = I2C_SS_SCL_LCNT_REG_RESET,
    .clock_cfg.fs_hcnt = I2C_FS_SCL_HCNT_REG_RESET,
    .clock_cfg.fs_lcnt = I2C_FS_SCL_LCNT_REG_RESET,
    .restart_en = I2C_RESTART_ENABLE,
    .speed = I2C_SPEED_STANDARD,
    .mode = I2C_MODE_MASTER,
    .addr_mode = I2C_ADDRESSING_7B,
    .address = LIS2DU12_ADDR,
    .tx_fifo_level = 1,
    .rx_fifo_level = 1,
};

void i2c_read_reg(uint8_t reg, uint8_t *data, uint16_t len)
{
    i2c_abort_t abort_code;
    i2c_master_transmit_buffer_sync(&reg, 1, &abort_code, I2C_F_NONE);
    i2c_master_receive_buffer_sync(data, len, &abort_code, I2C_F_ADD_STOP);
}

void i2c_write(uint8_t reg, uint8_t * bufp, uint16_t len) {
		i2c_abort_t abort_code;
		i2c_master_transmit_buffer_sync( &reg, 1, &abort_code, I2C_F_NONE);
		i2c_master_transmit_buffer_sync(bufp, len, &abort_code, I2C_F_ADD_STOP);
}

int main(void)
{
		// See underside of da14531 development kit for pin numbers
    GPIO_ConfigurePin(GPIO_PORT_0, GPIO_PIN_3, INPUT_PULLUP, PID_I2C_SDA, true);
    GPIO_ConfigurePin(GPIO_PORT_0, GPIO_PIN_4, INPUT_PULLUP, PID_I2C_SCL, true);
    
		// Config step
		system_init();
    i2c_init(&i2c_cfg);
	
		// Writing to a register
		uint8_t write = 0xB0;
		i2c_write(0x14,&write,1); // Write 0b10110000 to CTRL5, arbitraty setting but you have to change this to something to turn on the accel
	
		//setting fifo mode
		uint8_t FIFO_mode = 0x06;
		i2c_write(0x15,&FIFO_mode,1);
	
		// Reading a register
    uint8_t whoami = 0;
		// i2c_read_reg(register_address, var, length in bytes)
    i2c_read_reg(0x43, &whoami, 1);
	
		char buf[32];
		sprintf(buf, "WHO_AM_I = 0x%02X\r\n", whoami);
		printf_string(UART, buf);
	
		// X,Y,Z values are each stored in two registers. Need to concatenate them.
		uint8_t X_L = 0;
		uint8_t X_H = 0;
		uint8_t Y_L = 0;
		uint8_t Y_H = 0;
		uint8_t Z_L = 0;
		uint8_t Z_H = 0;
		uint8_t FIFO_status = 0;

		uint16_t FIFO_data[MAX_SAMPLES][3];

				
		uint8_t fifo2;
		while(1){
				// Testing FIFO buffer register status
		


				i2c_read_reg(0x27, &fifo2, 1);

				uint8_t FIFO_words = fifo2;
				sprintf(buf, "FIFO_words = %u\r\n", fifo2);
				//printf(UART, buf);
				
				uint8_t sample_count = 0;
				
				// If the buffer is full collect samples
				if (FIFO_words == MAX_SAMPLES){
					
					
					while (FIFO_words > sample_count){
						i2c_read_reg(0x28, &X_L, 1);
						i2c_read_reg(0x29, &X_H, 1);
						i2c_read_reg(0x2A, &Y_L, 1);
						i2c_read_reg(0x2B, &Y_H, 1);
						i2c_read_reg(0x2C, &Z_L, 1);
						i2c_read_reg(0x2D, &Z_H, 1);

						
						FIFO_data[sample_count][0] = ((X_H << 8) | X_L);
						FIFO_data[sample_count][1] = ((Y_H << 8) | Y_L);
						FIFO_data[sample_count][2] = ((Z_H << 8) | Z_L);
						sample_count += 1;
						
					}
					for (int i = 0; i < MAX_SAMPLES; i++) {
						char fifobuffer[128];
						sprintf(fifobuffer, "[%03d] X:%u, Y:%u, Z:%u\r\n", 
               i, 
               FIFO_data[i][0], 
               FIFO_data[i][1], 
               FIFO_data[i][2]);
						printf_string(UART, fifobuffer);
					}
				}
				

				
		}

//		while(1){
//				i2c_read_reg(0x28, &X_L, 1);
//				i2c_read_reg(0x29, &X_H, 1);
//				i2c_read_reg(0x2A, &Y_L, 1);
//				i2c_read_reg(0x2B, &Y_H, 1);
//				i2c_read_reg(0x2C, &Z_L, 1);
//				i2c_read_reg(0x2D, &Z_H, 1);
//			
//				int16_t raw_x = (int16_t)((X_H << 8) | X_L);
//				int16_t raw_y = (int16_t)((Y_H << 8) | Y_L);
//				int16_t raw_z = (int16_t)((Z_H << 8) | Z_L);
//				read_x = raw_x;
//				
//				char buf[256];
//				sprintf(buf, "X = %d     Y = %d     Z = %d\r\n", raw_x,raw_y,raw_z);
//				printf_string(UART, buf);
//		}
		
    //while (1);
}
