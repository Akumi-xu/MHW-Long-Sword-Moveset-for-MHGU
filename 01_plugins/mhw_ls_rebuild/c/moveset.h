#pragma once
#include <stdint.h>

enum { ML_NONE,ML_THRUST,ML_FADE_BACK,ML_FADE_LEFT,ML_FADE_RIGHT,
       ML_SPIRIT_STEP,ML_SPIRIT_III,ML_ROUND,ML_UPPER };
enum { ML_X=1,ML_A=2,ML_R=4,ML_B=8,ML_Y=16,ML_ZR=32 };
enum { ML_OFF,ML_PLAYING,ML_WAIT_BIND };
enum { ML_NO_COMMAND,ML_COMMIT,ML_RELEASE,ML_BIND_TIMEOUT };
typedef struct {
    unsigned semantic,world_clip,native_action,gu_clip;
    float length,buffer_open,execute_open;
} MLMove;
typedef struct {
    uint32_t actor,epoch,sample_sequence,bind_generation;
    unsigned eligible,native_action,motion,phase,held,gauge,motion_finished;
    unsigned heading,input_heading,magnitude;
    float animation_frame,simulation_delta;
} MLObservation;
typedef struct {
    unsigned kind,semantic,native_action,heading,transaction,input_heading,magnitude;
} MLCommand;
typedef struct {
    uint32_t actor,epoch,sample_sequence,previous_held;
    unsigned phase,current,transaction,bind_at_submit;
    unsigned queued,queued_heading,queued_mask,eligible_queue,consumed;
    unsigned target,bind_seen,adopted_native;
    unsigned queued_input_heading,queued_magnitude;
    float queue_expires,bind_wait,solo_wait;
    unsigned solo_key;
} MLState;
void ml_reset(MLState *s);
MLCommand ml_step(MLState *s,const MLObservation *o);
void ml_observe(MLState *s,const MLObservation *o);
const MLMove *ml_move(unsigned semantic);
unsigned ml_native_semantic(unsigned native_action,unsigned motion);
unsigned ml_should_filter(const MLState *s,const MLObservation *o,unsigned state,unsigned action);
int ml_heading_delta(unsigned requested,unsigned anchor);
unsigned ml_choose_fade(unsigned heading,unsigned requested,unsigned magnitude);
