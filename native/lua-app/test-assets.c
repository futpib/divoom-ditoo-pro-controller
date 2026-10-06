/* Use actual pinned stock bytes, with independently decoded RGB fixtures. */
static void test_assets(void) {
    unsigned previous_frames=frames;
    FILE *file=fopen("firmware/306007.MVA","rb");assert(file);
    assert(!fseek(file,0x60f+0x150000,SEEK_SET));
    assert(fread(stock_asset_rom,1,sizeof stock_asset_rom,file)>0x30000);
    assert(!ferror(file));fclose(file);
    file=fopen("target/lua-app/runtime/stock-images.rgb","rb");assert(file);
    unsigned char expected[768],actual[768];
    uint32_t expected_hash=0x811c9dc5;
    for (unsigned i=0;i<ASSET_IMAGE_COUNT;++i) {
        assert(fread(expected,1,sizeof expected,file)==sizeof expected);
        assert(!asset_decode(stock_asset_rom+asset_image_offset(i),actual));
        assert(!memcmp(actual,expected,sizeof actual));
        for (unsigned j=0;j<sizeof expected;++j) expected_hash=(expected_hash^expected[j])*0x01000193;
    }
    assert(fgetc(file)==EOF && !ferror(file));fclose(file);
    assert(ASSET_IMAGE_COUNT==598);
    check("return assets.count('image')..':'..assets.count('sound')..':'..assets.count('font')",DONE,"598:14:1");
    check("local a=assets.info('sound',6);return a.id..':'..a.native_id",DONE,"6:32");
    check("local a=assets.info('image',1);return a.width..':'..a.height..':'..a.colors..':'..a.delay_ms",DONE,"16:16:2:100");
    check("return assets.info('font',1).glyph_bytes",DONE,"32");
    check("return #assets.image(1)",DONE,"768");
    check("display.clear(0x123456);assert(display.image(0,0,1));return display.frame()==assets.image(1)",DONE,"true");
    check("display.clear(0x123456);assert(display.image(16,-16,1));return display.get(0,0)==0x123456",DONE,"true");
    /* The final stock frame is solid black: transparency must preserve every pixel. */
    check("display.clear(0x123456);assert(display.image(-4,-2,598,0));return display.get(0,0)==0x123456",DONE,"true");
    check("display.clear(0x123456);assert(display.image(-15,-15,598));return display.get(0,0)..':'..display.get(1,0)",DONE,"0:1193046");
    check("return assets.count('invalid')",ERROR,NULL);
    check("return assets.info('sound',14)",ERROR,NULL);
    check("return assets.info('font',2)",ERROR,NULL);
    check("return assets.image(0)",ERROR,NULL);
    check("return assets.image(599)",ERROR,NULL);
    check("return assets.image(1.5)",ERROR,NULL);
    check("return display.image(0,0,1,0x1000000)",ERROR,NULL);
    check("return display.glyph(0,0,65536,0)",ERROR,NULL);
    check("local g=assert(assets.glyph(33));return #g..':'..g:byte(1)..':'..g:byte(32)",DONE,"32:0:31");
    assert(font_page==0x1f30 && font_reads==1);
    check("local g=assert(assets.glyph(126));return g:byte(1)",DONE,"160");
    assert(font_page==0x1f30+93/8);
    check("local g=assert(assets.glyph(0xa1));return g:byte(1)",DONE,"192");
    assert(font_page==0x1f30+94/8);
    unsigned reads=font_reads;
    check("return assets.glyph(32)==string.rep(string.char(0),32)",DONE,"true");
    check("local g,e=assets.glyph(0);return tostring(g)..':'..e",DONE,"nil:glyph unavailable");
    check("return assets.glyph(0xd800)",DONE,"nil");
    check("return assets.glyph(0xffff)",DONE,"nil");
    check("return assets.glyph(127)",DONE,"nil");
    assert(font_reads==reads);
    font_bad_layout=1;
    check("local g,e=assets.glyph(65);return e",DONE,"font partition unavailable");
    assert(font_reads==reads);font_bad_layout=0;font_read_error=1;
    check("local g,e=display.glyph(0,0,65,0xffffff);return e",DONE,"font read failed");
    font_read_error=0;
    check("display.clear(0);assert(display.glyph(0,0,33,0x123456));return display.get(0,8)..':'..display.get(1,1)..':'..display.get(0,0)",DONE,"1193046:1193046:0");
    check("while true do assets.image(1) end",ERROR,"device call budget exceeded");
    check("while true do pcall(assets.glyph,65) end",ERROR,"device call budget exceeded");
    check("return 42",DONE,"42");
    /* Decoder rejects malformed headers and out-of-palette pixels. */
    unsigned char corrupt[1031]={0};
    memcpy(corrupt,stock_asset_rom+asset_images[0],45);corrupt[5]=1;
    assert(asset_decode(corrupt,actual));corrupt[5]=0;corrupt[1]=44;
    assert(asset_decode(corrupt,actual));
    memcpy(corrupt,stock_asset_rom+asset_images[2],80);corrupt[16]=0xff;
    assert(asset_decode(corrupt,actual));
    /* Run the exact hardware probe under the guarded VM before using USB. */
    char source[8193],message[96];
    file=fopen("tests/lua/assets-device.lua","rb");assert(file);
    size_t n=fread(source,1,sizeof(source)-1,file);assert(!ferror(file));fclose(file);source[n]=0;
    load(source,1);assert(app.state==ACTIVE);
    for (unsigned i=0;i<600;++i) { tick();assert(app.state==ACTIVE); }
    snprintf(message,sizeof message,"images 598 %d",(int32_t)expected_hash);
    assert(!strcmp(app.result,message));
    memcpy(app.work->message,"65",2);app.message_size=2;tick();
    assert(app.state==ACTIVE && !strncmp(app.result,"glyph 65 ",9));
    app.cancel=1;service();runtime_native_service();assert(!allocations);
    file=fopen("examples/lua/stock-assets.lua","rb");assert(file);
    n=fread(source,1,sizeof(source)-1,file);assert(!ferror(file));fclose(file);source[n]=0;
    load(source,1);assert(app.state==ACTIVE && !strcmp(app.result,"image 1/598"));
    runtime_adc_result(1U<<16 | 2);tick();runtime_adc_result(2U<<16 | 2);tick();
    assert(!strcmp(app.result,"image 2/598") && stock_alarm_state==0);
    memcpy(app.work->message,"glyph 0416",10);app.message_size=10;tick();
    assert(!strcmp(app.result,"glyph U+0416") && app.state==ACTIVE);
    memcpy(app.work->message,"sound 6",7);app.message_size=7;tick();runtime_native_service();
    assert(stock_alarm_state==4);
    for (unsigned i=0;i<60;++i) { tick();runtime_native_service(); }
    assert(app.state==ACTIVE && stock_alarm_state==0 && !strcmp(app.result,"sound stopped"));
    app.cancel=1;service();runtime_native_service();assert(!allocations);
    frames=previous_frames;
    puts("598 stock images match independent decoding; asset bounds, glyph addressing, clipping, transparency and budget recovery pass");
}
