#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/reboot.h>
#include <string.h>

#define DUAL_LOG_TAG "image_update"
#include <app/wifi_log.h>
#include <app/slot_selector.h>
#include <app/self_copy.h>

LOG_MODULE_REGISTER(image_update, LOG_LEVEL_INF);

#define TRAILER_SECTOR_SIZE    4096
#define VERIFY_BUFFER_SIZE     256

static uint8_t verify_buffer[VERIFY_BUFFER_SIZE] __aligned(4);

static int slots_crc32_compare(const struct flash_area *slot0,
			       const struct flash_area *slot1)
{
	uint32_t data_size;
	uint32_t crc0;
	uint32_t crc1;
	uint32_t remaining;
	uint32_t offset;
	size_t chunk;

	data_size = slot0->fa_size - TRAILER_SECTOR_SIZE;
	crc0 = 0;
	crc1 = 0;
	remaining = data_size;
	offset = 0;

	log_dual_inf("Verifying slot integrity (CRC32 of %u bytes)...",
		     data_size);

	while (remaining > 0)
	{
		chunk = MIN(remaining, sizeof(verify_buffer));

		flash_area_read(slot0, offset, verify_buffer, chunk);
		crc0 = crc32_ieee_update(crc0, verify_buffer, chunk);

		flash_area_read(slot1, offset, verify_buffer, chunk);
		crc1 = crc32_ieee_update(crc1, verify_buffer, chunk);

		remaining -= chunk;
		offset += chunk;
	}

	log_dual_inf("CRC32  slot0: 0x%08x  slot1: 0x%08x", crc0, crc1);

	if (crc0 != crc1)
	{
		log_dual_err("CRC mismatch — slots are not identical");
		return -EIO;
	}

	log_dual_inf("CRC match — slots are identical");
	return 0;
}

/*
 * Direct hardware reset via RTC_CNTL.
 *
 * sys_reboot(SYS_REBOOT_COLD) sometimes fails after heavy flash I/O
 * on ESP32 because the flash cache may be in an inconsistent state.
 * Writing to the RTC control register triggers a full chip reset at
 * the hardware level, independent of CPU / cache state.
 */
void image_update_reboot(void)
{
	k_sleep(K_MSEC(500));

	__asm__ __volatile__("memw");

	/*
	 * RTC_CNTL_OPTIONS0_REG (0x3FF48000), bit 31 = SW_SYS_RST.
	 * On ESP32 this triggers the same reset sequence as a power-on
	 * or external RST pin.
	 */
	*(volatile uint32_t *)0x3FF48000 = 0x80000000;

	/* Paranoia — spin forever if the reset doesn't take. */
	while (1)
	{
		__asm__ __volatile__("waiti 0");
	}
}

void image_update_perform(uint32_t boot_source)
{
	const struct flash_area *slot0_area;
	const struct flash_area *slot1_area;
	uint32_t header0[8];
	uint32_t header1[8];
	int error;

	log_dual_inf("Checking slot consistency...");

	error = flash_area_open(PARTITION_ID(slot0_partition), &slot0_area);

	if (0 != error)
	{
		log_dual_err("Cannot open slot0: %d", error);
		return;
	}

	error = flash_area_open(PARTITION_ID(slot1_partition), &slot1_area);

	if (0 != error)
	{
		log_dual_err("Cannot open slot1: %d", error);
		flash_area_close(slot0_area);
		return;
	}

	flash_area_read(slot0_area, 0, header0, sizeof(header0));
	flash_area_read(slot1_area, 0, header1, sizeof(header1));

	if (0 != memcmp(header0, header1, sizeof(header0)))
	
	{
		flash_area_close(slot0_area);
		flash_area_close(slot1_area);
		log_dual_inf("Slot headers differ — starting synchronisation");
		self_copy_sync_slots();
		return;
	}

	if (0 == boot_source)
	{
		error = slots_crc32_compare(slot0_area, slot1_area);
		flash_area_close(slot0_area);
		flash_area_close(slot1_area);

		if (0 != error)
		{
			log_dual_err("CRC mismatch — forcing re-copy");
			self_copy_sync_slots();
			return;
		}

		log_dual_inf("Slots in sync, booted from slot0 — switching to slot1");
		slot_selector_boot_slot1_and_reboot();
		return;
	}

	flash_area_close(slot0_area);
	flash_area_close(slot1_area);
	log_dual_inf("Running from slot1 — all good");
}
