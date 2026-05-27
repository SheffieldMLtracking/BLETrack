/**
 ****************************************************************************************
 *
 * @file user_active_scanner.c
 *
 * @brief Active scanner project source code.
 *
 * Copyright (c) 2012-2021 Renesas Electronics Corporation and/or its affiliates
 * The MIT License (MIT)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
 * OR OTHER DEALINGS IN THE SOFTWARE.
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup APP
 * @{
 ****************************************************************************************
 */
#include "rwip_config.h"             // SW configuration
#include <stdint.h>
#include "wkupct_quadec.h"
#include "rtc.h"
uint8_t receivedPacketsA[1000][6];__SECTION_ZERO("retention_mem_area0");
uint8_t rtcRead[1000][3];__SECTION_ZERO("retention_mem_area0");
uint32_t i = 0;__SECTION_ZERO("retention_mem_area0");
uint32_t y = 0; __SECTION_ZERO("retention_mem_area0");
uint32_t raw = 0; __SECTION_ZERO("retention_mem_area0");
uint32_t rtcReadIndex = 0;
volatile uint32_t stopscantime = 100000; __SECTION_ZERO("retention_mem_area0");
uint8_t adv_start_delay = 0;__SECTION_ZERO("retention_mem_area0");
bool adv_start_flag = false;__SECTION_ZERO("retention_mem_area0");
bool nesttrigger = false;__SECTION_ZERO("retention_mem_area0");

uint16_t receivedPacketsIndex = 0;__SECTION_ZERO("retention_mem_area0");
uint16_t sentDataIndex = 0;__SECTION_ZERO("retention_mem_area0");
bool endScanFlag = false;__SECTION_ZERO("retention_mem_area0");
bool rtcTransmitFlag = false;__SECTION_ZERO("retention_mem_area0");
bool in_nest = false; __SECTION_ZERO("retention_mem_area0");
uint32_t startIntervals[3] = {50, 150, 250};__SECTION_ZERO("retention_mem_area0");
uint8_t numIntervals = 3; __SECTION_ZERO("retention_mem_area0");
uint32_t turnOffDelay = 99999;__SECTION_ZERO("retention_mem_area0");
uint32_t sentDataIndexRtc = 0;__SECTION_ZERO("retention_mem_area0");

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "user_active_scanner.h"
#include "arch_system.h"
#include "timer1.h"
#include "adc.h"
//#include "adc_531.h"

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
*/

#define STOP_ADV_IN_SEC    1

 /* Duration of timer for connection parameter update request */
#define APP_PARAM_UPDATE_REQUEST_TO         (1000)   // 1000*10ms = 10sec, The maximum allowed value is 41943sec (4194300 * 10ms)

/* Advertising data update timer */
#define APP_ADV_DATA_UPDATE_TO              (3000)   // 3000*10ms = 30sec, The maximum allowed value is 41943sec (4194300 * 10ms)

/* Manufacturer specific data constants */
#define APP_AD_MSD_COMPANY_ID       (0xABCD)
#define APP_AD_MSD_COMPANY_ID_LEN   (2)
#define APP_AD_MSD_DATA_LEN         (sizeof(uint16_t))

#define APP_PERIPHERAL_CTRL_TIMER_DELAY 100

 timer_hnd app_adv_timeout_timer_used __SECTION_ZERO("retention_mem_area0");

// Manufacturer Specific Data ADV structure type
struct mnf_specific_data_ad_structure
{
    uint8_t ad_structure_size;
    uint8_t ad_structure_type;
    uint8_t company_id[APP_AD_MSD_COMPANY_ID_LEN];
    uint8_t proprietary_data[APP_AD_MSD_DATA_LEN];
};

uint8_t app_connection_idx                      __SECTION_ZERO("retention_mem_area0");
timer_hnd app_adv_data_update_timer_used        __SECTION_ZERO("retention_mem_area0");
timer_hnd app_param_update_request_timer_used   __SECTION_ZERO("retention_mem_area0");

// Retained variables
struct mnf_specific_data_ad_structure mnf_data  __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
// Index of manufacturer data in advertising data or scan response data (when MSB is 1)
uint8_t mnf_data_index                          __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
uint8_t stored_adv_data_len                     __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
uint8_t stored_scan_rsp_data_len                __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
uint8_t stored_adv_data[ADV_DATA_LEN]           __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
uint8_t stored_scan_rsp_data[SCAN_RSP_DATA_LEN] __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY

struct scan_configuration {
    /// Operation code.
    uint8_t     code;
    /// Own BD address source of the device
    uint8_t     addr_src;
    /// Scan interval
    uint16_t    interval;
    /// Scan window size
    uint16_t    window;
    /// Scanning mode
    uint8_t     mode;
    /// Scan filter policy
    uint8_t     filt_policy;
    /// Scan duplicate filtering policy
    uint8_t     filter_duplic;
};

static const struct scan_configuration user_scan_conf ={
    /// Operation code.
    .code = GAPM_SCAN_PASSIVE,
    /// Own BD address source of the device
    .addr_src = GAPM_CFG_ADDR_PUBLIC,
    /// Scan interval
    .interval = 100,
    /// Scan window size
    .window = 7,
    /// Scanning mode
    .mode = GAP_OBSERVER_MODE,
    /// Scan filter policy
    .filt_policy = SCAN_ALLOW_ADV_ALL,
    /// Scan duplicate filtering policy
    .filter_duplic = SCAN_FILT_DUPLIC_DIS
};
/* Generic timer settings */
#define FREE_RUN                        TIM1_FREE_RUN_ON    
#define COUNT_DIRECTION                 TIM1_CNT_DIR_UP
#define RELOAD_VALUE                    TIM1_RELOAD_MAX

/* Generic event Settings */
#define STAMP_TYPE                      TIM1_EVENT_STAMP_CNT        
#define EVENT_PIN                       (tim1_event_gpio_pin_t)(GPIO_SIGNAL_PIN + 1)

/* Define settings for Timer 1 */
#define TIMER1_ON_SYSCLK                1
#define TIMER1_ON_LPCLK                 0

#define INPUT_CLK               TIMER1_ON_LPCLK
#define INTERRUPT_MASK_TMR      TIM1_IRQ_MASK_OFF
#define TIMER1_OVF_COUNTER      1

// Timer1 counting configuration structure
static timer1_count_options_t timer1_config =
{	
	/*Timer1 input clock*/
	.input_clk 	= 	(tim1_clk_src_t)INPUT_CLK,
	
	/*Timer1 free run off*/
	.free_run 	= 	FREE_RUN,
	
	/*Timer1 IRQ mask*/
	.irq_mask	= 	INTERRUPT_MASK_TMR,
	
	/*Timer1 count direction*/
	.count_dir	= 	COUNT_DIRECTION,
	
	/*Timer1 reload value*/
	.reload_val	= 	RELOAD_VALUE
};

uint8_t timer1_cnt_ovf                          __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY

bool isValueInList(uint32_t value, uint32_t list[], uint8_t size) {
    for (int x = 0; x < size; x++) {
        if (list[x] == value) {
            return true; // Value found
        }
    }
    return false; // Value not found
}


static void read_adc_p0_1(void)
{
    adc_config_t adc_cfg =
    {
        .input_mode      = ADC_INPUT_MODE_SINGLE_ENDED,
        .input           = ADC_INPUT_SE_P0_1,
        .smpl_time_mult  = 8,
        .continuous      = false,
        .interval_mult   = 0,
        .input_attenuator= ADC_INPUT_ATTN_2X,
        .chopping        = false,
        .oversampling    = 0
    };
    // Init
    adc_init(&adc_cfg);
		adc_offset_calibrate(ADC_INPUT_MODE_SINGLE_ENDED);

    // Start conversion
    adc_start();

    raw = adc_correct_sample(adc_get_sample());

    adc_disable();
}

/**
 ****************************************************************************************
 * @brief Scan function
 * @return void
 ****************************************************************************************
 */
void user_scan_start(void)
{
			struct gapm_start_scan_cmd* cmd = KE_MSG_ALLOC(GAPM_START_SCAN_CMD,
															TASK_GAPM, TASK_APP,
															gapm_start_scan_cmd);

			cmd->op.code = user_scan_conf.code;
			cmd->op.addr_src = user_scan_conf.addr_src;
			cmd->interval = user_scan_conf.interval;
			cmd->window = user_scan_conf.window;
			cmd->mode = user_scan_conf.mode;
			cmd->filt_policy = user_scan_conf.filt_policy;
			cmd->filter_duplic = user_scan_conf.filter_duplic;

			// Send the message
			ke_msg_send(cmd);

			// We are now connectable
			ke_state_set(TASK_APP, APP_CONNECTABLE);
    
		arch_printf( "SCAN START v2.0\r\n");
}
void scan_stop()
{
	    // Disable Advertising
  struct gapm_cancel_cmd *cmd = app_gapm_cancel_msg_create();
    // Send the message
  app_gapm_cancel_msg_send(cmd);
}

/**
 ****************************************************************************************
 * @brief Scan function
 * @return void
 ****************************************************************************************
 */

// Init manf data
static void mnf_data_init()
{
    mnf_data.ad_structure_size = sizeof(struct mnf_specific_data_ad_structure ) - sizeof(uint8_t); // minus the size of the ad_structure_size field
    mnf_data.ad_structure_type = GAP_AD_TYPE_MANU_SPECIFIC_DATA;
    mnf_data.company_id[0] = APP_AD_MSD_COMPANY_ID & 0xFF; // LSB
    mnf_data.company_id[1] = (APP_AD_MSD_COMPANY_ID >> 8 )& 0xFF; // MSB
    mnf_data.proprietary_data[0] = 0;
    mnf_data.proprietary_data[1] = 0;
}

// Add an AD structure in the Advertising or Scan Response Data of the GAPM_START_ADVERTISE_CMD parameter struct.
static void app_add_ad_struct(struct gapm_start_advertise_cmd *cmd, void *ad_struct_data, uint8_t ad_struct_len, uint8_t adv_connectable)
{
    uint8_t adv_data_max_size = (adv_connectable) ? (ADV_DATA_LEN - 3) : (ADV_DATA_LEN);

    if ((adv_data_max_size - cmd->info.host.adv_data_len) >= ad_struct_len)
    {
        // Append manufacturer data to advertising data
        memcpy(&cmd->info.host.adv_data[cmd->info.host.adv_data_len], ad_struct_data, ad_struct_len);

        // Update Advertising Data Length
        cmd->info.host.adv_data_len += ad_struct_len;

        // Store index of manufacturer data which are included in the advertising data
        mnf_data_index = cmd->info.host.adv_data_len - sizeof(struct mnf_specific_data_ad_structure);
    }
    else if ((SCAN_RSP_DATA_LEN - cmd->info.host.scan_rsp_data_len) >= ad_struct_len)
    {
        // Append manufacturer data to scan response data
        memcpy(&cmd->info.host.scan_rsp_data[cmd->info.host.scan_rsp_data_len], ad_struct_data, ad_struct_len);

        // Update Scan Response Data Length
        cmd->info.host.scan_rsp_data_len += ad_struct_len;

        // Store index of manufacturer data which are included in the scan response data
        mnf_data_index = cmd->info.host.scan_rsp_data_len - sizeof(struct mnf_specific_data_ad_structure);
        // Mark that manufacturer data is in scan response and not advertising data
        mnf_data_index |= 0x80;
    }
    else
    {
        // Manufacturer Specific Data do not fit in either Advertising Data or Scan Response Data
        ASSERT_WARNING(0);
    }
    // Store advertising data length
    stored_adv_data_len = cmd->info.host.adv_data_len;
    // Store advertising data
    memcpy(stored_adv_data, cmd->info.host.adv_data, stored_adv_data_len);
    // Store scan response data length
    stored_scan_rsp_data_len = cmd->info.host.scan_rsp_data_len;
    // Store scan_response data
    memcpy(stored_scan_rsp_data, cmd->info.host.scan_rsp_data, stored_scan_rsp_data_len);
}

static struct gapm_start_advertise_cmd* create_direct_ad_msg(uint8_t ldc_enable)
{
    // Allocate a message for GAP
        struct gapm_start_advertise_cmd* adv_cmd = KE_MSG_ALLOC(GAPM_START_ADVERTISE_CMD,
                                                        TASK_GAPM,
                                                        TASK_APP,
                                                        gapm_start_advertise_cmd);

        if (ldc_enable)
        {
            adv_cmd->op.code = GAPM_ADV_DIRECT_LDC;
            adv_cmd->intv_min = user_adv_conf.intv_min;
            adv_cmd->intv_max = user_adv_conf.intv_max;
        }
        else
        {
            adv_cmd->op.code = GAPM_ADV_DIRECT;
            adv_cmd->intv_min = MS_TO_BLESLOTS(50);
            adv_cmd->intv_max = MS_TO_BLESLOTS(100);
        }
        adv_cmd->op.addr_src = user_adv_conf.addr_src;
        adv_cmd->channel_map = user_adv_conf.channel_map;
				
				// Set peer address to data to send
				if(rtcTransmitFlag == false)
				{
					if(sentDataIndex >= receivedPacketsIndex)
					{
						sentDataIndex = 0;
						rtcTransmitFlag = true;
					}
				
					if(sentDataIndex < 1000)
					{
						user_adv_conf.peer_addr[0] = receivedPacketsA[sentDataIndex][5];
						user_adv_conf.peer_addr[1] = receivedPacketsA[sentDataIndex][4];
						user_adv_conf.peer_addr[2] = receivedPacketsA[sentDataIndex][3];
						user_adv_conf.peer_addr[3] = receivedPacketsA[sentDataIndex][2];
						user_adv_conf.peer_addr[4] = receivedPacketsA[sentDataIndex][1];
						user_adv_conf.peer_addr[5] = receivedPacketsA[sentDataIndex][0];
					}
        
					sentDataIndex++;
				}
				
				if(rtcTransmitFlag == true)
				{
					
					if(sentDataIndexRtc >= rtcReadIndex)
					{
						sentDataIndexRtc = 0;
						rtcTransmitFlag = false;
					}
					
					if(sentDataIndexRtc < 1000)
					{
						user_adv_conf.peer_addr[0] = rtcRead[sentDataIndexRtc][0];
						user_adv_conf.peer_addr[1] = rtcRead[sentDataIndexRtc][1];
						user_adv_conf.peer_addr[2] = rtcRead[sentDataIndexRtc][2];
						user_adv_conf.peer_addr[3] = 0x00;
						user_adv_conf.peer_addr[4] = 0x00;
						user_adv_conf.peer_addr[5] = 0x00;
					}
					
					sentDataIndexRtc++;
				}
				
				memcpy(adv_cmd->info.direct.addr.addr, user_adv_conf.peer_addr, BD_ADDR_LEN*sizeof(uint8_t));
        adv_cmd->info.direct.addr_type = user_adv_conf.peer_addr_type;
    return adv_cmd;
}

// cancel ad (used in adv function)
void cancel_ad(void)
{
		if(app_adv_timeout_timer_used != EASY_TIMER_INVALID_TIMER)
		{
			app_adv_timeout_timer_used = EASY_TIMER_INVALID_TIMER;
		}
    struct gapm_cancel_cmd* cmd = KE_MSG_ALLOC(GAPM_CANCEL_CMD,
                                               TASK_GAPM,
                                               TASK_APP,
                                               gapm_cancel_cmd);

    cmd->operation = GAPM_CANCEL;
    ke_msg_send(cmd);
}

void user_app_adv_start(void)
{
		//arch_disable_sleep();
    // Schedule the next advertising data update
    //app_adv_data_update_timer_used = app_easy_timer(APP_ADV_DATA_UPDATE_TO, adv_data_update_timer_cb);

    struct gapm_start_advertise_cmd* cmd;
    cmd = create_direct_ad_msg(0);

    // Add manufacturer data to initial advertising or scan response data, if there is enough space
    app_add_ad_struct(cmd, &mnf_data, sizeof(struct mnf_specific_data_ad_structure), 1);
		if(app_adv_timeout_timer_used != EASY_TIMER_INVALID_TIMER)
		{
			app_adv_timeout_timer_used = EASY_TIMER_INVALID_TIMER;
		}
		
		app_adv_timeout_timer_used = app_easy_timer(STOP_ADV_IN_SEC, cancel_ad);
    ke_msg_send(cmd);
		//app_easy_gap_advertise_stop();

    // We are now connectable
    ke_state_set(TASK_APP, APP_CONNECTABLE);
}

// restart adv once a packet is sent
void user_app_on_adv_direct_complete(uint8_t param)
{
		app_adv_timeout_timer_used = app_easy_timer(STOP_ADV_IN_SEC, cancel_ad);
		struct gapm_start_advertise_cmd* cmd;
    cmd = create_direct_ad_msg(0);
    ke_msg_send(cmd);
}

/**
 ****************************************************************************************
 * @brief Timer IR function
 * @return void
 ****************************************************************************************
 */

static void timer1_overflow(void)
{
    if (timer1_cnt_ovf == TIMER1_OVF_COUNTER)
    {
      i++; // If TIMER_OVF_COUNTER = 1 then when i = 7 = 1 second
			//if (isValueInList(i, startIntervals,numIntervals))
			
			read_adc_p0_1();
			
			if (raw < 160)
			{
				if (nesttrigger == false)
				{
					in_nest = true;	
					nesttrigger = true;
				}

			}
			
			if (in_nest == true)
			{
				if (raw > 210)
				{
					arch_ble_force_wakeup();
					user_scan_start();
					stopscantime = i + 28;
					in_nest = false;
				}
			}
			
			if(i == stopscantime)
			{
				arch_ble_force_wakeup();
				scan_stop();
				timer1_stop();
				SetBits16(PMU_CTRL_REG, TIM_SLEEP, 1);
				wkupct_enable_irq(WKUPCT_PIN_SELECT(GPIO_PORT_0, GPIO_PIN_0), // select pin (GPIO_BUTTON_PORT, GPIO_BUTTON_PIN)
									WKUPCT_PIN_POLARITY(GPIO_PORT_0, GPIO_PIN_0, WKUPCT_PIN_POLARITY_HIGH), // polarity high
									1, // 1 event
									63); // debouncing time = 63
			}
			
//			if(i == 110)
//			{
//				arch_ble_force_wakeup();
//				scan_stop();
//				wkupct_enable_irq(WKUPCT_PIN_SELECT(GPIO_PORT_0, GPIO_PIN_0), // select pin (GPIO_BUTTON_PORT, GPIO_BUTTON_PIN)
//									WKUPCT_PIN_POLARITY(GPIO_PORT_0, GPIO_PIN_0, WKUPCT_PIN_POLARITY_HIGH), // polarity low
//									1, // 1 event
//									63); // debouncing time = 63
//				timer1_stop();
//				SetBits16(PMU_CTRL_REG, TIM_SLEEP, 1);
//			}
//			
//			
//        timer1_cnt_ovf = 0;
//        /* Wake up the device and print out the measured time */
//        //arch_ble_force_wakeup();                // Force the BLE to wake up
//        //app_easy_wakeup_set(user_timer_wakeup_ble);
//        //app_easy_wakeup();                      // Send the message to print
//    }
			timer1_cnt_ovf = 0;
    }
    timer1_cnt_ovf++;
	
}

void timer1_initialize(void)
{
    /*Enable PD_TIM power domain*/
	SetBits16(PMU_CTRL_REG, TIM_SLEEP, 0);                  // Enable the PD_TIM

	timer1_count_config(&timer1_config, timer1_overflow);   // Set the capture counting configurations for the timer

}

__STATIC_FORCEINLINE void timer1_clear_all_events(void)
{
    SetWord16(TIMER1_CLR_EVENT_REG, TIMER1_CLR_IN1_EVENT | TIMER1_CLR_IN2_EVENT | TIMER1_CLR_TIMER_EVENT);
}

/**
 ****************************************************************************************
 * @brief Init
 * @return void
 ****************************************************************************************
 */

static void rtc_interrupt_cb(uint8_t event)
{
	i = i+1;	
	
	read_adc_p0_1();
	rtcRead[i%1000][1] = (i >> 8) & 0xFF;; // MSB
	rtcRead[i%1000][0] = i & 0xFF; // LSB
	rtcRead[i%1000][2] = raw;
	if(rtcReadIndex < 1000)
	{
		rtcReadIndex = rtcReadIndex + 1;
	}
	
	if(i>30)
	{
						wkupct_enable_irq(WKUPCT_PIN_SELECT(GPIO_PORT_0, GPIO_PIN_3), // select pin (GPIO_BUTTON_PORT, GPIO_BUTTON_PIN)
									WKUPCT_PIN_POLARITY(GPIO_PORT_0, GPIO_PIN_3, WKUPCT_PIN_POLARITY_HIGH), // polarity high
									1, // 1 event
									63); // debouncing time = 63
	}
	
			
			if (raw < 100)
			{
				if (nesttrigger == false)
				{
					in_nest = true;	
					nesttrigger = true;
				}

			}
			
			if (in_nest == true)
			{
				if (raw > 190)
				{
					arch_ble_force_wakeup();
					user_scan_start();
					stopscantime = i + 6;
					in_nest = false;
				}
			}
			
			if (nesttrigger == true)
			{
				if (in_nest == false)
				{
					y = y + 1;
				}
			}
			
			if(y > 4)
			{
				if(nesttrigger == true)
				{
					arch_ble_force_wakeup();
					scan_stop();					
				}
				nesttrigger = false;
				
				SetBits16(PMU_CTRL_REG, TIM_SLEEP, 1);
				//user_app_adv_start();
			}
			
//			if(i == 100)
//			{
//				arch_ble_force_wakeup();
//				if(nesttrigger == true)
//				{
//					scan_stop();
//				}
//				
//				//scan_stop();
//				SetBits16(PMU_CTRL_REG, TIM_SLEEP, 1);
//				//wkupct_enable_irq(WKUPCT_PIN_SELECT(GPIO_PORT_0, GPIO_PIN_0), // select pin (GPIO_BUTTON_PORT, GPIO_BUTTON_PIN)
//				//					WKUPCT_PIN_POLARITY(GPIO_PORT_0, GPIO_PIN_0, WKUPCT_PIN_POLARITY_HIGH), // polarity high
//				//					1, // 1 event
//				//					63); // debouncing time = 63
//				user_app_adv_start();
//				
//			}
}

void userGPIOInterruptCallback() 
{
	arch_ble_force_wakeup();
	user_app_adv_start();


}
void user_app_on_init(void)
{
		i = 0;
	  y = 0;
		GPIO_Disable_HW_Reset();
	  //timer1_initialize();
	
    //timer1_clear_all_events();
    //timer1_enable_irq();
    //timer1_start();
	  app_param_update_request_timer_used = EASY_TIMER_INVALID_TIMER;
		app_adv_timeout_timer_used = EASY_TIMER_INVALID_TIMER;
	
	    mnf_data_init();
	
	
	   // Initialize Advertising and Scan Response Data
    memcpy(stored_adv_data, USER_ADVERTISE_DATA, USER_ADVERTISE_DATA_LEN);
    stored_adv_data_len = USER_ADVERTISE_DATA_LEN;
    memcpy(stored_scan_rsp_data, USER_ADVERTISE_SCAN_RESPONSE_DATA, USER_ADVERTISE_SCAN_RESPONSE_DATA_LEN);
    stored_scan_rsp_data_len = USER_ADVERTISE_SCAN_RESPONSE_DATA_LEN;
		
		GPIO_set_pad_latch_en(true);
		GPIO_ConfigurePin(GPIO_PORT_0, GPIO_PIN_3, INPUT_PULLDOWN, PID_GPIO, false);
		//GPIO_ConfigurePin(GPIO_PORT_0, GPIO_PIN_1, OUTPUT, PID_GPIO, true);
		GPIO_ConfigurePin(GPIO_PORT_0, GPIO_PIN_1, INPUT, PID_ADC, false);
		

		wkupct_register_callback(userGPIOInterruptCallback);
										
		arch_set_sleep_mode(app_default_sleep_mode);
		
		/* Power domain in which the RTC resides is disabled by default and so
   needs to be enabled before we can use it.  */
SetBits16(PMU_CTRL_REG, TIM_SLEEP, 0);
while ((GetWord32(SYS_STAT_REG) & TIM_IS_UP) != TIM_IS_UP);

/* Put RTC into known state */
rtc_reset();

/* Configure the RTC 100Hz clock, based on the LP clock frequency */
if (arch_clk_is_RCX20()) {
    uint32_t rcx_freq = 16000;
    /* Use the calibrated RCX frequency */
    rtc_clk_config(RTC_DIV_DENOM_1000, rcx_freq);
} else {
    rtc_clk_config(RTC_DIV_DENOM_1024, 32768);
}
rtc_clock_enable();

/* Default RTC configuration */
static const rtc_config_t cfg_rtc = {
    .hour_clk_mode = RTC_HOUR_MODE_24H,
    .keep_rtc = true,
};

/* Initialize RTC, it will be started when the date and time are set */
rtc_init(&cfg_rtc);

rtc_time_t time;
rtc_calendar_t clndr;
rtc_status_code_t status;

time.hour = 0;
time.minute = 0;
time.sec = 0;
time.hsec = 0;
time.hour_mode = RTC_HOUR_MODE_24H;
clndr.year = 1970;
clndr.month = 1;
clndr.mday = 1;
clndr.wday = 1;

/* Setting the time/date will also start the RTC if it was not running */
status = rtc_set_time_clndr(&time, &clndr);

rtc_register_intr(rtc_interrupt_cb, RTC_EVENT_SECOND);

//user_app_adv_start();
		
}


/**
 ****************************************************************************************
 * @brief Called upon device configuration complete message
 * @return void
 ****************************************************************************************
 */
void user_on_set_dev_config_complete( void )
{
	//();
		//user_scan_start();
}

/**
 ****************************************************************************************
 * @brief Scan complete function. Restart the scanning
 * @return void
 ****************************************************************************************
 */
void user_on_scan_complete(const uint8_t param){
    //arch_printf( "SCAN COMPLETE\r\n");
	// CAN ONLY START A TIMER FROM WITHIN HERE IF YOU START SCANNING FIRST - I guess this means
	// you cannot initiate a timer from sleep mode, only do a BLE core wakeup from one.
	// So maybe we set a flag, start scanning, start timer and then immediately use flag to stop scan so that true scanning
	// can start from our timer?????
	// Also appears that you cannot initate scanning from outside a callback? Maybe dependant on sleep mode?
	//  - yes timers dont work when instantiated while BLE core is sleeping
	//  - a timer can initiate scanning (tested so far with a timer started by callback)
	//  - a timer cannot wake up the device from sleep even when intantiated while awake
	// NOTE: app_easy_timer is a timer tied to the BLE core, therefore does not work when core is asleep
	// TRY: using timer 0/1/2
	//new_scan_start();
	//app_easy_timer(1000, test);
	//app_easy_timer(1000, new_scan_start);
}

/**
 ****************************************************************************************
 * @brief Advertising report function. Decode and display most popular advertising field
 * @return void
 ****************************************************************************************
 */
void user_adv_report_ind (struct gapm_adv_report_ind const * param ) {
/* Read time and date from on-chip RTC */
	if(param->report.evt_type == ADV_CONN_DIR )
	{
					//arch_printf("Found ADV_DIRECT_IND packet \r\n");
		uint8_t rssi_abs = 256 - param->report.rssi;
		//arch_printf("RSSI: -%d ", rssi_abs);
		// report the bluetooth device address
		//arch_printf( "[%02x:%02x:%02x:",  (int)param->report.adv_addr.addr[5], (int)param->report.adv_addr.addr[4], (int)param->report.adv_addr.addr[3] );
		//arch_printf( "%02x:%02x:%02x]\n\r", (int)param->report.adv_addr.addr[2], (int)param->report.adv_addr.addr[1], (int)param->report.adv_addr.addr[0] );
		//arch_printf( "%s", param->report);
		
		if(receivedPacketsIndex < 1000)
		{
			receivedPacketsA[receivedPacketsIndex][0] = rssi_abs;
			receivedPacketsA[receivedPacketsIndex][1] = (int)param->report.adv_addr.addr[4];
			receivedPacketsA[receivedPacketsIndex][2] = (int)param->report.adv_addr.addr[3];
			receivedPacketsA[receivedPacketsIndex][3] = (int)param->report.adv_addr.addr[2];
			receivedPacketsA[receivedPacketsIndex][4] = (int)param->report.adv_addr.addr[1];
			receivedPacketsA[receivedPacketsIndex][5] = (int)param->report.adv_addr.addr[0];
			receivedPacketsIndex = receivedPacketsIndex + 1;
		}
	}
	
}



/// @} APP
