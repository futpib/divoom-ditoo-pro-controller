#ifndef DITOO_BLUETOOTH_HOGP_H
#define DITOO_BLUETOOTH_HOGP_H
#include "bluetooth-hid.h"
#define BT_NAME_BYTES 32
unsigned runtime_hogp_enabled(void);
unsigned runtime_hogp_mode(unsigned enabled);
void runtime_hogp_command(unsigned op,unsigned value,const unsigned char *data,unsigned epoch,unsigned generation);
void runtime_hogp_service(unsigned epoch);
void runtime_hogp_status(struct hid_status *);
unsigned runtime_hogp_bond(unsigned index,unsigned char *address,unsigned *type);
unsigned runtime_hogp_name(const unsigned char *address,unsigned type,char *out);
unsigned runtime_hogp_key(unsigned op,unsigned value,unsigned modifiers);
#endif
