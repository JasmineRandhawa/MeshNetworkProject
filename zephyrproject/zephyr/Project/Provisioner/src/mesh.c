#include <zephyr.h>
#include <device.h>
#include <sys/printk.h>
#include <display/cfb.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh.h>

#include "mesh.h"
#include "board.h"

/* Provisoner static hardcoded information */
static const uint8_t my_device_uuid[16] = { 0xbb, 0xaa };
char * my_address = "0X0006";
char * my_device_name = "P";
static uint16_t  self_address = 0X0006;

/* declare other static variables */
static uint16_t node_address;
static uint8_t node_uuid[16];
static uint8_t device_key[16];
static  uint8_t network_key[16] ;
static  uint8_t application_key[16];

static uint32_t record_count = 1;

static struct stat {
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

K_SEM_DEFINE(sem_unprov_beacon, 0, 5);
K_SEM_DEFINE(sem_node_added, 0, 5);

static struct bt_mesh_cfg_cli cfg_cli = {

};

/* print node_info list info on the board */
void show_node_status()
{
	char str[100];
	int len, line = 0;
	const struct device *epd_dev;
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

	uint16_t from_address = ctx->addr;

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
		char msg_str[50];
		snprintf(msg_str, 50, "%s", message_str);
		char *address_name_token;
		char address_str[7];
		address_name_token = strtok(msg_str, "=");
		address_name_token = strtok(NULL, "=");
		snprintf(address_str,7,"0X000%s",address_name_token);
		uint16_t address_hex = (uint16_t)strtol(address_str, NULL, 0);
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

	//if message is of type "Neighbours Neighbours Update Info Message" with message code as U, 
	// update received info to corresponding neighbor neighbour record in node_info list
	if(strstr(message_str,"U")!=NULL)
	{
		char msg_str[50];
		snprintf(msg_str, 50, "%s", message_str);
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

/* configure self i.e. provisioner */
static void configure_self(struct bt_mesh_cdb_node *self)
{
	printk("Self-Configuring Started\n");

	//create random application key
	bt_rand(application_key, 16);

	char application_key_str[32 + 1];
	bin2hex(application_key, 16, application_key_str, sizeof(application_key_str));
	printk("Application key %s\n\n", application_key_str);

	// Map self to Application Key
	bt_mesh_cfg_app_key_add(NET_IDX, self->addr, NET_IDX, APP_IDX,application_key, NULL);

	// Bind self to vendor model
	bt_mesh_cfg_mod_app_bind_vnd(NET_IDX, self->addr, self->addr,APP_IDX, MOD_LF, BT_COMP_ID_LF, NULL);

	atomic_set_bit(self->flags, BT_MESH_CDB_NODE_CONFIGURED);

	// add self to the CDB
	if (IS_ENABLED(CONFIG_BT_SETTINGS)) 
		bt_mesh_cdb_node_store(self);
	
	self_address = self->addr;

	printk("Added self  -- Address : 0x%04x , Name: %s \n",self_address , get_my_device_name());
	printk("Self-Configuration complete\n\n");
}

/* configure beacon node */
static void configure_node(struct bt_mesh_cdb_node *node)
{
	printk("Node Configuring Started \n");

	// Map self to Application Key
	bt_mesh_cfg_app_key_add(NET_IDX, node->addr, NET_IDX, APP_IDX,application_key, NULL);

	// Bind self to vendor model
	bt_mesh_cfg_mod_app_bind_vnd(NET_IDX, node->addr, node->addr,APP_IDX, MOD_LF, BT_COMP_ID_LF, NULL);

	atomic_set_bit(node->flags, BT_MESH_CDB_NODE_CONFIGURED);

	// add self to the CDB
	if (IS_ENABLED(CONFIG_BT_SETTINGS)) 
		bt_mesh_cdb_node_store(node);

	//send provisioning message (with message code "Q") to beacon node
	char prov_name[2];
	snprintf(prov_name, 2, "%s",get_my_device_name());
	char node_name[4];
	snprintf(node_name, 4, "N%d", record_count);
	char provisioning_message[12];
	snprintf(provisioning_message, 12, "Q=%s=%s", prov_name,node_name);
	send_message(node->addr,provisioning_message);

	//add node to node_info list
	struct stat *stat = &node_info[get_node_info_list_record_count()];
	stat->addr= get_my_address();
	stat->humidity = get_humidity();
	stat->temperature = get_temperature();
	snprintf(stat->name, 2, "%s",get_my_device_name());
	snprintf(stat->neighbour_name, 4, "%s",node_name);
	stat->neighbour_addr = node->addr;
	record_count++;

	printk("Configured node 0x%04x \n",node->addr);
	printk("Node Configuration completed\n\n");
}

/* call back function for Provisioner after detecting the beacon node and reads its UUID */
static void unprovisioned_beacon(uint8_t uuid[16],
				 bt_mesh_prov_oob_info_t oob_info,
				 uint32_t *uri_hash)
{
	memcpy(node_uuid, uuid, 16);
	k_sem_give(&sem_unprov_beacon);
}

/* call back function for Provisioner after provisioning beacon node and storing the provisioned address of node */
static void node_added(uint16_t net_idx, uint8_t uuid[16], uint16_t node_addr,
		       uint8_t num_elem)
{
	node_address = node_addr;
	k_sem_give(&sem_node_added);
}

/* declare mesh profile */
static const struct bt_mesh_prov prov = {
	.uuid = my_device_uuid,
	.unprovisioned_beacon = unprovisioned_beacon,
	.node_added = node_added,
};

/* Initialize the Bluetooth Mesh Subsystem */
static void bt_ready(void)
{
	/* Initialize the device */
	board_init();

	printk("Device Initialized\n");
	board_show_text("Device Initialized\n",true);

	// print device info on serial log
	char device_uuid[8 + 1];
	bin2hex(get_my_device_uuid(), 4, device_uuid, sizeof(device_uuid));
	printk("Device UUID:  %s \n", device_uuid);
	printk("Device Name: %s \n", get_my_device_name());

	// print sensor info on serial log
	printk("Temperature : %d degree Celcius \n", get_temperature());
	printk("Humidity : %d%% \n\n", get_humidity());

	/* Intialize Bluetooth Mesh */
	printk("Bluetooth Initialized\n");
	printk("Mesh Initialized\n\n");
	bt_mesh_init(&prov, &comp);

	/* Load config settings */
	if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
		printk("Loading stored settings\n\n");
		settings_load();
	}

	int err;

	//create random network key and CDB 
	bt_rand(network_key, 16);
	err = bt_mesh_cdb_create(network_key);
	if (err == -EALREADY) {
		printk("Using stored CDB\n");
	} else if (err) {
		printk("Failed to create CDB (err %d)\n", err);
		return ;
	} else {
		printk("Created CDB\n");
		board_show_text("Created CDB",true);
	}

	char network_key_str[32 + 1];
	bin2hex(network_key, 16, network_key_str, sizeof(network_key_str));
	printk("Network key %s\n", network_key_str);

	//create device key
	uint8_t device_key_str[32 + 1];
	bt_rand(device_key, 16);
	bin2hex(device_key, 16, device_key_str, sizeof(device_key_str));
	printk("Device key %s\n", device_key_str);

	show_main();

	//provision self
	printk("Self Provisioning started\n");
	board_show_text("Self-Provisioning started",true);
	err = bt_mesh_provision(network_key, NET_IDX, FLAGS, IV_INDEX, self_address,device_key);
	if (err == -EALREADY) {
		printk("Already provisioned \n");
		printk("Using stored settings\n");
		printk("Self Provisioned Address  0x%04x\n\n", self_address);
	} else if (err) {
		printk("Provisioning failed (err %d)\n", err);
		return ;
	} else {
		printk("Self Provisioned Address  0x%04x\n", self_address);
		printk("Self Provisioning completed\n\n");
	}

	show_main();
}

/* check is provisioned node is configured or not */
static uint8_t check_unconfigured(struct bt_mesh_cdb_node *node, void *data)
{
	//If not configured , invoke configure function
	if (!atomic_test_bit(node->flags, BT_MESH_CDB_NODE_CONFIGURED))
	 {
		if (node->addr == self_address) 
			configure_self(node);
		else
			configure_node(node);
	}
	// if node is already configured send neighbours info message (with message code "R")  to all neighbours
	else
	{
		//send message to node when node is not same as me
		if(node->addr!=get_my_address() && node->addr!=BT_MESH_ADDR_UNASSIGNED)
		{
			for(int i =0 ;i< get_node_info_list_record_count() ; i++)
			{
				struct stat * stat = &node_info[i];
				if(stat->addr ==  get_my_address())
				{
					if(stat->neighbour_addr != node->addr && stat->neighbour_addr!=BT_MESH_ADDR_UNASSIGNED)
					{
						char neigh_info_message[12] ;
						char neigh_addr_string[8];
						snprintf(neigh_addr_string,8,"0x%04x",stat->neighbour_addr);
						char last_char_of_address_str = neigh_addr_string[strlen(neigh_addr_string) -1];

						snprintf(neigh_info_message,12,"R=%c=%s",last_char_of_address_str,stat->neighbour_name);
		;				send_message(node->addr,neigh_info_message);
					}	
				}
			}	
		}
	}

	return BT_MESH_CDB_ITER_CONTINUE;
}

/* start mesh operation */
void mesh_start(void)
{
	// Initialize Device, Bluetooth and Mesh 
	bt_enable(NULL);
	bt_ready();

	int err;
	// scan and provision/configure/add nodes
	while (1) {
		k_sem_reset(&sem_unprov_beacon);
		k_sem_reset(&sem_node_added);
		bt_mesh_cdb_node_foreach(check_unconfigured, NULL);

		// scan for beacon nodes
		printk("Waiting for unprovisioned beacon node...\n");
		err = k_sem_take(&sem_unprov_beacon, K_SECONDS(5));
		if (err == -EAGAIN) {
			continue;
		}

		// node detected
		char node_uuid_str[8 + 1];
		bin2hex(node_uuid, 2, node_uuid_str, sizeof(node_uuid_str));
		printk("Unprovisioned Beacon Node Detected with UUID : %s\n", node_uuid_str);

		// start provisioning node with next sequential address
		printk("Node Provisioning started \n");
		char node_address_str[7] , record_count_str[2];
		snprintf(record_count_str,2,"%d", record_count);
		snprintf(node_address_str , 7 ,"0X000%s", record_count_str);
		uint16_t node_addr = (uint16_t)strtol(node_address_str, NULL, 0);

		err = bt_mesh_provision_adv(node_uuid, NET_IDX, node_addr, 0);
		if (err < 0) {
			continue;
		}

		err = k_sem_take(&sem_node_added, K_SECONDS(10));
		if (err == -EAGAIN) {
			continue;
		}
		printk("Node Provisioned Address  0x%04x\n", node_address);
		printk("Node Provisioning Completed %s\n\n", node_uuid_str);
	}
}

/* get functions */
const uint8_t * get_my_device_uuid(void)
{
	return my_device_uuid;
}

uint16_t get_my_address(void)
{
	return self_address;
}

char * get_my_device_name(void)
{
	return my_device_name;
}

int get_node_info_list_record_count(void)
{
	return record_count - 1 ;
}

const uint8_t * get_network_key(void)
{
	return network_key;
}

const uint8_t * get_application_key(void)
{
	return application_key;
}

/* mesh functions */
bool mesh_is_initialized(void)
{
	return elements[0].addr != BT_MESH_ADDR_UNASSIGNED;
}
