#define STAT_COUNT 10

#define MOD_LF            0x0000
#define NET_IDX           0x000
#define APP_IDX           0x000
#define DEFAULT_TTL       31
#define MSG_MAX_LEN       12
#define OP_MSG        	  0xbb
#define OP_VND_MSG        BT_MESH_MODEL_OP_3(OP_MSG, BT_COMP_ID_LF)
#define IV_INDEX          0
#define FLAGS             0


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

/* get functions */
int get_node_info_list_record_count(void);
uint16_t get_my_address(void);
const uint8_t * get_my_device_uuid(void);
char * get_my_device_name(void);

void send_message(uint16_t to_address, const char * message_str);

/*mesh functions */
void mesh_start(void);
bool mesh_is_initialized(void);


