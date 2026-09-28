/* Read-only inspection of the stock filesystem metadata. No Lua binding,
 * caller-controlled physical address or flash writes. */
#include <stdint.h>
#include <string.h>

extern unsigned char *runtime_fs_context(void);
extern unsigned stock_page_read(unsigned, void *, unsigned);
extern void stock_reply(unsigned, unsigned, const void *, unsigned);

void runtime_storage_diagnostic(unsigned context, const unsigned char *data, unsigned length) {
    static uint32_t words[(48 + 128) / 4], page_words[256 / 4];
    static volatile unsigned busy;
    unsigned char *reply = (unsigned char *)words;
    if (busy) {
        const unsigned char error[16] = {'D','F','S','P',1,2};
        stock_reply(context,0x37,error,sizeof error); return;
    }
    busy = 1;
    memset(words,0,sizeof words);
    memcpy(reply,"DFSP",4); reply[4] = 1;
    unsigned n = 0, page = 0, result = 0;
    unsigned char *fs = runtime_fs_context();
    if (length != 12) reply[5] = 1;
    else {
        page = data[7] | ((unsigned)data[8] << 8); n = data[9];
        if (n > 1 || page >= 1024) reply[5] = 1;
        else if (!fs) reply[5] = 3;
        else {
            uint32_t bitmap, payload;
            memcpy(&bitmap,fs+8,4); memcpy(&payload,fs+4,4);
            if (bitmap != 0x9000 || payload != 0x9200) reply[5] = 4;
            else {
                memcpy(reply+16,fs,32);
                if (n) {
                    result = stock_page_read(0x9000+page/2,page_words,1);
                    memcpy(reply+48,(unsigned char *)page_words+(page%2)*128,128);
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
    busy = 0;
}
