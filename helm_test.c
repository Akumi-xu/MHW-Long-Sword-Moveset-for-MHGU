#include <assert.h>
#include <stdio.h>
#include <math.h>
#include "01_plugins/mhw_ls_rebuild/c/world_core.h"
#include "01_plugins/mhw_ls_rebuild/c/world_helm_stroke.h"
static void route(int manual){
 WState s={0};WObservation o={.base={.actor=1,.epoch=1,.sample_sequence=1,.native_action=243,.motion=0x5e3,.simulation_delta=1,.gauge=100},.state=16,.drawn=1,.alive=1,.rank=3,.resources_ready=1};
 w_step(&s,&o);s.move=W_AIR;s.source=172;s.slot=227;s.rank=3;
 int entered=0;
 for(int frame=0;frame<=90;frame++){
  ++o.base.sample_sequence;o.base.animation_frame=(float)frame;o.base.held=manual&&frame>=27&&frame<35?ML_R:0;
  WDecision d=w_step(&s,&o);w_input_finish(&s,&o,d);
  if(d.kind==W_ENTER){assert(d.move==W_DOWN&&frame==(manual?42:60)&&s.helm_cost_pending);entered=1;break;}
 }
 assert(entered);o.base.native_action=244;o.base.motion=0x5e4;o.base.animation_frame=36;o.base.held=0;o.grounded=1;++o.base.sample_sequence;
 WDecision land=w_step(&s,&o);w_input_finish(&s,&o,land);assert(land.kind==W_ENTER&&land.move==W_LAND&&s.helm_cost_pending);
 o.base.native_action=245;o.base.motion=0x5e5;int cost=0;
 for(int frame=6;frame<45;frame++){++o.base.sample_sequence;o.base.animation_frame=(float)frame;WDecision d=w_step(&s,&o);w_input_finish(&s,&o,d);cost+=d.rank_delta;}
 assert(cost==-1&&!s.helm_cost_pending&&w_leave(&s).rank_delta==0);
}
int main(void){
 route(0);route(1);
 float blade[16]={0},out[16],contact[3]={4,5,6};blade[8]=.6f;blade[9]=-.8f;blade[12]=100;blade[13]=200;blade[14]=300;
 world_helm_stroke(out,blade,contact);assert(out[8]==0&&out[9]==-1&&out[10]==0);
 assert(fabsf(out[12]-235)<.001f&&fabsf(out[13]-20)<.001f&&fabsf(out[14]-300)<.001f);
 contact[0]=9000;contact[1]=-9000;contact[2]=4000;
 world_helm_stroke(out,blade,contact);
 assert(fabsf(out[12]-235)<.001f&&fabsf(out[13]-20)<.001f&&fabsf(out[14]-300)<.001f);
 for(unsigned heading=0;heading<4;++heading){
  blade[8]=heading==0?1:heading==2?-1:0;blade[9]=0;blade[10]=heading==1?1:heading==3?-1:0;
  world_helm_stroke(out,blade,contact);
  for(unsigned k=0;k<3;++k)assert(fabsf(out[12+k]-(blade[12+k]+225*blade[8+k]))<.001f);
  assert(out[8]==0&&out[9]==-1&&out[10]==0);
 }
 contact[0]=4;contact[1]=5;contact[2]=6;
 world_helm_stroke(out,0,contact);assert(out[9]==-1&&out[12]==4&&out[13]==5&&out[14]==6);
 puts("PASS: released-input swing60; manual swing42; one tier cost; vertical absolute blade tip and missing-pose fallback");
}
