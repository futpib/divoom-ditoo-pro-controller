/* A bounded lease of the stock controller's single legacy advertising set.
 * Only the Bluetooth task calls this engine; buffers outlive every stock call. */
#include <stddef.h>
#include <string.h>
#include "bluetooth-advertising.h"
extern void *stock_alloc(unsigned);
extern void stock_free(void *);
extern unsigned stock_free_heap(void),stock_ticks(void);
extern unsigned char *volatile stock_hci_stack;
extern unsigned runtime_hogp_advertising_ready(void);
extern void stock_le_adv_data(unsigned,const unsigned char *);
extern void stock_le_scan_data(unsigned,const unsigned char *);
extern void stock_le_adv_enable(unsigned);
extern void stock_le_adv_params(unsigned,unsigned,unsigned,unsigned,const unsigned char *,unsigned,unsigned);
static struct advertising {
    struct ble_advertisement packet;
    unsigned ticket,epoch,phase,started;
    unsigned char *stack;
    const unsigned char *previous_data,*previous_scan;
    unsigned char previous_size,previous_scan_size,previous_enabled,parameters[14];
    const char *error;
} *advertising;
static unsigned adv_word(const unsigned char *p) { return p[0]|(p[1]<<8); }
static unsigned ad_valid(const unsigned char *p,unsigned size) {
    if(size>31) return 0;
    for(unsigned at=0;at<size;) {
        unsigned n=p[at++];if(!n || n>size-at) return 0;
        at+=n;
    }
    return 1;
}
unsigned runtime_advertising_valid(const struct ble_advertisement *p) {
    return p && p->size && ad_valid(p->data,p->size) && ad_valid(p->scan,p->scan_size) &&
        (p->type==0 || p->type==2 || p->type==3) && (p->type!=3 || !p->scan_size) &&
        p->interval_ms>=100 && p->interval_ms<=1000 &&
        p->duration_ms>=p->interval_ms && p->duration_ms<=5000;
}
unsigned runtime_advertising_busy(void) { return advertising!=NULL; }
static unsigned le_link(void) {
    unsigned char *c;memcpy(&c,stock_hci_stack+16,sizeof c);
    for(unsigned i=0;c && i<16;++i) {
        unsigned type;memcpy(&type,c+12,4);
        if(type<=1) return 1;
        memcpy(&c,c,sizeof c);
    }
    return c!=NULL;
}
const char *runtime_advertising_start(const struct ble_advertisement *p,unsigned ticket,unsigned epoch) {
    if(!runtime_advertising_valid(p)) return "invalid advertisement";
    if(!stock_hci_stack) return "Bluetooth unavailable";
    if(advertising || !runtime_hogp_advertising_ready() || le_link() || (stock_hci_stack[0x4e8]&7)) return "busy";
    unsigned char *h=stock_hci_stack;
    if(h[0x4dc]>31 || h[0x4e4]>31) return "unsupported advertising state";
    if(stock_free_heap()<sizeof(struct advertising)+32+24576) return "insufficient native heap";
    struct advertising *a=stock_alloc(sizeof *a);if(!a) return "insufficient native heap";
    memset(a,0,sizeof *a);a->packet=*p;a->ticket=ticket;a->epoch=epoch;a->stack=h;
    /* These offsets and pointer lifetimes are pinned to stock 306007's setters. */
    memcpy(&a->previous_data,h+0x4d8,sizeof a->previous_data);a->previous_size=h[0x4dc];
    memcpy(&a->previous_scan,h+0x4e0,sizeof a->previous_scan);a->previous_scan_size=h[0x4e4];
    memcpy(a->parameters,h+0x4ea,sizeof a->parameters);a->previous_enabled=h[0x4e6];
    a->phase=1;a->started=stock_ticks();advertising=a;stock_le_adv_enable(0);
    return NULL;
}
static void adv_finish(const char *error) {
    struct advertising *a=advertising;unsigned ticket=a->ticket;
    advertising=NULL;stock_free(a);runtime_advertising_complete(ticket,error);
}
static void adv_restore(const char *error) {
    struct advertising *a=advertising;const unsigned char *p=a->parameters;
    a->error=error;a->phase=4;a->started=stock_ticks();
    stock_le_adv_enable(0);
    stock_le_adv_params(adv_word(p),adv_word(p+2),p[4],p[5],p+8,p[6],p[7]);
    stock_le_adv_data(a->previous_size,a->previous_data);
    stock_le_scan_data(a->previous_scan_size,a->previous_scan);
    stock_le_adv_enable(a->previous_enabled);
}
void runtime_advertising_reset(void) {
    if(!advertising) return;
    if(stock_hci_stack==advertising->stack) adv_restore("Bluetooth reset");
    adv_finish("Bluetooth reset");
}
void runtime_advertising_service(unsigned epoch,unsigned cancel_ticket) {
    struct advertising *a=advertising;if(!a) return;
    if(stock_hci_stack!=a->stack) { adv_finish("Bluetooth reset");return; }
    unsigned char *h=stock_hci_stack;
    if(a->phase==2 || a->phase==3) {
        const unsigned char *data,*scan;
        memcpy(&data,h+0x4d8,sizeof data);memcpy(&scan,h+0x4e0,sizeof scan);
        if(data!=a->packet.data || scan!=a->packet.scan) {
            /* Stock source/reset paths may replace the advertiser. Preserve
             * their new state and detach only pointers still owned by us. */
            if(data==a->packet.data) stock_le_adv_data(a->previous_size,a->previous_data);
            if(scan==a->packet.scan) stock_le_scan_data(a->previous_scan_size,a->previous_scan);
            adv_finish("advertising replaced");return;
        }
    }
    if(a->phase!=4 && (epoch!=a->epoch || cancel_ticket==a->ticket)) adv_restore("cancelled");
    if(a->phase!=4 && le_link()) adv_restore("Bluetooth connection interrupted advertising");
    if(a->phase!=4 && a->error) adv_restore(a->error);
    unsigned age=stock_ticks()-a->started;
    if(a->phase==1 && !h[0x4e5]) {
        const unsigned char zero[6]={0};unsigned interval=a->packet.interval_ms*8/5;
        a->phase=2;a->started=stock_ticks();
        stock_le_adv_params(interval,interval,a->packet.type,0,zero,7,0);
        stock_le_adv_data(a->packet.size,a->packet.data);
        stock_le_scan_data(a->packet.scan_size,a->packet.scan);
        stock_le_adv_enable(1);return;
    }
    if(a->phase==2 && h[0x4e5] && !(h[0x4e8]&7)) {
        a->phase=3;a->started=stock_ticks();return;
    }
    if(a->phase==3 && age>=a->packet.duration_ms) { adv_restore(NULL);return; }
    if(a->phase<3 && age>=1000) { adv_restore("advertising setup timed out");return; }
    if(a->phase==4) {
        /* Setters have replaced both retained pointers before this allocation
         * can be freed. Wait for controller configuration to settle as well. */
        if(!(h[0x4e8]&7) && !!h[0x4e5]==!!a->previous_enabled) adv_finish(a->error);
        else if(age>=1000) adv_finish("advertising restore timed out");
    }
}
void runtime_advertising_event(unsigned type,const unsigned char *p,unsigned n) {
    if(!advertising || type!=4 || n<6 || p[0]!=0x0e || !p[5]) return;
    unsigned op=adv_word(p+3);
    if(op==0x2006 || op==0x2008 || op==0x2009 || op==0x200a)
        advertising->error="controller rejected advertising";
}
