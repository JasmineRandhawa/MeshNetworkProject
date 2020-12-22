#include <zephyr.h>
#include "mesh.h"

#ifdef CONFIG_NET_MGMT_EVENT
	#include "dhcp_config.h"
#else
	void initialize_dhcp() {
		printk("Network configuration not enabled\n");
	}
#endif

void main(void)
{
	initialize_dhcp();
	mesh_start();
}
