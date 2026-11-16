/* Private native sound resources. The probe mailbox exists only in an explicit
 * audio development build; it does not infer or substitute source events. */
#if ML_WORLD_AUDIO_PROBE || ML_WORLD_PRESENTATION
#include "world_audio_events.h"
#include "world_audio_index.h"
static const WAudioCueRange *world_audio_active_range;
static unsigned world_audio_range_source=~0u;
static void *world_audio_archive,*world_audio_request;
static unsigned world_audio_failed,world_audio_ready;
#if ML_WORLD_AUDIO_PROBE
static volatile unsigned world_audio_probe_command;
#else
static const unsigned world_audio_probe_command;
#endif
static unsigned world_audio_seen[(WORLD_AUDIO_CUE_COUNT+31)/32],world_audio_actor,world_audio_epoch,world_audio_sample;
static float world_audio_clock;
typedef struct {unsigned active,request,actor,epoch;float remaining,point[3];} WAudioJob;
static WAudioJob world_audio_jobs[16];
#if WORLD_AUDIO_WEAPON
static unsigned world_audio_last_choice[WORLD_AUDIO_WEAPON_GROUP_COUNT],world_audio_rng;
static u32 (*world_audio_original_position)(void *);
static unsigned world_audio_live_actor,world_audio_live_epoch;
typedef struct {unsigned serial,actor,epoch;} WAudioSpatialVoice;
static WAudioSpatialVoice world_audio_spatial_voices[128];
static unsigned world_audio_weapon_point(void *a,unsigned request,float *point)
{
    if(request<6||request>=WORLD_AUDIO_CONDITIONAL_FIRST)return 0;
#if WORLD_AUDIO_GROUND
    if(request==WORLD_AUDIO_HELM_WAVE)return 0;
#endif
    void *weapon=(void *)(uptr)word(a,0x23a0);
    if(!weapon||word(weapon,0x30)!=(u32)(uptr)a||word(weapon,0x13d0)!=9)return 0;
    uptr map=word(weapon,0x498);if(!map)return 0;
    const float *m=world_raw_bone(weapon,*(const unsigned char *)map);
    if(!m)return 0;
    const float *offset=world_audio_weapon_points[request];
    for(unsigned k=0;k<3;++k)point[k]=m[12+k]+m[k]*offset[0]+m[4+k]*offset[1]+m[8+k]*offset[2];
    return 1;
}
static u32 world_audio_position(void *voice)
{
    /* This native callback runs immediately before distance/panning is
     * evaluated. Only our private requests are changed; the normal mixer,
     * attenuation and voice lifecycle still belong to GU. */
    if(world_audio_ready&&word(voice,12)==(u32)(uptr)world_audio_request&&word(voice,172)==1&&
       word(voice,20)==world_audio_live_actor&&world_audio_live_actor){
        void *manager=*(void **)(main_base+0x2126d8c);
        uptr first=(uptr)manager+64,at=(uptr)voice;
        if(manager&&at>=first&&(at-first)%2784==0&&(at-first)/2784<128){
            WAudioSpatialVoice *v=world_audio_spatial_voices+(at-first)/2784;
            unsigned serial=word(voice,52),actor=world_audio_live_actor;
            if(v->serial!=serial||v->actor!=actor){
                *v=(WAudioSpatialVoice){serial,actor,world_audio_live_epoch};
                if(word(voice,16)>=6)log_event("world_audio_follow",actor,word(voice,16),at,serial);
            }
            void *a=(void *)(uptr)actor;float point[3];
            if(v->epoch==world_audio_live_epoch&&word(a,0)==main_base+VT&&
               byte(a,AREA)==v->epoch&&(int16_t)u16(a,HP)>0&&world_audio_weapon_point(a,word(voice,16),point))
                for(unsigned k=0;k<3;++k)set_real(voice,64+4*k,point[k]);
        }
    }
    return world_audio_original_position(voice);
}
static unsigned world_audio_install(void)
{
    if(*(u32 *)(main_base+0xbe28c4)!=0xe92d4ff0u||*(u32 *)(main_base+0xbe28c8)!=0xe28db01cu)return 0;
    forge_hook_create((void *)(main_base+0xbe28c4),(void *)world_audio_position,(void **)&world_audio_original_position);
    return world_audio_original_position!=0;
}
#endif

static void world_audio_play(void *a,unsigned request,const float *point)
{
    void *sm=*(void **)(main_base+0x1896f5c);if(!sm||!world_audio_ready)return;
    ((void (*)(void *,u32,void *,u32,void *,const float *,void *))(main_base+0x4ee980))
        (sm,byte(a,0x111a)?13:12,world_audio_request,request,a,point,a);
    log_event("world_audio_play",(u32)(uptr)a,request,(u32)(uptr)world_audio_request,(u32)(world_audio_clock*1000000));
}
static void world_audio_begin(void *a)
{
    for(unsigned i=0;i<sizeof(world_audio_seen)/sizeof(*world_audio_seen);++i)world_audio_seen[i]=0;
    world_audio_actor=(u32)(uptr)a;world_audio_epoch=byte(a,AREA);
}
static void world_audio_schedule(void *a,unsigned request,float delay,const float *point)
{
    if(delay<=0){world_audio_play(a,request,point);return;}
    for(unsigned i=0;i<16;++i)if(!world_audio_jobs[i].active){
        world_audio_jobs[i]=(WAudioJob){1,request,(u32)(uptr)a,byte(a,AREA),delay,{point[0],point[1],point[2]}};
        return;
    }
    log_event("world_audio_queue_full",(u32)(uptr)a,request,0,0);
}
#if WORLD_AUDIO_WEAPON
static void world_audio_weapon(void *a,const WAudioCue *cue)
{
    log_event("world_weapon_audio_cue",(u32)(uptr)a,cue->request,(u32)(real(a,FRAME)*1000),world.source);
    for(unsigned i=0;i<WORLD_AUDIO_WEAPON_GROUP_COUNT;++i){
        const WAudioWeaponGroup *g=world_audio_weapon_groups+i;if(g->source_request!=cue->request)continue;
        if(!world_audio_rng)world_audio_rng=(u32)(uptr)a^sample_sequence^0x9e3779b9u;
        unsigned r=world_audio_rng;r^=r<<13;r^=r>>17;r^=r<<5;world_audio_rng=r;
        unsigned last=world_audio_last_choice[i],count=g->count;
        unsigned choice=r%(last&&count>1?count-1:count);
        if(last&&count>1&&choice>=last-1)++choice;
        world_audio_last_choice[i]=choice+1;
        unsigned request=world_audio_weapon_choices[g->offset+choice];
        float point[3]={real(a,0x40),real(a,0x44),real(a,0x48)};
        world_audio_weapon_point(a,request,point);
        world_audio_schedule(a,request,g->delay,point);
        log_event("world_weapon_audio_choice",(u32)(uptr)a,cue->request,request,i);
    }
}
#endif
static void world_audio_epv(void *a,unsigned request,const float *origin)
{
    if(!world_audio_ready)return;
    unsigned yaw=word(a,HEADING);
    void *camera=(void *)(uptr)world_camera_object;
    if(camera&&word(camera,0x85c)==(u32)(uptr)a){
        float x=origin[0]-real(camera,0x40),z=origin[2]-real(camera,0x48);
        if(x*x+z*z>.00001f)yaw=world_direction(x,z);
    }
    float sn=world_sin(yaw),co=world_sin(yaw+16384);
    log_event("world_audio_cue",(u32)(uptr)a,request,(u32)(real(a,FRAME)*1000),(u32)(world_audio_clock*1000000));
    for(unsigned n=0;n<sizeof(world_audio_stems)/sizeof(*world_audio_stems);++n){
        const WAudioStem *stem=world_audio_stems+n;if(stem->source_request!=request)continue;
        float p[3]={origin[0]+co*stem->x+sn*stem->z,origin[1]+stem->y,origin[2]-sn*stem->x+co*stem->z};
        world_audio_schedule(a,stem->request,stem->delay,p);
    }
}
#if WORLD_AUDIO_GROUND
static void world_audio_blade(void *a,const float *point)
{
    world_audio_play(a,WORLD_AUDIO_HELM_WAVE,point);
}
static void world_audio_tail(void *a,float delay,const float *point)
{
    /* Preserve EPV spatial offsets at submission, including the two voices. */
    for(unsigned i=0;i<sizeof(world_audio_jobs)/sizeof(*world_audio_jobs);++i)if(!world_audio_jobs[i].active){
        world_audio_jobs[i]=(WAudioJob){1,25530,(u32)(uptr)a,byte(a,AREA),delay,{point[0],point[1],point[2]}};return;
    }
    log_event("world_audio_queue_full",(u32)(uptr)a,25530,0,0);
}
#else
static void world_audio_blade(void *a,const float *point){(void)a;(void)point;}
static void world_audio_tail(void *a,float d,const float *p){(void)a;(void)d;(void)p;}
#endif
static void world_audio_tick(void *a)
{
    if(world_audio_failed)return;
#if WORLD_AUDIO_WEAPON
    world_audio_live_actor=(u32)(uptr)a;world_audio_live_epoch=byte(a,AREA);
#endif
    if(!world_audio_archive){
        void *manager=*(void **)(main_base+0x2126a64);if(!manager)return;
        uptr *vt=*(uptr **)manager;
        world_audio_archive=((void *(*)(void *,void *,const char *,unsigned))vt[0x30/4])
            (manager,(void *)(main_base+0x2124df0),"arc/forge/world_audio",1);
        if(!world_audio_archive){world_audio_failed=1;return;}
        log_event("world_audio_queued",(u32)(uptr)a,(u32)(uptr)world_audio_archive,(u32)(uptr)&world_audio_probe_command,0);
    }
    if(!world_audio_ready){
        unsigned flags=byte(world_audio_archive,0x50);
        if(flags&0x50){world_audio_failed=1;log_event("world_audio_failed",(u32)(uptr)a,flags,0,0);return;}
        if(!(flags&1))return;
        unsigned n=word(world_audio_archive,0x68);void **objects=*(void ***)((uptr)world_audio_archive+0x64);
        if(n!=WORLD_AUDIO_RESOURCE_COUNT||!objects)return;
        for(unsigned i=0;i<n;++i){
            if(!objects[i])return;
            unsigned child=byte(objects[i],0x50);
            if(child&0x50){world_audio_failed=1;log_event("world_audio_failed",(u32)(uptr)a,child,i,0);return;}
            if(!(child&1))return;
            if(*(uptr *)objects[i]==main_base+0x1794210&&world_resource_path((const char *)objects[i]+8,"sound/se/pl/wp/swo/world/snd_world"))
                world_audio_request=objects[i];
        }
        if(!world_audio_request)return;
        world_audio_ready=1;
        log_event("world_audio_ready",(u32)(uptr)a,(u32)(uptr)world_audio_request,word(world_audio_request,0x68),0);
    }
    unsigned cmd=world_audio_probe_command;
#if ML_WORLD_AUDIO_PROBE
    world_audio_probe_command=0;
#endif
    if(cmd>=1&&cmd<=WORLD_AUDIO_REQUEST_COUNT)world_audio_play(a,cmd-1,(const float *)((uptr)a+0x40));
    if(world_audio_sample!=sample_sequence){
        world_audio_sample=sample_sequence;float dt=real(a,0x1c)/60.0f;
        if(dt>0&&dt<.5f)world_audio_clock+=dt;
        for(unsigned i=0;i<16;++i){
            WAudioJob *j=world_audio_jobs+i;if(!j->active)continue;
            if(j->actor!=(u32)(uptr)a||j->epoch!=byte(a,AREA)||(int16_t)u16(a,HP)<=0){j->active=0;continue;}
            if(dt>0&&dt<.5f)j->remaining-=dt;
            /* A relative countdown avoids a growing clock's float ULP
             * rounding a source0.4s delay up by a whole simulation frame. */
            if(j->remaining<=.000001f){
                if(j->request==25530)world_audio_epv(a,j->request,j->point);else world_audio_play(a,j->request,j->point);
                j->active=0;
            }
        }
    }
    if((world.move==W_GROUND && world.source!=363)||!world_bound||!world.move||world_audio_actor!=(u32)(uptr)a||world_audio_epoch!=byte(a,AREA)||
       (int16_t)u16(a,HP)<=0||u16(a,MOTION)!=0x500+world.slot)return;
    if(world_audio_range_source!=world.source){world_audio_range_source=world.source;world_audio_active_range=world_audio_cue_range(world.source);}
    if(!world_audio_active_range)return;
    for(unsigned n=0;n<world_audio_active_range->count;++n){
        unsigned i=world_audio_cue_indices[world_audio_active_range->first+n];
        const WAudioCue *cue=world_audio_cues+i;
        unsigned mask=1u<<(i%32),bucket=i/32;
        if((world_audio_seen[bucket]&mask)||real(a,FRAME)<cue->frame)continue;
        world_audio_seen[bucket]|=mask;
        if(cue->frame<world_entry_start)continue;
        if((cue->work==2007&&!world.foresight_gauge)||((cue->request==25525||cue->work==2005)&&!world.rank))continue;
#if WORLD_AUDIO_WEAPON
        if(!cue->work){world_audio_weapon(a,cue);continue;}
#endif
        float origin[3];for(unsigned k=0;k<3;++k)origin[k]=real(a,0x40+4*k);
        /* Source work3014 uses helper47 composed with its parent function12.
         * Other Iai work records use the effect's anchored actor position. */
        if(cue->work==3014)world_joint47(a,origin);
        world_audio_epv(a,cue->request,origin);
    }
}
#endif
