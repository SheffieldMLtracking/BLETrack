//prj.conf settings are required for this to function

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
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
//#include <zephyr/drivers/gpio.h>

static struct bt_le_adv_param adv_param; // Holds params for advertising
static bt_addr_le_t bond_addr; // Address to advertise to (reciever address)
static bt_addr_le_t identity_zero_addr; // Adress to initialize default ID 
static bt_addr_le_t transmit_addr; // Address to transmit from (transmitter address)

static void advertising_start(struct k_work *work);
static K_WORK_DEFINE(start_advertising_worker, advertising_start);

//#define SW0_NODE DT_ALIAS(sw0)
//static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
//static struct gpio_callback button_cb_data;

// ---------------------------- GPIO - IR --------------------------------------

void halleffect_triggered(const struct device *dev, struct gpio_callback *cb,
                    uint32_t pins)
{
    printk("HE triggered");
}

// ---------------------------- NECESSARY FUNCTIONS ----------------------------

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err == BT_HCI_ERR_ADV_TIMEOUT) {
		printk("Trying to re-start directed adv.\n");
		k_work_submit(&start_advertising_worker);
	}
	else if (err) {
		printk("Connection failed (err 0x%02x)\n", err);
	} else {
		printk("Connected\n");
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected (reason 0x%02x)\n", reason);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected
};

// ---------------------------- DIRECT ADVERTISING ----------------------------

// Worker to advertise
static void advertising_start(struct k_work *work)
{
	// Just for debug output
	int err;
	char toAddrOutput[BT_ADDR_LE_STR_LEN];
	char fromAddrOutput[BT_ADDR_LE_STR_LEN];

	// Copy reciever address to a variable
	bt_addr_le_copy(&bond_addr, BT_ADDR_LE_NONE); 
	bt_addr_le_from_str("80:EA:CA:70:00:04", "public", &bond_addr);
	
	bt_le_adv_stop(); // Stop advertising here to change value of transmitter address before transmission
	transmit_addr.a.val[0]=transmit_addr.a.val[0]+1; // Alter transmitter address
													 // (arbitrary change here as an example, in practical usage adress is set to encoded data here)
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
}

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

	//gpio_pin_configure_dt(&button, GPIO_INPUT);

	//gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH);

	//gpio_init_callback(&button_cb_data, halleffect_triggered, BIT(button.pin));
	//gpio_add_callback(button.port, &button_cb_data);
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
	k_work_submit(&start_advertising_worker);
	while (1) {
		k_sleep(K_FOREVER);
	}
	return 0;
}