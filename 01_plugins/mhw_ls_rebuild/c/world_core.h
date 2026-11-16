#pragma once
#include "moveset.h"
#include "world_events.h"
#include "world_inputs.h"
#include "world_controls.h"

enum { W_NONE,W_FORESIGHT,W_THRUST,W_RISE,W_AIR,W_DOWN,W_LAND,W_SHEATHE,W_SMALL,W_BIG,W_SHEATHE_END,W_GROUND };
enum { W_KEEP,W_ENTER,W_NATIVE,W_IDLE,W_ROLL,W_ABORT,W_NATIVE_R,W_MOTION };
typedef struct {
    unsigned actor,epoch,sample,held,waiting,ready;
    float remaining[3];
} WKeys;
void w_keys_tick(WKeys *keys,const MLObservation *o);
typedef struct {
    MLObservation base;
    unsigned state,drawn,alive,grounded,rank,contact,counter,resources_ready;
    unsigned previous_weapon_action;
} WObservation;
typedef struct {
    unsigned kind,move,source,slot,native_action,heading;
    int rank_delta,gauge_delta;
    unsigned gauge_clear,gauge_fill,regen_seconds;
    unsigned motion_contract;
    float start,blend,speed;
} WDecision;
typedef struct {
    unsigned actor,epoch,move,source,slot,heading,rank,previous_held,sample;
    WKeys keys;
    WControls controls;
    unsigned input_motion,gu_chord_wait;
    unsigned air_sheathe_sample,air_sheathe_held;
    float air_sheathe_buffer;
    unsigned bound,consumed,queued,queued_mask,queued_source,foresight_gauge;
    unsigned queued_word,queued_shift,queued_heading;
    unsigned counter_success,hit_success,rank_paid,follow_ready;
    unsigned helm_cost_pending;
    unsigned ground_node,main_source,spirit_paid;
    float entry_start;
    unsigned turn_seen,turn_sample,turn_origin;
    int turn_delta;
    float turn_age,turn_duration,turn_fraction;
    float queue_until,hold_seconds,last_frame,helm_cost_seconds,solo_age;
} WState;
const WClip *w_clip(unsigned source);
const WEvent *w_event(unsigned source,float frame);
enum {W_FLOAT_SPEED,W_FLOAT_TURN,W_FLOAT_CAMERA};
float w_float(unsigned source,unsigned channel,float frame);
unsigned w_source(unsigned motion);
unsigned w_foresight_stage(unsigned source,const WEvent *event);
unsigned w_action(unsigned move);
unsigned w_guild_attack(unsigned action);
unsigned w_guild_source(unsigned state,unsigned action,unsigned motion);
unsigned w_guild_spirit_opener(unsigned action,unsigned motion,unsigned previous);
unsigned w_private(unsigned state,unsigned action);
void w_reset(WState *s);
WDecision w_step(WState *s,const WObservation *o);
WDecision w_leave(WState *s);
void w_input_finish(WState *s,const WObservation *o,WDecision decision);
unsigned w_rotation(WState *s,const WObservation *o,unsigned *heading);
int w_root_heading(unsigned source,float frame);
