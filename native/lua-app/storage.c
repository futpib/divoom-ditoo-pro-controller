/* Read-only inspection of the stock filesystem metadata. No Lua binding,
 * caller-controlled physical address or flash writes. */
#include <stdint.h>
#include <string.h>

extern unsigned char *runtime_fs_context(void);
extern void *stock_alloc(unsigned);
extern void stock_free(void *);
extern unsigned stock_free_heap(void);
extern unsigned runtime_irq_save(void);
extern void runtime_irq_restore(unsigned);
extern unsigned stock_page_read(unsigned, void *, unsigned);
extern unsigned char *volatile stock_config_context;
extern unsigned stock_partition(unsigned, unsigned *);
extern void stock_reply(unsigned, unsigned, const void *, unsigned);

void runtime_storage_diagnostic(unsigned context, const unsigned char *data, unsigned length) {
    struct scratch { uint32_t words[(48+128)/4],page_words[256/4]; } *scratch;
    static volatile unsigned busy;

    unsigned irq=runtime_irq_save();
    if (busy) {
        runtime_irq_restore(irq);
        const unsigned char error[16] = {'D','F','S','P',1,2};
        stock_reply(context,0x37,error,sizeof error); return;
    }
    busy = 1;runtime_irq_restore(irq);
    if(stock_free_heap()<sizeof *scratch+8+24576 || !(scratch=stock_alloc(sizeof *scratch))) {
        const unsigned char error[16]={'D','F','S','P',1,5};
        stock_reply(context,0x37,error,sizeof error);busy=0;return;
    }
    unsigned char *reply=(unsigned char *)scratch->words;
    memset(scratch,0,sizeof *scratch);
    memcpy(reply,"DFSP",4); reply[4] = 1;
    unsigned n = 0, page = 0, result = 0;
    unsigned config=data[6]==12,base=0,limit=config ? 2560 : 1024;
    unsigned char *fs = config ? stock_config_context : runtime_fs_context();
    if (config) { memcpy(reply,"DCFG",4);stock_partition(5,&base); }
    if (length != 12) reply[5] = 1;
    else {
        page = data[7] | ((unsigned)data[8] << 8); n = data[9];
        if (n > 1 || page >= limit) reply[5] = 1;
        else if (!fs) reply[5] = 3;
        else {
            uint32_t bitmap, payload;
            memcpy(&bitmap,fs+(config ? 12 : 8),4); memcpy(&payload,fs+(config ? 8 : 4),4);
            if (config ? (bitmap!=base || base<0x100 || base>0xfb00 || payload!=base+0x100)
                       : (bitmap != 0x9000 || payload != 0x9200)) reply[5] = 4;
            else {
                memcpy(reply+16,fs,config ? 20 : 32);
                if (n) {
                    result = stock_page_read((config ? base : 0x9000)+page/2,scratch->page_words,1);
                    memcpy(reply+48,(unsigned char *)scratch->page_words+(page%2)*128,128);
                }
            }
        }
    }
    if (reply[5]) n = 0;
    reply[6] = page; reply[7] = page >> 8;
    memcpy(reply+8,&result,4);
    /* Stock BLE corrupts larger notifications despite its advertised MTU. */
    unsigned bytes = n*128; memcpy(reply+12,&bytes,4);
    stock_reply(context,0x37,reply,48+bytes);
    stock_free(scratch);busy = 0;
}
