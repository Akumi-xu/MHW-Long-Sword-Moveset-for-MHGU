/* World counter shells: source object.col group0 is a radius120 capsule,
 * endpoints(0,0,0)/(0,50,0), spawned at the owner's current position.
 * Detect against GU's already transformed attack shapes before body damage.
 * No global hurtbox is enlarged and no native collision report is fabricated.
 */
typedef struct {float remaining,point[3];} WDetector;
static WDetector world_detectors[3];
static unsigned world_detector_spawned;
static u32 (*world_original_player_query)(void *,void *,void *);
static u32 (*world_original_attack_shapes)(void *);
static unsigned world_detected_attack;
static void world_detector_clear(void)
{
    for(unsigned i=0;i<3;++i)world_detectors[i].remaining=0;
    world_detector_spawned=0;
    world_detected_attack=0;
}
static void world_detector_spawn(void *a,unsigned index,float seconds)
{
    WDetector *d=world_detectors+index;d->remaining=seconds;
    for(unsigned k=0;k<3;++k)d->point[k]=real(a,0x40+4*k);
    world_detector_spawned|=1u<<index;
    log_event("world_detector_spawn",(u32)(uptr)a,world.source,index,(u32)real(a,FRAME));
}
static void world_detector_begin(void *a)
{
    world_detector_clear();
    if(world.move==W_SMALL)world_detector_spawn(a,0,.12f);
    else if(world.move==W_BIG)world_detector_spawn(a,0,.25f);
    else if(world.move==W_FORESIGHT&&world.foresight_gauge)world_detector_spawn(a,0,.2f);
}
static void world_detector_tick(void *a,float dt)
{
    for(unsigned i=0;i<3;++i)world_detectors[i].remaining-=dt;
    if(world.move==W_FORESIGHT&&world.foresight_gauge&&world_bound&&u16(a,MOTION)==0x500+world.slot){
        for(unsigned i=1;i<3;++i)
            if(!(world_detector_spawned&(1u<<i))&&real(a,FRAME)>=10.0f*i)
                world_detector_spawn(a,i,.2f);
    }
}
static void world_counter_accept(void *a,unsigned type)
{
    if(world_counter_seen)return;
    world_counter_seen=1;
#if ML_WORLD_PRESENTATION
    if(ML_WORLD_STYLE_ENHANCED&&world.move==W_BIG&&world_attack_owned&&
       world_attack_actor==(u32)(uptr)a&&u16(a,0x3054)==world_attack_id)
        *(uint16_t *)((uptr)a+0x3072)=0;
    /* World shell 11's accepted contact calls group 2004, record 0
     * (request 25510). A swing or an outgoing sword hit cannot trigger it. */
    if(world.move==W_BIG || (world.move==W_FORESIGHT && world.foresight_gauge))
        world_audio_epv(a,25510,(const float *)((uptr)a+0x40));
#endif
    float duration=world.move==W_FORESIGHT?1.0f:2.0f;
    if(world_armor<duration)world_armor=duration;
    log_event("world_counter",(u32)(uptr)a,world.source,(u32)real(a,FRAME),type);
    ((void (*)(void *,u32,uptr,u32))(main_base+0x282624))(a,9,(uptr)a+0x2550,1);
}
static unsigned world_counter_type(unsigned type)
{
    return !(type>=25&&type<=29)&&type!=58&&type!=67&&type!=68&&type!=70;
}
static void world_detect_attack(void *manager,void *attack,void *a)
{
    if(a==(void *)(uptr)world.actor&&local_guild(a)&&world.epoch==byte(a,AREA)&&
       (int16_t)u16(a,HP)>0&&!world_counter_seen&&world.move&&world_bound&&
       u16(a,MOTION)==0x500+world.slot&&world_counter_type(byte(attack,0x3a))&&
       /* Native16938c identifies monster ownership through either collider
        * owner kind10 or originating owner kind6e (monster projectiles). */
       (byte(attack,0x10)==1||byte(attack,0x6e)==1)&&
       word(attack,0xc)!=(u32)(uptr)a){
        void *resource=(void *)(uptr)word(attack,0x84);
        void *group=resource?((void *(*)(void *,unsigned))(*(uptr **)resource)[0x44/4])
            (resource,word(attack,0x88)):0;
        unsigned count=group?word(group,4):0,first=word(attack,0x1c);
        if(count<=64&&first<64&&count<=64-first){
            for(unsigned j=0;j<count&&!world_counter_seen;++j){
                void *entries=(void *)(uptr)word(group,8);
                if(!entries||!word(entries,j*4))continue;
                const void *record=(const void *)((uptr)manager+0x2030+(first+j)*32);
                unsigned type=byte(record,0);float shape[12]={0},contact[4]={0};
                if(type<1||type>3)continue;
                for(unsigned k=0;k<3;++k)shape[k]=real(record,8+4*k);
                if(type==1)shape[3]=real(record,4);
                else{for(unsigned k=0;k<3;++k)shape[4+k]=real(record,20+4*k);shape[8]=real(record,4);}
                unsigned fn=type==1?0x168198:type==2?0x167b5c:0x1683dc;
                for(unsigned i=0;i<3&&!world_counter_seen;++i){
                    WDetector *d=world_detectors+i;if(d->remaining<=0)continue;
                    float capsule[12]={d->point[0],d->point[1],d->point[2],0,
                        d->point[0],d->point[1]+50,d->point[2],0,120,0,0,0};
                    if(((unsigned (*)(const void *,const void *,void *))(main_base+fn))(shape,capsule,contact)){
                        log_event("world_detector_contact",(u32)(uptr)a,word(attack,0xc),
                            byte(attack,0x10)|(byte(attack,0x3a)<<8),type|(i<<8));
                        world_counter_accept(a,byte(attack,0x3a));
                        world_detected_attack=(u32)(uptr)attack;
                    }
                }
            }
        }
    }
}
static u32 world_attack_shapes(void *manager)
{
    u32 result=world_original_attack_shapes(manager);
    void *a=(void *)(uptr)world.actor;
    if(!a||!world.move||!world_bound||world_counter_seen)return result;
    /* Native16b15c performs damage-packet and previous-attack-ID filtering
     * before16bb38. Counter shells must not inherit those body-only filters.
     *17260c has just updated every active attack's world-space geometry. */
    void *attack=(void *)(uptr)word(manager,0x2020);
    for(unsigned visited=0;attack&&visited<4096&&!world_counter_seen;++visited){
        if(byte(attack,7)==world.epoch&&(byte(attack,8)&1)&&
           ((unsigned (*)(void *))(main_base+0x169328))(attack))
            world_detect_attack(manager,attack,a);
        attack=(void *)(uptr)word(attack,0x2c);
    }
    return result;
}
static u32 world_player_query(void *manager,void *attack,void *a)
{
    /* Helm's physics owner spans the animation rebinds. Keep the body query
     * disabled across those handoffs until the actor touches the floor. */
    unsigned action=u16(a,ACTION);
    if(a==(void *)(uptr)world.actor&&world.move>=W_RISE&&world.move<=W_DOWN&&
       world_air_owner(a)&&local_guild(a)&&u16(a,STATE)==16&&
       action>=w_action(W_RISE)&&action<=w_action(W_DOWN)&&
       real(a,0x44)>real(a,0x10f0)+1)
        return 0;

    unsigned watched=a==(void *)(uptr)world.actor&&world_detected_attack==(u32)(uptr)attack;
    unsigned before=watched?byte(a,0x31e0):0;
    u32 result=world_original_player_query(manager,attack,a);
    if(watched){
        log_event("world_detector_body",(u32)(uptr)a,before,byte(a,0x31e0),(u32)real(a,FRAME));
        world_detected_attack=0;
    }
    return result;
}
