/* BLE HID over the pinned stock BTstack ABI. The Bluetooth task owns all
 * mutations and packets; snapshots and the bounded bond cache are read by Lua. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "bluetooth-hogp.h"
#include "bluetooth-trace.h"
#include "bluetooth-hogp-db.h"
typedef uint16_t (*read_fn)(uint16_t,uint16_t,uint16_t,unsigned char *,uint16_t);
typedef int (*write_fn)(uint16_t,uint16_t,uint16_t,uint16_t,unsigned char *,uint16_t);
extern void stock_att_init(const unsigned char *,read_fn,write_fn);
extern void stock_att_set_db(const unsigned char *);
extern uint16_t stock_att_request(void *,unsigned char *,uint16_t,unsigned char *);
extern void stock_ble_event(unsigned,unsigned,unsigned char *,unsigned);
extern void *stock_alloc(unsigned);
extern unsigned stock_ticks(void),stock_free_heap(void);
extern unsigned char *stock_hci_connection(unsigned);
extern void stock_hci_run(void);
extern unsigned stock_le_key_size(unsigned);
extern int stock_le_device_index(unsigned);
extern void stock_le_device_info(unsigned,int *,unsigned char *,unsigned char *);
extern void stock_le_delete_bond(unsigned,const unsigned char *);
extern void stock_sm_confirm(unsigned);
extern unsigned stock_att_notify(unsigned,unsigned,const void *,unsigned);
extern void stock_le_adv_data(unsigned,const unsigned char *);
extern void stock_le_scan_data(unsigned,const unsigned char *);
extern void stock_le_adv_enable(unsigned);
extern void stock_le_adv_params(unsigned,unsigned,unsigned,unsigned,const unsigned char *,unsigned,unsigned);
extern unsigned char *volatile stock_hci_stack;
extern volatile unsigned stock_battery_level;
static const unsigned char *original_db;
static read_fn original_read;
static write_fn original_write;
static unsigned initialized;
static uint16_t control_handle=0xffff;
static struct hogp {
    struct hid_status status;
    unsigned epoch,started,pair_at,pair_duration,pair_epoch,listen,phase,report,release_at;
    uint16_t handle;
    uint8_t cccd[2],down[2],suspended,scan_length;
    const unsigned char *scan_data;
    struct { uint8_t address[6],type,valid; } bonds[16];
} *ble;
static const unsigned char remote_adv[]={2,1,6,3,3,0x12,0x18,3,0x19,0x80,1};
static const unsigned char control_adv[]={2,1,6,3,3,0,0xab};
static const unsigned char remote_name[]={17,9,'D','i','t','o','o',' ','B','L','E',' ','R','e','m','o','t','e'};
static unsigned word(const unsigned char *p) { return p[0]|(p[1]<<8); }
static unsigned age(unsigned t) { return stock_ticks()-t; }
static void error(unsigned code) {
    ble->status.error=code;++ble->status.errors;
    runtime_bt_trace(5,1,&code,sizeof code);
}
static void reverse(unsigned char *d,const unsigned char *s) {
    for (unsigned i=0;i<6;++i) d[i]=s[5-i];
}
static void bonds(void) {
    for (unsigned i=0;i<16;++i) {
        int type=-1;unsigned char address[6]={0};
        stock_le_device_info(i,&type,address,NULL);
        ble->bonds[i].valid=type==0 || type==1;ble->bonds[i].type=type;
        reverse(ble->bonds[i].address,address);
    }
}
unsigned runtime_hogp_bond(unsigned i,unsigned char *a,unsigned *type) {
    if (!ble || i>=16 || !ble->bonds[i].valid) return 0;
    memcpy(a,ble->bonds[i].address,6);*type=ble->bonds[i].type;return 1;
}
static unsigned peer(unsigned handle,unsigned char *a,unsigned *type) {
    unsigned char *c=stock_hci_stack ? stock_hci_connection(handle) : NULL;
    if (!c) return 0;
    unsigned t;memcpy(&t,c+12,4);if (t>1) return 0;
    int index=stock_le_device_index(handle);
    if (index>=0 && index<16 && ble->bonds[index].valid) {
        memcpy(a,ble->bonds[index].address,6);*type=ble->bonds[index].type;
    } else { reverse(a,c+4);*type=t; }
    return 1;
}
static unsigned pairing(void) {
    return ble && ble->status.pairing && age(ble->pair_at)<ble->pair_duration;
}
static unsigned allowed(unsigned handle,unsigned adopt) {
    if (!ble || !ble->status.enabled || !ble->listen ||
            (ble->handle!=0xffff && ble->handle!=handle)) return 0;
    if (ble->handle==handle) return 1;
    unsigned char a[6];unsigned type;
    if (!peer(handle,a,&type)) return 0;
    unsigned any=0;for (unsigned i=0;i<6;++i) any|=ble->status.peer[i];
    if (any) {
        if (memcmp(a,ble->status.peer,6) || type!=ble->status.address_type) return 0;
    } else if (!pairing()) return 0;
    if (adopt && ble->handle==0xffff) {
        memcpy(ble->status.peer,a,6);ble->status.address_type=type;
        ble->handle=handle;ble->status.state=1;++ble->status.generation;
        ++ble->status.incoming;ble->started=stock_ticks();
    }
    return 1;
}
static void disconnect_handle(unsigned handle) {
    /* gap_disconnect was removed by the stock linker. Its exact operation is
     * SEND_DISCONNECT (8) then hci_run; the guarded stock dispatcher emits HCI
     * Disconnect and retains the connection until Disconnection Complete. */
    unsigned char *c=stock_hci_stack ? stock_hci_connection(handle) : NULL;unsigned state;
    if (!c) return;
    memcpy(&state,c+20,4);
    if (state>=8) return;
    state=8;memcpy(c+20,&state,4);stock_hci_run();
}
static void disconnect(void) {
    ble->phase=0;ble->status.busy=0;ble->status.pairing=0;
    ble->status.pair_remaining_ms=0;ble->listen=0;
    if (ble->handle!=0xffff) {
        ble->status.state=3;disconnect_handle(ble->handle);
    } else ble->status.state=0;
    ++ble->status.generation;
}
static uint16_t copy(const void *value,unsigned n,unsigned offset,unsigned char *out,unsigned capacity) {
    if (!out) return n;
    if (offset>=n) return 0;
    n-=offset;if (n>capacity) n=capacity;
    memcpy(out,(const unsigned char *)value+offset,n);return n;
}
static uint16_t read_value(uint16_t connection,uint16_t attribute,uint16_t offset,unsigned char *out,uint16_t capacity) {
    if (attribute<0x20) {
        if (attribute==3 && runtime_hogp_enabled()) return copy(remote_name+2,sizeof remote_name-2,offset,out,capacity);
        if (attribute==5 && runtime_hogp_enabled()) { const unsigned char a[]={0x80,1};return copy(a,2,offset,out,capacity); }
        return original_read(connection,attribute,offset,out,capacity);
    }
    unsigned char value[2]={0};unsigned n=1;
    if (attribute==0x32) value[0]=stock_battery_level>=7 ? 100 : stock_battery_level*100/7;
    else if (attribute==0x28 || attribute==0x2c) {
        if (ble && connection==ble->handle) value[0]=ble->down[attribute==0x2c];
    } else if (attribute==0x29 || attribute==0x2d) {
        n=2;if (ble && connection==ble->handle) value[0]=ble->cccd[attribute==0x2d];
    } else return 0;
    return copy(value,n,offset,out,capacity);
}
static int write_value(uint16_t connection,uint16_t attribute,uint16_t transaction,uint16_t offset,unsigned char *value,uint16_t n) {
    if (attribute<0x20) {
        /* The stock reply wrapper uses one global handle. Bind it to the
         * actual command writer, not the most recently connected HID host. */
        if (attribute==0x0d && !transaction && !offset) control_handle=connection;
        return original_write(connection,attribute,transaction,offset,value,n);
    }
    if (transaction) return 6;
    if (offset) return 7;
    if (attribute!=0x29 && attribute!=0x2d && attribute!=0x26) return 3;
    if (n!=(attribute==0x26 ? 1 : 2)) return 13;
    if (value[0]>1 || (n==2 && value[1])) return 19;
    if (!allowed(connection,0)) return 8;
    if (stock_le_key_size(connection)<7) return 15;
    if (!allowed(connection,1)) return 8;
    if (attribute==0x26) {
        ble->suspended=!value[0];
        if (ble->phase==1) { ble->phase=0;ble->status.busy=0;ble->down[ble->report]=0; }
        else if (ble->phase) ble->phase=3;
    } else {
        ble->cccd[attribute==0x2d]=value[0];
        if (!value[0] && ble->phase) disconnect();
    }
    return 0;
}
void runtime_hogp_init(const unsigned char *db,read_fn read,write_fn write) {
    original_db=db;control_handle=0xffff;original_read=read;original_write=write;
    if (ble) { memset(ble,0,sizeof *ble);ble->handle=0xffff; }
    initialized=1;stock_att_init(db,read_value,write_value);
}
void runtime_hogp_event(unsigned type,unsigned channel,unsigned char *p,unsigned n) {
    if (type==4 && n>=6 && p[0]==5 && word(p+3)==control_handle) control_handle=0xffff;
    if (type==4 && n>=2 && p[1]+2U<=n && ble && ble->status.enabled) {
        if ((p[0]==0xc8 || p[0]==0xcc) && n>=4) {
            unsigned handle=word(p+2);
            /* Re-encryption of a bond needs no confirmation. New pairing is
             * allowed only in the explicit, time-limited Lua pairing window. */
            if (p[0]==0xc8 && pairing() && allowed(handle,1)) stock_sm_confirm(handle);
            else disconnect_handle(handle);
            return;
        }
        if (p[0]>=0xc8 && p[0]<=0xd7) {
            /* Log event/status only, never SMP key or identity material. Do
             * not use the stock callback that erases bonds on PIN missing. */
            unsigned detail=n>11 ? p[11] : 0;runtime_bt_trace(5,p[0],&detail,4);
            bonds();return;
        }
        if (p[0]==5 && n>=6 && word(p+3)==ble->handle) {
            ++ble->status.closed;ble->status.close_status=p[5];
            ble->handle=0xffff;ble->phase=0;ble->status.busy=0;
            ble->cccd[0]=ble->cccd[1]=ble->down[0]=ble->down[1]=0;
            ble->suspended=0;ble->status.encryption_state=0;
            ble->status.state=ble->listen ? 4 : 0;++ble->status.generation;
            if (ble->listen) stock_le_adv_enable(1);
        }
    }
    stock_ble_event(type,channel,p,n);
}
unsigned runtime_hogp_enabled(void) { return ble && ble->status.enabled; }
unsigned runtime_hogp_mode(unsigned enabled) {
    if (!initialized || !stock_hci_stack) return 0;
    if (!ble) {
        if (!enabled) return 1;
        if (stock_free_heap()<16384 || !(ble=stock_alloc(sizeof *ble))) return 0;
        memset(ble,0,sizeof *ble);ble->handle=0xffff;
    }
    if (!!enabled==!!ble->status.enabled) return 1;
    if (ble->handle!=0xffff) { error(20);return 0; }
    ble->status.enabled=enabled;ble->status.transport=enabled;
    ble->status.keyboard_only=enabled;ble->status.state=0;ble->listen=0;
    ble->status.pairing=ble->status.pair_remaining_ms=0;
    if (enabled) {
        bonds();
        if (stock_hci_stack) {
            memcpy(&ble->scan_data,stock_hci_stack+0x4e0,sizeof ble->scan_data);
            ble->scan_length=stock_hci_stack[0x4e4];
        }
        stock_le_adv_data(sizeof remote_adv,remote_adv);
        stock_le_scan_data(sizeof remote_name,remote_name);
    } else {
        stock_le_adv_data(sizeof control_adv,control_adv);
        if (ble->scan_data && ble->scan_length<=31) stock_le_scan_data(ble->scan_length,ble->scan_data);
    }
    return 1;
}
unsigned runtime_hogp_key(unsigned op,unsigned value,unsigned modifiers) {
    static const unsigned char keys[]={40,41,44,79,80,81,82};
    static const unsigned char media[]={0xcd,0xe2,0xe9,0xea,0xb5,0xb6,0xb7};
    if (modifiers || (op!=HID_KEY && op!=HID_CONSUMER)) return 0;
    const unsigned char *map=op==HID_KEY ? keys : media;
    for (unsigned i=0;i<7;++i) if (value==map[i]) return 1U<<i;
    return 0;
}
void runtime_hogp_command(unsigned op,unsigned value,const unsigned char *data,unsigned epoch,unsigned generation) {
    if (!runtime_hogp_enabled()) return;
    if (op==HID_DISCONNECT) { disconnect();return; }
    if (op==HID_FORGET) {
        if (ble->status.state) { error(17);return; }
        for (unsigned i=0;i<16;++i) if (ble->bonds[i].valid && ble->bonds[i].type==data[6] &&
                !memcmp(ble->bonds[i].address,data,6)) {
            unsigned char a[6];reverse(a,data);stock_le_delete_bond(data[6],a);
            bonds();++ble->status.forgotten;ble->status.error=0;break;
        }
        return;
    }
    if (op==HID_CONNECT || op==HID_LISTEN || op==HID_PAIR) {
        if (ble->handle!=0xffff && !memcmp(ble->status.peer,data,6) && ble->status.address_type==data[6]) return;
        if (ble->handle!=0xffff || data[6]>1) { error(12);return; }
        memcpy(ble->status.peer,data,6);ble->status.address_type=data[6];
        ble->listen=1;ble->status.state=4;ble->status.error=0;++ble->status.generation;
        ble->status.pairing=op==HID_PAIR;
        ble->pair_at=stock_ticks();ble->pair_duration=value;ble->pair_epoch=epoch;
        /* A BLE remote is a peripheral: the host initiates the connection.
         * Undirected connectable advertising also keeps control recoverable. */
        const unsigned char zero[6]={0};stock_le_adv_params(48,80,0,0,zero,7,0);
        stock_le_adv_enable(1);return;
    }
    unsigned bit=runtime_hogp_key(op,value,data ? data[0] : 0);
    if (generation!=ble->status.generation || ble->status.state!=2 || ble->phase || ble->suspended) { error(13);return; }
    if (!bit) { error(14);return; }
    unsigned report=op==HID_CONSUMER;
    ble->down[report]=bit;ble->report=report;ble->phase=1;ble->epoch=epoch;
    ble->release_at=stock_ticks();ble->status.busy=1;
}
void runtime_hogp_service(unsigned epoch) {
    if (!runtime_hogp_enabled()) return;
    if (!stock_hci_stack) {
        memset(ble,0,sizeof *ble);ble->handle=0xffff;return;
    }
    if (ble->status.pairing) {
        unsigned ms=age(ble->pair_at);
        if (ms>=ble->pair_duration || epoch!=ble->pair_epoch) {
            ble->status.pairing=ble->status.pair_remaining_ms=0;
        } else ble->status.pair_remaining_ms=ble->pair_duration-ms;
    }
    if (ble->handle!=0xffff && ble->status.state!=3) {
        unsigned encrypted=stock_le_key_size(ble->handle)>=7;
        ble->status.encryption_state=encrypted ? 2 : 0;
        if (encrypted && ble->cccd[0] && ble->cccd[1]) {
            if (ble->status.state!=2) {
                ++ble->status.opened;bonds();
                unsigned char a[6];unsigned type;
                if (peer(ble->handle,a,&type)) {
                    memcpy(ble->status.peer,a,6);ble->status.address_type=type;
                }
            }
            ble->status.state=2;ble->status.pairing=ble->status.pair_remaining_ms=0;
        } else if (ble->status.state==2 || age(ble->started)>30000) {
            error(21);disconnect();return;
        }
        if (ble->phase && epoch!=ble->epoch) {
            if (ble->phase==1) { ble->phase=0;ble->status.busy=0;ble->down[ble->report]=0; }
            else ble->phase=3;
        }
        if (ble->phase==1) {
            unsigned rc=stock_att_notify(ble->handle,ble->report ? 0x2c : 0x28,&ble->down[ble->report],1);
            if (!rc) { ble->phase=2;ble->release_at=stock_ticks();++ble->status.sent; }
            else if (age(ble->release_at)>=1000) {
                error(0x700+rc);ble->phase=0;ble->status.busy=0;ble->down[ble->report]=0;
            }
        }
        if (ble->phase==2 && age(ble->release_at)>=80) ble->phase=3;
        if (ble->phase==3) {
            const unsigned char zero=0;
            if (!stock_att_notify(ble->handle,ble->report ? 0x2c : 0x28,&zero,1)) {
                ble->down[ble->report]=0;ble->phase=0;ble->status.busy=0;++ble->status.released;
            } else if (age(ble->release_at)>=1000) { error(10);disconnect(); }
        }
    }
}
void runtime_hogp_status(struct hid_status *s) {
    if (ble) *s=ble->status;else memset(s,0,sizeof *s);
}

unsigned runtime_hogp_control_notify(unsigned connection,unsigned attribute,const void *data,unsigned n) {
    if (!stock_hci_stack) return 2;
    if (control_handle!=0xffff && stock_hci_connection(control_handle)) connection=control_handle;
    return stock_att_notify(connection,attribute,data,n);
}

/* BlueZ opens HOGP immediately on discovery and requests encryption even for
 * a control-only client. Expose HID only to the selected bonded host or a
 * pairing candidate. Every connection retains the original control handles.
 * ATT executes synchronously on one Bluetooth task; restore the default view
 * before returning, including after errors and Write Without Response. */
uint16_t runtime_hogp_request(void *connection,unsigned char *request,uint16_t size,unsigned char *response) {
    unsigned handle=word(connection),visible=0;
    if (stock_hci_stack && allowed(handle,0)) {
        visible=pairing() || (ble->handle==handle && stock_le_key_size(handle)>=7);
        for (unsigned i=0;!visible && i<16;++i)
            visible=ble->bonds[i].valid && ble->bonds[i].type==ble->status.address_type &&
                !memcmp(ble->bonds[i].address,ble->status.peer,6);
    }
    stock_att_set_db(visible ? hogp_database : original_db);
    uint16_t n=stock_att_request(connection,request,size,response);
    stock_att_set_db(original_db);return n;
}
