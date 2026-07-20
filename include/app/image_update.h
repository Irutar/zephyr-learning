#ifndef APP_IMAGE_UPDATE_H
#define APP_IMAGE_UPDATE_H

#include <stdint.h>

/*
 * Perform slot synchronisation and optional slot switch.
 *
 * Called once at boot.  Compares slot0 / slot1 image headers:
 *   - Headers differ → self_copy slot0→slot1 (reboots internally).
 *   - Headers match, boot_source == 0 → CRC-verify → RTC switch to slot1.
 *   - Headers match, boot_source == 1 → already on slot1, nothing to do.
 *
 * This function may reboot and never return.
 */
void image_update_perform(uint32_t boot_source);

/*
 * Reliable cold reset via ESP32 RTC_CNTL register.
 *
 * sys_reboot(SYS_REBOOT_COLD) can hang after heavy flash I/O because
 * the flash cache is left in an inconsistent state.  Writing directly
 * to the RTC control register bypasses all software layers.
 */
void image_update_reboot(void);

#endif /* APP_IMAGE_UPDATE_H */
