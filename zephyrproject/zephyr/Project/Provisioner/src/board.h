/*
 * Copyright (c) 2018 Phytec Messtechnik GmbH
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

enum periph_device {
	DEV_IDX_HDC1010 = 0,
	DEV_IDX_MMA8652,
	DEV_IDX_APDS9960,
	DEV_IDX_EPD,
	DEV_IDX_NUMOF,
};

enum font_size {
	FONT_SMALL = 0,
	FONT_MEDIUM = 1,
	FONT_BIG = 2,
};


size_t first_name_len(const char *name);
void show_main(void);
void show_node_status();
void show_node_track( k_timeout_t duration);

void board_show_text(const char *text, bool center, k_timeout_t duration);
void board_blink_leds(void);
void board_add_hello(uint16_t addr, const char *name ,int8_t rssi, int16_t distance );
int set_led_state(uint8_t id, bool state);
int periphs_init(void);
int board_init(void);
