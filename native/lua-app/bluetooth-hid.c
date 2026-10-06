/* Classic HID over the pinned stock L2CAP/SDP ABI. All entry points except the
 * snapshot run on the Bluetooth task; Lua never owns a packet or held key. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "bluetooth-hid.h"
#include "bluetooth-hogp.h"
#include "bluetooth-trace.h"
extern void *volatile stock_bt_context;
extern unsigned char *volatile stock_bt_core;
extern volatile unsigned char stock_bt_manager[];
extern unsigned stock_ticks(void), stock_free_heap(void);
struct l2_event {
    uint8_t event, reserved; uint16_t status;
    unsigned char *remote;
    const void *psm;
    uint16_t length, reserved2;
    void *data;
};
struct psm { void (*callback)(unsigned,struct l2_event *); uint16_t id,mtu,minimum,flags; };
struct attribute { uint16_t id,length; const unsigned char *value; uint16_t flags; };
struct record { void *next,*previous; uint8_t count,pad[3]; struct attribute *attributes;
                uint32_t cod,handle,time; uint16_t internal[4]; };
struct security { void *next,*previous; void (*callback)(void *); uint32_t psm;
                  uint8_t level,min_key,pad[2]; };
#if __SIZEOF_POINTER__ == 4
_Static_assert(sizeof(struct record)==36, "stock SDP record ABI");
_Static_assert(sizeof(struct attribute)==12, "stock SDP attribute ABI");
_Static_assert(sizeof(struct psm)==12, "stock L2CAP PSM ABI");
_Static_assert(offsetof(struct l2_event,data)==16, "stock L2CAP callback ABI");
_Static_assert(sizeof(struct security)==20, "stock security record ABI");
#endif
struct packet { void *next,*previous; unsigned char *data; uint16_t length,flags;
                unsigned char internal[48]; };
extern unsigned stock_l2_register(struct psm *);
extern unsigned stock_l2_connect(struct psm *,unsigned,void *,unsigned,uint16_t *);
extern unsigned stock_l2_accept(unsigned,unsigned,unsigned);
extern unsigned stock_l2_disconnect(unsigned);
extern unsigned stock_l2_send(unsigned,struct packet *);
extern unsigned stock_sdp_add(struct record *);
extern unsigned stock_sdp_remove(struct record *);
extern unsigned stock_bt_class(unsigned);
extern unsigned stock_cmgr_register(void *,void (*)(void *,unsigned,unsigned));
extern unsigned stock_cmgr_connect(void *,const unsigned char *);
extern unsigned stock_cmgr_remove(void *);
extern unsigned stock_sec_register(struct security *);
extern void stock_l2_security(void *);
extern unsigned stock_bt_access(unsigned, const void *);
extern void *stock_bt_find_device(const unsigned char *);

/* Report 1: modifier byte, reserved byte, six keyboard usages. Report 2: one
 * 16-bit Consumer usage (Play/Pause 0xcd, Mute 0xe2). */
#define KEYBOARD_DESCRIPTOR \
    0x05,1,0x09,6,0xa1,1,0x85,1,0x05,7,0x19,0xe0,0x29,0xe7, \
    0x15,0,0x25,1,0x75,1,0x95,8,0x81,2,0x95,1,0x75,8,0x81,1, \
    0x95,6,0x75,8,0x15,0,0x26,0xff,0,0x19,0,0x2a,0xff,0,0x81,0, \
    0x05,8,0x19,1,0x29,5,0x95,5,0x75,1,0x91,2,0x95,1,0x75,3,0x91,1,0xc0, \
    0x05,0x0c,0x09,1,0xa1,1,0x85,2,0x15,0,0x26,0xff,3, \
    0x19,0,0x2a,0xff,3,0x75,16,0x95,1,0x81,0,0xc0
static const unsigned char descriptor[] = { KEYBOARD_DESCRIPTOR };
static const unsigned char descriptor_list[] = {
    0x35,sizeof descriptor+6,0x35,sizeof descriptor+4,0x08,0x22,
    0x25,sizeof descriptor,KEYBOARD_DESCRIPTOR
};
#define ATTR(name,...) static const unsigned char name[]={__VA_ARGS__}
ATTR(classes,0x35,3,0x19,0x11,0x24);
ATTR(protocols,0x35,13,0x35,6,0x19,1,0,0x09,0,0x11,0x35,3,0x19,0,0x11);
ATTR(additional,0x35,15,0x35,13,0x35,6,0x19,1,0,0x09,0,0x13,0x35,3,0x19,0,0x11);
ATTR(browse,0x35,3,0x19,0x10,2);
ATTR(language,0x35,9,0x09,0x65,0x6e,0x09,0,0x6a,0x09,1,0);
ATTR(profile,0x35,8,0x35,6,0x19,0x11,0x24,0x09,1,1);
ATTR(name,0x25,14,'D','i','t','o','o',' ','K','e','y','b','o','a','r','d');
ATTR(version,0x09,1,0x11);
ATTR(subclass,0x08,0x40);
ATTR(country,0x08,0);
ATTR(yes,0x28,1);
ATTR(no,0x28,0);
ATTR(hid_language,0x35,8,0x35,6,0x09,4,9,0x09,1,0);
ATTR(timeout,0x09,0x0c,0x80);
#define A(id,v) {id,sizeof(v),v,0}
static struct attribute attributes[]={
    A(1,classes),A(4,protocols),A(5,browse),A(6,language),A(9,profile),
    A(13,additional),A(0x100,name),A(0x201,version),A(0x202,subclass),
    A(0x203,country),A(0x204,yes),A(0x205,yes),A(0x206,descriptor_list),
    A(0x207,hid_language),A(0x209,yes),A(0x20a,no),A(0x20c,timeout),
    A(0x20d,yes),A(0x20e,no)
};
static struct {
    struct hid_status status;
    void *context,*core;
    struct record record;
    uint32_t manager[25];
    struct psm psm[2];
    struct security security[2];
    uint16_t cid[2];
    unsigned up[2], pending[2], active, epoch, started, phase, down, listening, outgoing;
    unsigned access_owned, access_previous, pair_started, pair_duration, pair_epoch;
    struct record *hidden[8];
    struct psm *blocked[8];
    unsigned previous_class;
    unsigned char report[10];
    struct { struct packet packet; unsigned char bytes[12]; unsigned busy,started; } tx[2];
} hid;

static void error(unsigned code) {
    hid.status.error=code; ++hid.status.errors;
    runtime_bt_trace(5,0,&code,sizeof code);
}
static unsigned elapsed(unsigned t,unsigned delay) { return (unsigned)(stock_ticks()-t)>=delay; }
static unsigned audio_psm(unsigned id) {
    /* RFCOMM includes HFP/HSP and serial control. SDP, HID and BLE stay up. */
    return id==3 || id==0x17 || id==0x19 || id==0x1b;
}
static struct psm **registered_psms(void) { return (struct psm **)(stock_bt_core+0x28fc); }
static unsigned audio_record(const struct record *r) {
    if (r->count>32 || !r->attributes) return 0;
    for (unsigned i=0;i<r->count;++i) {
        const struct attribute *a=&r->attributes[i];
        if (a->id!=1 || a->length<5 || a->length>64 || !a->value) continue;
        const unsigned char *v=a->value;
        /* The pinned stock profiles use a short sequence of 16-bit UUIDs. */
        if (v[0]!=0x35 || v[1]+2U!=a->length) continue;
        for (unsigned j=2;j+3<=a->length;j+=3) {
            if (v[j]!=0x19) break;
            unsigned uuid=v[j+1]*256U+v[j+2];
            if (uuid==0x1101 || uuid==0x1108 || (uuid>=0x110a && uuid<=0x110f) ||
                    uuid==0x1112 || uuid==0x111e || uuid==0x111f || uuid==0x1131) return 1;
        }
    }
    return 0;
}
static void audio_channels(void) {
    hid.status.audio_channels=0;
    for (unsigned i=0;i<8;++i) {
        const unsigned char *channel=stock_bt_core+0x291c+i*0x7c;
        uint16_t cid;struct psm *p;
        memcpy(&cid,channel+0x2c,2);memcpy(&p,channel+0x28,sizeof p);
        if (!cid || !p || !audio_psm(p->id)) continue;
        ++hid.status.audio_channels;
        /* Leave profile objects/callbacks alive to return packets and clean up.
         * Retry a pending connection once it has a remote CID; 6+ is closing. */
        if (channel[2]<6) stock_l2_disconnect(cid);
    }
}
static void mode(unsigned keyboard_only) {
    if (keyboard_only==hid.status.keyboard_only) return;
    struct psm **psms=registered_psms();
    if (keyboard_only) {
        struct record *head=(struct record *)(stock_bt_core+0x3068),*r=head->next;
        unsigned count=0,visited=0;
        /* Collect before mutating: corrupt/oversized lists cannot partially
         * disable the stack or make this Bluetooth-task operation unbounded. */
        while (r && r!=head && visited++<24) {
            if (audio_record(r)) {
                if (count==8) { error(18);return; }
                hid.hidden[count++]=r;
            }
            r=r->next;
        }
        if (r!=head) { error(18);return; }
        memcpy(&hid.previous_class,stock_bt_core+0x9a8,4);
        for (unsigned i=0;i<8;++i) if (psms[i] && audio_psm(psms[i]->id)) {
            hid.blocked[i]=psms[i];psms[i]=NULL;++hid.status.blocked_psms;
        }
        /* RemoveRecord unlinks synchronously and updates SDP continuation and
         * database state; its return is the database-update result, not unlink. */
        for (unsigned i=0;i<count;++i) stock_sdp_remove(hid.hidden[i]);
        hid.status.hidden_services=count;hid.status.keyboard_only=1;
        stock_bt_class(0x540);
        audio_channels();
    } else {
        for (unsigned i=0;i<8;++i) if (hid.blocked[i] && psms[i]) { error(19);return; }
        for (unsigned i=0;i<hid.status.hidden_services;++i) stock_sdp_add(hid.hidden[i]);
        for (unsigned i=0;i<8;++i) if (hid.blocked[i]) { psms[i]=hid.blocked[i];hid.blocked[i]=NULL; }
        stock_bt_class(hid.previous_class);
        hid.status.keyboard_only=hid.status.hidden_services=hid.status.blocked_psms=hid.status.audio_channels=0;
    }
    hid.status.error=0;
}
static void access_restore(void) {
    if (!hid.access_owned) return;
    unsigned rc=stock_bt_access(hid.access_previous,NULL);
    if (rc && rc!=2) return; /* Retry on the Bluetooth task if the stack is busy. */
    hid.access_owned=0;hid.status.pairing=hid.status.pair_remaining_ms=0;
}
static unsigned access_set(unsigned mode) {
    if (!hid.access_owned) hid.access_previous=stock_bt_core[0x792];
    unsigned rc=stock_bt_access(mode,NULL);
    if (rc && rc!=2) { error(0x600+rc);return 0; }
    hid.access_owned=1;return 1;
}
static void disconnect(void) {
    hid.active=hid.listening; hid.phase=0; hid.down=0;
    ++hid.status.generation;
    for (unsigned i=0;i<2;++i) if (hid.cid[i]) stock_l2_disconnect(hid.cid[i]);
    if (hid.status.enabled) stock_cmgr_remove(hid.manager);
    hid.status.state=(hid.cid[0] || hid.cid[1]) ? 3 : hid.listening ? 4 : 0;
    if (!hid.listening) access_restore();
}
static unsigned send(unsigned channel,const unsigned char *data,unsigned size) {
    if (!hid.up[channel] || hid.tx[channel].busy || size>12) return 0;
    memset(&hid.tx[channel].packet,0,sizeof hid.tx[channel].packet);
    memcpy(hid.tx[channel].bytes,data,size);
    hid.tx[channel].packet.data=hid.tx[channel].bytes;
    hid.tx[channel].packet.length=size; hid.tx[channel].packet.flags=1;
    hid.tx[channel].started=stock_ticks(); hid.tx[channel].busy=1;
    unsigned rc=stock_l2_send(hid.cid[channel],&hid.tx[channel].packet);
    if (rc!=2) { hid.tx[channel].busy=0; error(0x100+rc); return 0; }
    return 1;
}
static void connect_channel(unsigned i) {
    if (!hid.active || hid.cid[i] || hid.pending[i]) return;
    void *remote; memcpy(&remote,(unsigned char *)hid.manager+0x14,sizeof remote);
    if (!remote) return;
    hid.pending[i]=1;
    unsigned rc=stock_l2_connect(&hid.psm[i],hid.psm[i].id,remote,0,&hid.cid[i]);
    if (rc!=2) { hid.pending[i]=0; error(0x200+rc); disconnect(); }
}
static void link_event(void *manager,unsigned event,unsigned status) {
    (void)manager;
    runtime_bt_trace(4,event,&status,sizeof status);
    if (event==1 && !status) connect_channel(0);
    else if (event==1 && status) { error(0x300+status); disconnect(); }
}
static void control(const unsigned char *p,unsigned n) {
    unsigned char reply[12]={0}; unsigned size=1;
    if (!n || n>12) return;
    hid.status.control=p[0];
    switch (p[0]&0xf0) {
    case 0x10:
        if ((p[0]&15)==5) { hid.listening=0;disconnect(); }
        else if ((p[0]&15)==3) { hid.down=0;hid.phase=2; }
        return;
    case 0x40: /* GET_REPORT: current keyboard or consumer state. */
        if ((p[0]&3)!=1 || n<2 || (p[1]!=1 && p[1]!=2)) reply[0]=2;
        else { reply[0]=0xa1;reply[1]=p[1];size=p[1]==1 ? 10 : 4;
            if (hid.down && hid.report[1]==p[1]) memcpy(reply,hid.report,size); }
        break;
    case 0x50: /* Keyboard LED output is accepted but does not own Ditoo LEDs. */
        if (p[0]!=0x52 || n!=3 || p[1]!=1) reply[0]=4;
        break;
    case 0x60: reply[0]=0xa0;reply[1]=1;size=2;break;
    case 0x70: if (p[0]!=0x71) reply[0]=3;break; /* Report protocol only. */
    default: reply[0]=3;break;
    }
    if (!send(0,reply,size)) { error(4);disconnect(); }
}
static void l2_event(unsigned cid,struct l2_event *p) {
    if (!p || !p->psm) return;
    if (p->remote) {
        hid.status.authentication_state=p->remote[0xb1];
        hid.status.encryption_state=p->remote[0xb3];
        hid.status.key_type=p->remote[0xbe];
        hid.status.ssp=p->remote[0xa6]&1;
        hid.status.security_mode=stock_bt_core[0x96a];
    }
    const struct psm *psm=p->psm;
    unsigned i=psm->id==0x13;
    if (psm->id!=0x11 && psm->id!=0x13) return;
    if (p->event!=5 && p->event!=6) {
        unsigned char data[12]={psm->id,p->status,p->status>>8,cid,cid>>8};
        if(p->remote) memcpy(data+5,p->remote+0x54,6);
        runtime_bt_trace(3,p->event,data,11);
    }
    if (p->event==1) {
        ++hid.status.incoming;
        unsigned any=0;for(unsigned j=0;j<6;++j) any|=hid.status.peer[j];
        if (hid.active && hid.status.pairing && !elapsed(hid.pair_started,hid.pair_duration) &&
                !any && !i && p->remote && hid.status.state==4) {
            memcpy(hid.status.peer,p->remote+0x54,6);++hid.status.generation;
        }
        if (!hid.active || hid.status.state==3 || !p->remote || memcmp(p->remote+0x54,hid.status.peer,6) ||
                (hid.cid[i] && hid.cid[i]!=cid)) { stock_l2_accept(cid,3,0);return; }
        hid.cid[i]=cid;hid.pending[i]=1;
        if (!i) hid.outgoing=0;
        hid.status.state=1;hid.started=stock_ticks();
        if (stock_l2_accept(cid,0,0)!=2) { error(5);disconnect(); }
    } else if (p->event==2) {
        ++hid.status.opened;
        if (!hid.active || p->status || !p->remote || p->remote[0xb1]!=8 || p->remote[0xb3]!=2 ||
                memcmp(p->remote+0x54,hid.status.peer,6)) {
            stock_l2_disconnect(cid);error(6);return;
        }
        hid.cid[i]=cid;hid.up[i]=1;hid.pending[i]=0;
        if (!i && hid.outgoing) connect_channel(1);
        if (hid.up[0] && hid.up[1]) { hid.status.state=2;hid.status.error=0;access_restore(); }
    } else if (p->event==4) {
        if (hid.cid[i]!=cid) return;
        ++hid.status.closed;hid.status.close_status=p->status;hid.status.close_channel=psm->id;
        hid.cid[i]=0;hid.up[i]=hid.pending[i]=0;
        /* The stack returns pending packets before the channel-closed event. */
        hid.tx[i].busy=0;
        disconnect();
    } else if (p->event==5 && !i && hid.cid[0]==cid) control(p->data,p->length);
    else if (p->event==6 && p->data==&hid.tx[i].packet) {
        hid.tx[i].busy=0;
        if (p->status) { error(0x400+p->status);disconnect(); }
    }
}
static unsigned enable(void) {
    if (hid.status.enabled) return hid.status.enabled==1;
    if (!stock_bt_context || stock_free_heap()<16384) { error(7);return 0; }
    hid.context=stock_bt_context;hid.core=stock_bt_core;
    hid.psm[0]=(struct psm){l2_event,0x11,672,48,0};
    hid.psm[1]=(struct psm){l2_event,0x13,672,48,0};
    hid.record.count=sizeof attributes/sizeof *attributes;
    hid.record.attributes=attributes;
    /* Partial setup stays allocated and cannot be registered a second time. */
    hid.status.enabled=2;
    /* SSP level 2 permits the stock NoInputNoOutput pairing method. Level 3
     * requests a MITM-authenticated key which that method cannot produce. */
    for (unsigned i=0;i<2;++i) {
        hid.security[i]=(struct security){.callback=stock_l2_security,.psm=hid.psm[i].id,.level=0x22};
        if (stock_sec_register(&hid.security[i])) { error(16);return 0; }
    }
    if (stock_cmgr_register(hid.manager,link_event) || stock_l2_register(&hid.psm[0]) ||
            stock_l2_register(&hid.psm[1]) || stock_sdp_add(&hid.record)) { error(8);return 0; }
    stock_bt_class(0x2c0540); /* Preserve audio services, advertise a keyboard. */
    hid.status.enabled=1;return 1;
}
void runtime_hid_service(unsigned epoch) {
    runtime_hogp_service(epoch);
    if (!hid.status.enabled) return;
    unsigned registered=0;
    if (stock_bt_core && hid.core==stock_bt_core)
        for (unsigned i=0;i<8;++i) {
            void *p;memcpy(&p,stock_bt_core+0x28fc+i*sizeof p,sizeof p);
            if (p==&hid.psm[0] || p==&hid.psm[1]) ++registered;
        }
    /* Source changes can destroy and recreate the stack at the same address. */
    if (hid.context!=stock_bt_context || hid.core!=stock_bt_core ||
            (hid.status.enabled==1 && registered!=2)) {
        memset(&hid,0,sizeof hid);return;
    }
    if (hid.status.keyboard_only) audio_channels();
    if (hid.active && hid.status.state==1 && elapsed(hid.started,120000)) { error(9);disconnect(); }
    hid.status.access_mode=stock_bt_core[0x792];
    if (hid.status.pairing) {
        unsigned age=stock_ticks()-hid.pair_started;
        if (age>=hid.pair_duration || epoch!=hid.pair_epoch) access_restore();
        else hid.status.pair_remaining_ms=hid.pair_duration-age;
    } else if (hid.access_owned && !hid.active) access_restore();
    unsigned char *remote;memcpy(&remote,(unsigned char *)hid.manager+0x14,sizeof remote);
    if (remote && hid.active) {
        hid.status.authentication_state=remote[0xb1];hid.status.encryption_state=remote[0xb3];
        hid.status.key_type=remote[0xbe];hid.status.ssp=remote[0xa6]&1;
        hid.status.security_mode=stock_bt_core[0x96a];
    }
    for (unsigned i=0;i<2;++i)
        if (hid.tx[i].busy && elapsed(hid.tx[i].started,1000)) { error(10);disconnect();return; }
    if (hid.phase && (epoch!=hid.epoch || elapsed(hid.started,80))) hid.phase=2;
    if (hid.phase==2 && !hid.tx[1].busy) {
        unsigned char release[10]={0xa1,hid.report[1]};
        if (send(1,release,release[1]==1 ? 10 : 4)) {
            hid.down=0;hid.phase=0;++hid.status.released;
        } else { error(11);disconnect(); }
    }
    hid.status.busy=hid.phase || hid.tx[1].busy;
}
void runtime_hid_command(unsigned op,unsigned value,const unsigned char *data,
                         unsigned epoch,unsigned generation) {
    if (op==HID_MODE) {
        if (value>2) return;
        if (value==2) {
            if (hid.status.state || hid.tx[0].busy || hid.tx[1].busy) { error(20);return; }
            if (enable() && runtime_hogp_mode(1)) mode(1);
        } else if (runtime_hogp_mode(0) && enable()) mode(value);
        return;
    }
    if (runtime_hogp_enabled()) { runtime_hogp_command(op,value,data,epoch,generation);return; }
    if (op==HID_FORGET) {
        if (hid.status.state) { error(17);return; }
        unsigned char *entry=stock_bt_find_device(data),*remote=NULL;
        if (entry) memcpy(&remote,entry+0x28,sizeof remote);
        if (remote && remote[0x96]) { error(17);return; }
        for (unsigned i=0;i<8;++i) {
            volatile unsigned char *r=stock_bt_manager+7+i*26;
            unsigned match=r[25]!=0;
            for (unsigned j=0;j<6;++j) if (r[j]!=data[j]) match=0;
            if (!match) continue;
            /* Stock DdbDeleteRecord mishandles slot 7; compact the exact same
             * native array without clearing another peer's record. */
            for (unsigned j=i*26;j<7*26;++j) stock_bt_manager[7+j]=stock_bt_manager[7+j+26];
            for (unsigned j=0;j<26;++j) stock_bt_manager[7+7*26+j]=0;
            if (entry) { uint16_t flags;memcpy(&flags,entry+8,2);flags&=~6U;memcpy(entry+8,&flags,2); }
            stock_bt_manager[0]=1;++hid.status.forgotten;hid.status.error=0;return;
        }
        return;
    }
    if (op==HID_CONNECT || op==HID_LISTEN || op==HID_PAIR) {
        if (!enable()) return;
        if (hid.status.state==2 && !memcmp(hid.status.peer,data,6)) return;
        if (hid.status.state==1 && !memcmp(hid.status.peer,data,6)) return;
        if ((hid.status.state && hid.status.state!=4) || hid.tx[0].busy || hid.tx[1].busy) { error(12);return; }
        memcpy(hid.status.peer,data,6);++hid.status.generation;
        hid.listening=op==HID_LISTEN || op==HID_PAIR;
        if (hid.listening) {
            if (!access_set(op==HID_PAIR ? 3 : 2)) return;
            hid.status.pairing=op==HID_PAIR;hid.pair_started=stock_ticks();
            hid.pair_duration=value;hid.pair_epoch=epoch;
            hid.active=1;hid.status.state=4;hid.status.error=0;return;
        }
        hid.active=hid.outgoing=1;hid.status.state=1;hid.started=stock_ticks();
        unsigned rc=stock_cmgr_connect(hid.manager,hid.status.peer);
        if (rc!=0 && rc!=2) { error(0x500+rc);disconnect(); }
        else if (!rc) connect_channel(0);
        return;
    }
    if (op==HID_DISCONNECT) { hid.listening=0;if (hid.status.enabled==1) disconnect();return; }
    if (generation!=hid.status.generation || hid.status.state!=2 || hid.phase || hid.tx[1].busy) {
        error(13);return;
    }
    memset(hid.report,0,sizeof hid.report);hid.report[0]=0xa1;
    if (op==HID_KEY && value>0 && value<=255) {
        hid.report[1]=1;hid.report[2]=data[0];hid.report[4]=value;
    } else if (op==HID_CONSUMER && value>0 && value<=1023) {
        hid.report[1]=2;hid.report[2]=value;hid.report[3]=value>>8;
    } else { error(14);return; }
    hid.started=stock_ticks();hid.epoch=epoch;
    if (send(1,hid.report,hid.report[1]==1 ? 10 : 4)) {
        hid.phase=1;hid.down=1;hid.status.busy=1;++hid.status.sent;
    } else { error(15);disconnect(); }
}
void runtime_hid_status(struct hid_status *s) {
    *s=hid.status;
    if (runtime_hogp_enabled()) {
        runtime_hogp_status(s);s->hidden_services=hid.status.hidden_services;
        s->blocked_psms=hid.status.blocked_psms;s->audio_channels=hid.status.audio_channels;
    }
}
unsigned runtime_hid_preserve_link(unsigned caller) {
    /* These pinned speaker/UAC policy callers disconnect the entire ACL even
     * when only HID uses it. Explicit HID OFF and stock power-off use other
     * paths. Keyboard-only policy also applies while awaiting the saved TV. */
    return (caller==0x170a8 || caller==0x1759c) && hid.status.enabled==1 &&
        hid.status.keyboard_only && hid.context==stock_bt_context && hid.core==stock_bt_core;
}
