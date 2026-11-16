#include "moveset.h"
#if !ML_WORLD_INPUT_VM
#include "source_moves.h"
#endif

void ml_reset(MLState *s){*s=(MLState){0};}
#if !ML_WORLD_INPUT_VM
const MLMove *ml_move(unsigned semantic)
{
    for(unsigned i=0;i<sizeof(ml_moves)/sizeof(ml_moves[0]);++i)
        if(ml_moves[i].semantic==semantic)return &ml_moves[i];
    return 0;
}
#endif
int ml_heading_delta(unsigned requested,unsigned anchor)
{
    unsigned v=(requested-anchor)&0xffffu;
    return v>=0x8000u?(int)v-65536:(int)v;
}
#if !ML_WORLD_INPUT_VM
unsigned ml_choose_fade(unsigned heading,unsigned requested,unsigned magnitude)
{
    if(magnitude<50)return ML_FADE_BACK;
    int delta=ml_heading_delta(requested,heading);
    /* Actual GU camera-centered input trace: screen LEFT yields +0x4001
     * relative to hunter heading. b2 reverses the native clip choice after the user observed opposite visual travel; bind-only checks did not validate travel. */
    if(delta>=8192&&delta<=24576)return ML_FADE_RIGHT;
    if(delta<=-8192&&delta>=-24576)return ML_FADE_LEFT;
    return ML_FADE_BACK;
}
unsigned ml_native_semantic(unsigned action,unsigned motion)
{
    unsigned clip=(motion>>8)==5?motion-0x500u:0;
    /* Native back-fade selects context variants, including action11 after
     * thrust with a short transition clip before the shared clip103. */
    if(action>=8&&action<=14&&clip==103)return ML_FADE_BACK;
    for(unsigned i=0;i<sizeof(ml_moves)/sizeof(ml_moves[0]);++i){
        const MLMove *m=&ml_moves[i];
        if(action==m->native_action&&(clip==m->gu_clip||
            (m->semantic==ML_UPPER&&clip==116)))return m->semantic;
    }
    return ML_NONE;
}
static unsigned is_fade(unsigned n){return n>=ML_FADE_BACK&&n<=ML_FADE_RIGHT;}
static void clear_queue(MLState *s)
{s->queued=s->queued_mask=s->eligible_queue=s->solo_key=0;s->solo_wait=0;}

void ml_observe(MLState *s,const MLObservation *o)
{
    if(!o->eligible||s->actor!=o->actor||s->epoch!=o->epoch){ml_reset(s);return;}
    if(s->phase!=ML_WAIT_BIND)return;
    const MLMove *m=ml_move(s->target);
    if(!m)return;
    if(o->bind_generation>s->bind_at_submit&&
       ml_native_semantic(o->native_action,o->motion)==m->semantic){
        s->current=s->target;s->adopted_native=o->native_action;
        s->phase=ML_PLAYING;s->bind_seen=o->bind_generation;s->bind_wait=0;
    }
}

MLCommand ml_step(MLState *s,const MLObservation *o)
{
    MLCommand command={0};
    if(!o->eligible){ml_reset(s);return command;}
    /* Baseline owns the complete thrust transition: native input buffering,
     * cancel permission and direction. No synthetic timer or delayed offer. */
    if(o->native_action==4){ml_reset(s);return command;}
    if(s->actor!=o->actor||s->epoch!=o->epoch){
        ml_reset(s);s->actor=o->actor;s->epoch=o->epoch;
        s->sample_sequence=o->sample_sequence-1u;
    }
    unsigned fresh_sample=s->sample_sequence!=o->sample_sequence;
    unsigned pressed=fresh_sample?o->held&~s->previous_held:0;
    if(fresh_sample){s->sample_sequence=o->sample_sequence;s->previous_held=o->held;s->consumed&=o->held;}
    if(o->held&ML_B){ml_reset(s);command.kind=ML_RELEASE;return command;}
    if(s->phase==ML_WAIT_BIND){
        ml_observe(s,o);
        if(s->phase==ML_WAIT_BIND){
            if(fresh_sample&&o->simulation_delta>0&&o->simulation_delta<10)s->bind_wait+=o->simulation_delta;
            if(s->bind_wait>12){ml_reset(s);command.kind=ML_BIND_TIMEOUT;}
            return command;
        }
        /* Observation can acknowledge a bind, but never chain another
         * command in the same sample/transaction. */
        return command;
    }
    unsigned actual=ml_native_semantic(o->native_action,o->motion);
    if(s->phase==ML_OFF){
        if(!actual)return command;
        s->phase=ML_PLAYING;s->current=actual;s->adopted_native=o->native_action;
        s->bind_seen=o->bind_generation;clear_queue(s);
    }else if(actual!=s->current){
        ml_reset(s);command.kind=ML_RELEASE;return command;
    }
    const MLMove *m=ml_move(s->current);
    if(!m||!(o->animation_frame>=0&&o->animation_frame<10000))return command;
    /* A transition clip carries no semantic attack clock. */
    if(o->motion!=0x500u+m->gu_clip)return command;
    float frame=o->animation_frame;
    unsigned finish_gated=s->current==ML_THRUST&&is_fade(s->queued);
    if(s->queued&&s->eligible_queue&&!finish_gated&&frame>s->queue_expires)clear_queue(s);
    if(s->solo_key&&fresh_sample&&o->simulation_delta>0&&o->simulation_delta<10)s->solo_wait+=o->simulation_delta;

    unsigned choice=0,mask=0;
    if((o->held&(ML_X|ML_A|ML_R))==(ML_X|ML_A)&&!s->consumed){
        if(s->current==ML_THRUST){choice=ml_choose_fade(o->heading,o->input_heading,o->magnitude);mask=ML_X|ML_A;}
    }else if((o->held&ML_R)&&!(o->held&(ML_X|ML_A|ML_Y))&&!(s->consumed&ML_R)){
        if(is_fade(s->current)){choice=ML_SPIRIT_STEP;mask=ML_R;}
        else if(s->current==ML_SPIRIT_STEP&&o->gauge>=20){choice=ML_SPIRIT_III;mask=ML_R;}
        else if(s->current==ML_SPIRIT_III&&o->gauge>=25){choice=ML_ROUND;mask=ML_R;}
    }else if(s->current==ML_THRUST&&(o->held&ML_X)&&!(o->held&(ML_A|ML_R))&&!(s->consumed&ML_X)){
        if(pressed&ML_X){s->solo_key=ML_X;s->solo_wait=0;}
        if(s->solo_key){choice=ML_UPPER;mask=ML_X;}
    }
    if(choice&&(!s->queued||!s->eligible_queue)){
        s->queued=choice;s->queued_mask=mask;s->queued_heading=o->heading;
        s->queued_input_heading=o->input_heading;s->queued_magnitude=o->magnitude;
    }
    /* A two-button chord supersedes a not-yet-executed solo-X choice. */
    if(choice&&is_fade(choice)&&s->queued==ML_UPPER){
        clear_queue(s);s->queued=choice;s->queued_mask=mask;s->queued_heading=o->heading;
        s->queued_input_heading=o->input_heading;s->queued_magnitude=o->magnitude;
    }
    if(s->queued){
        unsigned held=(o->held&s->queued_mask)==s->queued_mask;
        if(!s->eligible_queue&&!held){clear_queue(s);return command;}
        if(frame>=m->buffer_open&&held){s->eligible_queue=1;s->queue_expires=frame+12;
            if(s->queue_expires<m->execute_open+4)s->queue_expires=m->execute_open+4;}
        /* Buffered input never grants permission to end the thrust.
         * Strict completion baseline, not certified World cancel timing. */
        unsigned ready=s->current==ML_THRUST&&is_fade(s->queued)
            ?o->motion_finished:frame>=m->execute_open;
        if(s->eligible_queue&&ready&&
           (s->queued!=ML_UPPER||s->solo_wait>=2)){
            const MLMove *target=ml_move(s->queued);
            command=(MLCommand){ML_COMMIT,s->queued,target->native_action,s->queued_heading,++s->transaction,s->queued_input_heading,s->queued_magnitude};
            s->target=s->queued;s->consumed|=s->queued_mask;s->phase=ML_WAIT_BIND;
            s->bind_at_submit=o->bind_generation;s->bind_wait=0;clear_queue(s);
        }
    }
    return command;
}

unsigned ml_should_filter(const MLState *s,const MLObservation *o,unsigned state,unsigned action)
{
    if(!o->eligible)return 0;
    if(o->native_action==4)return 0;
    /* System, hurt, aerial and scene transitions retain native authority. */
    if(state!=1&&state!=2&&state!=16)return 0;
    if((state==1||state==2)&&action!=0)return 0;
    if(o->held&ML_B)return 0;
    if(s->actor==o->actor&&s->epoch==o->epoch&&
       (s->phase==ML_WAIT_BIND||s->eligible_queue))return 1;
    if(state!=16||action>153)return 0;
    unsigned current=ml_native_semantic(o->native_action,o->motion);
    if(current==ML_THRUST&&((o->held&(ML_X|ML_A))!=0)&&!(o->held&ML_R))return 1;
    if((is_fade(current)||current==ML_SPIRIT_STEP||current==ML_SPIRIT_III)&&
       (o->held&ML_R)&&!(o->held&(ML_X|ML_A|ML_Y)))return 1;
    return 0;
}

#endif
