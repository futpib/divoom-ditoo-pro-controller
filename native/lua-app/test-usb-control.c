/* Exercise the exact IRQ mailbox, stream ring and dispatcher boundaries. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "usb-control.c"
static unsigned clock_ms, free_heap=100000, allocated, dispatched, legacy_rx, legacy_tx;
static unsigned irq_disabled;
unsigned char stock_usb_setup[8]={0x21,9,0x7d,2,0,0,0,1};
unsigned char *stock_command_context;
void *stock_alloc(unsigned n) { ++allocated; return malloc(n); }
void stock_free(void *p) { if (p) { --allocated; free(p); } }
unsigned stock_free_heap(void) { return free_heap; }
unsigned stock_ticks(void) { return clock_ms; }
unsigned stock_control_connected(void) { return 0; }
unsigned runtime_irq_save(void) { unsigned was=irq_disabled;irq_disabled=1;return !was; }
void runtime_irq_restore(unsigned enabled) { if (enabled) irq_disabled=0; }
void stock_usb_receive(const void *p,unsigned n) { (void)p;(void)n;++legacy_rx; }
void stock_usb_send(const void *p,unsigned n,unsigned k) {
    (void)k;assert(n==256); if (p!=usb.report) ++legacy_tx;
}
unsigned stock_response_original(unsigned c,unsigned o,unsigned op,const void *p,
        unsigned n,unsigned a,unsigned b,unsigned ack) {
    (void)c;(void)o;(void)op;(void)p;(void)n;(void)a;(void)b;(void)ack;
    assert(!irq_disabled);return 0;
}
void runtime_usb_dispatch(void *arg,void *context) {
    assert(context==usb.context);
    struct packet { uint16_t a,n,b,c; unsigned char *data; } *p=arg;
    assert(!irq_disabled);assert(p->n>=3 && p->n<=4096);
    ++dispatched;
    runtime_usb_response(0,255,p->data[0],p->data+1,p->n-3,0,0,0x55);
}
static unsigned sequence, session;
static unsigned char out[256];
static void request(unsigned op,unsigned ack) {
    memset(out,0,sizeof out);memcpy(out,"DUSB\1",5);out[5]=op;
    usb_put32(out+8,session);usb_put32(out+12,++sequence);usb_put32(out+24,ack);
}
static void submit(void) {
    runtime_usb_receive(out,256);assert(usb.pending);
    runtime_usb_service();assert(!usb.pending && !irq_disabled);
    runtime_usb_send(NULL,256,6);
}
static void open_session(void) {
    sequence=0;++session;request(1,0);submit();
    assert(usb.session==session && usb.sequence==1 && !usb.status && allocated==1);
}
static void frame(unsigned size) {
    unsigned char data[USB_FRAME]={1};assert(size>=7 && size<=sizeof data);
    usb_put16(data+1,size-4);data[3]=0x37;
    for (unsigned i=4;i<size-3;++i) data[i]=i;
    unsigned sum=0;for (unsigned i=1;i<size-3;++i) sum+=data[i];
    usb_put16(data+size-3,sum);data[size-1]=2;
    for (unsigned offset=0;offset<size;) {
        unsigned count=size-offset;if (count>USB_CHUNK) count=USB_CHUNK;
        request(2,usb.read);usb_put16(out+16,size);usb_put16(out+18,offset);
        usb_put16(out+20,count);memcpy(out+32,data+offset,count);submit();
        assert(!usb.status);offset+=count;
        assert(allocated==(offset==size ? 1 : 2));
        assert((usb.frame!=NULL)==(offset<size));
    }
}
int main(void) {
    runtime_usb_send(NULL,256,6);assert(!memcmp(usb.report,"DUSB\1",5));
    assert(usb_u16(usb.report+20)==4100 && !allocated);
    stock_usb_setup[2]=0;runtime_usb_receive(out,256);runtime_usb_send(out,256,6);
    assert(legacy_rx==1 && legacy_tx==1);stock_usb_setup[2]=0x7d;
    assert(!runtime_usb_connected());
    open_session();assert(runtime_usb_connected());
    frame(4100);assert(dispatched==1 && usb.written==4102);
    /* The response frame adds two bytes to the command frame. Queue supports it. */
    submit();assert(dispatched==1); /* Duplicate final fragment cannot reexecute. */
    unsigned consumed=0;
    while (usb.written!=usb.read) {
        unsigned count=usb_u16(usb.report+28);assert(count && count<=USB_CHUNK);
        assert(usb_u32(usb.report+24)==consumed);consumed+=count;
        request(3,consumed);submit();assert(!usb.status);
    }
    for (unsigned i=0;i<1500;++i) { frame(7);request(3,usb.written);submit(); }
    assert(dispatched==1501 && usb.read>USB_RING);
    request(3,usb.written+1);submit();assert(usb.status==USB_INVALID);
    request(4,0);submit();assert(!allocated && !usb.session);
    open_session();request(2,0);usb_put16(out+16,10);usb_put16(out+18,1);
    usb_put16(out+20,10);submit();assert(usb.status==USB_INVALID && dispatched==1501);
    open_session();request(2,0);usb_put16(out+16,7);usb_put16(out+20,7);
    memcpy(out+32,"\1\3\0\x37\0\0\2",7);submit();assert(usb.status==USB_FRAME_ERROR);
    open_session();unsigned char big[4091]={0};
    runtime_usb_response(0,255,1,big,sizeof big,0,0,0x55);
    runtime_usb_response(0,255,1,big,sizeof big,0,0,0x55);
    assert(usb.status==USB_OVERFLOW && usb.written==4100);
    open_session();clock_ms+=15001;runtime_usb_service();assert(!allocated);
    free_heap=30000;sequence=0;++session;request(1,0);submit();assert(usb.status==USB_MEMORY_ERROR && !allocated);
    /* Input allocation failure and incomplete frames release everything on
     * close, replacement, or expiry, without reducing the maximum packet. */
    free_heap=100000;open_session();free_heap=24576;
    request(2,0);usb_put16(out+16,4100);usb_put16(out+20,1);submit();
    assert(usb.status==USB_MEMORY_ERROR && allocated==1 && !usb.frame);
    request(4,0);submit();assert(!allocated);
    free_heap=100000;open_session();request(2,0);usb_put16(out+16,4100);usb_put16(out+20,1);submit();
    assert(!usb.status && allocated==2 && usb.frame);
    open_session();assert(allocated==1 && !usb.frame);
    request(2,0);usb_put16(out+16,4100);usb_put16(out+20,1);submit();
    clock_ms+=15001;runtime_usb_service();assert(!allocated && !usb.frame);
    open_session();request(4,0);submit();
    puts("USB bridge: fragmentation, duplicates, wrap, overflow, malformed frames, expiry and legacy routing passed");
}
