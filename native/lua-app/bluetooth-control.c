/* The native main task serializes these connection lifecycle callbacks. */
extern unsigned char *stock_command_context;
extern unsigned stock_control_open(unsigned,unsigned);
extern void stock_control_close(unsigned,unsigned);
extern unsigned stock_le_control_connected(unsigned);
extern unsigned runtime_hogp_enabled(void);

unsigned runtime_control_open(unsigned context,unsigned transport) {
    unsigned char *p=stock_command_context;
    /* A HID-only BLE host reserves the stock parser without using it. Do not
     * interrupt an actual controller or a partially received command. */
    if(transport==1 && runtime_hogp_enabled() && p && !p[2] && !p[5] && !p[7])
        stock_control_close(context,0);
    return stock_control_open(context,transport);
}
void runtime_control_close(unsigned context,unsigned transport) {
    stock_control_close(context,transport);
    /* Restore the still-connected BLE host's control service without touching
     * its radio link, encryption, HID subscriptions or pairing. */
    if(transport==1 && runtime_hogp_enabled() && stock_le_control_connected(1))
        stock_control_open(context,0);
}
