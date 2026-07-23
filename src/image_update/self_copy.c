#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/crc.h>

#define DUAL_LOG_TAG "self_copy"
#include <app/wifi_log.h>
#include <app/self_copy.h>
#include <app/image_update.h>

LOG_MODULE_REGISTER(self_copy, LOG_LEVEL_INF);

#define COPY_BUFFER_SIZE         4096
#define TRAILER_SECTOR_SIZE      4096

static uint8_t copy_buffer[COPY_BUFFER_SIZE] __aligned(4);

/*
 * The MCUboot trailer (swap state: magic, image_ok, copy_done) lives in
 * the last sector of every slot.  A byte-for-byte copy would carry slot0's
 * trailer into slot1, making MCUboot see a pending upgrade on the next
 * boot.  The ESP32 THREADPTR register gets corrupted during MCUboot's
 * flash erase/write operations, so any upgrade attempt causes EXCCAUSE 29
 * (store prohibited).
 *
 * We skip the trailer sector entirely and explicitly erase it on slot1 —
 * MCUboot interprets all-0xFF as "no swap state".
 *
 * After copying, a CRC32 verification pass compares slot0 and slot1
 * byte-for-byte (excluding the trailer).  If the CRCs differ the copy is
 * retried once; on persistent mismatch the device halts rather than
 * risking a boot from a corrupted slot.
 */
static int verify_copy(const struct flash_area *source,
		       const struct flash_area *target,
		       uint32_t data_size)
{
	uint32_t source_crc;
	uint32_t target_crc;
	uint32_t remaining;
	uint32_t offset;
	size_t chunk;
	int error;

	source_crc = 0;
	target_crc = 0;

	log_dual_inf("Verifying copy — computing CRC32 of both slots...");

	remaining = data_size;
	offset = 0;
	
	while (remaining > 0)
	{
		chunk = MIN(remaining, sizeof(copy_buffer));

		error = flash_area_read(source, offset, copy_buffer, chunk);

		if (0 != error)
		{
			log_dual_err("Verify read slot0 failed at 0x%x: %d",
				     offset, error);
			return error;
		}
		source_crc = crc32_ieee_update(source_crc, copy_buffer, chunk);

		error = flash_area_read(target, offset, copy_buffer, chunk);

		if (0 != error)
		{
			log_dual_err("Verify read slot1 failed at 0x%x: %d",
				     offset, error);
			return error;
		}
		target_crc = crc32_ieee_update(target_crc, copy_buffer, chunk);

		remaining -= chunk;
		offset += chunk;
	}

	log_dual_inf("CRC32  slot0: 0x%08x  slot1: 0x%08x", source_crc, target_crc);

	if (source_crc != target_crc)
	{
		log_dual_err("CRC32 mismatch — copy is corrupted");
		return -EIO;
	}

	log_dual_inf("CRC32 match — copy verified");
	return 0;
}

int self_copy_sync_slots(void)
{
	const struct flash_area *source_area;
	const struct flash_area *target_area;
	uint32_t copy_size;
	uint32_t remaining;
	uint32_t offset;
	size_t chunk;
	int error;
	int attempt;

	log_dual_inf("Opening slot0 (image-0) for reading...");
	error = flash_area_open(PARTITION_ID(slot0_partition), &source_area);

	if (0 != error)
	{
		log_dual_err("Cannot open slot0: %d", error);
		return error;
	}

	log_dual_inf("Opening slot1 (image-1) for writing...");
	error = flash_area_open(PARTITION_ID(slot1_partition), &target_area);

	if (0 != error)
	{
		log_dual_err("Cannot open slot1: %d", error);
		flash_area_close(source_area);
		return error;
	}

	if (source_area->fa_size > target_area->fa_size)
	{
		log_dual_err("Source (%u bytes) larger than target (%u bytes) — aborting",
			source_area->fa_size, target_area->fa_size);
		flash_area_close(source_area);
		flash_area_close(target_area);
		return -E2BIG;
	}

	copy_size = source_area->fa_size - TRAILER_SECTOR_SIZE;

	for (attempt = 1; attempt <= 2; attempt++)
	{
		log_dual_inf("Erasing slot1 (%u bytes, offset 0x%lx)...",
			target_area->fa_size, (unsigned long)target_area->fa_off);
		error = flash_area_erase(target_area, 0, target_area->fa_size);
		if (0 != error)
		{
			log_dual_err("Erase failed: %d", error);
			goto out;
		}

		log_dual_inf("Copying slot0 → slot1 (%u bytes, skipping %u-byte trailer)...",
			copy_size, TRAILER_SECTOR_SIZE);
		remaining = copy_size;
		offset = 0;

		while (remaining > 0)
		{
			chunk = MIN(remaining, sizeof(copy_buffer));

			error = flash_area_read(source_area, offset, copy_buffer, chunk);

			if (0 != error)
			{
				log_dual_err("Read failed at offset 0x%x: %d", offset, error);
				goto out;
			}

			error = flash_area_write(target_area, offset, copy_buffer, chunk);

			if (0 != error)
			{
				log_dual_err("Write failed at offset 0x%x: %d", offset, error);
				goto out;
			}

			remaining -= chunk;
			offset += chunk;
		}

		error = verify_copy(source_area, target_area, copy_size);

		if (0 == error)
		{
			break;
		}

		if (2 == attempt)
		{
			log_dual_err("Copy verification failed after retry — halting");
			goto out;
		}

		log_dual_wrn("Retrying copy (attempt %d)...", attempt + 1);
	}

	log_dual_inf("Erasing slot1 trailer sector (%u bytes at offset 0x%lx)...",
		TRAILER_SECTOR_SIZE,
		(unsigned long)(target_area->fa_off + copy_size));

	error = flash_area_erase(target_area, copy_size, TRAILER_SECTOR_SIZE);

	if (0 != error)
	{
		log_dual_err("Trailer erase failed: %d", error);
		goto out;
	}

	log_dual_inf("Slot1 synchronized (trailer clean, CRC verified)");
	log_dual_inf("Rebooting after sync...");
	image_update_reboot();

out:
	flash_area_close(source_area);
	flash_area_close(target_area);
	return error;
}
