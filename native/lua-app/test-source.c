static void test_source(void) {
    for(unsigned i=1;i<=SOURCE_PAGES+1;++i) {
        fail_allocation_at=i;assert(!source_new(SOURCE_LIMIT));assert(!allocations);
    }
    unsigned char data[SOURCE_LIMIT],copy[SOURCE_LIMIT];
    for(unsigned i=0;i<sizeof data;++i) data[i]=(unsigned char)(i*17);
    struct source *s=source_new(sizeof data);assert(s);
    source_copy(s,0,data,511,1);source_copy(s,511,data+511,513,1);
    source_copy(s,1024,data+1024,sizeof data-1024,1);
    source_copy(s,0,copy,sizeof copy,0);assert(!memcmp(data,copy,sizeof data));
    assert(source_matches(s,data,sizeof data));data[513]^=1;
    assert(!source_matches(s,data,sizeof data));data[513]^=1;
    struct source_cursor cursor={s,0};size_t n;
    for(unsigned i=0;i<SOURCE_PAGES;++i) {
        const char *chunk=source_reader(NULL,&cursor,&n);
        assert(chunk && n==SOURCE_CHUNK && !memcmp(chunk,data+i*SOURCE_CHUNK,n));
        assert(allocations==SOURCE_PAGES+1-i);
    }
    assert(!source_reader(NULL,&cursor,&n) && !n && allocations==1);
    source_free(s);assert(!allocations);
    /* Lexer tokens, strings and comments span reader blocks; errors also release
     * every unread block. Upload pieces need not align with reader blocks. */
    char program[SOURCE_LIMIT+1];memset(program,' ',SOURCE_LIMIT);program[SOURCE_LIMIT]=0;
    memcpy(program,"return [=[",10);memcpy(program+SOURCE_LIMIT-3,"]=]",3);
    load(program,0);assert(app.state==DONE && !allocations);
    memcpy(program,"return [=[",10);memset(program+SOURCE_LIMIT-3,' ',3);
    load(program,0);assert(app.state==ERROR && !allocations);
    memset(&app,0,sizeof app);
    puts("Source blocks: partial allocation rollback, unaligned copies, progressive release, spanning tokens and syntax-error cleanup passed");
}
