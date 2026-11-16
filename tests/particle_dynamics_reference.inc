#include "world_particle_dynamics.h"
typedef struct {uptr particle,generator,resource;unsigned age,seen;float vertices[16];} WParticleOrigin;
static WParticleOrigin world_particle_origins[512];
static unsigned world_particle_restore(void *g,void *resource,void *p,unsigned age,float *vertices)
{
    unsigned slot=((uptr)p>>4)&511;
    for(unsigned i=0;i<512;++i){
        WParticleOrigin *origin=world_particle_origins+((slot+i)&511);
        if(origin->particle!=(uptr)p&&origin->seen&&sample_sequence-origin->seen<=2)continue;
        unsigned fresh=origin->particle!=(uptr)p||origin->generator!=(uptr)g||
            origin->resource!=(uptr)resource||age<origin->age||sample_sequence-origin->seen>2;
        if(fresh){
            origin->particle=(uptr)p;origin->generator=(uptr)g;origin->resource=(uptr)resource;
            for(unsigned k=0;k<16;++k)origin->vertices[k]=vertices[k];
        }else for(unsigned k=0;k<16;++k)vertices[k]=origin->vertices[k];
        origin->age=age;origin->seen=sample_sequence;
        return 1;
    }
    return 0;
}
static float world_series(float rate,float age)
{
    if(rate==1)return age;
    unsigned n=(unsigned)age;
    float sum=0,term=1;
    for(unsigned i=0;i<n;++i){sum+=term;term*=rate;}
    return sum+(age-n)*term;
}
static void world_particle_motion(void *g,void *resource)
{
    const WParticleDynamics *entry=0;
    for(unsigned i=0;i<sizeof(world_particle_dynamics)/sizeof(*world_particle_dynamics);++i){
        const WParticleDynamics *candidate=world_particle_dynamics+i;
        if(candidate->index==u16(g,30)&&world_sfx_path((const char *)resource+8,candidate->path)){
            entry=candidate;break;
        }
    }
    if(!entry)return;
    const float *v=entry->p;
    for(void *p=(void *)(uptr)word(g,176);p;p=(void *)(uptr)word(p,4)){
        if(byte(p,125)!=2)continue;
        float age=(float)word(p,20);
        if(!(age>=0&&age<=512))continue;
        float *vertices=(float *)((uptr)p+u16(g,212)),center[3]={0,0,0};
        if(!world_particle_restore(g,resource,p,(unsigned)age,vertices))continue;
        for(unsigned k=0;k<3;++k)for(unsigned i=0;i<4;++i)center[k]+=vertices[4*i+k]*.25f;
        float distance=world_series(v[9],age);
        float direction[3],magnitude=0;
        for(unsigned k=0;k<3;++k){
            direction[k]=entry->radial?center[k]*v[6+k]+v[3+k]:v[k];
            magnitude+=direction[k]*direction[k];
        }
#ifdef __arm__
        __asm__("vsqrt.f32 %0, %1":"=t"(magnitude):"t"(magnitude));
#else
        magnitude=sqrtf(magnitude);
#endif
        unsigned seed=(unsigned)u16(p,8)*2654435761u;
        float speed_value=v[17]+v[18]*((float)(seed&65535)/32767.5f-1);
        for(unsigned k=0;k<3;++k)direction[k]=magnitude>0?direction[k]/magnitude:(k==1?1:0);
        for(unsigned k=0;k<3;++k){
            float endpoints[2]={(vertices[k]+vertices[8+k])*.5f-center[k],
                                (vertices[4+k]+vertices[12+k])*.5f-center[k]};
            float speed=direction[k]*speed_value;
            float shift=speed*distance-(k==1?v[10]*age*(age+1)*.5f:0);
            float scale=1+v[11+k]*world_series(v[14+k],age);
            if(scale<0)scale=0;
            for(unsigned i=0;i<4;++i){
                float offset=vertices[4*i+k]-center[k];
                if(entry->mode==0){
                    offset-=endpoints[i&1];
                    float history=age<entry->history?age:entry->history;
                    offset-=speed*history*(i&1);
                }
                vertices[4*i+k]=center[k]+offset*scale+shift;
            }
        }
    }
}
