/* EP0 only copies reports. Commands run on the native main task, never in IRQ. */
#include <stdint.h>
#include <string.h>

#define USB_REPORT 256
#define USB_HEADER 32
#define USB_CHUNK (USB_REPORT-USB_HEADER)
#define USB_FRAME 4100
#define USB_RING 8192
#define USB_CONTEXT 0x178
#define USB_REPORT_ID 0x7d

extern void *stock_alloc(unsigned);
extern void stock_free(void *);
extern unsigned stock_free_heap(void);
extern unsigned stock_ticks(void);
extern unsigned stock_control_connected(void);
extern unsigned runtime_irq_save(void);
extern void runtime_irq_restore(unsigned);
extern void stock_usb_receive(const void *, unsigned);
extern void stock_usb_send(const void *, unsigned, unsigned);
extern unsigned char stock_usb_setup[];
extern unsigned char *stock_command_context;
extern void runtime_usb_dispatch(void *, void *);
extern unsigned stock_response_original(unsigned, unsigned, unsigned, const void *, unsigned,
                                         unsigned, unsigned, unsigned);

enum { USB_OK, USB_INVALID, USB_BUSY, USB_MEMORY_ERROR, USB_OVERFLOW,
       USB_SESSION_ERROR, USB_FRAME_ERROR };
static struct {
    uint32_t *context;
    unsigned char inbox[USB_REPORT], report[USB_REPORT];
    volatile unsigned pending, seen;
    unsigned char *memory, *frame;
    unsigned session, sequence, read, written, received, total, status;
} usb;

/* Stock periodic app notifications must also run with a USB-only controller. */
unsigned runtime_usb_connected(void) {
    return usb.memory!=NULL || stock_control_connected();
}

static unsigned usb_u16(const unsigned char *p) { return p[0] | (unsigned)p[1]<<8; }
static unsigned usb_u32(const unsigned char *p) { return usb_u16(p) | usb_u16(p+2)<<16; }
static void usb_put16(unsigned char *p, unsigned n) { p[0]=n; p[1]=n>>8; }
static void usb_put32(unsigned char *p, unsigned n) { usb_put16(p,n); usb_put16(p+2,n>>16); }

void runtime_usb_receive(const unsigned char *p, unsigned n) {
    if (stock_usb_setup[2] != USB_REPORT_ID) { stock_usb_receive(p,n); return; }
    if (n != USB_REPORT || usb_u16(stock_usb_setup+6)!=USB_REPORT || memcmp(p,"DUSB\1",5)) return;
    usb.seen = stock_ticks();
    if (usb.pending) return; /* Host polls the sequence before submitting another report. */
    memcpy(usb.inbox,p,USB_REPORT);
    __asm__ volatile ("" ::: "memory");
    usb.pending = 1;
}

void runtime_usb_send(const void *original, unsigned n, unsigned kind) {
    if (stock_usb_setup[2] != USB_REPORT_ID) { stock_usb_send(original,n,kind); return; }
    unsigned irq = runtime_irq_save();
    unsigned char *p = usb.report;
    memset(p,0,USB_REPORT); memcpy(p,"DUSB\1",5);
    p[5]=usb.status; p[6]=(usb.memory ? 1 : 0) | (usb.pending ? 2 : 0);
    usb_put32(p+8,usb.session); usb_put32(p+12,usb.sequence);
    usb_put16(p+16,usb.received); usb_put16(p+18,usb.total);
    usb_put16(p+20,USB_FRAME); usb_put16(p+22,1); /* Native command bridge. */
    usb_put32(p+24,usb.read); usb_put16(p+30,USB_RING);
    unsigned count = usb.written-usb.read;
    if (count>USB_CHUNK) count=USB_CHUNK;
    usb_put16(p+28,count);
    for (unsigned i=0;i<count;++i) p[USB_HEADER+i]=usb.memory[(usb.read+i)%USB_RING];
    usb.seen=stock_ticks();
    runtime_irq_restore(irq);
    /* Persistent storage remains unchanged until the next serialized EP0 request. */
    stock_usb_send(p,USB_REPORT,kind);
}

static void usb_close(void) {
    unsigned irq=runtime_irq_save();
    void *memory=usb.memory;
    usb.memory=NULL; usb.session=usb.sequence=usb.read=usb.written=0;
    usb.received=usb.total=usb.status=0;
    runtime_irq_restore(irq);
    stock_free(memory);usb.context=NULL;
    stock_free(usb.frame);usb.frame=NULL;
}

static unsigned usb_frame(void) {
    unsigned char *p=usb.frame;
    unsigned size=usb.total;
    if (size<7 || p[0]!=1 || p[size-1]!=2 || usb_u16(p+1)+4!=size) return USB_FRAME_ERROR;
    unsigned sum=0;
    for (unsigned i=1;i<size-3;++i) sum+=p[i];
    if ((sum&65535)!=usb_u16(p+size-3)) return USB_FRAME_ERROR;
    /* Exactly the stock parser's packet layout; handlers run synchronously. */
    struct { uint16_t pad, length, received, reserved; unsigned char *data; }
        packet={0,size-4,size-4,0,p+3};
    runtime_usb_dispatch(&packet,usb.context);
    return USB_OK;
}

void runtime_usb_service(void) {
    if (!usb.pending) {
        if (usb.memory && (unsigned)(stock_ticks()-usb.seen)>15000) usb_close();
        return;
    }
    const unsigned char *p=usb.inbox;
    unsigned session=usb_u32(p+8), seq=usb_u32(p+12), op=p[5];
    if (op==1 && session && seq==1 && session!=usb.session) {
        usb_close();
        unsigned char *memory=NULL;
        if (stock_free_heap()<USB_RING+USB_CONTEXT+8+24576 || !(memory=stock_alloc(USB_RING+USB_CONTEXT))) {
            usb.status=USB_MEMORY_ERROR;
        } else {
            memset(memory,0,USB_RING+USB_CONTEXT);
            usb.context=(uint32_t *)(memory+USB_RING);
            unsigned irq=runtime_irq_save();
            usb.memory=memory;
            usb.session=session; usb.sequence=seq;
            runtime_irq_restore(irq);
        }
    } else if (!usb.memory || session!=usb.session) {
        usb.status=USB_SESSION_ERROR;
    } else if (op==4 && seq==usb.sequence+1) {
        usb_close();
    } else if (seq==usb.sequence) {
        /* An uncertain SET_REPORT may be retried without executing it twice. */
    } else if (seq!=usb.sequence+1 || usb.status) {
        if (!usb.status) usb.status=USB_INVALID;
    } else {
        unsigned ack=usb_u32(p+24);
        unsigned irq=runtime_irq_save();
        if (ack-usb.read>usb.written-usb.read) usb.status=USB_INVALID;
        else usb.read=ack;
        runtime_irq_restore(irq);
        if (!usb.status && op==2) {
            unsigned total=usb_u16(p+16), offset=usb_u16(p+18), count=usb_u16(p+20);
            if (!count || count>USB_CHUNK || total<7 || total>USB_FRAME ||
                offset!=usb.received || offset+count>total ||
                (offset && total!=usb.total)) usb.status=USB_INVALID;
            else {
                /* Retain only the response ring between commands. The input
                 * frame is main-task-only and sized to this command, freeing
                 * the former idle 4100-byte buffer for Lua and native audio. */
                if (!offset && (stock_free_heap()<total+8+24576 || !(usb.frame=stock_alloc(total))))
                    usb.status=USB_MEMORY_ERROR;
                if (!usb.status) {
                    usb.total=total;
                    memcpy(usb.frame+offset,p+USB_HEADER,count);
                    usb.received+=count;
                    if (usb.received==total) {
                        unsigned result=usb_frame();
                        if (result) usb.status=result;
                        stock_free(usb.frame);usb.frame=NULL;
                        usb.received=usb.total=0;
                    }
                }
            }
        } else if (!usb.status && op!=3 && op!=4) usb.status=USB_INVALID;
        if (op==4 && !usb.status) usb_close();
        else usb.sequence=seq;
    }
    __asm__ volatile ("" ::: "memory");
    usb.pending=0;
}

/* Mirror native replies, including asynchronous events, without changing BT. */
unsigned runtime_usb_response(unsigned context, unsigned outer, unsigned opcode,
        const unsigned char *data, unsigned size, unsigned a, unsigned b, unsigned ack) {
    unsigned copied=0, irq=runtime_irq_save();
    if (usb.memory && !usb.status) {
        unsigned total=size+9;
        if (size>USB_FRAME-7 || total>USB_RING-(usb.written-usb.read)) usb.status=USB_OVERFLOW;
        else {
            /* Normal notifications use their command as the original opcode. */
            unsigned char head[6]={1,0,0,4,outer==255 ? opcode : outer,outer==255 ? ack : 0x55};
            usb_put16(head+1,size+5);
            unsigned sum=0, cursor=usb.written;
            for (unsigned i=0;i<6;++i) {
                usb.memory[cursor++%USB_RING]=head[i];
                if (i) sum+=head[i];
            }
            for (unsigned i=0;i<size;++i) {
                usb.memory[cursor++%USB_RING]=data[i]; sum+=data[i];
            }
            usb.memory[cursor++%USB_RING]=sum;
            usb.memory[cursor++%USB_RING]=sum>>8;
            usb.memory[cursor++%USB_RING]=2;
            usb.written=cursor; copied=1;
        }
    }
    runtime_irq_restore(irq);
    if (stock_command_context && stock_command_context[7] && stock_command_context[2]<2)
        return stock_response_original(context,outer,opcode,data,size,a,b,ack) || copied;
    return copied;
}
