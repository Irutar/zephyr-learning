#ifndef APP_SELF_COPY_H
#define APP_SELF_COPY_H

/*
 * Erase slot1, copy slot0's image into it (excluding the MCUboot trailer
 * sector), then reboot.  Returns only on unrecoverable flash error.
 */
int self_copy_sync_slots(void);

#endif /* APP_SELF_COPY_H */
