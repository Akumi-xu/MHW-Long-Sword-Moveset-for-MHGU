#pragma once
#include "world_gesture_vm.h"
typedef struct {
    unsigned actor,epoch,sample,initialized,changed_primary,force_clear;
    WButton buttons[4]; /* X,A,R,B */
    WCommands commands;
    WCommandQueries queries;
    uint32_t previous_queries[5],permission[8];
} WControls;

static inline void w_controls_tick(WControls *s,unsigned actor,unsigned epoch,unsigned sample,
    unsigned held,float seconds,const uint32_t *permission,const float *direction,
    unsigned stick,unsigned face_direction,unsigned roll_direction)
{
    if(!s->initialized||s->actor!=actor||s->epoch!=epoch){
        *s=(WControls){.actor=actor,.epoch=epoch,.sample=sample,.initialized=1};
        for(unsigned i=0;i<4;++i){
            WButton *b=s->buttons+i;b->raw=b->previous=b->held=(held>>i)&1;
            b->wait_seconds=i==2?.10f:i<2?.07f:0;
        }
        for(unsigned i=0;i<8;++i)s->permission[i]=permission[i];
        return;
    }
    if(s->sample==sample)return;
    s->sample=sample;
    if(!(seconds>0&&seconds<.5f))seconds=0;
    for(unsigned i=0;i<8;++i)s->permission[i]=permission[i];
    for(unsigned i=0;i<4;++i){
        s->commands.direction[i]=direction[i];
        s->buttons[i].raw=(held>>i)&1;w_button_tick(s->buttons+i,seconds);
    }
    for(unsigned i=0;i<5;++i)s->commands.observed[i]=s->commands.raw[i]=0;
    WGestures g=w_gestures(s->buttons,s->buttons+1,s->buttons+2,s->buttons+3,
                          stick,face_direction,roll_direction);
    unsigned global=w_permission_bit(permission,202)|(w_permission_bit(permission,203)<<1);
    /* Only visit emitted gestures, preserving increasing command order. */
    for(unsigned word=0;word<5;++word){
        uint32_t active=g.raw[word]|g.pressed[word];
        while(active){
            unsigned index=__builtin_ctz(active),id=word*32+index;
            uint32_t bit=1u<<index;
            if(g.raw[word]&bit)
                w_command_collect(&s->commands,id,w_command_stage(permission,id),global,0,0,0);
            if(g.pressed[word]&bit)w_command_promote(&s->commands,id);
            active&=active-1;
        }
    }
    w_command_conflicts(&s->commands,&s->queries,s->previous_queries);
    for(unsigned i=0;i<5;++i){s->queries.other[i]=s->queries.tested[i];s->queries.tested[i]=0;}
}
static inline unsigned w_controls_query(WControls *s,unsigned id,unsigned edge,unsigned mode)
{
    int bit=id<137?world_command_bits[id]:-1;
    unsigned execute=w_permission_bit(s->permission,bit)||w_permission_bit(s->permission,203);
    return w_command_query(&s->commands,&s->queries,id,
        edge?(mode?W_QUERY_INVERSE_EDGE:W_QUERY_EDGE):W_QUERY_LEVEL,mode,1,execute);
}
/* Native122f440: changing a primary motion clears raw execute grants while
 * retaining buffered commands for1230230 to test against the new timeline. */
static inline void w_controls_bind(WControls *s)
{for(unsigned i=0;i<5;++i)s->commands.raw_executable[i]=0;s->changed_primary=1;}
static inline void w_controls_finish(WControls *s,const uint32_t *permission)
{
    static const uint32_t secondary[8]={0};
    w_command_prune(&s->commands,&s->queries,s->previous_queries,permission,secondary,
                    s->force_clear,s->changed_primary,0);
    s->force_clear=s->changed_primary=0;
}
