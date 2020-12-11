enum font_size
{
	FONT_SMALL = 0,
	FONT_MEDIUM = 1,
	FONT_BIG = 2,
};

void show_main(void);

/*board functions */
void board_show_text(const char *text, bool center);
void board_init(void);
void board_blink_leds(void);
size_t print_line(enum font_size font_size, int row, const char *text, size_t len, bool center);

/* sensor functions */
unsigned int get_temperature(void);
int get_humidity(void);

