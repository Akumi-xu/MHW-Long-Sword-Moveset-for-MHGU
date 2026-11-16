/* The EPV record owns the selector. Emit each selected resource once, allowing
 * its native generator to handle its own burst count and particle lifetime. */
static float world_tf_origins[3][3];
static float world_thrust_pose[16],world_thrust_anchor[3];
static unsigned world_thrust_actor,world_thrust_epoch,world_thrust_line_seen;
#include "world_thrust_stroke.h"
static void world_tf_follow(void *g,void *resource)
{
    if(resource==world_thrust_resources[0]||resource==world_thrust_resources[1]){
        void *a=(void *)(uptr)world_thrust_actor;
        if(!a||world_thrust_actor!=world_bound_actor||world_thrust_epoch!=byte(a,AREA)||(int16_t)u16(a,HP)<=0)return;
        void *w=(void *)(uptr)word(a,0x23a0);
        const float *m=w&&word(w,0x30)==(u32)(uptr)a?world_bone(w,0):0;
        for(void *p=(void *)(uptr)word(g,176);p;p=(void *)(uptr)word(p,4)){
            if(byte(p,125)!=2)continue;
            if(word(p,20)<10&&m)for(unsigned k=0;k<16;++k)world_thrust_pose[k]=m[k];
            float *v=(float *)((uptr)p+u16(g,212));
            world_thrust_stroke(v,world_thrust_pose,world_thrust_anchor);
            if(!world_thrust_line_seen){
                world_thrust_line_seen=1;
                log_event("world_thrust_line_pose",(u32)(uptr)a,word(world_thrust_pose,32),word(world_thrust_pose,36),word(world_thrust_pose,40));
            }
        }
        return;
    }
    if(world.move==W_THRUST){
        if(resource!=world_blade_resource)return;
        void *a=(void *)(uptr)world_bound_actor;
        if(!a||!world_bound||world.epoch!=byte(a,AREA)||u16(a,MOTION)!=0x500+world.slot)return;
        void *w=(void *)(uptr)word(a,0x23a0);
        const float *m=w&&word(w,0x30)==(u32)(uptr)a?world_bone(w,0):0;
        if(!m)return;
        for(void *p=(void *)(uptr)word(g,176);p;p=(void *)(uptr)word(p,4)){
            if(byte(p,125)!=2)continue;
            float *v=(float *)((uptr)p+u16(g,212));
            for(unsigned i=0;i<4;++i)for(unsigned k=0;k<3;++k)
                v[4*i+k]=m[12+k]-world_blade_anchor[k]+m[8+k]*(i&1?225.f:105.f)+m[k]*5;
        }
        return;
    }
    if(world.move!=W_FORESIGHT)return;
    unsigned index=resource==world_foresight_resources[0]?0:
        resource==world_foresight_resources[1]?1:
        resource==world_foresight_resources[2]?2:3;
    if(index==3)return;
    void *a=(void *)(uptr)world_bound_actor;
    if(!a||world.epoch!=byte(a,AREA)||u16(a,MOTION)!=0x500+world.slot)return;
    const float *bone=world_bone(a,index?0:1);
    if(!bone)return;
    float delta[3];
    for(unsigned k=0;k<3;++k)
        delta[k]=bone[12+k]-(index==2?50*bone[4+k]:0)-world_tf_origins[index][k];
    for(void *p=(void *)(uptr)word(g,176);p;p=(void *)(uptr)word(p,4)){
        if(byte(p,125)!=2)continue;
        float *vertices=(float *)((uptr)p+u16(g,212));
        for(unsigned v=0;v<4;++v)for(unsigned k=0;k<3;++k)vertices[4*v+k]+=delta[k];
    }
}
static void world_tf_tick(void *a,float frame)
{
    if(world.move==W_FORESIGHT&&world.foresight_gauge&&!(world_sfx_seen&16)){
        world_sfx_seen|=16;
        /* EFX joints are skeleton indices: World2 is function1, World1
         * is function13 (GU0). They are not GU function IDs2 and1. */
        const float *body=world_bone(a,1),*pelvis=world_bone(a,0);
        if(body){
            for(unsigned k=0;k<3;++k)world_tf_origins[0][k]=body[12+k];
            world_sfx_oriented(a,body+12,0x3d02,body);
            world_sfx_oriented(a,body+12,0x3d04,body);
        }
        if(pelvis){
            for(unsigned k=0;k<3;++k)world_tf_origins[1][k]=pelvis[12+k];
            world_sfx_oriented(a,pelvis+12,0x3d05,pelvis);
            float p[3];
            for(unsigned k=0;k<3;++k)p[k]=pelvis[12+k]-50*pelvis[4+k];
            for(unsigned k=0;k<3;++k)world_tf_origins[2][k]=p[k];
            world_sfx_oriented(a,p,0x3d06,pelvis);
        }
        /* Source joint zero is actor-space. Its local Y is -30 in dist. */
        float dist[3]={real(a,0x40),real(a,0x44)-30,real(a,0x48)};
        world_sfx_emit(a,dist,0x3d03);
    }
    if(world.move!=W_THRUST||!world.rank)return;
    if(frame>=10&&frame<24&&!(world_sfx_seen&256)){
        world_sfx_seen|=256;
        for(unsigned k=0;k<3;++k)world_blade_anchor[k]=real(a,0x40+4*k);
        world_sfx_emit(a,world_blade_anchor,0x3f80);
    }
    if(frame>=84&&!(world_sfx_seen&16)){
        world_sfx_seen|=16;
        void *w=(void *)(uptr)word(a,0x23a0);
        const float *m=w&&word(w,0x30)==(u32)(uptr)a?world_bone(w,0):0;
        float p[3];for(unsigned k=0;k<3;++k)p[k]=m?m[12+k]:real(a,0x40+4*k);
        world_sfx_emit(a,p,0x3f6a);
        if(m){
            world_thrust_actor=(u32)(uptr)a;world_thrust_epoch=byte(a,AREA);world_thrust_line_seen=0;
            for(unsigned k=0;k<16;++k)world_thrust_pose[k]=m[k];
            for(unsigned k=0;k<3;++k)world_thrust_anchor[k]=p[k];
            world_sfx_emit(a,p,0x3d16);world_sfx_emit(a,p,0x3d1c);
            log_event("world_thrust_line",(u32)(uptr)a,(u32)(frame*1000),world.rank,0);
        }
    }
}
