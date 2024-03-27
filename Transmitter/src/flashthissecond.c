//prj.conf settings are required for this to function

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/settings/settings.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>

static struct bt_le_adv_param adv_param; // Holds params for advertising
static bt_addr_le_t bond_addr; // Address to advertise to (reciever address)
static bt_addr_le_t identity_zero_addr; // Adress to initialize default ID 
static bt_addr_le_t transmit_addr; // Address to transmit from (transmitter address)

struct sensor_value val;
const struct device *const dev = DEVICE_DT_GET(DT_ALIAS(qdec0));

#define HE0_NODE DT_ALIAS(sw0)
static const struct gpio_dt_spec HESensor = GPIO_DT_SPEC_GET(HE0_NODE, gpios);
static struct gpio_callback halleffect_cb_data;

float currentAngle = 0; // Stores angle of rotation
float actualDegrees = 0;

// ---------------------------- HALL EFFECT INTERRUPT -------------------------

void halleffect_cb(const struct device *dev, struct gpio_callback *cb,
                    uint32_t pins)
{
    //printk("HE triggered");
	currentAngle = 0;
	actualDegrees = 0;
}

// ---------------------------- DIRECT ADVERTISING ----------------------------

// Ensures settings are loaded
static void bt_ready(void)
{
	printk("Bluetooth initialized\n");
	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}
}

int main(void)
{
	gpio_pin_configure_dt(&HESensor, GPIO_INPUT);
	gpio_pin_interrupt_configure_dt(&HESensor, GPIO_INT_EDGE_BOTH);
	gpio_init_callback(&halleffect_cb_data, halleffect_cb, BIT(HESensor.pin));
	gpio_add_callback(HESensor.port, &halleffect_cb_data);

	int err;
	// NOTE: ID creation should be before bt_enable to get around IRK (I think) and privacy must be turned off in the config
	// Create ID 0, this is needed (I think?) but the identity does not get used
	bt_addr_le_from_str("DA:EE:AA:FF:FF:41", "random", &identity_zero_addr); // Has to be different to ID 1 upon initialization
 	err = bt_id_create(&identity_zero_addr, NULL);
	// Debug output
 	if (err < 0) {
 		printk("Creating new ID 0 failed (err %d)\n", err);
 	}

	// Create ID 1, this is the identity we will use for transmission
	bt_addr_le_from_str("DA:EE:AA:FF:FF:00", "random", &transmit_addr);
	err = bt_id_create(&transmit_addr, NULL); 
	// Debug output
 	if (err < 0) {
 		printk("Creating new ID 1 failed (err %d)\n", err);
 	}

	// Enable bluetooth connectivity
	err = bt_enable(NULL);
	// Debug output
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	// Begin advertisement
	bt_ready();

	while (1) {
		char toAddrOutput[BT_ADDR_LE_STR_LEN];
		char fromAddrOutput[BT_ADDR_LE_STR_LEN];

		// Copy reciever address to a variable
		bt_addr_le_copy(&bond_addr, BT_ADDR_LE_NONE); 
		bt_addr_le_from_str("80:EA:CA:70:00:04", "public", &bond_addr);

		bt_le_adv_stop(); // Stop advertising here to change value of transmitter address before transmission
		
		sensor_sample_fetch(dev);
		sensor_channel_get(dev, SENSOR_CHAN_ROTATION, &val);

		actualDegrees = (val.val1 / 488.3);
		currentAngle += actualDegrees;

		if (currentAngle >= 360)
		{
			currentAngle = fmod(currentAngle, 360);
		}

		//printk("Position = %d degrees\n", currentAngle);

		
		
		//transmit_addr.a.val[0]=transmit_addr.a.val[0]+1; // Alter transmitter address
		int uniqueID = 10;
		int angleHex = (currentAngle - fmod(currentAngle, 16.0f)) / 16;
		int angleRemainder = fmod(currentAngle, 16.0f);

		// Get current clock cycle (clock runs at 32768 Hz on nrf52dk_nrf52832 & (presumably) on the EV-BT832X)
		int systemClockSpeed = 32768;
		uint32_t systemClock = k_cycle_get_32() / (systemClockSpeed / 1000); // Divide to give ms accuracy.

		// Transmitted Packets are in the format:
			// 00 : First Two Hex Digits of Time : Next Two Hex Digits of Time : Last Two Hex Digits of Time : First Two Hex Digits of Angle : Last Hex Digit of Angle & ID
		transmit_addr.a.val[4] = systemClock / 65536;
		transmit_addr.a.val[3] = (systemClock % 65536) / 256;
		transmit_addr.a.val[2] = systemClock % 256;
		transmit_addr.a.val[1] = angleHex;
		transmit_addr.a.val[0] = uniqueID + (angleRemainder * 16);

		//transmit_addr.a.val[0] = uniqueID;
		//transmit_addr.a.val[1] = transmit_addr.a.val[1] + 1;
		bt_id_reset(1,&transmit_addr, NULL); // Update ID with new transmitter address

		// Debug output addresses
		bt_addr_le_to_str(&bond_addr, toAddrOutput, sizeof(toAddrOutput));
		bt_addr_le_to_str(&transmit_addr, fromAddrOutput, sizeof(fromAddrOutput));
		printk("Direct advertising to %s from %s\n", toAddrOutput, fromAddrOutput);

		// Send direct advertisment
		adv_param = *BT_LE_ADV_CONN_DIR(&bond_addr); // Advertise to reciever address
		adv_param.id=1; // Set to ID 1 - this is the ID we are using to set our transmitter address
		err = bt_le_adv_start(&adv_param, NULL, 0, NULL, 0);

		// Output debug error
		if (err) {
			printk("Advertising failed to start (err %d)\n", err);
		} else {
			printk("Advertising successfully started\n");
		}	
		k_msleep(20);
	}
	return 0;
}
