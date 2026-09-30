/* Included by runtime.c. Only immutable stock resources are addressable;
 * decoding uses bounded stack scratch and never enters the native display UI. */
#include "assets-index.h"
#ifdef ASSET_TEST_ROM
extern unsigned char stock_asset_rom[];
#else
extern const unsigned char stock_asset_rom[];
#endif

#define ASSET_IMAGE_COUNT (sizeof asset_images / sizeof asset_images[0])

static unsigned asset_image_offset(unsigned i) { return asset_image_bases[i/32]+asset_images[i]; }

static unsigned asset_u16(const unsigned char *p) { return p[0] | ((unsigned)p[1]<<8); }
static unsigned asset_kind(lua_State *L) {
    const char *kinds[] = {"image","sound","font",NULL};
    return luaL_checkoption(L,1,NULL,kinds);
}
static int asset_count(lua_State *L) {
    unsigned kind=asset_kind(L);
    lua_pushinteger(L,kind==0 ? ASSET_IMAGE_COUNT : kind==1 ? 14 : 1);return 1;
}
static const unsigned char *asset_image_ptr(lua_State *L,int arg) {
    unsigned id=integer(L,arg,1,ASSET_IMAGE_COUNT);
    return stock_asset_rom+asset_image_offset(id-1);
}
static int asset_info(lua_State *L) {
    unsigned kind=asset_kind(L),id=integer(L,2,kind==1 ? 0 : 1,
        kind==0 ? ASSET_IMAGE_COUNT : kind==1 ? 13 : 1);
    lua_createtable(L,0,5);field(L,"id",id);
    if (kind==1) {
        field(L,"native_id",stock_asset_rom[0xdd80+id]);
    } else {
        field(L,"width",16);field(L,"height",16);
        if (!kind) {
            const unsigned char *p=stock_asset_rom+asset_image_offset(id-1);
            field(L,"colors",p[6] ? p[6] : 256);field(L,"delay_ms",asset_u16(p+3));
        } else field(L,"glyph_bytes",32);
    }
    return 1;
}

/* The stock loader at 0x51c84 maps ASCII plus 16 Unicode ranges, then
 * reads a 256-byte page of eight column-major 16x16 monochrome glyphs. */
static const char *asset_read_glyph(unsigned cp,unsigned char glyph[32]) {
    if (cp==32) { memset(glyph,0,32);return NULL; }
    if (cp<33 || (cp>=0xd800 && cp<=0xdfff)) return "glyph unavailable";
    unsigned index=cp-33;
    if (cp>126) {
        const unsigned char *ranges=stock_asset_rom+0x2a9e0;
        index=94;
        unsigned i;
        for (i=0;i<16;++i) {
            unsigned first=asset_u16(ranges+i*4),last=asset_u16(ranges+i*4+2);
            if (cp<first) return "glyph unavailable";
            if (cp<=last) { index+=cp-first;break; }
            index+=last-first+1;
        }
        if (i==16) return "glyph unavailable";
    }
    unsigned base=0,bytes=stock_partition(3,&base);
    if (base!=0x1f30 || bytes!=0x119000 || index>=bytes/32)
        return "font partition unavailable";
    uint32_t page[64];
    if (stock_page_read(base+index/8,page,1)) return "font read failed";
    memcpy(glyph,(unsigned char *)page+(index%8)*32,32);return NULL;
}
static int asset_glyph(lua_State *L) {
    unsigned cp=integer(L,1,0,65535);unsigned char glyph[32];
    native_budget();
    const char *error=asset_read_glyph(cp,glyph);
    if (error) return failure(L,error);
    lua_pushlstring(L,(const char *)glyph,sizeof glyph);return 1;
}
static int draw_glyph(lua_State *L) {
    int x=integer(L,1,-16,16),y=integer(L,2,-16,16);
    unsigned cp=integer(L,3,0,65535),rgb=color(L,4);unsigned char glyph[32];
    native_budget();
    const char *error=asset_read_glyph(cp,glyph);
    if (error) return failure(L,error);
    for (unsigned i=0;i<16;++i) {
        unsigned column=asset_u16(glyph+i*2);
        for (unsigned j=0;j<16;++j) if (column & (1U<<j)) pixel(x+i,y+j,rgb);
        runtime_poll();
    }
    lua_pushboolean(L,1);return 1;
}

/* Independent palette frames only: exactly 256 indices, LSB first. No
 * native decoder, shared palette state, arbitrary address or file handle. */
static const char *asset_decode(const unsigned char *p,unsigned char rgb[768]) {
    unsigned colors=p[6] ? p[6] : 256,bits=0;
    for (unsigned n=colors-1;n;n>>=1) ++bits;
    if (p[0]!=0xaa || p[5] || asset_u16(p+1)!=7+3*colors+32*bits)
        return "invalid stock image";
    const unsigned char *indices=p+7+3*colors;
    unsigned bit=0;
    for (unsigned i=0;i<256;++i) {
        unsigned index=0;
        for (unsigned k=0;k<bits;++k,++bit) index|=((indices[bit/8]>>(bit%8))&1U)<<k;
        if (index>=colors) return "invalid stock palette";
        memcpy(rgb+i*3,p+7+index*3,3);
        if (!(i&31)) runtime_poll();
    }
    return NULL;
}
static int asset_image(lua_State *L) {
    const unsigned char *p=asset_image_ptr(L,1);unsigned char rgb[768];
    native_budget();const char *error=asset_decode(p,rgb);
    if (error) return failure(L,error);
    lua_pushlstring(L,(const char *)rgb,sizeof rgb);return 1;
}
static int draw_image(lua_State *L) {
    int x=integer(L,1,-16,16),y=integer(L,2,-16,16);
    const unsigned char *p=asset_image_ptr(L,3);
    unsigned transparent=lua_isnoneornil(L,4) ? 0x1000000 : color(L,4);
    unsigned char rgb[768];native_budget();
    const char *error=asset_decode(p,rgb);if (error) return failure(L,error);
    for (unsigned i=0;i<256;++i) {
        const unsigned char *c=rgb+i*3;
        unsigned value=(c[0]<<16)|(c[1]<<8)|c[2];
        if (value!=transparent) pixel(x+i%16,y+i/16,value);
    }
    lua_pushboolean(L,1);return 1;
}
