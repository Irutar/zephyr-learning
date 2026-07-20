/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 * Copyright (c) 2020 Arm Limited
 * Copyright (c) 2021-2023 Nordic Semiconductor ASA
 * Copyright (c) 2025 Aerlync Labs Inc.
 * Copyright (c) 2025 Siemens Mobility GmbH
 * Copyright (c) 2026 Espressif Systems (Shanghai) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/devicetree.h>
#include <zephyr/devicetree/partitions.h>

#include <bootloader_init.h>
#include <esp_image_loader.h>

#include "bootutil/bootutil_log.h"
#include "bootutil/bootutil.h"
#include "do_boot.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);

#define IMAGE_INDEX_0   0
#define IMAGE_INDEX_1   1

#define PRIMARY_SLOT    0
#define SECONDARY_SLOT  1

#define IMAGE0_PRIMARY_START_ADDRESS \
	DT_PROP_BY_IDX(DT_NODE_BY_PARTITION_LABEL(image_0), reg, 0)
#define IMAGE0_PRIMARY_SIZE \
	DT_PROP_BY_IDX(DT_NODE_BY_PARTITION_LABEL(image_0), reg, 1)

#define IMAGE1_PRIMARY_START_ADDRESS \
	DT_PROP_BY_IDX(DT_NODE_BY_PARTITION_LABEL(image_1), reg, 0)
#define IMAGE1_PRIMARY_SIZE \
	DT_PROP_BY_IDX(DT_NODE_BY_PARTITION_LABEL(image_1), reg, 1)

/* ---- local modification start: RTC slot selector defines ---- */
#define RTC_SLOT_SEL_ADDR  0x50001FFC
#define RTC_SLOT_SEL_MAGIC 0x424F4F54
/* ---- local modification end ---- */

/*
 * ---- local modification start: do_boot() rework ----
 *
 * Original logic:
 *   int slot = (rsp->br_image_off == IMAGE0_PRIMARY_START_ADDRESS) ?
 *                      PRIMARY_SLOT : SECONDARY_SLOT;
 *   start_cpu0_image(IMAGE_INDEX_0, slot, rsp->br_hdr->ih_hdr_size);
 *
 * Replaced with:
 *   1. Read RTC slow memory at 0x50001FFC.
 *   2. If magic == 0x424F4F54 → boot slot1 (single-shot, magic cleared).
 *   3. Otherwise → always boot slot0 (never consult br_image_off —
 *      the standard swap/upgrade path corrupts THREADPTR on ESP32).
 *   4. Clear THREADPTR before chain-load to guard against residual
 *      garbage from flash erase/write operations.
 */
void do_boot(const struct boot_rsp *rsp)
{
	uint32_t slot_sel;
	int image_index;
	int slot;

	BOOT_LOG_INF("br_image_off = 0x%x", rsp->br_image_off);
	BOOT_LOG_INF("ih_hdr_size = 0x%x", rsp->br_hdr->ih_hdr_size);

	slot_sel = *(volatile uint32_t *)RTC_SLOT_SEL_ADDR;

	if (RTC_SLOT_SEL_MAGIC == slot_sel) {
		BOOT_LOG_INF("RTC slot select: booting slot1");
		image_index = IMAGE_INDEX_0;
		slot = SECONDARY_SLOT;
		*(volatile uint32_t *)RTC_SLOT_SEL_ADDR = 0;
	} else {
		image_index = IMAGE_INDEX_0;
		slot = PRIMARY_SLOT;
	}

	__asm__ __volatile__("movi a2, 0\n\t"
			     "wur.THREADPTR a2\n\t"
			     : : : "a2");

	start_cpu0_image(image_index, slot, rsp->br_hdr->ih_hdr_size);
}
/* ---- local modification end ---- */
