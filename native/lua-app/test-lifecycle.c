/* Drive the same worker step/reaper used by the device, including failed starts. */
static void lifecycle_upload(const char *text,unsigned commit) {
    unsigned n=strlen(text);unsigned char request[SOURCE_LIMIT+11]={0x37,0x7f,'D','L','U','A',3};
    request[7]=n;request[8]=n>>8;runtime_command(0,request,12);
    assert(app.state==UPLOADING && !app.task && !app.work && !app.frame);
    request[6]=4;request[7]=request[8]=0;memcpy(request+9,text,n);
    runtime_command(0,request,n+11);assert(app.received==n);
    if(commit) { request[6]=5;runtime_command(0,request,9); }
}
static void test_lifecycle(void) {
    for(unsigned i=0;i<12;++i) {
        unsigned created=tasks_created,deleted=tasks_deleted;
        lifecycle_upload(i%2 ? "while true do end" : "return 42",1);
        assert(app.state==RUNNING && app.task && tasks_created==created+1);
        assert(worker_step());
        assert(app.state==(i%2 ? ERROR : DONE) && !app.work && !app.frame && !allocations);
        assert(app.retiring && app.task);worker_reap();
        assert(!app.task && !app.retiring && tasks_deleted==deleted+1);
    }
    /* Starting another upload/worker before the main reaper runs cannot wake
     * an old retired worker or leave two owners of Lua. */
    lifecycle_upload("return 42",1);assert(worker_step() && app.retiring);
    lifecycle_upload("return 43",1);assert(app.task && !app.retiring);
    assert(worker_step() && !strcmp(app.result,"43"));worker_reap();
    fail_task=1;lifecycle_upload("return 42",1);
    assert(app.state==ERROR && !allocations && !app.task);fail_task=0;
    lifecycle_upload("return 42",0);unsigned created=tasks_created;
    clock_ms+=30001;runtime_native_service();
    assert(app.state==IDLE && !allocations && tasks_created==created);
    lifecycle_upload("return {}",1);app.resident=1;
    assert(!worker_step() && app.L && app.work && !app.frame);
    app.action=1;assert(!worker_step() && app.state==PAUSED && app.work);
    app.cancel=1;assert(worker_step());worker_reap();assert(!allocations);
    lifecycle_upload("return 42",1);fail_alloc=1;
    assert(worker_step() && app.state==ERROR && !app.work && !allocations);
    fail_alloc=0;worker_reap();
    lifecycle_upload("return {update=function() display.clear() end}",1);app.resident=1;
    assert(!worker_step() && app.work && !app.frame);
    fail_alloc=1;clock_ms+=FRAME_MS;assert(worker_step());
    assert(app.state==ERROR && !strcmp(app.result,"display allocation failed") && !allocations);
    fail_alloc=0;worker_reap();
    /* Diagnostic scratch exists only while the read is dispatched. */
    unsigned char request[12]={0x37,0x7f,'D','L','U','A',10};
    runtime_storage_diagnostic(0,request,sizeof request);assert(!allocations);
    fail_alloc=1;runtime_storage_diagnostic(0,request,sizeof request);
    assert(reply[5]==5 && !allocations);fail_alloc=0;
    memset(&app,0,sizeof app);memset(&saved,0,sizeof saved);
    memset(&peripheral,0,sizeof peripheral);
    puts("Lifecycle: upload expiry, worker start/stop/restart, paused ownership, OOM and diagnostic cleanup passed");
}
