static void arena_check(void) {
    unsigned used=0,reserved=0;
    for(unsigned i=0;i<ARENA_PAGES;++i) {
        if(!app.work->arena[i]) { assert(!app.work->capacity[i]);continue; }
        unsigned offset=0;reserved+=app.work->capacity[i];
        while(offset<app.work->capacity[i]) {
            struct block *b=(struct block *)(app.work->arena[i]+offset);
            assert(b->size>=16 && !(b->size&7) && b->size<=app.work->capacity[i]-offset);
            if(!b->free) used+=b->size;
            offset+=b->size;
        }
        assert(offset==app.work->capacity[i]);
    }
    assert(used==app.used && reserved==app.reserved && used<=reserved && reserved<=MEMORY_LIMIT);
}
static void test_allocator(void) {
    memset(&app,0,sizeof app);
    assert(workspace_acquire());
    unsigned char *p=allocate(NULL,NULL,0,768);assert(p);memset(p,0xa5,768);
    unsigned before=app.used;
    assert(allocate(NULL,p,768,32)==p && app.used<before);
    unsigned char *q=allocate(NULL,NULL,0,200);assert(q && allocations==2);memset(q,0x5a,200);
    allocate(NULL,q,200,0);
    assert(allocate(NULL,p,32,600)==p && allocations==2);
    for(unsigned j=0;j<32;++j) assert(p[j]==0xa5);
    arena_check();allocate(NULL,p,600,0);assert(allocations==1 && !app.used && !app.reserved);
    unsigned char *pointers[48]={0};unsigned sizes[48]={0},rng=0x12345678;
    for(unsigned step=0;step<5000;++step) {
        rng=rng*1664525U+1013904223U;unsigned i=rng%48;
        rng=rng*1664525U+1013904223U;unsigned n=(rng%7) ? rng%3000+1 : 0;
        unsigned char *next=allocate(NULL,pointers[i],sizes[i],n);
        if(next) {
            if(pointers[i]) for(unsigned j=0;j<(n<sizes[i] ? n : sizes[i]);++j) assert(next[j]==i+1);
            memset(next,i+1,n);pointers[i]=next;sizes[i]=n;
        } else if(!n) { pointers[i]=NULL;sizes[i]=0; }
        for(unsigned k=0;k<48;++k) for(unsigned j=0;j<sizes[k];++j) assert(pointers[k][j]==k+1);
        arena_check();
    }
    for(unsigned i=0;i<48;++i) allocate(NULL,pointers[i],sizes[i],0);
    arena_check();assert(allocations==1 && !app.reserved);
    discard(DONE);assert(!allocations);
    memset(&app,0,sizeof app);
    puts("Allocator: shrink reclamation, in-place growth, moving realloc, quota, alignment and 5000 randomized operations passed");
}
