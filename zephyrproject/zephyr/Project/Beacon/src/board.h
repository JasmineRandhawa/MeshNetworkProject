#define STAT_COUNT 10

#if defined(CONFIG_SSD16XX)
#define DISPLAY_DRIVER "SSD16XX"
#else
#error Unsupported board
#endif

#if defined(CONFIG_SSD16XX)
#define DISPLAY_DRIVER "SSD16XX"
#endif

#if defined(CONFIG_SSD1306)
#define DISPLAY_DRIVER "SSD1306"
#endif

#ifndef DISPLAY_DRIVER
#define DISPLAY_DRIVER "DISPLAY"
#endif

enum font_size {
	FONT_SMALL = 0,
	FONT_MEDIUM = 1,
	FONT_BIG = 2,
};

void show_main(void);

/*board functions */
void board_show_text(const char *text, bool center);
void board_add_node_and_neighbours_data(uint16_t from_address, char * message_str , double distance, int16_t rssi);
void board_init(void);
void board_blink_leds(void);

/* sensor functions */
unsigned int get_temperature(void);
int get_humidity(void);
