#ifndef DITOO_BLUETOOTH_ADVERTISING_H
#define DITOO_BLUETOOTH_ADVERTISING_H
#include <stdint.h>
struct ble_advertisement {
    uint16_t interval_ms,duration_ms;
    uint8_t type,size,scan_size;
    unsigned char data[31],scan[31];
};
unsigned runtime_advertising_valid(const struct ble_advertisement *);
unsigned runtime_advertising_busy(void);
const char *runtime_advertising_start(const struct ble_advertisement *,unsigned,unsigned);
void runtime_advertising_service(unsigned,unsigned);
void runtime_advertising_event(unsigned,const unsigned char *,unsigned);
void runtime_advertising_reset(void);
void runtime_advertising_complete(unsigned,const char *);
#endif
