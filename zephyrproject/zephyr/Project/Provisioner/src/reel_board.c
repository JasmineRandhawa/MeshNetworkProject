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

#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh/access.h>

#include "mesh.h"
#include "board.h"

struct font_info {
	uint8_t columns;
} fonts[] = {
	[FONT_BIG] = { .columns = 12 },
	[FONT_MEDIUM] = { .columns = 14 },
	[FONT_SMALL] = { .columns = 26 },
};

struct k_delayed_work led_timer;
static const struct device *epd_dev;

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
size_t print_line(enum font_size font_size, int row, const char *text,
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

	len = snprintf(str, sizeof(str), "Mesh Info :\n");
	print_line(FONT_SMALL, line++, str, len, true);

	char uuid_hex_str[8 + 1];
	bin2hex(get_my_device_uuid(), 2, uuid_hex_str, sizeof(uuid_hex_str));
	len = snprintf(str, sizeof(str), "Name : %s   UUID: %s", get_my_device_name() , uuid_hex_str);
	print_line(FONT_SMALL, line++, str, len, false);

	if( !mesh_is_initialized())
	{
		len = snprintf(str, sizeof(str), "Address: %s ", "Unassigned");
		print_line(FONT_SMALL, line++, str, len, false);
		len = snprintf(str, sizeof(str), "Status : %s ", "UnProvisioned");
		print_line(FONT_SMALL, line++, str, len, false);
	}
	else
	{
		len = snprintf(str, sizeof(str), "Address : 0x%04x ", get_my_address());
		print_line(FONT_SMALL, line++, str, len, false);
		len = snprintf(str, sizeof(str), "Status : %s ", "Provisioned");
		print_line(FONT_SMALL, line++, str, len, false);
	}
	
	len = snprintf(str, sizeof(str), "Temp :%dC  Humidity :%d%%\n",
		       get_temperature(),get_humidity());
	print_line(FONT_SMALL, line++, str, len, false);  
	len = snprintf(str, sizeof(str), "Node Count : %d",get_node_info_list_record_count()+1);
	print_line(FONT_SMALL, line++, str, len, false);

	cfb_framebuffer_finalize(epd_dev);
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
