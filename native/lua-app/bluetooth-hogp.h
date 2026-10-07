#ifndef DITOO_BLUETOOTH_HOGP_H
#define DITOO_BLUETOOTH_HOGP_H
#include "bluetooth-hid.h"
#define BT_NAME_BYTES 32
#define HID_PROFILE_USAGES 16
struct hid_profile {
    unsigned char name_size,keys,media,wake;
    uint16_t appearance,usage[HID_PROFILE_USAGES*2];
    char name[30];
};
unsigned runtime_hogp_configure(const struct hid_profile *);
unsigned runtime_hogp_profile_equal(const struct hid_profile *);
unsigned runtime_hogp_enabled(void);
unsigned runtime_hogp_advertising_ready(void);
unsigned runtime_hogp_mode(unsigned enabled);
void runtime_hogp_command(unsigned op,unsigned value,const unsigned char *data,unsigned epoch,unsigned generation);
void runtime_hogp_service(unsigned epoch);
void runtime_hogp_status(struct hid_status *);
unsigned runtime_hogp_bond(unsigned index,unsigned char *address,unsigned *type);
unsigned runtime_hogp_name(const unsigned char *address,unsigned type,char *out);
unsigned runtime_hogp_key(unsigned op,unsigned value,unsigned modifiers);
#endif
