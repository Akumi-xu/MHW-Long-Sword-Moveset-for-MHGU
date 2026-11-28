/* Every delayed shell resolves EPV 2006/0 at its own contact point. */
static void world_helm_impact(void *a,const float *point)
{
    world_sfx_emit(a,point,0x3fc0);
}

static struct {
    unsigned actor, area, tier, next;
    float point[3], start;
} world_helm_stroke;

static void world_helm_stroke_begin(void *a,float frame)
{
    /* The landing owns the line even when the blade misses or the pose cancels. */
    unsigned heading=u16(a,HEADING);
    world_helm_stroke.actor=(u32)(uptr)a;
    world_helm_stroke.area=byte(a,AREA);
    world_helm_stroke.tier=world_helm_tier;
    world_helm_stroke.next=1;
    world_helm_stroke.start=world_elapsed+(30.0f-frame)/60.0f;
    world_helm_stroke.point[0]=real(a,0x40)+120*world_sin(heading);
    world_helm_stroke.point[1]=real(a,0x44)+190;
    world_helm_stroke.point[2]=real(a,0x48)+120*world_sin(heading+16384u);
}

static void world_helm_stroke_tick(void *a)
{
    if(!world_helm_stroke.next)return;
    if(world_helm_stroke.actor!=(u32)(uptr)a||world_helm_stroke.area!=byte(a,AREA)||(int16_t)u16(a,HP)<=0){
        world_helm_stroke.next=0;
        return;
    }
    float age=(world_elapsed-world_helm_stroke.start)*60;
    const float *point=world_helm_stroke.point;
    if(world_helm_stroke.next==1&&age>=0){
        /* The white-rank reference landing does not show the long red line. */
        if(world_helm_stroke.tier==1){
            float ground[3]={point[0],point[1]-150,point[2]};
            world_sfx_emit(a,ground,0x3f83);
            world_helm_stroke.next=0;
            return;
        }
        world_sfx_emit(a,point,0x3f81);
        world_helm_stroke.next=2;
    }
    while(world_helm_stroke.next>=2&&age>=3+world_helm_stroke.next){
        unsigned phase=world_helm_stroke.next-2;
        if(!phase){
            world_sfx_emit(a,point,0x3f84);
            world_sfx_emit(a,point,0x3f83);
        }
        for(unsigned i=0;i<2;++i){
            float tail[3]={point[0]+(phase?1.5f:-1.5f),point[1]-25+(i*2+phase)*64,point[2]+(i?-2.f:2.f)};
            world_sfx_emit(a,tail,0x3f82);
        }
        world_helm_stroke.next=phase?0:3;
    }
}

static void world_big_fx_tick(void *a,float frame)
{
    if(world.move!=W_BIG)return;
    float point[3];
    if(frame>=(world.source==308||world.source==310?10:12)&&!(world_sfx_seen&4)){
        if(world_joint47(a,point)){
            world_sfx_seen|=4;
            world_sfx_emit(a,point,0x3f64);
        }
    }
    if(world.counter_success&&frame>=12&&!(world_sfx_seen&8)&&world_joint47(a,point)){
        world_sfx_seen|=8;
        world_sfx_emit(a,point,0x3fc2);
    }
    unsigned rank=world.rank;
    if(!rank||rank>3)return;
    /* On contact, open the visual precursor before the second hit's GU
     * hitstop. A whiff reaches the same wall-clock cue later on the motion. */
    float second=world_iai_contact?45:55;
    if(rank==2)second-=2;
    if(frame>=second&&!(world_sfx_seen&(1u<<5))){
        world_sfx_seen|=1u<<5;
        world_iai_second_start=frame;
        for(unsigned k=0;k<3;++k)world_sfx_origin[k]=real(a,0x40+4*k);
        world_sfx_origin[1]+=83;
        world_sfx_emit(a,world_sfx_origin,rank==1?0x3f6b:rank==2?0x3f70:0x3f71);
        world_sfx_emit(a,world_sfx_origin,rank==1?0x3fc3:0x3fc4);
    }
    if(!(world_sfx_seen&(1u<<10))&&frame>=(world.source==310?10:12)){
        /* The direct white lens lives at the root. Yellow/red dispatch
         * Action4 from joint30, two frames after the root starts. */
        if(rank>1){
            if(frame<(world.source==310?12:14))return;
            void *weapon=(void *)(uptr)word(a,0x23a0);
            const float *bone=weapon&&word(weapon,0x30)==(u32)(uptr)a?world_bone(weapon,0):0;
            if(!bone)return;
            for(unsigned k=0;k<3;++k)point[k]=bone[12+k]+bone[8+k]*178.f;
        }else if(!world_joint47(a,point))return;
        world_sfx_seen|=1u<<10;
        world_sfx_emit(a,point,rank==1?0x3fc5:0x3fc6);
    }
}
