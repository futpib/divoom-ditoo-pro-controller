#ifndef DITOO_BLUETOOTH_TRACE_H
#define DITOO_BLUETOOTH_TRACE_H
void runtime_bt_trace_service(void);
void runtime_bt_trace(unsigned kind, unsigned event, const void *data, unsigned length);
void runtime_bt_trace_hci(const unsigned char *packet);
void runtime_bt_trace_stack(unsigned event, const unsigned char *params);
void runtime_bt_trace_read(unsigned context, const unsigned char *data, unsigned length);
unsigned runtime_bt_trace_app_disconnect(unsigned caller);
void runtime_bt_trace_force_disconnect(const unsigned char *remote, unsigned reason, unsigned force, unsigned caller);
void runtime_bt_trace_link_disconnect(const unsigned char *remote, unsigned caller);
#endif
