#include <zephyr.h>
#include <string.h>
#include <sys/printk.h>
#include <stdio.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh.h>
#include <bluetooth/hci.h>
#include <drivers/sensor.h>

#include "mesh.h"
#include "board.h"

#define MOD_LF            0x0000
#define APP_IDX           0x000
#define DEFAULT_TTL       31
#define HELLO_MAX         8
#define OP_HELLO          0xbb
#define OP_VND_HELLO      BT_MESH_MODEL_OP_3(OP_HELLO, BT_COMP_ID_LF)

static uint16_t provisioner_addr, node_addr;
static uint8_t dev_uuid[16] = { 0xdd, 0xee };
bool is_prov_complete = false;
bool is_message_recieved_from_prov = false;

static void send_status_thread(uint16_t provisioner_addr)
{
		while(1)
		{
			//board_show_text("Sending Status \n to Provisioner\n\n ", false, K_SECONDS(5));
			send_hello(provisioner_addr);
			show_main();
			k_sleep(K_SECONDS(8));
		}
}

//call back function to receive messages from other nodes
static void vnd_hello(struct bt_mesh_model *model,
		      struct bt_mesh_msg_ctx *ctx,
		      struct net_buf_simple *buf  ,int8_t rssi)
{
	char displaystr[32];
	printk("Message from provisioner : 0x%04x\n", ctx->addr);
	sprintf(displaystr, "Message \n from Provisioner 0x%04x", ctx->addr);
	board_show_text(displaystr, false, K_SECONDS(8));
	show_main();
	provisioner_addr = ctx->addr;
	is_message_recieved_from_prov = true;
	board_blink_leds();

	char str[32];
	size_t len =  MIN(buf->len, HELLO_MAX);
	memcpy(str, buf->data, len);
	str[len] = '\0';
	send_status_thread(provisioner_addr);
	
}

//define root and vendor models
static struct bt_mesh_cfg_srv cfg_srv = {
	.relay = BT_MESH_RELAY_ENABLED,
	.beacon = BT_MESH_BEACON_DISABLED,
	.frnd = BT_MESH_FRIEND_NOT_SUPPORTED,
	.default_ttl = DEFAULT_TTL,
	.net_transmit = BT_MESH_TRANSMIT(2, 20),
	.relay_retransmit = BT_MESH_TRANSMIT(3, 20),
};

static struct bt_mesh_cfg_cli cfg_cli = {
};

static struct bt_mesh_model root_models[] = {
	BT_MESH_MODEL_CFG_CLI(&cfg_cli),
	BT_MESH_MODEL_CFG_SRV(&cfg_srv),
};

static const struct bt_mesh_model_op vnd_ops[] = {
	{ OP_VND_HELLO, 1, vnd_hello },
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

// send Status to the provisioner
void send_hello(uint16_t prov_addr)
{
	printk("Sending Status to provisioner : 0x%04x\n", prov_addr);
	NET_BUF_SIMPLE_DEFINE(msg, 3 + HELLO_MAX + 4);
	struct bt_mesh_msg_ctx ctx = {
		.app_idx = APP_IDX,
		.addr = prov_addr,
		.send_ttl = DEFAULT_TTL,
	};
	const char *name = bt_get_name();
	bt_mesh_model_msg_init(&msg, OP_VND_HELLO);
	net_buf_simple_add_mem(&msg, name,MIN(HELLO_MAX, first_name_len(name)));
	int err = bt_mesh_model_send(&vnd_models[0], &ctx, &msg, NULL, NULL);
	if(err != 0)
	{
		printk("Sending Status Failed\n");
		board_show_text("Sending Status Failed\n\n ", false, K_SECONDS(5));
		return;
	}
	printk("Success: Status Sent to provisioner : 0x%04x\n", prov_addr);
	//board_show_text("Success :\n Status Sent \n to provisioner\n\n ", false, K_SECONDS(5));
}

//callback function for provisioning complete
static void prov_complete(uint16_t net_idx, uint16_t addr)
{
	node_addr = addr;
	printk("Provisioned Complete\n");
	printk("Node Address: 0x%04x\n",addr);
	is_prov_complete=true;
}

static const struct bt_mesh_prov prov = {
	.uuid = dev_uuid,
	.complete = prov_complete,
};

static int bt_ready(void)
{
	int err;

	/* Initialize the beacon device */
	err = board_init();
	
	/* Intialize Bluetooth Mesh */
	err = bt_mesh_init(&prov, &comp);
	if (err) {
		return err;
	}
	
	/* Load config settings */
	if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
		printk("Loading stored settings\n");
		settings_load();
	}
	
	//print events and update board display
	printk("Initializing Device...\n");
	char device_uuid[8+1];
	bin2hex(get_uuid(), 4, device_uuid, sizeof(device_uuid));
	printk("Device UUID:  %s \n",device_uuid);
	const char *name = bt_get_name();
	printk("Device Name: %s \n", name);
	printk("Bluetooth initialized\n");
	printk("Mesh initialized\n");
	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	if(!is_prov_complete)
	{
		/* Enable Advertising as unprovisioned beacon */
		bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);
		printk("Advertising enabled\n");
		board_show_text("Advertising enabled\n" ,false, K_SECONDS(5) );
	}
	else
		node_addr = elements[0].addr;
	show_main();
	return 0;
}

//start mesh operation
void mesh_start(void)
{
	int err;
	node_addr = BT_MESH_ADDR_UNASSIGNED;
	provisioner_addr =  BT_MESH_ADDR_UNASSIGNED;
	is_prov_complete = false;
	is_message_recieved_from_prov=false;

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(NULL);
	bt_ready();

}

bool mesh_is_initialized(void)
{
	return elements[0].addr != BT_MESH_ADDR_UNASSIGNED;
}

bool mesh_is_prov_complete(void)
{
	return is_message_recieved_from_prov;
}

uint16_t get_my_addr(void)
{
	return node_addr;
}

uint16_t get_prov_addr(void)
{
	return provisioner_addr;
}

const uint8_t * get_uuid(void)
{
	return dev_uuid;
}
