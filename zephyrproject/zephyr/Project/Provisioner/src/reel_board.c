/*
 * Copyright (c) 2018 Phytec Messtechnik GmbH
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <display/cfb.h>
#include <sys/printk.h>
#include <drivers/flash.h>
#include <storage/flash_map.h>

#include <string.h>
#include <stdio.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh/access.h>
#include <bluetooth/hci.h>

#include "mesh.h"
#include "board.h"

struct font_info {
	uint8_t columns;
} fonts[] = {
	[FONT_BIG] = { .columns = 12 },
	[FONT_MEDIUM] = { .columns = 16 },
	[FONT_SMALL] = { .columns = 25 },
};

#define STAT_COUNT 128

static const struct device *epd_dev;
static bool pressed;
static const struct device *gpio;

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

size_t first_name_len(const char *name)
{
	size_t len;

	for (len = 0; *name; name++, len++) {
		switch (*name) {
		case ' ':
		case ',':
		case '\n':
			return len;
		}
	}

	return len;
}

struct k_delayed_work led_timer;

static size_t print_line(enum font_size font_size, int row, const char *text,
			 size_t len, bool center)
{
	uint8_t font_height, font_width;
	uint8_t line[fonts[FONT_SMALL].columns + 1];
	int pad;

	cfb_framebuffer_set_font(epd_dev, font_size);

	len = MIN(len, fonts[font_size].columns);
	memcpy(line, text, len);
	line[len] = '\0';

	if (center) {
		pad = (fonts[font_size].columns - len) / 2U;
	} else {
		pad = 0;
	}

	cfb_get_font_size(epd_dev, font_size, &font_width, &font_height);

	if (cfb_print(epd_dev, line, font_width * pad, font_height * row)) {
		printk("Failed to print a string\n");
	}

	return len;
}

static size_t get_len(enum font_size font, const char *text)
{
	const char *space = NULL;
	size_t i;

	for (i = 0; i <= fonts[font].columns; i++) {
		switch (text[i]) {
		case '\n':
		case '\0':
			return i;
		case ' ':
			space = &text[i];
			break;
		default:
			continue;
		}
	}

	/* If we got more characters than fits a line, and a space was
	 * encountered, fall back to the last space.
	 */
	if (space) {
		return space - text;
	}

	return fonts[font].columns;
}

void board_blink_leds(void)
{
	k_delayed_work_submit(&led_timer, K_MSEC(100));
}


static struct stat {
	uint16_t addr;
	char name[9];
	int8_t rssi;
	int16_t distance;
	uint16_t hello_count;

} stats[STAT_COUNT] = {
	[0 ...(STAT_COUNT - 1)] = {},
};

static uint32_t stat_count;

#define NO_UPDATE -1

static int add_hello(uint16_t addr, const char *name ,int8_t rssi, int16_t distance)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(stats); i++) {
		struct stat *stat = &stats[i];

		if (!stat->addr) {
			stat->addr = addr;
			strncpy(stat->name, name, sizeof(stat->name) - 1);
			stat->distance = distance;
			stat->rssi = rssi;
			stat->hello_count = 1U;
			stat_count++;
			return i;
		}

		if (stat->addr == addr) {
			/* Update name, incase it has changed */
			strncpy(stat->name, name, sizeof(stat->name) - 1);
			stat->distance = distance;
			stat->rssi = rssi;
			if (stat->hello_count < 0xffff) {
				stat->hello_count++;
				return i;
			}

			return NO_UPDATE;
		}
	}

	return NO_UPDATE;
}

void board_add_hello(uint16_t addr, const char *name ,int8_t rssi, int16_t distance )
{
	uint32_t sort_i;

	sort_i = add_hello(addr, name,rssi,distance);
	if (sort_i != NO_UPDATE) {
	}
}

void show_main(void)
{
	char str[100];
	int len, line = 0;
	cfb_framebuffer_clear(epd_dev, true);

	len = snprintk(str, sizeof(str), "Mesh Info:\n");
	print_line(FONT_SMALL, line++, str, len, true);

	len = snprintk(str, sizeof(str), "Device Name: %s", bt_get_name());
	print_line(FONT_SMALL, line++, str, len, false);

	char uuid_hex_str[8 + 1];
	bin2hex(get_uuid(), 4, uuid_hex_str, sizeof(uuid_hex_str));
	len = snprintk(str, sizeof(str), "Device UUID:  %s", uuid_hex_str);
	print_line(FONT_SMALL, line++, str, len, false);

	len = snprintk(str, sizeof(str), "Addr:0x%04x  Node Count:%u", get_my_addr(),stat_count + 1);
	print_line(FONT_SMALL, line++, str, len, false);

	char net_key_str[100];
	bin2hex(get_net_key(), 8, net_key_str, sizeof(net_key_str));
	len = snprintk(str, sizeof(str), "N/w key: %s", net_key_str);
	print_line(FONT_SMALL, line++, str, len, false);

	char app_key_str[100];
	bin2hex(get_app_key(), 8, app_key_str, sizeof(app_key_str));
	len = snprintk(str, sizeof(str), "App key: %s", app_key_str);
	print_line(FONT_SMALL, line++, str, len, false);

	char dev_key_str[100];
	bin2hex(get_dev_key(), 8, dev_key_str, sizeof(dev_key_str));
	len = snprintk(str, sizeof(str), "Dev key: %s", dev_key_str);
	print_line(FONT_SMALL, line++, str, len, false);
	cfb_framebuffer_finalize(epd_dev);
}

void board_show_text(const char *text, bool center, k_timeout_t duration)
{
	int i;

	cfb_framebuffer_clear(epd_dev, true);

	for (i = 0; i < 3; i++) {
		size_t len;

		while (*text == ' ' || *text == '\n') {
			text++;
		}

		len = get_len(FONT_BIG, text);
		if (!len) {
			break;
		}

		text += print_line(FONT_MEDIUM, i, text, len, center);
		if (!*text) {
			break;
		}
	}

	cfb_framebuffer_finalize(epd_dev);

	if (!K_TIMEOUT_EQ(duration, K_FOREVER)) {
		show_main();
	}
}

void show_node_status( )
{
	char str[100];
	int len, line = 0;
	cfb_framebuffer_clear(epd_dev, true);

	int i;

	len = snprintk(str, sizeof(str), "My Addr: 0x%04x \n" ,get_my_addr());
	print_line(FONT_SMALL, line++, str, len, false);

	len = snprintk(str, sizeof(str), "Messages: \n");
	print_line(FONT_SMALL, line++, str, len, true);

	for (i = 0; i < stat_count ; i++) {
		struct stat *stat = &stats[i];

		len = snprintk(str, sizeof(str), "Node Addr: 0x%04x \n" ,stat->addr);
		print_line(FONT_SMALL, line++, str, len, false);

		len = snprintk(str ,sizeof(str), "Node Name: %s", stat->name);
		print_line(FONT_SMALL, line++, str, len, false);

		len = snprintk(str, sizeof(str), "Node Rssi: %d", stat->rssi);
		print_line(FONT_SMALL, line++, str, len, false);

		len = snprintk(str, sizeof(str), "Node Distance: %u meteres", stat->distance);
		print_line(FONT_SMALL, line++, str, len, false);
	}
	cfb_framebuffer_finalize(epd_dev);
}

void show_node_track( k_timeout_t duration)
{
	char str[100];
	int len, line = 0;
	cfb_framebuffer_clear(epd_dev, true);
	//char name[9];
	int i;

	len = snprintk(str, sizeof(str), "Node Track: \n");
	print_line(FONT_SMALL, line++, str, len, true);

	for (i = 0; i < ARRAY_SIZE(stats); i++) {
		//struct stat *stat = &stats[i];
		//uint16_t distance = stat->distance;
		//strncpy(stat->name, name, sizeof(stat->name) - 1);
	}
	cfb_framebuffer_finalize(epd_dev);

	if (!K_TIMEOUT_EQ(duration, K_FOREVER)) {
		show_node_status();
	}
}

static bool button_is_pressed(void)
{
	return gpio_pin_get(gpio, DT_GPIO_PIN(DT_ALIAS(sw0), gpios)) > 0;
}


static int configure_button(void)
{

	gpio = device_get_binding(DT_GPIO_LABEL(DT_ALIAS(sw0), gpios));
	if (!gpio) {
		return -ENODEV;
	}

	gpio_pin_configure(gpio, DT_GPIO_PIN(DT_ALIAS(sw0), gpios),
			   GPIO_INPUT | DT_GPIO_FLAGS(DT_ALIAS(sw0), gpios));

	gpio_pin_interrupt_configure(gpio, DT_GPIO_PIN(DT_ALIAS(sw0), gpios),
				     GPIO_INT_EDGE_BOTH);

	return 0;
}

int set_led_state(uint8_t id, bool state)
{
	return gpio_pin_set(leds[id].dev, leds[id].pin, state);
}

static void led_timeout(struct k_work *work)
{
	static int led_cntr;
	int i;

	/* Disable all LEDs */
	for (i = 0; i < ARRAY_SIZE(leds); i++) {
		set_led_state(i, 0);
	}

	/* Stop after 5 iterations */
	if (led_cntr >= (ARRAY_SIZE(leds) * 5)) {
		led_cntr = 0;
		return;
	}

	/* Select and enable current LED */
	i = led_cntr++ % ARRAY_SIZE(leds);
	set_led_state(i, 1);

	k_delayed_work_submit(&led_timer, K_MSEC(100));
}

static int configure_leds(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(leds); i++) {
		leds[i].dev = device_get_binding(leds[i].name);
		if (!leds[i].dev) {
			printk("Failed to get %s device\n", leds[i].name);
			return -ENODEV;
		}

		gpio_pin_configure(leds[i].dev, leds[i].pin,
				   leds[i].flags | GPIO_OUTPUT_INACTIVE);
	}

	k_delayed_work_init(&led_timer, led_timeout);
	return 0;
}

static int erase_storage(void)
{
	const struct device *dev;

	dev = device_get_binding(DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL);

	return flash_erase(dev, FLASH_AREA_OFFSET(storage),
			   FLASH_AREA_SIZE(storage));
}


int board_init(void)
{
	epd_dev = device_get_binding(DT_LABEL(DT_INST(0, solomon_ssd16xxfb)));
	if (epd_dev == NULL) {
		printk("SSD16XX device not found\n");
		return -ENODEV;
	}

	if (cfb_framebuffer_init(epd_dev)) {
		printk("Framebuffer initialization failed\n");
		return -EIO;
	}

	cfb_framebuffer_clear(epd_dev, true);

	if (configure_button()) {
		printk("Failed to configure button\n");
		return -EIO;
	}

	if (configure_leds()) {
		printk("LED init failed\n");
		return -EIO;
	}

	pressed = button_is_pressed();
	if (pressed) {
		printk("Erasing storage\n");
		board_show_text("Resetting Device", false, K_SECONDS(4));
		erase_storage();
	}
	return 0;
}
