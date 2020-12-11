#include <zephyr.h>

#include <sys/printk.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh.h>

#include "mesh.h"
#include "board.h"

/*declare static variables */
static uint8_t my_device_uuid[16] = { 0xbb, 0xbb };
static uint16_t provisioner_address, my_address;
static char my_device_name[4], provisioner_device_name[2];
static bool is_message_recieved_from_prov = false;

/*compute distance in metres from RSSI value */
static double calculate_distance_in_metres(int8_t rssi)
{
	double default_tx = -59; // tx power of nrf52 chip is -59dm at 1m
	double env_factor = 2.7; //constant for environment factor . Range 2 to 4
	double exponent = (default_tx - rssi) / (10 * env_factor);
	double base = 10.0;
	double result = pow(base, exponent);
	return result;
}

/* receive message from a node */
static void receive_message(struct bt_mesh_model *model , struct bt_mesh_msg_ctx *ctx ,
		      		        struct net_buf_simple *buf , int8_t rssi)
{
	// If Sender address is my own address or unassigned address, return 
	if (ctx->addr == get_my_address() || ctx->addr== BT_MESH_ADDR_UNASSIGNED) 
		return;

	// extract message content		
	char message_str[32];
	size_t len =  MIN(buf->len, MSG_MAX_LEN);
	memcpy(message_str, buf->data, len);
	message_str[len] = '\0';
	printk("Received Message %s from 0x%04x \n\n", message_str, ctx->addr);
	
	// Compute distance in metres from RSSI  value 
	double distance  = calculate_distance_in_metres(rssi);
	
	// if distnace is less than 2 m , blink leds 
	if (distance < 2 ) 
		board_blink_leds();
	
	// extract assigned device name and provisioner name from message string and save them to variables
	if(strstr(message_str,"Q")!=NULL) // Q is the mssage code for provisioning message
	{
		char msg_str[32];
		snprintf(msg_str, 32, "%s", message_str);

		// break "=" delimited message into tokens 
		char * msg_token = strtok(msg_str, "=");

		//extract provisioner device name
		msg_token = strtok(NULL, "=");
		snprintf(provisioner_device_name, 2, "%s", msg_token);
		msg_token = strtok(NULL, "=");

		//provisioner address as sender address
		provisioner_address = ctx->addr;

		//extract my device name
		snprintf(my_device_name, 4, "%s", msg_token);

		//acknowledge reception of provisioning message
		is_message_recieved_from_prov = true;

		show_main();
	}
	
	// store the message information to the list
	board_add_node_and_neighbours_data(ctx->addr , message_str , distance , rssi);
}

static struct bt_mesh_cfg_cli cfg_cli = {
};

static struct bt_mesh_model root_models[] = {
	BT_MESH_MODEL_CFG_CLI(&cfg_cli),
	BT_MESH_MODEL_CFG_SRV,
};

static const struct bt_mesh_model_op vnd_ops[] = {
	{ OP_VND_MSG, 1, receive_message },
	BT_MESH_MODEL_OP_END,
};

static struct bt_mesh_model vnd_models[] = {
	BT_MESH_MODEL_VND(BT_COMP_ID_LF, MOD_LF, vnd_ops, NULL, NULL),
};

static struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(0, root_models, vnd_models),
};

static const struct bt_mesh_comp comp = {
	.cid = BT_COMP_ID_LF,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

/*get first word of message string */
static size_t get_first_word(const char * message_str)
{
	size_t len; 
	for (len = 0; * message_str; message_str++, len++)
	{
		switch (* message_str)
		{
		case ' ':
		case ',':
		case '\n':
			return len;
		}
	}
	return len;
}

/* send message to a node */
void send_message(uint16_t to_address , const char * message_str)
{
	// if to_address is my address or uassigned address, return
	if(to_address==get_my_address() || to_address==BT_MESH_ADDR_UNASSIGNED )
		return;

	//decklare message buffer of size 20
	NET_BUF_SIMPLE_DEFINE(buf, 3 + MSG_MAX_LEN + 4);
	struct bt_mesh_msg_ctx ctx = {
		.app_idx = APP_IDX,
		.addr = to_address,
		.send_ttl = DEFAULT_TTL,
	};
	bt_mesh_model_msg_init(&buf, OP_VND_MSG);
	net_buf_simple_add_mem(&buf, message_str, MIN(MSG_MAX_LEN, get_first_word(message_str)));
	if(bt_mesh_model_send(&vnd_models[0], &ctx, &buf, NULL, NULL)==0)
		printk("Sending Message %s to 0x%04x\n\n", message_str , to_address);
	else
		return;
}

/* callback function for provisioning complete */
static void prov_complete(uint16_t net_idx, uint16_t assigned_address)
{
	my_address = assigned_address;
	printk("Provisioning Complete \n");
	printk("Assigned Node Address: 0x%04x\n\n", assigned_address);
	show_main();
}

/* declare mesh profile */
static const struct bt_mesh_prov prov = {
	.uuid = my_device_uuid,
	.complete = prov_complete,
};

/* Initialize the Bluetooth Mesh Subsystem */
static void bt_ready(void)
{
	// Initialize the beacon device 
	printk("Initializing Device...\n");
	board_init();
	
	// print device info on serial log
	char device_uuid_str[8+1];
	bin2hex(get_my_device_uuid() , 2 , device_uuid_str, sizeof(device_uuid_str));
	printk("Device UUID:  %s \n\n", device_uuid_str);

	// print sensor info on serial log
	printk("Temperature : %d degree celsius \n", get_temperature());
	printk("Humidity : %d%% \n\n", get_humidity());

	// Intialize Bluetooth Mesh
	printk("Bluetooth initialized\n");
	printk("Mesh initialized\n\n");
	bt_mesh_init(&prov, &comp);
	
	// Load settings 
	if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
		printk("Loading bluetooth stored settings\n");
		settings_load();
	}

	//enable advertising
	bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);
	printk("Advertising enabled\n");
	board_show_text("Advertising enabled\n\n" ,false);
	
	show_main();
}

/* start mesh operation */
void mesh_start(void)
{
	//intitialize variables
	my_address = BT_MESH_ADDR_UNASSIGNED;
	provisioner_address =  BT_MESH_ADDR_UNASSIGNED;
	is_message_recieved_from_prov = false;

	// Initialize Device, Bluetooth and Mesh 
	bt_enable(NULL);
	bt_ready();
}

/* get functions */
const uint8_t * get_my_device_uuid(void)
{
	return my_device_uuid;
}

uint16_t get_my_address(void)
{
	return my_address;
}

uint16_t get_provisioner_address(void)
{
	return provisioner_address;
}

char * get_my_device_name(void)
{
	return my_device_name;
}

char * get_provisioner_device_name(void)
{
	return provisioner_device_name;
}

/* mesh functions */
bool mesh_is_initialized(void)
{
	return elements[0].addr != BT_MESH_ADDR_UNASSIGNED;
}

bool mesh_is_prov_complete(void)
{
	return is_message_recieved_from_prov;;
}

