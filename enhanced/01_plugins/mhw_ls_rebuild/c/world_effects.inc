/* Isolated native EFL transport experiment. Never enabled in installed builds. */
#if ML_WORLD_FX_PROBE
static void *world_fx_archive,*world_fx_pel,*world_fx_handle,*world_fx_manager;
static unsigned world_fx_failed,world_fx_ready;
static volatile unsigned world_fx_probe_command;
static unsigned world_fx_test_mode,world_fx_timeline,world_fx_seen;
static void world_fx_begin(void *a){(void)a;world_fx_seen=0;}
static u32 (*world_fx_original_step)(void *);
static u32 (*world_fx_original_particle)(void *,void *,void *);
static unsigned world_fx_path(const char *,const char *);
static unsigned world_fx_owned(void *effect)
{
    if(!effect)return 0;
    /* Both callers receive live cEffect instances from the native engine.
     * Identify the private EFL resource, without borrowing an unrelated
     * proof-controller subclass's parent layout. */
    void *resource=(void *)(uptr)word(effect,244);
    return resource&&world_fx_path((char *)resource+8,"effect/world/probe_wp03_002");
}
static u32 world_fx_particle(void *generator,void *particle,void *params)
{
    u32 result=world_fx_original_particle(generator,particle,params);
    if(world_fx_owned((void *)(uptr)word(generator,8))){
        if(world_fx_test_mode==2){set_real(particle,64,real(particle,64)*4);set_real(particle,68,real(particle,68)*4);}
        log_event("world_fx_particle",(u32)(uptr)particle,word(particle,64),word(particle,136),result);
        void *life=(void *)(uptr)(word(generator,36)+word(generator,196)+u16(generator,216)*u16(particle,8));
        log_event("world_fx_life",(u32)(uptr)particle,word(life,4),word(life,8),word(life,12));
    }
    return result;
}
static u32 world_fx_step(void *effect)
{
    u32 result=world_fx_original_step(effect);
    if(world_fx_owned(effect)){
        void *g=(void *)(uptr)word(effect,496);
        if(g)log_event("world_fx_step",(u32)(uptr)effect,word(effect,268),word(g,176),word(g,136));
    }
    return result;
}
static unsigned world_fx_install(void)
{
    if(word((void *)(main_base+0x9b7108),0)!=0xe92d4010||word((void *)(main_base+0x9b710c),0)!=0xed2d8b04||
       word((void *)(main_base+0xa7d870),0)!=0xe92d47f0||word((void *)(main_base+0xa7d874),0)!=0xed2d8b02)return 0;
    forge_hook_create((void *)(main_base+0x9b7108),(void *)world_fx_step,(void **)&world_fx_original_step);
    forge_hook_create((void *)(main_base+0xa7d870),(void *)world_fx_particle,(void **)&world_fx_original_particle);
    return world_fx_original_step&&world_fx_original_particle;
}
static unsigned world_fx_path(const char *a,const char *b)
{
    for(unsigned i=0;i<64;++i){char c=a[i]=='\\'?'/':a[i];if(c!=b[i])return 0;if(!c)return 1;}
    return 0;
}
static void world_fx_tick(void *a)
{
    if(world_fx_failed)return;
    if(!world_fx_archive){
        void *manager=*(void **)(main_base+0x2126a64);if(!manager)return;
        uptr *vt=*(uptr **)manager;
        world_fx_archive=((void *(*)(void *,void *,const char *,unsigned))vt[0x30/4])
            (manager,(void *)(main_base+0x2124df0),"arc/forge/world_effects",1);
        if(!world_fx_archive){world_fx_failed=1;return;}
        log_event("world_fx_queued",(u32)(uptr)a,(u32)(uptr)world_fx_archive,(u32)(uptr)&world_fx_probe_command,0);
    }
    if(!world_fx_ready){
        unsigned flags=byte(world_fx_archive,0x50);
        if(flags&0x50){world_fx_failed=1;log_event("world_fx_failed",(u32)(uptr)a,flags,0,0);return;}
        if(!(flags&1))return;
        unsigned n=word(world_fx_archive,0x68);void **objects=*(void ***)((uptr)world_fx_archive+0x64);
        if(n!=4||!objects)return;
        for(unsigned i=0;i<n;++i){
            if(!objects[i])return;
            unsigned child=byte(objects[i],0x50);
            if(child&0x50){world_fx_failed=1;log_event("world_fx_failed",(u32)(uptr)a,child,i,0);return;}
            if(!(child&1))return;
            if(*(uptr *)objects[i]==main_base+0x175261c&&world_fx_path((char *)objects[i]+8,"effect/pel/pl/world_probe"))
                world_fx_pel=objects[i];
        }
        if(!world_fx_pel)return;
        void *manager=*(void **)(main_base+0x18509e8);
        if(!manager||*(void **)(main_base+0x188a0d4)!=manager||*(uptr *)manager!=main_base+0x1731534)return;
        world_fx_handle=((void *(*)(void *,void *))(main_base+0x3268d8))(manager,world_fx_pel);
        if(!world_fx_handle||word(world_fx_handle,4)>=word(manager,0x5c)){world_fx_failed=1;return;}
        world_fx_manager=manager;world_fx_ready=1;
        log_event("world_fx_ready",(u32)(uptr)a,(u32)(uptr)world_fx_handle,word(world_fx_handle,4),n);
    }
    unsigned cmd=world_fx_probe_command;world_fx_probe_command=0;
    if(cmd==3){world_fx_timeline=1;world_fx_seen=0;}
    unsigned work=0;
    if(world_fx_timeline&&world_bound&&world.source==306&&world.move==W_SHEATHE&&
       world.actor==(u32)(uptr)a&&world.epoch==byte(a,AREA)&&(int16_t)u16(a,HP)>0&&
       u16(a,STATE)==16&&u16(a,ACTION)==246&&u16(a,MOTION)==0x500+world.slot){
        float frame=real(a,FRAME);
        if(frame>=32&&!(world_fx_seen&1)){world_fx_seen|=1;work=3010;}
        else if(frame>=(ML_WORLD_STYLE_ENHANCED?WORLD_SHEATHE_CLOSE_FRAME:76)&&
                !(world_fx_seen&2)){world_fx_seen|=2;work=3011;}
    }
    if(((cmd>=1&&cmd<=2)||work)&&*(void **)(main_base+0x18509e8)==world_fx_manager){
        world_fx_test_mode=cmd==2?2:1;
        float point[3]={real(a,0x40),real(a,0x44)+(cmd==2?220:100),real(a,0x48)};
        if(work&&!world_joint47(a,point))return;
        if(work)log_event("world_fx_cue",(u32)(uptr)a,world.source,work,(u32)(real(a,FRAME)*1000));
        unsigned char request[0x110] __attribute__((aligned(16)));
        ((void (*)(void *))(main_base+0x40b10))(request);put(request,8,2);
        /* Native31C3A8 mask bit0 merges inner+16 translation. Efx_exc's
         * separate inner+176 field is a shader/reference point, not origin. */
        put(request,20,word(request,20)|1);
        for(unsigned k=0;k<3;++k)set_real(request,32+4*k,point[k]);
        put(request,44,0);
        log_event("world_fx_origin",(u32)(uptr)a,word(request,32),word(request,36),word(request,40));
        uptr *vt=*(uptr **)a;
        uptr effect=((uptr (*)(void *,void *,u32,u32,void *,uptr))vt[0x138/4])
            (a,world_fx_handle,2,0x3f50,request,word(a,0x3310));
        log_event("world_fx_emit",(u32)(uptr)a,(u32)effect,word(a,0x3310),0x3f50);
        ((void (*)(void *))(main_base+0x40b50))(request);
    }
}
#else
static void world_fx_tick(void *a){(void)a;}
static void world_fx_begin(void *a){(void)a;}
#endif
