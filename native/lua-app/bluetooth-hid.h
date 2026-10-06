#ifndef DITOO_BLUETOOTH_HID_H
#define DITOO_BLUETOOTH_HID_H
#include <stdint.h>
enum { HID_CONNECT=1, HID_DISCONNECT, HID_KEY, HID_CONSUMER, HID_LISTEN, HID_PAIR, HID_FORGET, HID_MODE, HID_CONFIGURE };
struct hid_status {
    unsigned enabled, state, generation, sent, released, errors, error, busy;
    unsigned char peer[6];
    unsigned incoming, opened, closed, close_status, close_channel, control;
    unsigned authentication_state, encryption_state, key_type, security_mode, ssp;
    unsigned access_mode, pairing, pair_remaining_ms, forgotten;
    unsigned keyboard_only, hidden_services, blocked_psms, audio_channels;
    unsigned transport, address_type;
};
void runtime_hid_service(unsigned epoch);
void runtime_hid_command(unsigned op, unsigned value, const unsigned char *data,
                         unsigned epoch, unsigned generation);
void runtime_hid_status(struct hid_status *);
unsigned runtime_hid_preserve_link(unsigned caller);
#endif
