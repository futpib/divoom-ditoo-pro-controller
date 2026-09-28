/* Fixed bindings apply only to the SHA-pinned 306007 application. */
typedef unsigned int u32;
typedef unsigned char u8;
extern void stock_version(u32 context);
extern void stock_set_version(const u8 *data);
extern void stock_reply(u32 context, u32 opcode, const void *data, u32 size);
extern u32 stock_free_heap(void);
extern void stock_suspend(void);
extern void stock_resume(void);

void probe(u32 context, const u8 *data, u32 length) {
    if (length < 4) return;
    if (data[1] == 0) { stock_version(0); return; }
    if (length < 10 || data[1] != 0x7f || data[2] != 'D' ||
        data[3] != 'L' || data[4] != 'U' || data[5] != 'A') {
        stock_set_version(data + 2);
        return;
    }
    u32 gp, sp;
    __asm__ volatile ("move %0, $gp" : "=r"(gp));
    __asm__ volatile ("move %0, $sp" : "=r"(sp));
    u32 reply[8] = {0x41554c44, 1, 0, 0, 0, gp, sp, length};
    if (data[6] != 0) { reply[1] |= 0x100; }
    else {
        stock_suspend();
        reply[2] = stock_free_heap();
        u32 *block = *(u32 **)(gp - 29212);
        /* Read the allocator's own free list while scheduling is suspended. */
        for (u32 n = 0; block && n < 4096; ++n) {
            u32 size = block[4];
            if (size > reply[3]) reply[3] = size;
            ++reply[4];
            block = (u32 *)block[0];
        }
        stock_resume();
    }
    stock_reply(context, 0x37, reply, sizeof reply);
}
