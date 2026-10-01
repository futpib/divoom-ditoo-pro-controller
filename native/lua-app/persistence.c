/* Included by runtime.c. Only the stock main task enters the configuration
 * journal. Two fixed-size records retain the previous valid generation while
 * a replacement is written. No Lua-provided model, slot or address is used. */
extern unsigned char *volatile stock_config_context;
extern unsigned stock_config_find(unsigned, unsigned, void *, void *);
extern void *stock_config_read(unsigned, unsigned, uint16_t *);
extern void stock_config_write(unsigned, unsigned, const void *, unsigned);
extern unsigned stock_partition(unsigned, unsigned *);
extern unsigned stock_page_read(unsigned, void *, unsigned);

#define PERSIST_MODEL 0xd7
#define SETTINGS_LIMIT 128
struct saved_record { uint32_t magic, kind, sequence, length, crc, reserved; unsigned char data[]; };
static struct {
    unsigned initialized, boot_done, boot_skip, boot_started, clock_started;
    unsigned settings_size, pending_size;
    unsigned char settings[SETTINGS_LIMIT], pending[SETTINGS_LIMIT];
} saved;

static unsigned persist_capacity(unsigned kind) { return kind==1 ? SOURCE_LIMIT : SETTINGS_LIMIT; }
static unsigned persist_crc(const struct saved_record *r) {
    uint32_t crc=~0U;const unsigned char *p=(const unsigned char *)r;
    for (unsigned i=0;i<sizeof *r+r->length;++i) {
        unsigned v=i>=16 && i<20 ? 0 : p[i];crc^=v;
        for (unsigned j=0;j<8;++j) crc=(crc>>1)^(0xedb88320U & (0U-(crc&1)));
    }
    return ~crc;
}
static unsigned persist_layout(void) {
    unsigned char *c=stock_config_context;unsigned base, payload, partition=0, used;uint16_t journal;
    if (!c) return 0;
    memcpy(&base,c+12,4);memcpy(&payload,c+8,4);memcpy(&used,c+4,4);memcpy(&journal,c+2,2);
    stock_partition(5,&partition);
    return base==partition && base>=0x100 && base<=0xfb00 && payload==base+0x100 &&
        c[0]<=102 && journal<=256 && journal>=(c[0]>=52 ? 2U : c[0] ? 1U : 0U) &&
        used>=c[0] && used<=1024;
}
/* A malformed owned record is ignored on boot. Foreign records and unexpected
 * allocation sizes refuse writes instead of reusing an occupied namespace. */
static struct saved_record *persist_read(unsigned kind,unsigned bank,unsigned *foreign) {
    unsigned char entry[8]={0}, scratch[256];
    unsigned slot=(kind-1)*2+bank,bytes=sizeof(struct saved_record)+persist_capacity(kind);
    if (stock_config_find(PERSIST_MODEL,slot,entry,scratch)==0xffff) return NULL;
    if (entry[2]!=(bytes+4+255)/256) { *foreign=1;return NULL; }
    uint16_t page;unsigned base;memcpy(&page,entry+4,2);memcpy(&base,stock_config_context+8,4);
    if (page+entry[2]>1024 || stock_page_read(base+page,scratch,1)) { *foreign=1;return NULL; }
    unsigned stored=scratch[0]|((unsigned)scratch[1]<<8),magic;memcpy(&magic,scratch+2,4);
    if (stored!=bytes || magic!=0x41554c44) {
        /* An erased page after an interrupted journal append has no payload. */
        unsigned erased=1;for(unsigned i=0;i<256;++i) if(scratch[i]!=255) erased=0;
        if (!erased) *foreign=1;return NULL;
    }
    uint16_t n=0;struct saved_record *r=stock_config_read(PERSIST_MODEL,slot,&n);
    if (!r) return NULL;
    if (n<bytes || r->magic!=0x41554c44 || r->kind!=kind || r->reserved ||
        r->length>persist_capacity(kind) || r->crc!=persist_crc(r)) { stock_free(r);return NULL; }
    return r;
}
static struct saved_record *persist_latest(unsigned kind,unsigned *bank,unsigned *foreign) {
    struct saved_record *a=persist_read(kind,0,foreign),*b=persist_read(kind,1,foreign);
    if (b && (!a || (int32_t)(b->sequence-a->sequence)>0)) { stock_free(a);*bank=1;return b; }
    stock_free(b);*bank=0;return a;
}
/* App records consume source blocks; settings remain a contiguous byte string. */
static const char *persist_save(unsigned kind,const void *data,unsigned size,unsigned *changed) {
    *changed=0;
    if ((kind<1 || kind>3) || size>persist_capacity(kind) || (!data && size)) return "invalid storage value";
    unsigned bytes=sizeof(struct saved_record)+persist_capacity(kind),bank=0,foreign=0;
    if (!persist_layout()) return "storage layout unavailable";
    /* The native writer allocates a copy, scratch page and old record. */
    if (stock_free_heap()<bytes*4+8192) return "insufficient storage heap";
    struct saved_record *old=persist_latest(kind,&bank,&foreign);
    if (foreign) { stock_free(old);return "storage namespace occupied"; }
    if (old && old->length==size && (!size || (kind==1 ? source_matches(data,old->data,size) : !memcmp(old->data,data,size)))) { stock_free(old);return NULL; }
    unsigned seq=old ? old->sequence+1 : 1,slot=(kind-1)*2+(old ? bank^1 : 0);
    stock_free(old);
    unsigned char entry[8],scratch[256];
    if (stock_config_context[0]>=102 && stock_config_find(PERSIST_MODEL,slot,entry,scratch)==0xffff)
        return "storage journal has no free record slots";
    struct saved_record *r=stock_alloc(bytes);
    if (!r) return "storage allocation failed";
    memset(r,0,bytes);r->magic=0x41554c44;r->kind=kind;r->sequence=seq;r->length=size;
    if (size) {
        if(kind==1) source_copy((struct source *)data,0,r->data,size,0);
        else memcpy(r->data,data,size);
    }
    r->crc=persist_crc(r);
    *changed=1;stock_config_write(PERSIST_MODEL,slot,r,bytes);
    unsigned bad=0;struct saved_record *verify=persist_read(kind,slot&1,&bad);
    unsigned ok=verify && !bad && !memcmp(r,verify,bytes);
    stock_free(verify);stock_free(r);
    return ok ? NULL : "storage readback mismatch";
}
