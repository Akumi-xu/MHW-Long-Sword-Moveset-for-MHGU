#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "01_plugins/mhw_ls_rebuild/c/world_iai_stroke.h"
static void quad(float *v){float q[16]={-100,-2,10,1,100,-2,10,1,-100,2,10,1,100,2,10,1};memcpy(v,q,sizeof q);}
int main(void){
 float v[16];quad(v);world_iai_stroke(v,0);assert(fabsf(v[4]-v[0]-5)<.001f);assert(v[1]==-2&&v[9]==2);
 quad(v);world_iai_stroke(v,.08f);assert(fabsf(v[4]-v[0]-200)<.001f);
 quad(v);world_iai_stroke(v,.5f);assert(fabsf(v[9]-v[1]-.48f)<.001f&&fabsf(v[4]-v[0]-200)<.001f);
 quad(v);world_iai_stroke(v,1.f);assert(fabsf(v[9]-v[1]-.28f)<.001f);
 float previous=100;
 for(unsigned n=8;n<100;++n){quad(v);world_iai_stroke(v,n/60.f);float width=v[9]-v[1];assert(width<=previous+.00001f);previous=width;for(unsigned k=0;k<4;++k)assert(v[4*k+3]==1);}
 for(unsigned rank=1;rank<=3;++rank){float due=world_iai_line_delay(rank);assert(due>=.7f&&due<=.75f);for(unsigned hz=30;hz<=120;hz*=2){unsigned n=(unsigned)ceilf(due*hz);assert(n/(float)hz>=due-.00001f&&n/(float)hz-due<=1.f/hz+.00001f);}}
 /* Rotate and translate the source quad; the shaping must follow its plane. */
 float base[16],rotated[16];quad(base);quad(rotated);
 for(unsigned i=0;i<4;++i){float x=rotated[4*i],y=rotated[4*i+1];rotated[4*i]=-y+30;rotated[4*i+1]=x+50;}
 world_iai_stroke(base,.2f);world_iai_stroke(rotated,.2f);
 for(unsigned i=0;i<4;++i){assert(fabsf(rotated[4*i]-(-base[4*i+1]+30))<.001f);assert(fabsf(rotated[4*i+1]-(base[4*i]+50))<.001f);}
 puts("PASS: bright seed -> full span; thin stable tail; arbitrary plane; World onset range;30/60/120Hz threshold bounds");
}
