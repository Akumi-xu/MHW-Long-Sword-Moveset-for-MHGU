/* Retain the native strip width when moving its endpoints onto the posed blade. */
static void world_thrust_stroke(float *out,const float *pose,const float *anchor)
{
    float transverse[3],width2=0,axis2=0,dot=0;
    for(unsigned k=0;k<3;++k){transverse[k]=(out[k]-out[8+k])*.5f;width2+=transverse[k]*transverse[k];axis2+=pose[8+k]*pose[8+k];dot+=transverse[k]*pose[8+k];}
    float perpendicular2=0;
    for(unsigned k=0;k<3;++k){if(axis2>0.000001f)transverse[k]-=pose[8+k]*dot/axis2;perpendicular2+=transverse[k]*transverse[k];}
    if(perpendicular2<0.000001f){
        perpendicular2=0;
        for(unsigned k=0;k<3;++k){transverse[k]=pose[k];perpendicular2+=transverse[k]*transverse[k];}
    }
    float scale=perpendicular2>0.000001f?__builtin_sqrtf((width2>0.000001f?width2:1)/perpendicular2):0;
    for(unsigned i=0;i<4;++i)for(unsigned k=0;k<3;++k)
        out[4*i+k]=pose[12+k]-anchor[k]+pose[8+k]*(i&1?340.f:-40.f)+transverse[k]*scale*(i<2?1:-1);
}
