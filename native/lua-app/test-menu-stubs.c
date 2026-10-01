static unsigned char menu_context[0x990];
unsigned char *volatile stock_menu_context;
const struct menu_node stock_music_menu[]={
    {4,0,{0},NULL},{6,0,{0},NULL},{7,0,{0},NULL},{29,2,{0},NULL}
};
const struct menu_node stock_game_menu[]={{14,0,{0},NULL},{29,1,{0},NULL}};
unsigned char stock_usb_descriptor[18]={[10]=0x1e,[11]=0x17};
static unsigned usb_disconnects,usb_initializations,keyboard_light,menu_stock_id;
void stock_usb_disconnect(void) { ++usb_disconnects; }
void stock_usb_init(void) { ++usb_initializations; }
void stock_usb_mode_original(unsigned mode,unsigned vid,unsigned pid) {
    assert(vid==0x8888 && (mode==2 || mode==7 || mode==8));
    stock_usb_descriptor[10]=pid;stock_usb_descriptor[11]=pid>>8;
}
void stock_usb_mode(unsigned mode,unsigned vid,unsigned pid) { runtime_usb_select(mode,vid,pid); }
void stock_source_original(unsigned n) { stock_set_source(n); }
void stock_keyboard_light(unsigned n) { keyboard_light=n; }
static void fake_menu_open(void) {
    memset(menu_context,0,sizeof menu_context);stock_menu_context=menu_context;
    const struct menu_node *table=runtime_menu_root;memcpy(menu_context+0x20,&table,sizeof table);
}
void stock_menu_close(unsigned n) { (void)n;stock_menu_context=NULL; }
unsigned stock_menu_dispatch(unsigned id) { menu_stock_id=id;stock_menu_close(0);return 1; }
void stock_menu_reset(void) { menu_context[1]=0; }
unsigned stock_menu_action(unsigned action) {
    assert(stock_menu_context);
    const struct menu_node *table;memcpy(&table,menu_context+0x20,sizeof table);
    unsigned n=0;while(table[n].id!=29) ++n;
    if(action<2) menu_context[1]=(menu_context[1]+n+(action ? -1 : 1))%n;
    else if(action==3) {
        table=runtime_menu_root;memcpy(menu_context+0x20,&table,sizeof table);stock_menu_reset();
    } else if(action==2) {
        const struct menu_node *children=table[menu_context[1]].children;
        if(!children) return 1;
        memcpy(menu_context+0x20,&children,sizeof children);stock_menu_reset();
    }
    return 0;
}
static void fake_menu_timer(void) {
    if(stock_menu_context && menu_context[7]==1) { menu_context[7]=3;runtime_menu_select(menu_id()); }
}
