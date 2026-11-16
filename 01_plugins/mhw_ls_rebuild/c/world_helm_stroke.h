/* Use the blade pose at emission; a target's later hit point may have moved. */
static void world_helm_stroke(float *out,const float *blade,const float *fallback)
{
    for(unsigned k=0;k<16;++k)out[k]=0;
    out[9]=-1;
    for(unsigned k=0;k<3;++k)out[12+k]=blade?blade[12+k]+225*blade[8+k]:fallback[k];
}
