#define MOD_LF            0x0000
#define APP_IDX           0x000
#define DEFAULT_TTL       31
#define MSG_MAX_LEN       12
#define OP_MSG        	  0xbb
#define OP_VND_MSG        BT_MESH_MODEL_OP_3(OP_MSG, BT_COMP_ID_LF)

/* get functions */
const uint8_t * get_my_device_uuid(void);
uint16_t get_my_address(void);
uint16_t get_provisioner_address(void);
char * get_my_device_name(void);
char * get_provisioner_device_name(void);

void send_message(uint16_t to_address, const char * message_str);

/* mesh functions */
bool mesh_is_initialized(void);
bool mesh_is_prov_complete(void);
void mesh_start(void);
