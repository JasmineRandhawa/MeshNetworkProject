/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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

#define MOD_LF 0x0000
#define OP_HELLO 0xbb
#define OP_VND_HELLO BT_MESH_MODEL_OP_3(OP_HELLO, BT_COMP_ID_LF)

#define IV_INDEX 0
#define DEFAULT_TTL 31
#define NET_IDX 0x000
#define APP_IDX 0x000
#define FLAGS 0
#define HELLO_MAX 8

static const uint8_t dev_uuid[16] = { 0xdd, 0xdd };
static uint16_t node_addr, self_addr;
static uint8_t node_uuid[16];
static uint8_t device_key[16];

static const uint8_t net_key[16] = {
	0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
	0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
};
static const uint8_t app_key[16] = {
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
};

K_SEM_DEFINE(sem_unprov_beacon, 0, 5);
K_SEM_DEFINE(sem_node_added, 0, 5);

static struct bt_mesh_cfg_srv cfg_srv = {
	.relay = BT_MESH_RELAY_ENABLED,
	.beacon = BT_MESH_BEACON_DISABLED,
	.frnd = BT_MESH_FRIEND_NOT_SUPPORTED,
	.default_ttl = DEFAULT_TTL,

	/* 3 transmissions with 20ms interval */
	.net_transmit = BT_MESH_TRANSMIT(2, 20),
	.relay_retransmit = BT_MESH_TRANSMIT(3, 20),
};

static struct bt_mesh_cfg_cli cfg_cli = {

};

static void vnd_hello(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx,
		      struct net_buf_simple *buf, int8_t rssi)
{
	char str[32];

	char displaystr[32];
	size_t len;

	printk("Hello message from 0x%04x\n\n", ctx->addr);

	if (ctx->addr == bt_mesh_model_elem(model)->addr) {
		return;
	}

	len = MIN(buf->len, HELLO_MAX);
	memcpy(str, buf->data, len);
	str[len] = '\0';

	board_add_hello(ctx->addr, str);
	sprintf(displaystr, " says hi! %d", rssi);
	strcat(str,displaystr);
	board_show_text(str, false, K_SECONDS(5));

	board_blink_leds();
	show_main();
}

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


void send_hello(uint16_t addr)
{
	NET_BUF_SIMPLE_DEFINE(msg, 3 + HELLO_MAX + 4);
	struct bt_mesh_msg_ctx ctx = {
		.app_idx = APP_IDX,
		.addr = addr,
		.send_ttl = DEFAULT_TTL,
	};

	char app_key_str[32 + 1];
	bin2hex(app_key, 16, app_key_str, sizeof(app_key_str));

	const char *name = app_key_str;

	bt_mesh_model_msg_init(&msg, OP_VND_HELLO);
	net_buf_simple_add_mem(&msg, name, MIN(HELLO_MAX, first_name_len(name)));

	if (bt_mesh_model_send(&vnd_models[0], &ctx, &msg, NULL, NULL) == 0) {
		board_show_text("Saying \"hi!\" to node\n\n", false,
				K_SECONDS(5));
	} else {
		board_show_text("Sending Failed!\n", false, K_SECONDS(2));
	}
	show_main();
}

//set up configureation database (CDB): mapp CDB to the application key
static void setup_cdb(void)
{
	struct bt_mesh_cdb_app_key *key;

	key = bt_mesh_cdb_app_key_alloc(NET_IDX, APP_IDX);
	if (key == NULL) {
		printk("Failed to allocate app-key 0x%04x\n", APP_IDX);
		return;
	}

	memcpy(key->keys[0].app_key, app_key, 16);

	char app_key_str[32 + 1];
	bin2hex(app_key, 16, app_key_str, sizeof(app_key_str));
	printk("Application key %s\n\n", app_key_str);

	if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
		bt_mesh_cdb_app_key_store(key);
	}
}

//Provisioner configures itself
static void configure_self(struct bt_mesh_cdb_node *self)
{
	int err;

	printk("Self-Configuring Started\n");

	/* Add Application Key */
	err = bt_mesh_cfg_app_key_add(NET_IDX, self->addr, NET_IDX, APP_IDX,app_key, NULL);
	if (err < 0) {
		return;
	}

	err = bt_mesh_cfg_mod_app_bind_vnd(NET_IDX, self->addr, self->addr,APP_IDX, MOD_LF, BT_COMP_ID_LF, NULL);
	if (err < 0) {
		return;
	}

	atomic_set_bit(self->flags, BT_MESH_CDB_NODE_CONFIGURED);

	if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
		bt_mesh_cdb_node_store(self);
	}
	printk("Self-Configuration complete\n\n");
	self_addr = self->addr;
	show_main();
}

//Provisioner configures beacon nodes
static void configure_node(struct bt_mesh_cdb_node *node)
{
	int err;

	printk("Node Configuring Started \n");

	/* Add Application Key */
	err = bt_mesh_cfg_app_key_add(NET_IDX, node->addr, NET_IDX, APP_IDX, app_key, NULL);
	if (err < 0) {
		return;
	}
	/* Bind to Health model */
	err = bt_mesh_cfg_mod_app_bind_vnd(NET_IDX, node->addr, node->addr,APP_IDX, MOD_LF, BT_COMP_ID_LF,NULL);
	if (err < 0) {
		return;
	}
	atomic_set_bit(node->flags, BT_MESH_CDB_NODE_CONFIGURED);

	if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
		bt_mesh_cdb_node_store(node);
	}
	printk("Node Configuration completed\n\n");
	send_hello(node->addr);
	k_sleep(K_SECONDS(5));
	show_main();
	k_sleep(K_SECONDS(5));
}

//call back function for Provisioner after detecting the beacon node and reads its UUID
static void unprovisioned_beacon(uint8_t uuid[16],
				 bt_mesh_prov_oob_info_t oob_info,
				 uint32_t *uri_hash)
{
	memcpy(node_uuid, uuid, 16);
	k_sem_give(&sem_unprov_beacon);
}

//call back function for Provisioner after provisioning beacon node and storing the provisioned address of node
static void node_added(uint16_t net_idx, uint8_t uuid[16], uint16_t addr,
		       uint8_t num_elem)
{
	node_addr = addr;
	k_sem_give(&sem_node_added);
}

static const struct bt_mesh_prov prov = {
	.uuid = dev_uuid,
	.unprovisioned_beacon = unprovisioned_beacon,
	.node_added = node_added,
};

static int bt_ready(void)
{
	int err;

	/* Initialize the provisioner device */
	err = board_init();

	/* Intialize Bluetooth Mesh */
	err = bt_mesh_init(&prov, &comp);
	if (err) {
		return err;
	}

	printk("Initializing Device...\n");

	/* Load config settings */
	if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
		printk("Loading stored settings\n\n");
		settings_load();
	}

	//print events and update board display
	
	char device_uuid[8 + 1];
	bin2hex(get_uuid(), 4, device_uuid, sizeof(device_uuid));
	printk("Device UUID:  %s \n", device_uuid);
	const char *name = bt_get_name();
	printk("Device Name: %s \n\n", name);
	printk("Bluetooth initialized\n");
	printk("Mesh initialized\n\n");
	show_main();

	//create CDB and Mesh network
	err = bt_mesh_cdb_create(net_key);
	if (err == -EALREADY) {
		printk("Using stored CDB\n");
	} else if (err) {
		printk("Failed to create CDB (err %d)\n", err);
		return err;
	} else {
		printk("Created CDB\n");
		char net_key_str[32 + 1];
		bin2hex(net_key, 16, net_key_str, sizeof(net_key_str));
		printk("Network key %s\n", net_key_str);
		setup_cdb();
	}

	//provision self
	uint8_t dev_key[16];
	bt_rand(dev_key, 16);
	memcpy(device_key, dev_key, 16);
	printk("Self Provisioning started\n");
	uint16_t addr;
	do {
		err = bt_rand(&addr, sizeof(addr));
		if (err) {
			continue;
		}
	} while (!addr);

	/* Make sure it's a unicast address (highest bit unset) */
	addr &= ~0x8000;
	self_addr = addr;
	err = bt_mesh_provision(net_key, NET_IDX, FLAGS, IV_INDEX, addr,dev_key);

	if (err == -EALREADY) {
		printk("Using stored settings\n");
	} else if (err) {
		printk("Provisioning failed (err %d)\n", err);
		return err;
	} else {
		char dev_key_str[32 + 1];
		bin2hex(dev_key, 16, dev_key_str, sizeof(dev_key_str));
		printk("Device key %s\n", dev_key_str);
		printk("Self Provisioned Address  0x%04x\n", self_addr);
		printk("Self Provisioning completed\n\n");
	}

	return 0;
}

static uint8_t check_unconfigured(struct bt_mesh_cdb_node *node, void *data)
{
	if (!atomic_test_bit(node->flags, BT_MESH_CDB_NODE_CONFIGURED)) {
		if (node->addr == self_addr) {
			configure_self(node);
		} else {
			configure_node(node);
		}
	}

	return BT_MESH_CDB_ITER_CONTINUE;
}

void mesh_start(void)
{
	int err;

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(NULL);
	bt_ready();

	//provision and confiure nodes
	while (1) {
		k_sem_reset(&sem_unprov_beacon);
		k_sem_reset(&sem_node_added);
		bt_mesh_cdb_node_foreach(check_unconfigured, NULL);

		printk("Waiting for unprovisioned node...\n\n");
		err = k_sem_take(&sem_unprov_beacon, K_SECONDS(15));
		if (err == -EAGAIN) {
			continue;
		}
		char uuid_hex_str[32 + 1];
		bin2hex(node_uuid, 16, uuid_hex_str, sizeof(uuid_hex_str));
		printk("Unprovioned Node Detcted with UUID : %s\n",
		       uuid_hex_str);
		printk("Node Provisioning started \n");
		uint16_t addr;
		int err;
		do {
			err = bt_rand(&addr, sizeof(addr));
			if (err) {
				continue;
			}
		} while (!addr);

		/* Make sure it's a unicast address (highest bit unset) */
		addr &= ~0x8000;
		err = bt_mesh_provision_adv(node_uuid, NET_IDX, addr, 0);
		if (err < 0) {
			continue;
		}
		err = k_sem_take(&sem_node_added, K_SECONDS(10));
		if (err == -EAGAIN) {
			continue;
		}
		printk("Node Provisioned Address  0x%04x\n", node_addr);
		printk("Node Provisioning Completed %s\n\n", uuid_hex_str);
	}
}

const uint8_t *get_uuid(void)
{
	return dev_uuid;
}

bool mesh_is_initialized(void)
{
	return self_addr != BT_MESH_ADDR_UNASSIGNED;
}


uint16_t get_my_addr(void)
{
	return self_addr;
}

const uint8_t *get_net_key(void)
{
	return net_key;
}

uint8_t *get_dev_key(void)
{
	return device_key;
}

const uint8_t *get_app_key(void)
{
	return app_key;
}
