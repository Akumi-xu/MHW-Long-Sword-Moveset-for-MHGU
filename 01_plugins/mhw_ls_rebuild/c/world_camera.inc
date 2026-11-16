/* Source Iai trigger31 requests an absolute camera turn twice (20/37).
 * World camera150/340 telemetry proves positive pitch raises its eye, while
 * GU camera9d8 lowers it; target pitch therefore maps to -3degrees here.
 * GU yaw describes look->eye rather than eye->look, hence the half turn.
 * Source nearest-main-bone selection currently uses GU posed bones: a rig
 * adaptation, pending per-monster semantic main-bone mapping.
 */
static void (*world_original_camera)(void *);
static unsigned world_camera_actor,world_camera_epoch,world_camera_seen;
static unsigned world_camera_sample,world_camera_active,world_camera_object;
static unsigned world_camera_origin[2];
static int world_camera_delta[2];
static float world_camera_age,world_camera_duration;

static unsigned world_direction(float x,float z)
{
    float ax=x<0?-x:x,az=z<0?-z:z;
    unsigned lo=0,hi=16384;
    while(hi-lo>1){
        unsigned mid=(lo+hi)/2;
        if(world_sin(mid)*az<world_sin(mid+16384)*ax)lo=mid;else hi=mid;
    }
    unsigned n=(lo+hi)/2;
    if(z<0)n=32768-n;
    return (x<0?65536-n:n)&65535;
}
static void world_camera_begin(void *a)
{
    world_camera_actor=(u32)(uptr)a;world_camera_epoch=byte(a,AREA);
    world_camera_seen=world_camera_active=0;
}
static unsigned world_camera_target(void *a,unsigned *yaw)
{
    WFollow *job=world_follow+7;
    if(!job->target||job->area!=byte(a,AREA))return 0;
    void *target=world_monster(job->uid);
    if((u32)(uptr)target!=job->target||byte(target,AREA)!=job->area)return 0;
    const float *hit=job->bone<0?0:world_raw_bone(target,(unsigned)job->bone);
    if(!hit)return 0;
    int bone=world_nearest_bone(target,a);
    const float *near=bone<0?0:world_raw_bone(target,(unsigned)bone);
    float v[3],distance=0;
    for(unsigned k=0;k<3;++k){
        float p=near?near[12+k]:real(target,0x40+4*k);
        v[k]=(p+hit[12+k]+job->point[k])*.5f-real(a,0x40+4*k);
        distance+=v[k]*v[k];
    }
    if(!(distance>2500)||!(v[0]*v[0]+v[2]*v[2]>.000001f))return 0;
    unsigned direction=world_direction(v[0],v[2]);
    if(!ml_heading_delta(direction,word(a,HEADING)))return 0;
    *yaw=(direction+32768)&65535;return 1;
}
static void world_camera(void *camera)
{
    void *a=(void *)(uptr)word(camera,0x85c);
    unsigned valid=local_guild(a)&&world_camera_actor==(u32)(uptr)a&&
        world_camera_epoch==byte(a,AREA)&&(int16_t)u16(a,HP)>0;
    if(!valid)world_camera_active=0;
    else {
        if(world_camera_object!=(u32)(uptr)camera){
            world_camera_object=(u32)(uptr)camera;world_camera_active=0;
            log_event("world_camera_owner",(u32)(uptr)a,(u32)(uptr)camera,word(camera,0x9e0),word(camera,0x9e4));
        }
        unsigned fresh=world_camera_sample!=sample_sequence;float step=0;
        if(fresh){
            world_camera_sample=sample_sequence;
            float dt=real(a,0x1c)/60;
            if(dt>0&&dt<.5f){
                step=dt;
                if(world.move==W_BIG&&u16(a,STATE)==16&&u16(a,ACTION)==w_action(W_BIG)&&
                   u16(a,MOTION)==0x500+world.slot){
                    float frame=real(a,FRAME);
                    for(unsigned i=0;i<2;++i)if(!(world_camera_seen&(1u<<i))&&frame>=(i?37:20)){
                        world_camera_seen|=1u<<i;
                        unsigned yaw;
                        if(world_camera_target(a,&yaw)){
                            world_camera_origin[0]=word(camera,0x9e0)&65535;
                            world_camera_origin[1]=word(camera,0x9e4)&65535;
                            world_camera_delta[0]=ml_heading_delta((65536-546),world_camera_origin[0]);
                            world_camera_delta[1]=ml_heading_delta(yaw,world_camera_origin[1]);
                            /* Source TIML float keys0:.7 and37:.4, linear.
                             * Sample at the consumed frame, not at nominal20. */
                            world_camera_duration=w_float(world.source,W_FLOAT_CAMERA,frame);
                            world_camera_age=0;world_camera_active=1;
                            log_event("world_camera_request",(u32)(uptr)a,i,yaw,(u32)(world_camera_duration*1000000));
                        }
                    }
                }
            }
        }
        if(world_camera_active){
            float t=world_camera_age/world_camera_duration;if(t>1)t=1;
            float q=1-t;q*=q;float ease=1-q*q;
            for(unsigned k=0;k<2;++k){
                unsigned angle=(world_camera_origin[k]+(int)(world_camera_delta[k]*ease))&65535;
                put(camera,0x9e8+4*k,angle);put(camera,0x9e0+4*k,angle);put(camera,0x9d8+4*k,angle);
            }
            if(t>=1){world_camera_active=0;log_event("world_camera_done",(u32)(uptr)a,word(camera,0x9e0),word(camera,0x9e4),0);}
            /* Original samples the current clock, then advances it. The
             * read-only training trace agrees with a one-frame clock lag. */
            world_camera_age+=step;
        }
    }
    /* Keep GU's floor/obstruction constraints and eye-position calculation. */
    world_original_camera(camera);
}
