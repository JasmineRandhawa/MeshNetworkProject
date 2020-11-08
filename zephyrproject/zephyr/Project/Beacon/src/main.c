/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>

#include <string.h>

#include <settings/settings.h>

#include "mesh.h"

void main(void)
{
	mesh_start();
}
