#include <zephyr.h>
#include <device.h>

#include <display/cfb.h>
#include <drivers/flash.h>
#include <storage/flash_map.h>
#include <drivers/gpio.h>
#include <drivers/sensor.h>

#include <sys/printk.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh/access.h>

#include "mesh.h"
#include "board.h"

static uint32_t record_count;
struct k_delayed_work led_timer;
static const struct device *epd_dev;

struct font_info {
	uint8_t columns;
} fonts[] = {
	[FONT_BIG] = { .columns = 12 },
	[FONT_MEDIUM] = { .columns = 14 },
	[FONT_SMALL] = { .columns = 26 },
};

/* declare stat list that contains all the mesh information */
static struct stat
{
	uint16_t addr;
	char name[3];
	int temperature;
	int humidity;
	uint16_t neighbour_addr;
	char neighbour_name[4];
	int neighbour_temperature;
	int neighbour_humidity;
	int8_t rssi;
	int8_t distance;
} node_info[STAT_COUNT] = {
	[0 ...(STAT_COUNT - 1)] = {},
};

static struct {
	const struct device *dev;
	const char *name;
	gpio_pin_t pin;
	gpio_flags_t flags;
} leds[] = { { .name = DT_GPIO_LABEL(DT_ALIAS(led0), gpios),
	       .pin = DT_GPIO_PIN(DT_ALIAS(led0), gpios),
	       .flags = DT_GPIO_FLAGS(DT_ALIAS(led0), gpios) },
	     { .name = DT_GPIO_LABEL(DT_ALIAS(led1), gpios),
	       .pin = DT_GPIO_PIN(DT_ALIAS(led1), gpios),
	       .flags = DT_GPIO_FLAGS(DT_ALIAS(led1), gpios) },
	     { .name = DT_GPIO_LABEL(DT_ALIAS(led2), gpios),
	       .pin = DT_GPIO_PIN(DT_ALIAS(led2), gpios),
	       .flags = DT_GPIO_FLAGS(DT_ALIAS(led2), gpios) } };

/* print line */
static size_t print_line(enum font_size font_size, int row, const char *text,
						 size_t len, bool center)
{
	uint8_t font_height, font_width;
	uint8_t line[fonts[FONT_SMALL].columns + 1];
	int pad;
	epd_dev = device_get_binding(DISPLAY_DRIVER);
	cfb_framebuffer_set_font(epd_dev, font_size);

	len = MIN(len, fonts[font_size].columns);
	memcpy(line, text, len);
	line[len] = '\0';

	if (center)
		pad = (fonts[font_size].columns - len) / 2U;
	else
		pad = 0;
	cfb_get_font_size(epd_dev, font_size, &font_width, &font_height);
	cfb_print(epd_dev, line, font_width * pad, font_height * row);
	return len;
}

/* get humidity sensor value from "ti_hdc" sample */
int get_humidity(void)
{
	const struct device *dev;
	struct sensor_value humidity;

	dev = device_get_binding(DT_LABEL(DT_INST(0, ti_hdc)));
	sensor_sample_fetch(dev);
	sensor_channel_get(dev, SENSOR_CHAN_HUMIDITY, &humidity);

	int humidity_value = (int)humidity.val1;
	return humidity_value;
}


/* get temperature sensor value from "ti_hdc" sample */
unsigned int get_temperature(void)
{
	const struct device *dev;
	struct sensor_value temp;

	dev = device_get_binding(DT_LABEL(DT_INST(0, ti_hdc)));
	sensor_sample_fetch(dev);
	sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);

	int temperature_value = (int)temp.val1;
	if (temperature_value < 0)
		temperature_value = 0;
	return temperature_value;
}

/* Display main screen info */
void show_main()
{
	char str[100];
	int len, line = 0;
	epd_dev = device_get_binding(DISPLAY_DRIVER);
	cfb_framebuffer_clear(epd_dev, true);

	// if node is not provisioned 
	if (!mesh_is_initialized() || !mesh_is_prov_complete())
	{
		len = snprintf(str, sizeof(str), "Mesh Info :\n");
		print_line(FONT_SMALL, line++, str, len, true);

		len = snprintf(str, sizeof(str), "Name : Unassigned\n");
		print_line(FONT_SMALL, line++, str, len, false);

		char device_uuid_str[8+1];
		bin2hex(get_my_device_uuid() , 2 , device_uuid_str, sizeof(device_uuid_str));
		len = snprintf(str, sizeof(str), "UUID : %s", device_uuid_str);
		print_line(FONT_SMALL, line++, str, len, false);

		len = snprintf(str, sizeof(str), "Address : %s", "Unassigned");
		print_line(FONT_SMALL, line++, str, len, false);

		len = snprintf(str, sizeof(str), "Status : Advertising\n");
		print_line(FONT_SMALL, line++, str, len, false);
	}
	// if node is provisioned 
	else if (mesh_is_initialized() && mesh_is_prov_complete())
	{

		len = snprintf(str, sizeof(str), "Mesh Info :\n");
		print_line(FONT_SMALL, line++, str, len, true);

		len = snprintf(str, sizeof(str), "Name : %s", get_my_device_name());
		print_line(FONT_SMALL, line++, str, len, false);

		char device_uuid_str[8+1];
		bin2hex(get_my_device_uuid() , 2 , device_uuid_str, sizeof(device_uuid_str));
		len = snprintf(str, sizeof(str), "UUID : %s", device_uuid_str);
		print_line(FONT_SMALL, line++, str, len, false);

		len = snprintf(str, sizeof(str), "Address : 0x%04x\n", get_my_address());
		print_line(FONT_SMALL, line++, str, len, false);

		len = snprintf(str, sizeof(str), "Status : Provisioned\n");
		print_line(FONT_SMALL, line++, str, len, false);
	}
	
	len = snprintf(str, sizeof(str), "Temperature : %d C \n", get_temperature());
	print_line(FONT_SMALL, line++, str, len, false);

	len = snprintf(str, sizeof(str), "Humidity : %d%%\n",get_humidity());
	print_line(FONT_SMALL, line++, str, len, false);

	cfb_framebuffer_finalize(epd_dev);
}

/* return node_info list record count */
static int get_node_info_list_record_count()
{
	return record_count;
}

/* print node_info list info on the board */
void show_node_status()
{
	char str[100];
	int len, line = 0;
	epd_dev = device_get_binding(DISPLAY_DRIVER);
	cfb_framebuffer_clear(epd_dev, true);

	 // print my info
	len = snprintf(str, sizeof(str), "Node :%s: 0x%04x :%dC %d%% \n",
									 get_my_device_name(), get_my_address(), get_temperature(), get_humidity());
	print_line(FONT_SMALL, line++, str, len, true);

	// print my neighbours info
	len = snprintf(str, sizeof(str), "%s", "My Neighbours: \n");
	print_line(FONT_SMALL, line++, str, len, true);

	for (int i = 0; i < get_node_info_list_record_count(); i++)
	{
		struct stat *stat = &node_info[i];
		if (stat->addr != BT_MESH_ADDR_UNASSIGNED)
		{
			if (stat->addr == get_my_address())
			{
				char temp_hum[50];
				if (stat->neighbour_temperature >= 0 && stat->neighbour_humidity >= 0)
					snprintf(temp_hum, 50, "%dC %d%%", stat->neighbour_temperature, stat->neighbour_humidity);
				else
					snprintf(temp_hum, 20, "%s", "");
				int8_t round_result = (int8_t) round(stat->distance); 
				len = snprintf(str, sizeof(str), "%s:0x%04x %dm %d %s\n",
								   stat->neighbour_name, stat->neighbour_addr, round_result,stat->rssi,
								    temp_hum);
				print_line(FONT_SMALL, line++, str, len, false);
			}
		}
	}

	// print neighbours neighbours Info
	if(get_node_info_list_record_count() > 1)
	{
		len = snprintf(str, sizeof(str), "%s", "-----------------------\n");
		print_line(FONT_SMALL, line++, str, len, false);

		len = snprintf(str, sizeof(str), "%s", "Among Neighbours: \n");
		print_line(FONT_SMALL, line++, str, len, true);
	
		for (int i = 0; i < get_node_info_list_record_count(); i++)
		{
			struct stat *stat = &node_info[i];
			if (stat->addr != BT_MESH_ADDR_UNASSIGNED)
			{
				if (stat->addr != get_my_address())
				{
					int8_t round_result =  (int8_t) round(stat->distance); 
					len = snprintf(str, sizeof(str), "%s:0x%04x %s:0x%04x %dm\n",
								stat->name, stat->addr,
								stat->neighbour_name, stat->neighbour_addr, round_result);
					print_line(FONT_SMALL, line++, str, len, false);
				}
			}
		}
	}
	cfb_framebuffer_finalize(epd_dev);
}

/* check if record already exists in node_info list */
static bool check_if_record_exists(uint16_t address_hex)
{
	if(address_hex != BT_MESH_ADDR_UNASSIGNED && address_hex != get_my_address())
	{
		int is_record_exists = 0;
		for (int i = 0; i <  get_node_info_list_record_count(); i++)
		{
			struct stat *stat = &node_info[i];
			if (stat->addr == get_my_address())
			{
				if (stat->neighbour_addr == address_hex)
				{
					is_record_exists = 1;
					return is_record_exists;
				}
			}
		}
		return is_record_exists;
	}
	else
		return 1;
}

/* check if  neighbour record already exists in node_info list */
static bool check_if_record_neighbour_exists(uint16_t ctx_addr, uint16_t address_hex)
{
	int is_record_exists = 0;
	for (int i = 0; i <  get_node_info_list_record_count(); i++)
	{
		struct stat *stat = &node_info[i];
		if (address_hex != get_my_address() && ((stat->addr == ctx_addr && stat->neighbour_addr == address_hex) ||
											 (stat->neighbour_addr == ctx_addr && stat->addr == address_hex)))
		{
			is_record_exists = 1;
			return is_record_exists;
		}
	}
	return is_record_exists;
}


void board_add_node_and_neighbours_data(uint16_t from_address, char * message_str, double distance, int16_t rssi)
{
	// if message is of type "Provisioning Message" with message code as Q , 
	// then add record of node and provisioner as node's neigbour in node_info list
	if (strstr(message_str, "Q") != NULL) 
	{
		record_count = 0;
		struct stat *self_stat = &node_info[record_count];
		snprintf(self_stat->name, 3, "%s", get_my_device_name());
		char msg_str[20];
		snprintf(msg_str, 20, "%s", message_str);
		char *msg_token = strtok(msg_str, "=");
		msg_token = strtok(NULL, "=");
		snprintf(self_stat->neighbour_name, 4, "%s", get_provisioner_device_name());
		self_stat->addr = get_my_address();
		self_stat->humidity = get_humidity();
		self_stat->temperature = get_temperature();
		self_stat->neighbour_addr = from_address;
		self_stat->rssi = rssi;
		self_stat->distance = distance;
		record_count++;
	}

	//if message is of type "Neighbours Info Message" with message code as R , 
	// then add the record for node and extracted address as node's neigbour in node_info list
	if (strstr(message_str, "R") != NULL) 
	{
		char msg_str[20];
		snprintf(msg_str, 20, "%s", message_str);
		char *address_name_token;
		char address_str[7];
		address_name_token = strtok(msg_str, "=");
		address_name_token = strtok(NULL, "=");
		snprintf(address_str,7,"0X000%s",address_name_token);
		uint16_t address_hex = (uint16_t)strtol(address_str, NULL, 0);
		int is_record_exists = check_if_record_exists(address_hex);
		
		if (is_record_exists == 0)
		{
			struct stat *self_stat = &node_info[record_count];
			self_stat->addr = get_my_address();
			snprintf(self_stat->name, 3, "%s", get_my_device_name());
			self_stat->distance = self_stat->distance ;
			self_stat->humidity = get_humidity();
			self_stat->temperature = get_temperature();
			self_stat->addr = get_my_address();
			self_stat->rssi = self_stat->rssi ;
			self_stat->neighbour_addr = address_hex;
			address_name_token = strtok(NULL, "=");
			snprintf(self_stat->neighbour_name, 4, "%s", address_name_token);
			record_count++;
		}
	}

	// update distance and rssi value in node_info list with respect to neighbour with address as from_address
	for (int i = 0; i <  get_node_info_list_record_count(); i++)
	{
		struct stat *stat = &node_info[i];
		if (stat->addr == get_my_address())
		{
			if (stat->neighbour_addr == from_address)
			{
				stat->distance = distance;
				stat->rssi = rssi;
			}
		}
	}

	//if message is of type "Sensor Info Message" with message code as S, 
	// then save sensor info of neighbor with address as from_address
	if(strstr(message_str,"S")!=NULL) 
	{
		for (int i = 0; i <  get_node_info_list_record_count(); i++)
		{
			struct stat *stat = &node_info[i];
			if (stat->addr == get_my_address())
			{
				if (stat->neighbour_addr == from_address)
				{
					char msg_str[50];
					snprintf(msg_str, 50, "%s", message_str);
					char *msg_token;
					msg_token = strtok(msg_str, "=");
					msg_token = strtok(NULL, "=");
					snprintk(stat->neighbour_name, 4, "%s", msg_token);
					msg_token = strtok(NULL, "=");
					stat->neighbour_temperature = atoi(msg_token);
					msg_token = strtok(NULL, "=");
					stat->neighbour_humidity= atoi(msg_token);
					stat->rssi = rssi;
					stat->distance = distance;
				}
			}
		}
	}

	//if message is of type "Neighbours Neighbours Message" with message code as T, 
	// then add a recors of it in node_info list
	if(strstr(message_str,"T")!=NULL)
	{
		char msg_str[20];
		snprintf(msg_str, 20, "%s", message_str);
		char *address_name_token;
		char address_str[7];
		address_name_token = strtok(msg_str, "=");
		address_name_token = strtok(NULL, "=");
		snprintf(address_str,7,"0X000%s",address_name_token);
		uint16_t address_hex = (uint16_t)strtol(address_str, NULL, 0);
		if(address_hex!= BT_MESH_ADDR_UNASSIGNED)
		{
			int is_record_exists = check_if_record_neighbour_exists(from_address,address_hex);
			
			if (is_record_exists==0) 
			{
				struct stat *stat = &node_info[record_count];
				stat->addr = from_address;
				stat->neighbour_addr = address_hex;
				address_name_token = strtok(NULL, "=");
				snprintf(stat->neighbour_name, 4, "%s", address_name_token);
				record_count++;
			}
		}	
	}

	//if message is of type "Neighbours Neighbours Update Info Message" with message code as U, 
	// update received info to corresponding neighbor neighbour record in node_info list
	if(strstr(message_str,"U")!=NULL)
	{
		char msg_str[20];
		snprintf(msg_str, 20, "%s", message_str);
		char *address_name_token;
		char address_str[7];
		address_name_token = strtok(msg_str, "=");
		address_name_token = strtok(NULL, "=");
		snprintf(address_str,7,"0X000%s",address_name_token);
		uint16_t address_hex = (uint16_t)strtol(address_str, NULL, 0);
		int is_record_exists = check_if_record_neighbour_exists(from_address, address_hex);
		if (is_record_exists == 1)
		{
			for (int i = 0; i <  get_node_info_list_record_count(); i++)
			{
				struct stat *stat = &node_info[i];
				if(stat->addr == from_address && stat->addr  != get_my_address() &&  stat->neighbour_addr == address_hex)
				{
					address_name_token = strtok(NULL, "=");
					int8_t round_value = atoi(address_name_token);
					stat->distance = round_value ;
					address_name_token = strtok(NULL, "=");
					snprintf(stat->name, 3, "%s", address_name_token);
				}
			}
		}
	}
	
	// send sensor Info , Neighbours Neighbours Info  stores in node_info list to all my neighbours
	for (int i = 0; i < get_node_info_list_record_count() ; i++) 
	{
		struct stat *stat = &node_info[i];
		if(stat->addr == get_my_address())
		{
			char sensor_info_message[12];
			snprintf(sensor_info_message, 12, "S=%s=%d=%d", get_my_device_name(),get_temperature(), get_humidity());
			send_message(stat->neighbour_addr, sensor_info_message);	

			if(stat->neighbour_addr != from_address)
			{
				char addr_string[8];
				snprintf(addr_string,8,"0x%04x",stat->neighbour_addr);
				char c = addr_string[strlen(addr_string) -1];

				char neighbour_addr_name_msg[10];
				snprintf(neighbour_addr_name_msg,10,"T=%c=%s",c,stat->neighbour_name);						
				send_message(from_address,neighbour_addr_name_msg);
								
				char neighbour_addr_dist_message[17],dev_name[4];
				snprintf(dev_name, 4, "%s",get_my_device_name());
				int8_t round_result = (int8_t) round(stat->distance); 
				snprintf(neighbour_addr_dist_message, 17, "U=%c=%d=%s", c,round_result,dev_name);
				send_message(from_address,neighbour_addr_dist_message);	
			}	
		}
			
	}

	// print all reciors of node_info list on serial log 
	printk ("Record No. : Node_Address Node_Name  Node_Temperature  Node_Humidity Neighbour_Address Neighbour_Name Neighbour_Temperature Neighbour_Humidity Distance Rssi\n ");
	for (int i = 0; i < get_node_info_list_record_count() ; i++) {
	struct stat *stat = &node_info[i];
	printk ("Record %d : 0x%04x %s %dC %d%%    0x%04x %s %dC %d%%    %dm %d\n ",
			i+1 ,stat->addr, stat->name, stat->temperature,stat->humidity,
			stat->neighbour_addr , stat->neighbour_name,stat->neighbour_temperature,stat->neighbour_humidity,
			stat->distance,stat->rssi);
	}
	printk("\n ");

	//display node_info list info on board
	show_node_status();
}

/*blink board leds lights */
void board_blink_leds(void)
{
	k_delayed_work_submit(&led_timer, K_MSEC(100));
}

/*set led state */
int set_led_state(uint8_t id, bool state)
{
	return gpio_pin_set(leds[id].dev, leds[id].pin, state);
}

/* set led timeout */
static void led_timeout(struct k_work *work)
{
	static int led_cntr;
	int i;

	// Disable all LEDs
	for (i = 0; i < ARRAY_SIZE(leds); i++) {
		set_led_state(i, 0);
	}

	// Stop after 5 iterations
	if (led_cntr >= (ARRAY_SIZE(leds) * 5)) {
		led_cntr = 0;
		return;
	}

	// Select and enable current LED 
	i = led_cntr++ % ARRAY_SIZE(leds);
	set_led_state(i, 1);

	k_delayed_work_submit(&led_timer, K_MSEC(100));
}

/* configure board leds */
static int configure_leds(void)
{
	for (int i = 0; i < ARRAY_SIZE(leds); i++) {
		leds[i].dev = device_get_binding(leds[i].name);
		if (!leds[i].dev) {
			return -ENODEV;
		}
		gpio_pin_configure(leds[i].dev, leds[i].pin,
				   leds[i].flags | GPIO_OUTPUT_INACTIVE);
	}
	k_delayed_work_init(&led_timer, led_timeout);
	return 0;
}

/*show text on board display */
void board_show_text(const char * message_str, bool center)
{
	int line = 0;
	epd_dev = device_get_binding(DISPLAY_DRIVER);
	cfb_framebuffer_clear(epd_dev, true);
	print_line(FONT_SMALL, line++, message_str, strlen(message_str), center);
	cfb_framebuffer_finalize(epd_dev);
}

/*initialize device board */
void board_init(void)
{
	epd_dev = device_get_binding(DISPLAY_DRIVER);	
	cfb_framebuffer_init(epd_dev) ;
	cfb_framebuffer_clear(epd_dev, true);
	configure_leds();
}
