/* World cFinishStepJump copies ActionParam4[0] (runtime 0x9f0):
 * vector +0x10, gravity +0x20, time rate +0x24. cFinishAirAttack
 * applies ActionParam4[1] at source trigger bit4, frame30 (runtime0xa20).
 * File offsets1476..1512 are retained in world_physics_source.json.
 * GU velocities use simulation frames; the native floor solver owns position.
 */
static NativeCall world_original_floor;
static unsigned world_air_owned,world_air_actor,world_air_epoch,world_down_applied;
static unsigned world_floor_depth,world_saved_mode,world_saved_tag;
static float world_saved_gravity;
static float world_rise_dx,world_rise_dz,world_rise_hold;
static unsigned world_air_sample;
static unsigned world_air_owner(void *a)
{
    return world_air_owned&&world_air_actor==(u32)(uptr)a&&world_air_epoch==byte(a,AREA);
}
static void world_zero_velocity(void *a)
{
    for(unsigned i=0;i<3;++i){put(a,0x1010+4*i,0);put(a,0x1020+4*i,0);}
}
static float world_sin(unsigned angle)
{
    int n=(int)(angle&65535u);if(n>32768)n-=65536;
    if(n>16384)n=32768-n;else if(n<-16384)n=-32768-n;
    float x=(float)n*(3.14159265358979323846f/32768.0f),t=x*x;
    return x*(1+t*(-1.0f/6+t*(1.0f/120+t*(-1.0f/5040+t*(1.0f/362880-t/39916800)))));
}
static void world_physics_end(void *a)
{
    if(!world_air_owned)return;
    if(world_air_owner(a)){
        unsigned airborne=real(a,0x44)>real(a,0x10f0)+1;
        *(unsigned char *)((uptr)a+0x2505)=airborne?2:world_saved_mode;
        *(unsigned char *)((uptr)a+0x2ed3)=airborne?1:world_saved_tag;
        if(!airborne)world_zero_velocity(a);
        set_real(a,0x1024,airborne&&world_saved_gravity>=0?-.2272727f:world_saved_gravity);
    }
    world_air_owned=world_down_applied=0;
}
static void world_physics_begin(void *a,unsigned move,unsigned magnitude)
{
    if(move<W_RISE||move>W_DOWN){world_physics_end(a);return;}
    if(!world_air_owner(a)){
        world_air_owned=1;world_air_actor=(u32)(uptr)a;world_air_epoch=byte(a,AREA);
        world_saved_mode=byte(a,0x2505);world_saved_tag=byte(a,0x2ed3);
        world_saved_gravity=real(a,0x1024);
    }
    *(unsigned char *)((uptr)a+0x2505)=2;
    *(unsigned char *)((uptr)a+0x2ed3)=1;
    if(move==W_RISE){
        world_zero_velocity(a);
        /* Two live World runs: 16 updates at 0.02, ending at F0.32.
         * This timer begins at GU motion bind, after the hit request. */
        world_rise_hold=16.0f/60.0f;world_air_sample=sample_sequence;
        set_real(a,0x1010,world_rise_dx/60);set_real(a,0x1018,world_rise_dz/60);
    }else if(move==W_AIR){
        set_real(a,0x1014,1050.0f*1.3f/60.0f);
        set_real(a,0x1024,-1250.0f*1.3f*1.3f/3600.0f);
        float forward=magnitude>=50?200.0f*1.3f/60.0f:0;
        set_real(a,0x1010,world_sin(world.heading)*forward);
        set_real(a,0x1018,world_sin(world.heading+16384u)*forward);
        world_down_applied=0;
    }
}
static void world_physics_tick(void *a)
{
    if(!world_air_owner(a)||!world.move||!world_bound||u16(a,MOTION)!=0x500+world.slot||
       world.actor!=(u32)(uptr)a||world.epoch!=byte(a,AREA))return;
    *(unsigned char *)((uptr)a+0x2505)=2;
    *(unsigned char *)((uptr)a+0x2ed3)=1;
    if(world.move==W_RISE){
        if(world_air_sample!=sample_sequence){
            world_air_sample=sample_sequence;float dt=real(a,0x1c)/60;
            if(dt>0&&dt<.5f)world_rise_hold-=dt;
        }
        float speed=world_rise_hold>0.00001f?.02f:1;
        set_real(a,0x50c,speed);set_real(a,0x14b0,speed);
    }
    if(world.move!=W_DOWN||real(a,FRAME)<30)return;
    unsigned initial=!world_down_applied;
    if(initial||(real(a,0x44)>real(a,0x10f0)+1&&real(a,0x1024)>=0)){
        /* Zeroed gravity can strand this private action above the native floor.
         * Restore only the owned airborne dive, retaining existing fall speed. */
        if(initial){put(a,0x1010,0);put(a,0x1018,0);world_down_applied=1;}
        if(initial||real(a,0x1014)>=0)set_real(a,0x1014,-3000.0f*1.5f/60.0f);
        set_real(a,0x1024,-1200.0f*1.5f*1.5f/3600.0f);
        log_event(initial?"world_dive":"world_dive_resume",(u32)(uptr)a,(u32)real(a,FRAME),word(a,0x44),word(a,0x10f0));
    }
}
static u32 world_floor(void *a,u32 b,u32 c,u32 d)
{
    unsigned owned=local_guild(a)&&world.actor==(u32)(uptr)a&&world.move&&
        world.epoch==byte(a,AREA)&&(world_air_owner(a)||world.move==W_FORESIGHT);
    if(!owned)return world_original_floor(a,b,c,d);
    world_physics_tick(a);++world_floor_depth;
    u32 result=world_original_floor(a,b,c,d);
    --world_floor_depth;return result;
}
