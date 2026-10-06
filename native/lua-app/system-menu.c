/* Included by runtime.c. Native navigation and the original menu remain in
 * charge; this adds leaves whose actions run on the stock main task. No VM,
 * uploaded script, or extra framebuffer is needed to launch a saved app. */
struct menu_node { uint16_t id,image; unsigned char style[8]; const struct menu_node *children; };
extern const struct menu_node stock_music_menu[],stock_game_menu[];
extern unsigned char *volatile stock_menu_context;
extern void stock_menu_close(unsigned);
extern unsigned stock_menu_dispatch(unsigned);
extern void stock_menu_reset(void);
extern unsigned stock_menu_action(unsigned);
extern void stock_usb_mode(unsigned,unsigned,unsigned),stock_usb_init(void),stock_usb_disconnect(void);
extern void stock_usb_mode_original(unsigned,unsigned,unsigned),stock_source_original(unsigned);
extern unsigned char stock_usb_descriptor[];
extern void stock_keyboard_light(unsigned);

#define NODE(id,image,child) {id,image,{0,3,2,0,4,10,10,0},child}
static const struct menu_node tools_menu[]={
    {8,0x154,{0,3,2,1,4,10,10,0},NULL}, {9,0x157,{0,3,3,1,4,10,10,0},NULL},
    {10,0x156,{0,3,4,1,4,10,10,0},NULL}, {11,0x155,{0,3,3,1,4,10,10,0},NULL},
    NODE(32,0x154,NULL), {29,0,{0},NULL}
};
const struct menu_node runtime_menu_root[]={
    {0,0x13b,{0,3,1,0,4,10,10,0},stock_music_menu},
    NODE(12,0x13c,NULL),NODE(1,0x13d,tools_menu),
    {3,0x139,{0,3,4,0,4,10,10,0},NULL},{2,0x13a,{0,3,5,0,4,10,10,0},stock_game_menu},
    NODE(30,0x13c,NULL),NODE(31,0x13d,NULL),{29,0,{0},NULL}
};
#undef NODE

enum { MENU_NATIVE, MENU_SETTINGS, MENU_AUTOSTART, MENU_REMOVE, MENU_BT,
       MENU_DEVICES, MENU_LIGHTS, MENU_INDICATOR, MENU_USB, MENU_MEMO, MENU_DELETE,
       MENU_FORGET };
static struct {
    volatile unsigned input,request,injected;
    unsigned page,index,parent,changed,rendered,notice_at,operation,operation_at;
    unsigned usb_at,usb_stage,bt_at;
    const char *notice;
    unsigned char peer[7],bonds[16][7],bond_count;
} menu;

static unsigned menu_id(void) {
    unsigned char *c=stock_menu_context;
    if(!c || c[1]>=15) return 29;
    const struct menu_node *table;memcpy(&table,c+0x20,sizeof table);
    return table ? table[c[1]].id : 29;
}
static unsigned menu_visible(void) {
    return stock_menu_context && (menu.page || menu_id()>=30);
}
static void menu_notice(const char *s) { menu.notice=s;menu.notice_at=stock_ticks();menu.changed=stock_ticks(); }
static void menu_page(unsigned page,unsigned index) {
    menu.page=page;menu.index=index;menu.changed=stock_ticks();menu.notice=NULL;
    menu.input=menu.injected=0;
}
static void menu_open(void) {
    unsigned char event[12]={0,0x11,22};stock_event(event);
}
static void menu_root(void) {
    menu_page(0,0);
    if(stock_menu_context) stock_menu_close(0);
    menu_open();
}
static unsigned menu_count(void) {
    if(menu.page==MENU_SETTINGS) return 7;
    if(menu.page==MENU_MEMO || menu.page==MENU_LIGHTS || menu.page==MENU_BT) return 3;
    if(menu.page==MENU_DEVICES) return menu.bond_count ? menu.bond_count : 1;
    return 2;
}
static unsigned menu_key(unsigned key,unsigned event) {
    if(!menu.page || !stock_menu_context) return 0;
    if(event==2 && !menu.input) menu.input=key+1;
    return 1;
}
static void menu_request(unsigned request) {
    saved.boot_done=1;
    menu.request=request;
    if(app.state==RUNNING || app.state==ACTIVE || app.state==PAUSED || app.state==SAVING) app.cancel=1;
}
static int app_menu(lua_State *L) { (void)L;menu_request(1);return 0; }

static void system_load(void) {
    unsigned bank=0,foreign=0;struct saved_record *r=persist_latest(3,&bank,&foreign);
    if(r && r->length==sizeof preferences && r->data[0]==1 &&
            r->data[1]<=1 && r->data[2]<=2 && r->data[3]<=2 && r->data[4]<=1 && r->data[5]<=1 &&
            !r->data[6] && !r->data[7]) memcpy(&preferences,r->data,sizeof preferences);
    stock_free(r);
    if(preferences.lights==1) stock_keyboard_light(1);
    led_refresh();peripheral.indicator_dirty=1;
    menu.usb_stage=1;menu.usb_at=stock_ticks();
}
static const char *system_save(const void *data) {
    if(!saved.initialized) return "STORAGE NOT READY";
    if(!memcmp(data,&preferences,sizeof preferences)) return NULL;
    if(peripheral.writes>=64 || (peripheral.writes && stock_ticks()-peripheral.last_write<1000))
        return "WAIT THEN RETRY";
    unsigned changed=0;const char *error=persist_save(3,data,sizeof preferences,&changed);
    if(changed) { ++peripheral.writes;peripheral.last_write=stock_ticks(); }
    return error;
}
static void menu_setting(void) {
    struct system_preferences next=preferences;next.version=1;
    unsigned p=menu.page,n=menu.index;
    if(p==MENU_AUTOSTART) next.autostart_off=n;
    if(p==MENU_BT) next.bt_mode=n==2 ? 0 : n+1;
    if(p==MENU_LIGHTS) next.lights=n;
    if(p==MENU_INDICATOR) next.indicator_off=n;
    if(p==MENU_USB) next.usb_noaudio=n;
    const char *error=system_save(&next);
    if(error) { menu_notice(error);return; }
    preferences=next;led_refresh();peripheral.indicator_dirty=1;
    if(p==MENU_LIGHTS && n==1) stock_keyboard_light(1);
    if(p==MENU_USB) {
        if(n && stock_get_source()==7) stock_set_source(0);
        if(!n && stock_usb_power) stock_set_source(7);
        menu.usb_stage=1;menu.usb_at=stock_ticks();
    }
    menu_notice("SAVED");
}
static void menu_bonds(void) {
    menu.bond_count=0;
    struct hid_status s;runtime_hid_status(&s);
    if(s.transport) {
        for(unsigned i=0;i<16;++i) {
            unsigned type;
            if(runtime_hogp_bond(i,menu.bonds[menu.bond_count],&type))
                menu.bonds[menu.bond_count++][6]=type;
        }
    } else for(unsigned i=0;i<8;++i) if(stock_bt_manager[7+i*26+25]) {
        for(unsigned j=0;j<6;++j) menu.bonds[menu.bond_count][j]=stock_bt_manager[7+i*26+j];
        menu.bonds[menu.bond_count][6]=0;
        ++menu.bond_count;
    }
    menu_page(MENU_DEVICES,0);
}
static unsigned menu_job(unsigned op,unsigned slot,const unsigned char *data) {
    if(peripheral.cleanup || peripheral.state==JOB_QUEUED || peripheral.state==JOB_RUNNING) return 0;
    peripheral.op=op;peripheral.slot=slot;peripheral.value=0;
    memset(peripheral.data,0,sizeof peripheral.data);
    if(data) memcpy(peripheral.data,data,7);
    peripheral.job_epoch=peripheral.epoch;
    peripheral.state=JOB_QUEUED;menu.operation=op;menu.operation_at=stock_ticks();return 1;
}
static void menu_confirm(void) {
    if(!menu.index) {
        if(menu.page==MENU_FORGET) menu_bonds();
        else menu_page(menu.page==MENU_DELETE ? MENU_MEMO : MENU_SETTINGS,menu.parent);
        return;
    }
    if(menu.page==MENU_REMOVE) {
        if(peripheral.writes>=64 || (peripheral.writes && stock_ticks()-peripheral.last_write<1000)) {
            menu_notice("WAIT THEN RETRY");return;
        }
        unsigned changed=0;const char *error=persist_save(1,NULL,0,&changed);
        if(changed) { ++peripheral.writes;peripheral.last_write=stock_ticks(); }
        menu_page(MENU_SETTINGS,menu.parent);menu_notice(error ? "REMOVE FAILED" : "REMOVED");
    } else if(menu.page==MENU_DELETE) {
        if(menu_job(MEMO_DELETE,0,NULL)) menu_notice("DELETING");
    } else if(menu.page==MENU_FORGET) {
        struct hid_status s;runtime_hid_status(&s);
        if(s.state || stock_avrcp_state() || stock_a2dp_state()) {
            /* Disconnecting an unrelated peer is not part of forgetting this bond. */
            if((s.state && (memcmp(s.peer,menu.peer,6) || (s.transport && s.address_type!=menu.peer[6]))) ||
                    stock_avrcp_state() || stock_a2dp_state()) {
                menu_notice("DISCONNECT AUDIO FIRST");return;
            }
            if(menu_job(BT_HID,HID_DISCONNECT,NULL)) menu_notice("DISCONNECTING");
        } else if(menu_job(BT_HID,HID_FORGET,menu.peer)) menu_notice("FORGETTING");
    }
}
static void menu_select(void) {
    if(menu.operation) return;
    if(menu.page==MENU_SETTINGS) {
        static const unsigned char pages[]={MENU_AUTOSTART,MENU_REMOVE,MENU_BT,MENU_DEVICES,MENU_LIGHTS,MENU_INDICATOR,MENU_USB};
        unsigned p=pages[menu.index],index=0;menu.parent=menu.index;
        if(p==MENU_DEVICES) { menu_bonds();return; }
        if(p==MENU_AUTOSTART) index=preferences.autostart_off;
        if(p==MENU_BT) index=preferences.bt_mode ? preferences.bt_mode-1 : 2;
        if(p==MENU_LIGHTS) index=preferences.lights;
        if(p==MENU_INDICATOR) index=preferences.indicator_off;
        if(p==MENU_USB) index=preferences.usb_noaudio;
        menu_page(p,index);
    } else if(menu.page==MENU_DEVICES) {
        if(!menu.bond_count) return;
        memcpy(menu.peer,menu.bonds[menu.index],7);menu_page(MENU_FORGET,0);
    } else if(menu.page==MENU_REMOVE || menu.page==MENU_DELETE || menu.page==MENU_FORGET) menu_confirm();
    else if(menu.page==MENU_MEMO) {
        if(menu.index==2) { menu.parent=2;menu_page(MENU_DELETE,0); }
        else if(menu_job(menu.index ? (stock_sound_playing() ? AUDIO_STOP : MEMO_PLAY) :
                (peripheral.recording ? MEMO_STOP : MEMO_START),0,NULL)) menu_notice("WAIT");
    } else menu_setting();
}
static void menu_back(void) {
    if(menu.operation) return;
    if(menu.page==MENU_SETTINGS || menu.page==MENU_MEMO) {
        stop_recording();if(peripheral.preview_owned) { stock_sound_stop();peripheral.preview_owned=0; }
        menu_root();
    } else if(menu.page==MENU_DELETE) menu_page(MENU_MEMO,2);
    else if(menu.page==MENU_FORGET) menu_bonds();
    else if(menu.page==MENU_USB && menu.parent==7) menu_root();
    else menu_page(MENU_SETTINGS,menu.parent);
}

unsigned runtime_menu_select(unsigned id) {
    if(id==7 && preferences.usb_noaudio) {
        menu.parent=7;menu_page(MENU_USB,1);menu_notice("USB AUDIO OFF");
    } else if(id==30) menu_request(2);
    else if(id==31) menu_page(MENU_SETTINGS,0);
    else if(id==32) menu_page(MENU_MEMO,0);
    else return stock_menu_dispatch(id);
    return 1;
}

/* Called from the existing descriptor selector; mode 8 is the native storage
 * mode and is deliberately left intact. Boot starts control-only until the
 * configuration journal has been read on the main task. */
unsigned runtime_usb_mode(unsigned mode) {
    return mode==2 && (!saved.initialized || preferences.usb_noaudio) ? 7 : mode;
}
void runtime_usb_select(unsigned mode,unsigned vid,unsigned pid) {
    unsigned selected=runtime_usb_mode(mode);
    stock_usb_mode_original(selected,vid,selected==mode ? pid : 0x171e);
}
void runtime_source_select(unsigned source) {
    if(source!=7 || !preferences.usb_noaudio) stock_source_original(source);
}
static void system_usb_service(void) {
    if(!menu.usb_stage || !saved.initialized || stock_ticks()-menu.usb_at<500) return;
    unsigned wanted=stock_get_source()==7 && !preferences.usb_noaudio ? 0x1719 : 0x171e;
    unsigned current=stock_usb_descriptor[10]|(unsigned)stock_usb_descriptor[11]<<8;
    if(!stock_usb_power || (menu.usb_stage==1 && (current==wanted || current==0x1723))) { menu.usb_stage=0;return; }
    if(menu.usb_stage==1) { stock_usb_disconnect();menu.usb_stage=2;menu.usb_at=stock_ticks();return; }
    stock_usb_mode(wanted==0x1719 ? 2 : 7,0x8888,wanted);stock_usb_init();menu.usb_stage=0;
}
static void system_bt_service(void) {
    if(!preferences.bt_mode || !stock_bt_context || stock_ticks()-menu.bt_at<1000) return;
    struct hid_status s;runtime_hid_status(&s);
    if(s.enabled && s.keyboard_only==(preferences.bt_mode==1)) return;
    unsigned values[8]={peripheral.epoch,s.generation,HID_MODE,preferences.bt_mode==1};
    stock_bt_enqueue(BT_HID_COMMAND,values,sizeof values);menu.bt_at=stock_ticks();
}

static void menu_text(const char *s,int y,unsigned color) {
    unsigned len=strlen(s),offset=0,age=stock_ticks()-menu.changed;
    if(len>4 && age>700) offset=((age-700)/140)%((len+4)*4);
    int x=len<4 ? (16-(int)len*4)/2 : -(int)offset;
    for(unsigned k=0;k<len;++k,x+=4) {
        unsigned ch=(unsigned char)s[k],bits=0;
        if(ch>='a' && ch<='z') ch-=32;
        if(ch>='0' && ch<='9') bits=font[ch-'0'];
        else if(ch>='A' && ch<='Z') bits=font[ch-'A'+10];
        else if(ch==':') bits=1040;
        for(unsigned j=0;j<5;++j) for(unsigned i=0;i<3;++i)
            if(bits&(1U<<(j*3+i))) pixel(x+i,y+j,color);
    }
}
static void menu_address(char *out,const unsigned char *a) {
    struct hid_status s;runtime_hid_status(&s);
    if(s.transport && runtime_hogp_name(a,a[6],out)) return;
    const char *hex="0123456789ABCDEF";
    for(unsigned i=0;i<6;++i) { unsigned v=a[5-i];out[i*3]=hex[v>>4];out[i*3+1]=hex[v&15];out[i*3+2]=':'; }out[17]=0;
}
static void menu_draw(void) {
    if(!menu_visible() || app.L || app.state==RUNNING || stock_ticks()-menu.rendered<80) return;
    menu.rendered=stock_ticks();
    const char *label="",*title="SET";char address[BT_NAME_BYTES+8];
    unsigned p=menu.page,n=menu.index,count=menu_count(),color=0x30b0ff;
    static const char *const settings[]={"AUTOSTART","REMOVE APP","BLUETOOTH","SAVED DEVICES","KEY LIGHTS","BATTERY INDICATOR","USB MODE"};
    if(!p) { unsigned id=menu_id();title=id==30 ? "LUA" : id==31 ? "SET" : "REC";
        label=id==30 ? "SAVED APP" : id==31 ? "SETTINGS" : "VOICE MEMO";count=0;
    } else if(p==MENU_SETTINGS) label=settings[n];
    else if(p==MENU_AUTOSTART) { title="AUTO";label=n ? "OFF" : "ON"; }
    else if(p==MENU_BT) { title="BT";label=n==2 ? "APP CONTROL" : n ? "SPEAKER AND REMOTE" : "REMOTE ONLY"; }
    else if(p==MENU_LIGHTS) { title="KEYS";label=n==0 ? "APP AND STOCK" : n==1 ? "STOCK LIGHTS" : "OFF"; }
    else if(p==MENU_INDICATOR) { title="BATT";label=n ? "OFF" : "AUTO"; }
    else if(p==MENU_USB) { title="USB";label=n ? "CHARGE AND CONTROL" : "AUDIO AND CONTROL"; }
    else if(p==MENU_MEMO) { title=peripheral.recording ? "REC" : "MEMO";
        label=n==0 ? (peripheral.recording ? "STOP RECORDING" : "RECORD") : n==1 ? (stock_sound_playing() ? "STOP PLAYBACK" : "PLAY") : "DELETE";
        if(peripheral.recording) color=0xff3020;
    } else if(p==MENU_DEVICES) { title="BT";
        if(menu.bond_count) { menu_address(address,menu.bonds[n]);label=address; } else label="NO SAVED DEVICES";
    } else { title=n ? "YES" : "NO";color=0xffa030;
        label=p==MENU_REMOVE ? "REMOVE SAVED APP" : p==MENU_DELETE ? "DELETE MEMO" : "FORGET DEVICE";
        if(p==MENU_FORGET) { memcpy(address,"FORGET ",7);menu_address(address+7,menu.peer);label=address; }
    }
    if(menu.notice) label=menu.notice;
    if(!frame_acquire()) return;
    memset(app.frame,0,FRAME_BYTES);
    menu_text(title,1,color);menu_text(label,9,0xffffff);
    if(count) for(unsigned i=0;i<count;++i) pixel((16-(int)count*2)/2+i*2,15,i==n ? color : 0x102028);
    runtime_screen(app.frame);
}
static void menu_service(void) {
    if(native_priority()) { menu.input=menu.injected=0;return; }
    system_usb_service();system_bt_service();
    if(stock_menu_context && (app.state==UPLOADING || app.state==SAVING)) {
        stop_recording();menu_page(0,0);stock_menu_close(0);
    }
    if(menu.request && app.state!=RUNNING && app.state!=ACTIVE && app.state!=PAUSED && app.state!=SAVING) {
        unsigned request=menu.request;menu.request=0;
        if(app.state==UPLOADING) { source_free(app.source);app.source=NULL;app.state=DONE; }
        if(request==1) { stop_recording();menu_root(); }
        else {
            const char *error=runtime_load_saved();
            if(error) menu_notice(error);
            else { menu_page(0,0);if(stock_menu_context) stock_menu_close(0); }
        }
    }
    if(!stock_menu_context) {
        if(menu.page) { stop_recording();menu_page(0,0); }
        menu.input=0;return;
    }
    if(menu.injected) {
        unsigned key=menu.injected-1;menu.injected=0;
        if(runtime_adc_result((2U<<16)|key)!=0xff) {
            if(key==7) stock_menu_close(0);
            else {
                unsigned action=key==2 ? 1 : key==3 ? 0 : key==4 ? 2 : 3;
                unsigned result=stock_menu_action(action);
                if(stock_menu_context) stock_menu_context[7]=result==1 ? 1 : result==2 ? 2 : 0;
            }
        }
    }
    if(menu.page) {
        /* The stock leaf callback has already disabled its animation timer. */
        unsigned key=menu.input;menu.input=0;
        if(key && !menu.operation) {
            --key;
            if(key==0 || key==7) menu_back();
            else if(key==2 || key==3) { unsigned n=menu_count();menu.index=(menu.index+n+(key==2 ? -1 : 1))%n;menu.changed=stock_ticks();menu.notice=NULL; }
            else if(key==4) menu_select();
        }
    }
    if(menu.operation && menu.operation<100 && peripheral.state==JOB_DONE) {
        unsigned op=menu.operation;menu.operation=0;
        if(peripheral.error) menu_notice(peripheral.error);
        else if(op==BT_HID) {
            if(peripheral.slot==HID_DISCONNECT) { menu.operation=100;menu.operation_at=stock_ticks(); }
            else { menu.operation=101;menu.operation_at=stock_ticks(); }
        } else { if(op==MEMO_DELETE) menu_page(MENU_MEMO,2);menu_notice("OK"); }
    }
    if(menu.operation>=100) {
        struct hid_status s;runtime_hid_status(&s);
        if(stock_ticks()-menu.operation_at>5000) { menu.operation=0;menu_notice("WAIT THEN RETRY"); }
        else if(menu.operation==100 && !s.state) {
            menu.operation=0;if(!menu_job(BT_HID,HID_FORGET,menu.peer)) menu_notice("BUSY");
        } else if(menu.operation==101 && !keyboard_bonded(menu.peer,menu.peer[6]) && s.forgotten==peripheral.bt_forget_saved) {
            menu.operation=0;menu_bonds();menu_notice("FORGOTTEN");
        }
    }
    if(menu.notice && !menu.operation && stock_ticks()-menu.notice_at>2200) { menu.notice=NULL;menu.changed=stock_ticks(); }
    menu_draw();
}

/* Bounded menu diagnostics use the same navigation and action handlers as
 * physical input. Six read-only chunks expose the small RGB framebuffer. */
static void menu_command(unsigned context,const unsigned char *data,unsigned length) {
    unsigned char reply[160]={'D','M','N','U',1};
    unsigned action=length==12 ? data[7] : 255,key=length==12 ? data[8] : 0,chunk=length==12 ? data[9] : 0;
    if(action>2 || chunk>=6 || (action==2 && key!=0 && key!=2 && key!=3 && key!=4 && key!=7)) reply[5]=1;
    else if(action==1) menu_request(1);
    else if(action==2) {
        if(!stock_menu_context || menu.injected) reply[5]=2;
        else menu.injected=key+1;
    }
    reply[6]=menu.page;reply[7]=menu.index;reply[8]=menu_id();reply[9]=menu_count();
    reply[10]=stock_menu_context!=NULL;reply[11]=menu_visible();reply[12]=menu.operation;
    reply[13]=menu.request;reply[14]=app.state;reply[15]=saved.initialized;
    memcpy(reply+16,&preferences,sizeof preferences);
    reply[24]=stock_usb_descriptor[10];reply[25]=stock_usb_descriptor[11];reply[26]=chunk;
    unsigned irq=runtime_irq_save();
    if(chunk<6 && app.frame) memcpy(reply+32,app.frame+chunk*128,128);
    runtime_irq_restore(irq);
    stock_reply(context,0x37,reply,sizeof reply);
}
