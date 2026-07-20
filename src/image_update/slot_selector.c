#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#define DUAL_LOG_TAG "slot_selector"
#include <app/wifi_log.h>
#include <app/slot_selector.h>
#include <app/image_update.h>

LOG_MODULE_REGISTER(slot_selector, LOG_LEVEL_INF);

uint32_t slot_selector_magic_read(void)
{
	return *(volatile uint32_t *)RTC_SLOT_SELECTOR_ADDR;
}

void slot_selector_magic_write(uint32_t value)
{
	*(volatile uint32_t *)RTC_SLOT_SELECTOR_ADDR = value;
}

uint32_t slot_selector_boot_source_read(void)
{
	return *(volatile uint32_t *)RTC_BOOT_SOURCE_ADDR;
}

void slot_selector_boot_source_write(uint32_t slot_number)
{
	*(volatile uint32_t *)RTC_BOOT_SOURCE_ADDR = slot_number;
}

void slot_selector_boot_slot1_and_reboot(void)
{
	log_dual_inf("Requesting boot from slot1 on next reset");

	slot_selector_boot_source_write(1);
	slot_selector_magic_write(RTC_SLOT_SELECTOR_MAGIC);

	image_update_reboot();
}
