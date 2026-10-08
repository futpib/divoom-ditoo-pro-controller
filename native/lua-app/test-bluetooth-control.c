#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bluetooth-control.c"
static unsigned char context[0x178];
unsigned char *stock_command_context=context;
static unsigned remote,le_connected,closed,opened;
unsigned runtime_hogp_enabled(void) { return remote; }
unsigned stock_le_control_connected(unsigned role) { assert(role==1);return le_connected; }
unsigned stock_control_open(unsigned c,unsigned transport) {
    assert(c==0);++opened;
    if(!stock_command_context) { stock_command_context=context;context[2]=255; }
    if(context[2]!=255) return 0;
    context[2]=transport;context[5]=context[7]=0;return 1;
}
void stock_control_close(unsigned c,unsigned transport) {
    assert(c==0);++closed;
    if(stock_command_context && context[2]==transport) {
        context[2]=255;context[5]=context[7]=0;
    }
}
int main(void) {
    remote=le_connected=1;
    assert(runtime_control_open(0,1) && context[2]==1 && closed==1);
    runtime_control_close(0,1);assert(context[2]==0 && opened==2);
    /* An active controller and an incomplete BLE frame retain ownership. */
    for(unsigned field=5;field<=7;field+=2) {
        context[field]=1;unsigned before=closed;
        assert(!runtime_control_open(0,1) && context[2]==0 && closed==before);
        context[field]=0;
    }
    remote=0;assert(!runtime_control_open(0,1) && context[2]==0);
    remote=1;assert(runtime_control_open(0,1));
    /* BLE arriving or leaving cannot reset an active serial session. */
    context[7]=1;assert(!runtime_control_open(0,0));
    runtime_control_close(0,0);assert(context[2]==1 && context[7]==1);
    le_connected=0;runtime_control_close(0,1);assert(context[2]==255);
    assert(runtime_control_open(0,1));le_connected=1;
    runtime_control_close(0,1);assert(context[2]==0 && !context[7]);
    stock_command_context=NULL;assert(runtime_control_open(0,1) && context[2]==1);
    remote=0;runtime_control_close(0,1);assert(context[2]==255);
    puts("Bluetooth control ownership and HID-only handoff tests passed");
}
