/*
 * Boot slot selector using ESP32 RTC slow memory.
 *
 * RTC memory survives warm reboots.  Two words are reserved near the
 * end of RTC slow memory (8 KB region at 0x50000000):
 *
 *   0x50001FFC — magic word (0x424F4F54 = "BOOT").
 *                Written by the app before reboot, read by MCUboot
 *                (arch/esp32.c do_boot).  MCUboot clears it after
 *                use (single-shot).
 *   0x50001FF8 — boot source info.
 *                0 = slot0, 1 = slot1.  Written before reboot,
 *                read on next boot to display the active slot.
 */
#ifndef APP_SLOT_SELECTOR_H
#define APP_SLOT_SELECTOR_H

#define RTC_SLOT_SELECTOR_ADDR   0x50001FFC
#define RTC_SLOT_SELECTOR_MAGIC  0x424F4F54
#define RTC_BOOT_SOURCE_ADDR     0x50001FF8

uint32_t slot_selector_magic_read(void);
void     slot_selector_magic_write(uint32_t value);
uint32_t slot_selector_boot_source_read(void);
void     slot_selector_boot_source_write(uint32_t slot_number);

void slot_selector_boot_slot1_and_reboot(void);

#endif /* APP_SLOT_SELECTOR_H */
