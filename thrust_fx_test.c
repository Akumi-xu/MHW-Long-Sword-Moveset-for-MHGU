#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "01_plugins/mhw_ls_rebuild/c/world_thrust_stroke.h"
static void quad(float *v){float q[16]={-100,-2,0,1,100,-2,0,1,-100,2,0,1,100,2,0,1};memcpy(v,q,sizeof q);}
int main(void){
 float pose[16]={0},anchor[3]={10,20,30},v[16];pose[12]=100;pose[13]=200;pose[14]=300;pose[0]=1;
 for(unsigned h=0;h<6;++h){
  pose[8]=h==0?1:h==2?-1:h==4?.6f:0;pose[9]=h==4?-.8f:h==5?1:0;pose[10]=h==1?1:h==3?-1:0;
  quad(v);world_thrust_stroke(v,pose,anchor);
  for(unsigned end=0;end<2;++end){float width=0,dot=0;for(unsigned k=0;k<3;++k){float center=(v[4*end+k]+v[8+4*end+k])*.5f;assert(fabsf(center+anchor[k]-pose[12+k]-pose[8+k]*(end?340.f:-40.f))<.001f);float w=v[4*end+k]-v[8+4*end+k];width+=w*w;dot+=w*pose[8+k];}assert(fabsf(width-16)<.001f&&fabsf(dot)<.001f);}
  float length=0;for(unsigned k=0;k<3;++k)length+=(v[4+k]-v[k])*(v[4+k]-v[k]);assert(fabsf(sqrtf(length)-380)<.001f);for(unsigned i=0;i<4;++i)assert(v[4*i+3]==1);
 }
 puts("PASS: 380-unit blade span, nonzero strip area, four headings, slant, parallel-width fallback, relative origin, native homogeneous coordinates");
}
