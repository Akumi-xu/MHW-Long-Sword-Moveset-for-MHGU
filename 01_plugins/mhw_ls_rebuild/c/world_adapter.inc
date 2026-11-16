/* Included in the guarded native adapter. Private actions never enter GU's
 * stock weapon switch. Native hurt/scene requests retain their destination. */
static WState world;
#include "world_gauge_gain.h"
static unsigned world_normal_gauge_tenths;
static NativeCall world_original_gauge_gain;
static u32 world_gauge_gain(void *a,u32 amount,u32 c,u32 d)
{
    int gain=(int)amount;
    unsigned own=local_guild(a)&&world.actor==(u32)(uptr)a&&world.epoch==byte(a,AREA)&&
        (int16_t)u16(a,HP)>0&&!world.move&&world_normal_gauge_action(u16(a,STATE),u16(a,ACTION));
    unsigned before=own?word(a,0x2780):0;
    if(own&&before<100)gain=world_normal_gauge_gain(gain,&world_normal_gauge_tenths);
    u32 result=world_original_gauge_gain(a,(u32)gain,c,d);
    if(own){
        if(word(a,0x2780)>=100)world_normal_gauge_tenths=0;
        if((int)amount>0)log_event("world_normal_gain",(u32)(uptr)a,amount,(u32)gain,(before<<16)|word(a,0x2780));
    }
    return result;
}
static unsigned world_loop_actor,world_loop_epoch,world_loop_action,world_loop_facing;
static void world_loop_heading(void *a)
{
    if(!world_loop_actor)return;
    if(world_loop_actor!=(u32)(uptr)a||world_loop_epoch!=byte(a,AREA)||
       u16(a,STATE)!=16||u16(a,ACTION)!=world_loop_action||(int16_t)u16(a,HP)<=0||world.move){
        world_loop_actor=0;return;
    }
    put(a,HEADING,world_loop_facing);put(a,TARGET,world_loop_facing);
}
#include "world_blend.h"
static float world_entry_blend;
static void *world_idle_actor;static unsigned world_idle_motion,world_idle_epoch;
static float world_entry_start,world_entry_speed=1;
#if ML_WORLD_INPUT_VM
static unsigned world_input_trace_sample,world_input_trace_actor,world_input_trace_flags[5];
static void world_input_trace(void *a,const WObservation *o)
{
    if(world_input_trace_sample==sample_sequence&&world_input_trace_actor==(u32)(uptr)a)return;
    if(world_input_trace_actor!=(u32)(uptr)a){
        world_input_trace_actor=(u32)(uptr)a;
        for(unsigned i=0;i<5;++i)world_input_trace_flags[i]=0;
        log_event("world_input_vm",(u32)(uptr)a,1,0,0);
    }
    world_input_trace_sample=sample_sequence;
    const WCommands *c=&world.controls.commands;
    static const unsigned commands[]={25,32,39,53,60};
    unsigned source=w_source(o->base.motion);
    if((o->state==1||o->state==2)&&o->base.native_action<=5)source=1;
    const WEvent *e=w_event(source,o->base.animation_frame);
    for(unsigned i=0;i<5;++i){
        unsigned id=commands[i],word=id/32,bit=1u<<(id%32);
        unsigned flags=((c->raw[word]&bit)!=0)|(((c->observed[word]&bit)!=0)<<1)|
            (((c->buffered[word]&bit)!=0)<<2)|(((c->executable[word]&bit)!=0)<<3)|
            (((c->raw_executable[word]&bit)!=0)<<4);
        unsigned p=world_command_bits[id];unsigned permission=e?(e->input[(p-1)/32]>>((p-1)%32))&3:0;
        unsigned record=flags|(o->base.held<<8)|(permission<<16);
        if(record==world_input_trace_flags[i])continue;
        world_input_trace_flags[i]=record;
        log_event("world_input_command",(u32)(uptr)a,id|(source<<8),
            record,(u32)(o->base.animation_frame*1000));
    }
}
#endif
/* Preserve a native solo request until its raw key has matured AND source
 * TIML permits execution. The raw-key clock already ran during recovery. */
static struct {unsigned active,actor,epoch,motion,generation,action,flags,button,owner_action,owner_state;} world_r_gate;
#include "world_hits.h"
static unsigned world_bound,world_bound_actor,world_bound_action,world_last_sample;
static float world_regen,world_helm_regen,world_regen_fraction;
static float world_regen_hud_frame;
static void (*world_original_spirit_hud)(void *);
static void world_spirit_hud(void *hud)
{
    extern void ml_gauge_glow(unsigned);
    ml_gauge_glow(0);
    world_original_spirit_hud(hud);
    float remaining=world_regen>world_helm_regen?world_regen:world_helm_regen;
    if(remaining<=0)return;
    void *a=(void *)(uptr)world.actor;
    if(!local_guild(a)||byte(a,AREA)!=world.epoch||(int16_t)u16(a,HP)<=0)return;
    /* Sampled warm white peak and envelope in the World training recording. */
    float frame=world_regen_hud_frame;
    float strength=frame<20?0:frame<25?(frame-20)*0.14f:
        frame<35?0.7f+(frame-25)*0.03f:frame<55?(55-frame)*0.05f:0;
    unsigned gauge=word(a,0x2780);
    if(strength>0&&gauge<=100)ml_gauge_glow((gauge<<8)|(unsigned)(strength*255));

}
static void (*world_original_timers)(void *);
static void world_timers(void *a)
{
    unsigned own=local_guild(a)&&world.actor==(u32)(uptr)a&&world.epoch==byte(a,AREA)&&
        (int16_t)u16(a,HP)>0&&(world_regen>0||world_helm_regen>0);
    unsigned gauge=word(a,0x2780);float decay=real(a,0x3328);
    world_original_timers(a);
    /* Native119eef0 contains passive gauge decay and other independent timers.
     * Source skips only that decay while either regeneration effect is active. */
    if(own){put(a,0x2780,gauge);*(float *)((uptr)a+0x3328)=decay;}
}
static int world_hit=-1;
static unsigned world_attack_id,world_attack_actor,world_attack_owned,world_contact_seen;
static unsigned world_counter_seen;
static void set_real(void *a,u32 at,float x){*(float *)((uptr)a+at)=x;}
#include "world_physics.inc"
typedef float (__attribute__((pcs("aapcs-vfp"))) *AttackScale)(void *);
static AttackScale world_original_attack_scale;
static float __attribute__((pcs("aapcs-vfp"))) world_attack_scale(void *collider)
{
    float scale=world_original_attack_scale(collider);
    if(world.move==W_FORESIGHT&&world_attack_owned&&world.foresight_gauge&&
       (world.counter_success||world_counter_seen)&&
       (uptr)collider==(uptr)world_attack_actor+0x3020&&u16(collider,0x34)==world_attack_id)
        scale*=1.3f; /* Source wp03_param offset1317, native physical scale. */
    return scale;
}
static u32 (*world_original_damage)(void *);
static u32 (*world_original_hp)(void *,int,void *,u32);
static float world_armor;
static unsigned world_damage_depth;
static unsigned world_super_depth;
static u32 (*world_original_flinch_resist)(void *);
static u32 world_flinch_resist(void *a)
{
    /* GU 2ee508 asks this predicate only for its native light reactions.
     * Retain its subsequent KO, status and heavy-reaction decisions. */
    if(world_super_depth && a==(void *)(uptr)world.actor)return 1;
    return world_original_flinch_resist(a);
}
#if ML_WORLD_PRESENTATION
static void world_audio_epv(void *,unsigned,const float *);
static void world_audio_blade(void *,const float *);
static void world_audio_tail(void *,float,const float *);
#endif
#if ML_WORLD_PRESENTATION
static void world_big_counter_flash(void *);
#endif
#include "world_detector.inc"
static u32 world_hp(void *a,int damage,void *result,u32 flags)
{
    int original=damage;unsigned before=u16(a,HP);
    /* Native2F0C9C receives a signed HP delta: its callers pass -damage.
     * Keep healing untouched and retain at least one point of real damage. */
    if(world_damage_depth&&a==(void *)(uptr)world.actor&&world_armor>0&&damage<0)
        damage=damage<-1?damage/2:-1;
    u32 value=world_original_hp(a,damage,result,flags);
    if(a==(void *)(uptr)world.actor&&original<0)
        log_event("world_hp_hit",(u32)(uptr)a,(u32)original,(u32)damage,(before<<16)|u16(a,HP));
    return value;
}
static u32 world_damage(void *a)
{
    if(!local_guild(a)||world.actor!=(u32)(uptr)a||world.epoch!=byte(a,AREA)||
       (int16_t)u16(a,HP)<=0)return world_original_damage(a);
    unsigned packet=byte(a,0x31e0),type=byte(a,0x3216);
    unsigned eligible=packet&&(int16_t)u16(a,0x321c)>=0&&world_counter_type(type);
    if(packet&&world.move)log_event("world_incoming",(u32)(uptr)a,world.source,(u32)real(a,FRAME),type|(eligible<<16));
    unsigned super=0;
    if(eligible&&world.move&&world_bound&&u16(a,STATE)==16&&
       u16(a,ACTION)==w_action(world.move)&&u16(a,MOTION)==0x500+world.slot){
        float f=real(a,FRAME);const WEvent *e=w_event(world.source,f);
        /* Original PlMotionCommon::mFlag bit25 feeds player+8948 bit22.
         * Read the animation key at contact so recovery loses armor on time. */
        super=world.move!=W_GROUND&&e&&(e->common&0x02000000u);
        unsigned iai=(world.move==W_SMALL||world.move==W_BIG)&&e&&(e->common&0x04000000u);
        unsigned foresight=world.move==W_FORESIGHT&&world.foresight_gauge&&f>=0&&f<45;
        if(iai||foresight){
            *(unsigned char *)((uptr)a+0x31e0)=0;
        }
    }
    unsigned armored=eligible&&world_armor>0;
    /* Native collision37694c adds attack+56 to the pending signed halfword
     *3226. Damage2f38f8 feeds it to2f42d0 (KO accumulator2898, threshold50).
     * World PLP Hyper_Armor_Stun_Damage_Rate is0. Suppress only this packet;
     * do not clear existing KO buildup, its decay clock, or other statuses. */
    unsigned incoming_stun=u16(a,0x3226),stun_before=word(a,0x2898);
    if(armored||super)*(uint16_t *)((uptr)a+0x3226)=0;
    if(super)log_event("world_super_hit",(u32)(uptr)a,world.source,(u32)real(a,FRAME),type);
    world_super_depth+=super;
    world_damage_depth+=armored;
    u32 result=world_original_damage(a);
    world_damage_depth-=armored;
    world_super_depth-=super;
    if(eligible&&incoming_stun)
        log_event("world_stun",(u32)(uptr)a,incoming_stun|(armored<<16)|(super<<17),stun_before,word(a,0x2898));
    return result;
}
static void world_close_hit(void *a)
{
    if(world_attack_owned&&world_attack_actor==(u32)(uptr)a&&u16(a,0x3054)==world_attack_id)
        *(unsigned char *)((uptr)a+0x3025)=0;
    world_attack_owned=0;world_hit=-1;world_contact_seen=0;
}
static unsigned world_contact(void *a)
{
    if(!world_attack_owned||world_attack_actor!=(u32)(uptr)a||u16(a,0x3054)!=world_attack_id||
       byte(a,0x30d4)!=1||!word(a,0x30cc))return 0;
    unsigned target=word(a,0x30d0);
    void *manager=(void *)(uptr)*(u32 *)(main_base+0x1851fe0);
    if(!target||!manager)return 0;
    /* Validate ownership through the live registry before following a pointer
     * retained by last frame's collision result. */
    for(unsigned i=0;i<10;++i)if(word(manager,0x1c+4*i)==target)
        return word((void *)(uptr)target,0x1428)!=0&&byte((void *)(uptr)target,AREA)==byte(a,AREA);
    return 0;
}
#if ML_WORLD_PRESENTATION
static void world_helm_impact(void *,const float *);
static void world_helm_line(void *,const float *);
static void world_sfx_emit(void *,const float *,unsigned);
#endif
#include "world_follow.inc"
#include "world_camera.inc"
#include "world_audio.inc"
#if ML_WORLD_PRESENTATION
#include "world_simple_fx.inc"
#else
#include "world_effects.inc"
#endif
static unsigned world_ready(void *a)
{
    void *r=(void *)(uptr)word(a,0xf04+20);
    void *bank=r?(void *)(uptr)word(r,0x64):0;
    void *h=(void *)(uptr)word(a,0x3198),*s=(void *)(uptr)word(a,0x319c);
    void *hdp=h?(void *)(uptr)word(h,0x64):0,*hts=s?(void *)(uptr)word(s,0x64):0;
    return bank&&word(bank,0)==0x00544d4c&&u16(bank,6)>246&&word(bank,8+246*4)&&
        hdp&&word(hdp,0)==0x00504448&&word(hdp,8)>=WORLD_HIT_COUNT&&
        hts&&word(hts,0)==0x00535448&&word(hts,8)>=WORLD_SHAPE_COUNT;
}
static void world_pay(void *a,WDecision d)
{
    if(d.rank_delta)((void (*)(void *,int))(main_base+0x119ed98))(a,d.rank_delta);
    /* Native gauge subtraction can refuse during GU's no-cost timer. Foresight
     * spends the whole source gauge, including its full-gauge glow timer. */
    if(d.gauge_clear){put(a,0x2780,0);set_real(a,0x3334,0);}
    if(d.gauge_delta){int next=(int)word(a,0x2780)+d.gauge_delta;put(a,0x2780,next<0?0:next>100?100:(unsigned)next);}
    if(d.gauge_fill)((void (*)(void *,int,int))(main_base+0x2a309c))(a,100,-1);
    if(d.regen_seconds)world_regen=(float)d.regen_seconds;
}
static void world_cleanup(void *a)
{
    world_close_hit(a);world_counter_seen=0;world_detector_clear();
    if(world_bound&&world_bound_actor==(u32)(uptr)a){
        set_real(a,0x50c,1);set_real(a,0x14b0,1);
        for(unsigned i=0;i<4;++i)put(a,0x1100+4*i,0);
    }
    world_bound=0;
}
static float world_motion_speed(void *a,const WEvent *e)
{
    float speed=w_float(world.source,W_FLOAT_SPEED,real(a,FRAME));
    /* Default to World's Quick Sheathe 3 during its flagged start phase:
     * 1 + (1.4 - 1) * wp03_param8b0(0.4). Do not stack GU's skill again. */
    if(world.move==W_SHEATHE&&(e->common2&0x80000u))speed*=1.16f;
    return speed;
}
static void world_hit_tick(void *a,const WEvent *e)
{
    unsigned helm=e->hit==82&&(world.move==W_DOWN||world.move==W_LAND);
    /* Source COL lingering flag/e4 is a1.5-simulation-frame per-target
     * cooldown (12d71b2 ->12d5370 ->12dd59c). Fresh native IDs bypass GU's
     * once-per-swing ledger; the per-target query enforces that cooldown. */
    if(world_hit==e->hit&&(!helm||world_helm_hit_sample==sample_sequence))return;
    if(helm)world_helm_hit_sample=sample_sequence;
    world_close_hit(a);world_hit=e->hit;
    if(e->hit<0||(helm&&world_helm_count>=7))return;
    const WHit *hit=0;
    for(unsigned i=0;i<sizeof(world_hits)/sizeof(*world_hits);++i)
        if(world_hits[i].source==(unsigned)e->hit){hit=world_hits+i;break;}
    if(!hit)return;
    ((void (*)(void *,u32,u32,u32))(main_base+0x2b7240))(a,0,hit->hit,0);
    world_attack_id=u16(a,0x3054);world_attack_actor=(u32)(uptr)a;world_attack_owned=1;
    put(a,0x30a8,hit->shape);
#if ML_WORLD_PRESENTATION
    /* Hide only the first Iai contact flash; damage and source sound remain. */
    /* v1.0.1 retains the native first-contact Iai flash. */
#endif
    set_real(a,0x307c,0);set_real(a,0x3080,10000);
    if(world.move==W_THRUST||world.move==W_DOWN||world.move==W_LAND)
        put(a,0x3050,word(a,0x3050)|0x08000000u);
    log_event("world_hit_open",(u32)(uptr)a,e->hit,hit->hit,(u32)real(a,FRAME));
}
static void world_commit(void *a,WDecision d)
{
    unsigned owned_abort=d.kind==W_ABORT && world_bound && world_bound_actor==(u32)(uptr)a &&
        u16(a,STATE)==16 && u16(a,ACTION)==world_bound_action;
    unsigned gauge_before=word(a,0x2780);
    world_pay(a,d);
#if ML_WORLD_INPUT_VM
    if(d.kind==W_ENTER && d.move==W_GROUND && world.ground_node==1)
        log_event("world_spirit_charge",(u32)(uptr)a,gauge_before,word(a,0x2780),world.spirit_paid);
#endif
    if(d.kind==W_KEEP)return;
    world_idle_actor=0;
    if(d.kind==W_IDLE && world_bound && world_bound_actor==(u32)(uptr)a){
        world_idle_actor=a;world_idle_motion=u16(a,MOTION);world_idle_epoch=byte(a,AREA);
    }
    unsigned loop=d.kind==W_NATIVE&&
        ((d.native_action==25&&(u16(a,MOTION)==0x569||u16(a,MOTION)==0x570))||(d.native_action==4&&u16(a,MOTION)==0x56b));
    world_loop_actor=loop?(u32)(uptr)a:0;
    if(loop){world_loop_epoch=byte(a,AREA);world_loop_action=d.native_action;world_loop_facing=d.heading;}
    world_r_gate.active=0;
    if(d.kind!=W_ENTER||d.move<W_RISE||d.move>W_DOWN)world_physics_end(a);
    if(d.kind==W_ENTER){
#if ML_WORLD_PRESENTATION
        if(d.move==W_LAND)world_helm_landed=world_elapsed;
#endif
#if ML_WORLD_PRESENTATION && WORLD_AUDIO_GROUND
        const float *point=(const float *)((uptr)a+0x40);
        if(
           (d.move==W_FORESIGHT && world.foresight_gauge) || (d.move==W_THRUST && world.rank))
            world_audio_epv(a,25505,point);
        if(d.move==W_BIG && world.rank)world_audio_epv(a,25605,point);
        if(d.move==W_RISE)world_audio_epv(a,25515,point);
        if(d.move==W_LAND){
            if(world_helm_count)world_audio_tail(a,1,world_helm_sound_point);
        }
#endif
        if(d.move==W_RISE){
            world_helm_tier=world.rank;
            world_helm_count=world_helm_hit_sample=world_helm_emitted=0;
            world_helm_line_hit=0;
            for(unsigned i=0;i<7;++i){world_follow[i].live=0;world_follow[i].remaining=0;}
            for(unsigned i=0;i<10;++i)world_primary_cooldown[i].target=0;
            world_rise_dx=real(a,0x3040)-real(a,0x40);
            world_rise_dz=real(a,0x3048)-real(a,0x48);
        }
        if(d.move==W_BIG){world_follow[7]=(WFollow){0};world_camera_begin(a);}
    }
    world_cleanup(a);ml_reset(&controller);
    ++submitting;
    if(d.kind==W_ENTER||d.kind==W_MOTION){
        log_event("world_source_transition",(u32)(uptr)a,w_source(u16(a,MOTION)),d.source,(u32)(real(a,FRAME)*1000));
        world_entry_blend=d.motion_contract?d.blend:world_iai_blend(d.source,w_source(u16(a,MOTION)),real(a,FRAME));
        world_entry_start=d.motion_contract?d.start:0;world_entry_speed=d.motion_contract?d.speed:1;
        put(a,HEADING,d.heading);put(a,TARGET,d.heading);
        if(d.kind==W_ENTER)virtual_request(a,16,w_action(d.move),0);
        log_event("world_enter",(u32)(uptr)a,d.move,d.source,d.slot);
    }else if(d.kind==W_NATIVE){
        log_event("world_native_follow",(u32)(uptr)a,d.native_action,u16(a,MOTION),(u32)real(a,FRAME));
        unsigned flags=d.native_action>=16?0x8000:0;
        if((d.native_action==16||d.native_action==17)&&(u16(a,MOTION)==0x569||u16(a,MOTION)==0x56b)){
            /* Preserve the observed native solo-R request's turn flags.
             *2ab670 consumes1000/8000 differently when adjusting heading. */
            flags=0x1000;
            log_event("world_chord_solo",(u32)(uptr)a,d.native_action,flags,0);
        }
        put(a,HEADING,d.heading);put(a,TARGET,d.heading);
        virtual_request(a,16,d.native_action,flags);
        world_loop_heading(a);
    }else if(d.kind==W_NATIVE_R){
        unsigned current=u16(a,ACTION),previous=u16(a,0x250c),gauge=word(a,0x2780),target=0;
        /* Native reference: I=16/17 powered/unpowered. SpiritStab22
         * resumes II24; upper25 from II18/19/23/24 resumes III27. Native
         * II costs15 and III costs20; selected native handlers own spending. */
        if(current==22)target=gauge>=15?24:0;
        else if(current==25&&(previous==18||previous==19||previous==23||previous==24))target=gauge>=20?27:0;
        else target=gauge>=15?16:17;
        log_event("world_chord_solo",(u32)(uptr)a,target,0x1000,gauge);
        if(target)virtual_request(a,16,target,0x1000);
    }else if(d.kind==W_ROLL){
        put(a,HEADING,d.heading);put(a,TARGET,d.heading);
        virtual_request(a,1,6,12);
    }
    else if(d.kind==W_IDLE)((void (*)(void *,u32,u32,u32,u32,u32,u32))(main_base+0x2b6e94))(a,0,0,0,0,0,0);
    else if(owned_abort)virtual_request(a,1,0,0);
    --submitting;
}
static unsigned world_request(void *a,unsigned state,unsigned action,unsigned flags)
{
    MLObservation raw=observe(a,0);w_keys_tick(&world.keys,&raw);
    if(world.actor!=(u32)(uptr)a||world.epoch!=byte(a,AREA)||(int16_t)u16(a,HP)<=0)return 0;
    if(world_floor_depth&&world_air_owner(a)&&state==64&&action==33)return 1;
    /* GU has already queried the floor at the new X/Z. Its normal snap
     * ends at 30 units; extend only the bound Foresight's ordinary fall. */
    if(world_floor_depth&&world.move==W_FORESIGHT&&world_bound&&world_bound_actor==(u32)(uptr)a&&
       u16(a,STATE)==16&&u16(a,ACTION)==w_action(W_FORESIGHT)&&u16(a,MOTION)==0x500+world.slot&&
       state==4&&action==1){
        float floor=real(a,0x10f0),drop=real(a,0x44)-floor;
        if(drop>0&&drop<=120){
            set_real(a,0x44,floor);set_real(a,0x1014,0);
            *(unsigned char *)((uptr)a+0x2505)=0;
            log_event("world_step_down",(u32)(uptr)a,(u32)(drop*100),action,(u32)real(a,FRAME));return 1;
        }
    }
    if(world_damage_depth&&world_armor>0&&(int16_t)u16(a,HP)>0&&
       (state==4||state==8||state==64))return 1;
#if ML_WORLD_INPUT_VM
    unsigned attack=state==16&&w_guild_attack(action);
    /* Native stick+B uses side rolls 13/14 before the combo VM runs. */
    unsigned roll=state==1&&(action==6||action==13||action==14);
    if(world.move){
        if((attack||roll||(state==1&&action==0)) &&
           u16(a,STATE)==16 && u16(a,ACTION)==w_action(world.move))return 1;
        world_physics_end(a);
        world_commit(a,w_leave(&world));world_cleanup(a);
        world.controls=(WControls){0};
        log_event("world_interrupt",(u32)(uptr)a,state,action,0);
        return 0;
    }
    if(world_r_gate.active&&(!attack||action!=world_r_gate.action))world_r_gate.active=0;
    if(!(attack||roll)||!(byte(a,DRAWN)&1)||
       (word(a,AIR)&0x400)||!world_ready(a))return 0;
    unsigned source=w_guild_source(u16(a,STATE),u16(a,ACTION),u16(a,MOTION));
    if(!source)return 0;
    /* GU can offer III before the pad update or replay it after release.
     * The command VM alone dispatches solo R from these ordinary attacks. */
    if(attack&&(action==16||action==17||action==18||action==19||action==26||action==27)&&
       w_guild_spirit_opener(u16(a,ACTION),u16(a,MOTION),u16(a,0x250c)))return 1;
    unsigned partner=held&(ML_X|ML_A|ML_B|ML_Y|ML_ZR);
    const WEvent *event=w_event(source,source==1?0:real(a,FRAME));
    /* GU can offer the draw-upper before the command VM sees this pad
     * sample. The same thrust's X edge owns the native combo upper. */
    if(attack&&action==7&&((source==105&&u16(a,ACTION)==4)||(source==112&&u16(a,ACTION)==22))&&event&&
       ((event->input[WI_X.word]>>WI_X.shift)&3)&&
       ((held&ML_X)||w_command_has(world.controls.commands.buffered,25)||
        w_command_has(world.controls.commands.executable,25)))return 1;
    /* GU may submit a buffered solo after both physical buttons were
     * released, but before the World partner pulse or execute window.
     * Reserve only a chord actually collected on this same primary motion. */
    const WControls *controls=&world.controls;
    if(world_r_gate.active&&world_r_gate.actor==(u32)(uptr)a&&
       world_r_gate.generation==bind_generation&&world_r_gate.motion==u16(a,MOTION)&&
       world_r_gate.owner_action==u16(a,ACTION))return 1;
    if(event&&world.input_motion==u16(a,MOTION)&&!(held&(ML_Y|ML_ZR))){
        static const unsigned ids[]={53,60,24};
        for(unsigned i=0;i<(source==1?1u:3u);++i){
            unsigned id=ids[i];
            WInput input=i==0?WI_THRUST:i==1?WI_FORESIGHT:WI_SHEATHE;
            if(!(i==1?w_foresight_stage(source,event):((event->input[input.word]>>input.shift)&3)))continue;
            if(world.gu_chord_wait==id||w_command_has(controls->commands.buffered,id)||
               w_command_has(controls->commands.executable,id))return 1;
        }
    }
    /* A late native solo can be submitted on the leading physical edge.
     * In an open chord window keep that exact GU packet until the leading
     * button's World confirmation, or until its overlapping chord wins. */
    if(attack&&(u16(a,STATE)==16||source==1)&&event&&world.input_motion==u16(a,MOTION)&&
       (held==ML_R||held==ML_X||held==ML_A)){
        unsigned button=held==ML_R?2:held==ML_A?1:0;
        WInput input=held==ML_A?WI_FORESIGHT:WI_THRUST;
        unsigned open=(held==ML_A?(w_foresight_stage(source,event)&2):((event->input[input.word]>>input.shift)&2))||
            (held==ML_R&&(((event->input[WI_SHEATHE.word]>>WI_SHEATHE.shift)|
                           w_foresight_stage(source,event))&2));
        const WButton *key=controls->buttons+button;
        /* Idle has the global execute bit, not per-command TIML bits. */
        if(source==1)open=held==ML_R||held==ML_X;
        if(open&&!key->held&&(key->waiting||!key->raw)){
            world_r_gate.active=1;world_r_gate.actor=(u32)(uptr)a;world_r_gate.epoch=byte(a,AREA);
            world_r_gate.motion=u16(a,MOTION);world_r_gate.generation=bind_generation;
            world_r_gate.action=action;world_r_gate.flags=flags;world_r_gate.button=button;
            world_r_gate.owner_action=u16(a,ACTION);world_r_gate.owner_state=u16(a,STATE);return 1;
        }
    }
    if(!(held&ML_R))return 0;
    if(source==1)return partner==ML_X;
    WInput input;
    if(partner==ML_X)input=WI_THRUST;
    else if(partner==ML_A)input=WI_FORESIGHT;
    else if(partner==ML_B)input=WI_SHEATHE;
    else return 0;
    return event && (partner==ML_A?w_foresight_stage(source,event):((event->input[input.word]>>input.shift)&3));
#else
    if(world.move){
        if((state==1&&action<=14)||state==2||state==16)return 1;
        world_commit(a,w_leave(&world));world_cleanup(a);
        log_event("world_interrupt",(u32)(uptr)a,state,action,0);
    }
    unsigned source=w_source(u16(a,MOTION));
    const WEvent *event=w_event(source,real(a,FRAME));
    if(!world.move&&u16(a,STATE)==16&&state==16&&action==16&&
       (source==105||source==107)&&held&ML_A&&!(held&(ML_X|ML_B))&&event&&
       ((event->input[WI_FORESIGHT.word]>>WI_FORESIGHT.shift)&3)&&world_ready(a)){
        /* A native buffered R is still a real command if its tap ended
         * before this request. A held A upgrades it to the World chord. */
        world.queued=W_FORESIGHT;world.queued_mask=ML_R|ML_A;world.queued_source=source;
        world.queued_word=WI_FORESIGHT.word;world.queued_shift=WI_FORESIGHT.shift;
        world.queued_heading=u16(a,HEADING);
        return 1;
    }
    /* GU can publish its buffered X request after the raw tap has ended.
     * Its native permission is authoritative for this ordinary GU motion.
     * Redirect draw-upper7 to combo-upper25, preserving the native timing
     * and current facing rather than inserting draw prep116 / a re-aim. */
    if(!world.move&&source==105&&u16(a,STATE)==16&&u16(a,ACTION)==4&&
       state==16&&action==7&&world_ready(a)&&!(word(a,AIR)&0x400)){
        if(held&ML_R)return 1;
        world.queued=0;world.consumed|=ML_X;
        log_event("world_loop_native",(u32)(uptr)a,action,25,(u32)(real(a,FRAME)*1000));
        world_commit(a,(WDecision){.kind=W_NATIVE,.native_action=25,.heading=u16(a,HEADING)});
        return 1;
    }
    if(!world.move&&u16(a,STATE)==16&&state==16&&action>=16&&action<154&&
       source>=101&&source<=110&&source!=104&&source!=106&&source!=108&&
       world_ready(a)&&!(word(a,AIR)&0x400)&&event&&
       ((event->input[WI_R.word]>>WI_R.shift)&3)&&
       (held==ML_R||(world.queued==0x300&&world.queued_source==source)))return 1;
    if(world_r_gate.active&&world_r_gate.actor==(u32)(uptr)a&&world_r_gate.epoch==byte(a,AREA)&&
       world_r_gate.motion==u16(a,MOTION)&&world_r_gate.generation==bind_generation&&
       state==16&&action==world_r_gate.action)return 1;
    if(!world.move&&u16(a,STATE)==16&&source&&event&&world_ready(a)&&
       !(word(a,AIR)&0x400)&&state==16&&action>=16&&action<154&&
       held==ML_R&&((event->input[WI_FORESIGHT.word]>>WI_FORESIGHT.shift)&3)&&
       (!(world.keys.ready&ML_R)||((event->input[WI_R.word]>>WI_R.shift)&3)!=3)){
        if(!world_r_gate.active){
            world_r_gate.active=1;world_r_gate.actor=(u32)(uptr)a;world_r_gate.epoch=byte(a,AREA);
            world_r_gate.motion=u16(a,MOTION);world_r_gate.generation=bind_generation;
            world_r_gate.action=action;world_r_gate.flags=flags;
            log_event("world_chord_wait",(u32)(uptr)a,action,u16(a,MOTION),(u32)real(a,FRAME));
        }
        return 1;
    }
    if(!world.move&&u16(a,STATE)==16&&state==16&&action<154&&
       !(held&(ML_R|ML_B|ML_Y))&&(held&(ML_X|ML_A))!=(ML_X|ML_A)&&
       ((source==105&&(held&ML_X))||(source==107&&(held&ML_A))))return 1;
    /* Native R can submit spirit-I before the common update when gauge>0.
     * Reserve the neutral R+X gesture at that request boundary as well. */
    if((held&(ML_R|ML_X))==(ML_R|ML_X)&&!(held&(ML_A|ML_B))&&
       (byte(a,DRAWN)&1)&&!(word(a,AIR)&0x400)&&world_ready(a)&&
       (u16(a,STATE)==1||u16(a,STATE)==2)&&u16(a,ACTION)<=5&&state==16)return 1;
    /* Reserve only a recognized source combo; native X+A fade stays native. */
    unsigned selection=(held&ML_R)&&((held&(ML_X|ML_A|ML_B))!=0);
    if(selection&&w_source(u16(a,MOTION))&&u16(a,STATE)==16&&
       ((state==16&&action<154)||(state==1&&(action==0||action==6))))return 1;
    return 0;
#endif
}
static unsigned world_update(void *a,const MLObservation *b)
{
    world_loop_heading(a);
#if ML_WORLD_AUDIO_PROBE || ML_WORLD_PRESENTATION
    world_audio_tick(a);
#endif
    world_fx_tick(a);
    WObservation o={.base=*b,.state=u16(a,STATE),.previous_weapon_action=u16(a,0x250c),.drawn=byte(a,DRAWN)&1,
        .alive=(int16_t)u16(a,HP)>0,.grounded=!(word(a,AIR)&0x400),
        .rank=((unsigned (*)(void *))(main_base+0x119ed80))(a),.resources_ready=world_ready(a)};
    o.contact=world_contact(a);o.counter=world_counter_seen;
    if(world_air_owner(a))o.grounded=real(a,0x44)<=real(a,0x10f0)+1&&real(a,0x1014)<=0;
    /* Guild's aerial spirit action uses AIR=0x2008, not the ground handler's 0x400 bit. */
    if(o.state==16&&((b->native_action>=72&&b->native_action<=75&&b->motion>=0x599&&b->motion<=0x59c)||b->motion==0x598))
        o.grounded=real(a,0x44)<=real(a,0x10f0)+1&&real(a,0x1014)<=0;
    if(o.contact&&!world_contact_seen){world_contact_seen=1;
        log_event("world_contact",(u32)(uptr)a,world.source,word(a,0x30d0),word(a,0x30cc));}
    if(world.actor!=(u32)(uptr)a||world.epoch!=o.base.epoch){
        world_normal_gauge_tenths=0;
        world_physics_end(a);world_cleanup(a);world_regen=world_helm_regen=world_regen_fraction=world_armor=0;
        world_follow_clear();
    }
    if(world_last_sample!=sample_sequence){
        world_last_sample=sample_sequence;float dt=o.base.simulation_delta/60.0f;
        if(dt>0&&dt<.5f){
            world_detector_tick(a,dt);
            if(o.alive&&o.resources_ready)world_follow_tick(a,dt);
            if(world_armor>0)world_armor-=dt;
            if(world_regen>0||world_helm_regen>0){
                /* World capture: consecutive gauge pulses are 61 simulation frames apart. */
                world_regen_hud_frame+=o.base.simulation_delta;
                if(world_regen_hud_frame>=61.0f)world_regen_hud_frame-=61.0f;
                float used=0;
                if(world_regen>0){float t=world_regen<dt?world_regen:dt;world_regen-=t;used+=t;}
                if(world_helm_regen>0){float t=world_helm_regen<dt?world_helm_regen:dt;world_helm_regen-=t;used+=t;}
                /* Source2123a72..2123b50 adds dt / rate15 to a0..1 gauge.
                 * Helm and Iai have independent timers and add together. */
                world_regen_fraction+=used*(100.0f/15.0f);
                unsigned points=(unsigned)world_regen_fraction;
                if(points){world_regen_fraction-=(float)points;
                    unsigned gauge=word(a,0x2780),next=gauge+points;
                    put(a,0x2780,next>100?100:next);
                    if(next>=100)world_regen_fraction=0;
                    if(gauge<100&&next>=100){
                        typedef void (__attribute__((pcs("aapcs-vfp"))) *FullGauge)(void *,float);
                        ((FullGauge)(main_base+0x119ee7c))(a,1800.0f);
                    }}
            }else world_regen_hud_frame=0;
        }
        if(!o.alive||!o.resources_ready){world_normal_gauge_tenths=0;world_regen=world_helm_regen=world_armor=0;world_follow_clear();}
    }
    if(world.move&&u16(a,ACTION)==w_action(world.move)&&!world_bound){
        ((void (*)(void *,u32,u32,u32))(main_base+0x282fd8))(a,0,0,0);
        *(unsigned char *)((uptr)a+5)=1;
        /* GU53a1dc adds two frames to positive blends and otherwise sets
         * internal10000 (root motion during interpolation). Flag80 avoids
         * that stock root mode; flag1 lets native280c1c advance the action
         * clock while poses blend (default GU freezes it until blend ends).
         * Keep the source duration, frame0 and unmodified event clock. */
        original_layer(a,0,5000+world.slot,world_entry_blend>0?0x81:0,
                       world_entry_blend>2?world_entry_blend-2:0,world_entry_start,world_entry_speed);
        log_event("world_blend",(u32)(uptr)a,world.source,(u32)world_entry_blend,(u32)real(a,0x4f4));
        /* Stock common action1:2 uses vtable46c / 2a2560(a,0) to sheath.
         * Keep its packed flags intact while playing source cIaigiriWpOff312. */
        if(world.move==W_SHEATHE_END)
            ((void (*)(void *,unsigned))(main_base+0x2a2560))(a,0);
        world_physics_begin(a,world.move,o.base.magnitude);
        world_bound=1;world_bound_action=w_action(world.move);world_bound_actor=(u32)(uptr)a;
#if ML_WORLD_AUDIO_PROBE || ML_WORLD_PRESENTATION
        world_audio_begin(a);
#endif
        world_fx_begin(a);
        world_detector_begin(a);
        o.base=observe(a,1);
        log_event("world_bound",(u32)(uptr)a,world.move,u16(a,MOTION),word(a,0x1c));
        if(world.move==W_GROUND)log_event("world_ground_phase",(u32)(uptr)a,world.source,
            world.ground_node,(u32)(real(a,FRAME)*1000));
    }
    unsigned heading;
    if(w_rotation(&world,&o,&heading)){
        put(a,HEADING,heading);put(a,TARGET,heading);o.base.heading=heading;
    }
    if(world.move==W_BIG&&world_bound&&u16(a,MOTION)==0x500+world.slot){
        /* World's animated root turns the actor, not just the skeleton's
         * extraction space. Keep the entry aim separate from that half turn.
         * Native GU pose and floor/collision evaluation remain authoritative. */
        unsigned facing=(world.heading+w_root_heading(world.source,real(a,FRAME)))&65535u;
        put(a,HEADING,facing);put(a,TARGET,facing);o.base.heading=facing;
    }
    WDecision d=w_step(&world,&o);
#if ML_WORLD_INPUT_VM
    world_input_trace(a,&o);
#endif
    world_commit(a,d);w_input_finish(&world,&o,d);
    if(world_r_gate.active){
        unsigned valid=world_r_gate.actor==(u32)(uptr)a&&world_r_gate.epoch==byte(a,AREA)&&
            o.alive&&o.drawn&&o.grounded&&o.resources_ready&&!world.move&&
            u16(a,MOTION)==world_r_gate.motion&&bind_generation==world_r_gate.generation;
#if ML_WORLD_INPUT_VM
        if(!valid||u16(a,STATE)!=world_r_gate.owner_state||u16(a,ACTION)!=world_r_gate.owner_action)world_r_gate.active=0;
        else if(!world.controls.buttons[world_r_gate.button].waiting&&!world.gu_chord_wait){
            unsigned action=world_r_gate.action,flags=world_r_gate.flags;world_r_gate.active=0;
            ++submitting;virtual_request(a,16,action,flags);--submitting;
            return 1;
        }
#else
        if(!valid||u16(a,STATE)!=16||(held&(ML_A|ML_X|ML_B|ML_Y))||world.queued){
            world_r_gate.active=0;
        }else{
            const WEvent *solo=w_event(w_source(u16(a,MOTION)),real(a,FRAME));
            unsigned stage=solo?(solo->input[WI_R.word]>>WI_R.shift)&3:0;
            if(!stage)world_r_gate.active=0;
            else if((world.keys.ready&ML_R)&&stage==3){
                unsigned action=world_r_gate.action,flags=world_r_gate.flags;world_r_gate.active=0;
                ++submitting;virtual_request(a,16,action,flags);--submitting;
                log_event("world_chord_solo",(u32)(uptr)a,action,flags,0);
                return 1;
            }
        }
 #endif
    }
    if(world.move&&world_bound&&u16(a,MOTION)==0x500+world.slot){
        const WEvent *e=w_event(world.source,real(a,FRAME));
        if(e){float speed=world_motion_speed(a,e);set_real(a,0x50c,speed);set_real(a,0x14b0,speed);world_hit_tick(a,e);}
        world_physics_tick(a);
        if(world.move==W_SHEATHE&&real(a,FRAME)>=w_clip(world.source)->length-1){
            set_real(a,0x50c,0);set_real(a,0x14b0,0);
        }
    }
    return world.move||d.kind==W_ENTER||d.kind==W_NATIVE||d.kind==W_NATIVE_R||d.kind==W_ROLL||d.kind==W_IDLE;
}
#include "world_damage_compat.h"
static unsigned world_install(void)
{
#if ML_WORLD_FX_PROBE
    if(!world_fx_install())return 0;
#endif
#if (ML_WORLD_AUDIO_PROBE || ML_WORLD_PRESENTATION) && WORLD_AUDIO_WEAPON
    if(!world_audio_install())return 0;
#endif
    const u32 entries[][3]={
        {0x2a309c,0xe92d4070u,0xe1a04000u},
        {0x64b414,0xe92d48f0u,0xe1a04000u},
        {0x23d80,0xe92d4070u,0xed2d8b02u},
        {0x2ee508,0xe92d4ff0u,0xe28db01cu},
        {0x2f0c9c,0xe92d47f0u,0xe1a07002u},
        {0x2f4ee0,0xe92d4070u,0xe1a04000u},
        {0x169548,0xe92d4830u,0xe1a04000u},
        {0x119eef0,0xe92d4830u,0xe1a04000u},
        {0x16bb38,0xe92d4ff0u,0xe28db01cu},
        {0x16c0c0,0xe92d4ff0u,0xe28db01cu},
        {0x171350,0xe92d4ff0u,0xe24dd004u},
        {0x474674,0xe92d4830u,0xe1a04000u},
        {0x17260c,0xe92d4ff0u,0xe24dd0e4u},
        {0x2e846c,0xe92d4df0u,0xe28db018u}
    };
    for(unsigned i=0;i<sizeof(entries)/sizeof(*entries);++i){
        const u32 *entry=(const u32 *)(main_base+entries[i][0]);
        const u32 *detour=entry[0]==0xe51ff004u&&entry[1]&&!(entry[1]&3)?(const u32 *)(uptr)entry[1]:0;
        unsigned valid=entries[i][0]==0x2ee508?world_player_damage_entry_matches(entry,detour):
            entries[i][0]==0x2e846c?world_floor_entry_matches(entry,detour):
            entry[0]==entries[i][1]&&entry[1]==entries[i][2];
        if(!valid){
            log_event("world_entry_refused",entries[i][0],entry[0],entry[1],entries[i][1]);
            return 0;
        }
    }
    const u32 *monster_entry=(const u32 *)(main_base+0x170968);
    const u32 *monster_detour=monster_entry[0]==0xe51ff004u&&monster_entry[1]&&!(monster_entry[1]&3)?(const u32 *)(uptr)monster_entry[1]:0;
    if(!world_damage_entry_matches(monster_entry,monster_detour)){
        log_event("world_entry_refused",0x170968,monster_entry[0],monster_entry[1],0xe92d41f0u);return 0;
    }
    forge_hook_create((void *)(main_base+0x2a309c),(void *)world_gauge_gain,(void **)&world_original_gauge_gain);
    forge_hook_create((void *)(main_base+0x2ee508),(void *)world_damage,(void **)&world_original_damage);
    forge_hook_create((void *)(main_base+0x2f4ee0),(void *)world_flinch_resist,(void **)&world_original_flinch_resist);
    forge_hook_create((void *)(main_base+0x2f0c9c),(void *)world_hp,(void **)&world_original_hp);
    forge_hook_create((void *)(main_base+0x2e846c),(void *)world_floor,(void **)&world_original_floor);
    forge_hook_create((void *)(main_base+0x169548),(void *)world_attack_scale,(void **)&world_original_attack_scale);
    forge_hook_create((void *)(main_base+0x64b414),(void *)world_spirit_hud,(void **)&world_original_spirit_hud);
    forge_hook_create((void *)(main_base+0x119eef0),(void *)world_timers,(void **)&world_original_timers);
    forge_hook_create((void *)(main_base+0x16bb38),(void *)world_player_query,(void **)&world_original_player_query);
    forge_hook_create((void *)(main_base+0x16c0c0),(void *)world_monster_query,(void **)&world_original_monster_query);
    forge_hook_create((void *)(main_base+0x170968),(void *)world_monster_damage,(void **)&world_original_monster_damage);
    forge_hook_create((void *)(main_base+0x474674),(void *)world_shell_position,(void **)&world_original_shell_position);
    forge_hook_create((void *)(main_base+0x17260c),(void *)world_attack_shapes,(void **)&world_original_attack_shapes);
    forge_hook_create((void *)(main_base+0x23d80),(void *)world_camera,(void **)&world_original_camera);
    return world_original_gauge_gain&&world_original_spirit_hud&&world_original_flinch_resist&&world_original_damage&&world_original_hp&&world_original_floor&&world_original_attack_scale&&world_original_timers&&world_original_player_query&&world_original_monster_query&&world_original_monster_damage&&world_original_shell_position&&world_original_attack_shapes&&world_original_camera;
}
