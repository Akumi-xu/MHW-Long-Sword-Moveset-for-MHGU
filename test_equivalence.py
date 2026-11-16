from pathlib import Path
import re,subprocess
lab=Path(__file__).parent;reference=lab/'tests/particle_dynamics_reference.inc'
old=reference.read_text().replace('#include "world_particle_dynamics.h"','').replace('world_sfx_path','world_resource_path')
for name in('WParticleOrigin','world_particle_origins','world_particle_restore','world_series','world_particle_motion'):
 old=re.sub(r'\b'+name+r'\b','old_'+name,old)
stub=r'''
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>
typedef uintptr_t uptr;
static unsigned sample_sequence=1,particle_age,particle_seed=3,generator_index,path_calls;
static void *test_particle;
static uptr word(void *p,unsigned off){if(off==176)return (uptr)test_particle;if(p==test_particle&&off==20)return particle_age;if(p==test_particle&&off==4)return 0;assert(0);return 0;}
static unsigned u16(void*p,unsigned off){(void)p;if(off==30)return generator_index;if(off==212)return 0;if(off==8)return particle_seed;assert(0);return 0;}
static unsigned byte(void*p,unsigned off){assert(p==test_particle&&off==125);return 2;}
static unsigned world_resource_path(const char*a,const char*b){++path_calls;for(unsigned i=0;i<64;++i){char c=a[i]=='\\'?'/':a[i];if(c!=b[i])return 0;if(!c)return 1;}return 0;}
#include "01_plugins/mhw_ls_rebuild/c/world_particle_dynamics.h"
'''
test=r'''
#include "01_plugins/mhw_ls_rebuild/c/world_particle_dynamics.inc"
#include "01_plugins/mhw_ls_rebuild/c/world_audio_events.h"
#include "01_plugins/mhw_ls_rebuild/c/world_audio_index.h"
int main(void){
 char resources[20][80]={{0}};void *objects[20];unsigned count=0;
 for(unsigned i=0;i<20;++i){unsigned j;for(j=0;j<count;++j)if(!strcmp(resources[j]+8,world_particle_dynamics[i].path))break;if(j==count){strcpy(resources[count]+8,world_particle_dynamics[i].path);objects[count]=resources[count];++count;}}
 world_particle_bind(objects,count);
 unsigned comparisons=0;float vertices[16],reference[16],input[16]={-14,-3,8,1,13,-3,9,1,-14,3,8,1,13,3,9,1};test_particle=vertices;
 for(unsigned track=0;track<20;++track){
  memset(old_world_particle_origins,0,sizeof old_world_particle_origins);memset(world_particle_origins,0,sizeof world_particle_origins);generator_index=world_particle_dynamics[track].index;
  for(unsigned age=0;age<=512;++age){
   particle_age=age;++sample_sequence;
   for(unsigned k=0;k<4;++k){unsigned column=k?k+13:9;float rate=world_particle_dynamics[track].p[column];float a=old_world_series(rate,(float)age),b=world_series(world_particle_series_indices[track][k],age);assert(!memcmp(&a,&b,sizeof a));++comparisons;}
   memcpy(vertices,input,sizeof input);old_world_particle_motion((void*)1,world_particle_resources[track]);memcpy(reference,vertices,sizeof reference);
   memcpy(vertices,input,sizeof input);unsigned before=path_calls;world_particle_motion((void*)1,world_particle_resources[track]);assert(path_calls==before);assert(!memcmp(reference,vertices,sizeof reference));
  }
 }
 unsigned selected[298],indexed[298],cue_cases=0;unsigned seed=17;
 for(unsigned source=0;source<501;++source){
  const WAudioCueRange *range=world_audio_cue_range(source);
  for(unsigned frame=0;frame<=400;++frame){
   unsigned a=0,b=0;seed=seed*1664525u+1013904223u;
   for(unsigned i=0;i<WORLD_AUDIO_CUE_COUNT;++i)if(world_audio_cues[i].source==source&&world_audio_cues[i].frame<=frame&&((i*2654435761u)&seed))selected[a++]=i;
   if(range)for(unsigned n=0;n<range->count;++n){unsigned i=world_audio_cue_indices[range->first+n];assert(i<WORLD_AUDIO_CUE_COUNT);if(world_audio_cues[i].frame<=frame&&((i*2654435761u)&seed))indexed[b++]=i;}
   assert(a==b&&!memcmp(selected,indexed,a*sizeof(*selected)));++cue_cases;
  }
 }
 assert(!world_audio_cue_range(~0u));
 printf("PASS: %u float32 curve comparisons; 10260 full particle transforms bit-identical; no per-frame path comparisons; %u indexed audio cases preserve cue order/identity\n",comparisons,cue_cases);
}
'''
(lab/'equivalence_test.c').write_text(stub+old+test)
subprocess.run(['gcc','-O2',str(lab/'equivalence_test.c'),'-lm','-o',str(lab/'equivalence_test.exe')],check=True)
subprocess.run([str(lab/'equivalence_test.exe')],check=True)
