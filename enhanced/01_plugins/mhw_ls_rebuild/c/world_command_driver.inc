/* Source-owned input kernel. Legacy manual queues below w_step's VM branch
 * remain available only for historical development-build comparisons. */
typedef struct {WState *s;const WObservation *o;uint64_t known,values;} WGroundQuery;
static unsigned world_property(WGroundQuery *q,unsigned property)
{
    uint64_t bit=(uint64_t)1<<property;
    if(q->known&bit)return (q->values&bit)!=0;
    q->known|=bit;unsigned value=0;WState *s=q->s;const WObservation *o=q->o;
    static const unsigned commands[]={39,53,60,25,46,51,52,32};
    if(property<8)value=w_controls_query(&s->controls,commands[property],1,0);
    else switch(property){
    case 8:value=o->base.motion_finished;break;
    case 9:value=1;break; /* contact is validated by the native monster registry */
    case 10:value=o->rank>=1;break;
    case 11:value=w_controls_query(&s->controls,24,1,0);break;
    case 12:value=s->hit_success;break;
    case 13:value=o->grounded;break;
    case 14:value=1;break; /* source Finish gauge threshold is0; colour is separate */
    case 15:value=o->base.gauge>=25;break;
    case 16:value=o->base.gauge>=15;break;
    case 17:value=o->base.gauge>=20;break;
    case 18:value=s->counter_success&&s->hit_success;break;
    }
    if(value)q->values|=bit;
    return value;
}
static int world_query_node(WGroundQuery *q,unsigned node)
{
    const WGroundNode *n=w_ground_node(node);if(!n)return -1;
    for(unsigned i=0;i<n->count;++i){
        const WGroundEdge *edge=world_ground_edges+n->offset+i;
        unsigned stack[32],top=0;
        for(unsigned j=0;j<edge->count;++j){
            WGroundTerm term=world_ground_terms[edge->offset+j];
            if(term.op<=1){if(top>=32)return -1;stack[top++]=term.op?world_property(q,term.value):term.value;}
            else{
                if(term.value>top)return -1;
                unsigned result=term.op==2;
                for(unsigned k=0;k<term.value;++k){unsigned v=stack[--top];if(term.op==2)result&=v;else result|=v;}
                stack[top++]=result;
            }
        }
        if(top==1&&stack[0])return (int)edge->target;
    }
    return -1;
}
/* Rise follow-ups specify an execute frame and a PreFrame (buffer length).
 * EndFrame zero has no explicit cutoff. */
static unsigned world_follow_stage(float frame,unsigned start,unsigned pre,unsigned end)
{
    if(end&&frame>end)return 0;
    return frame>=start?3:frame>=start-pre?1:0;
}

static void world_input_permission(const WState *s,unsigned source,const WEvent *event,
                                   float frame,uint32_t *permission)
{
    if(!ML_WORLD_STYLE_ENHANCED||s->move!=W_BIG){
        for(unsigned i=0;i<8;++i)permission[i]=event&&i<7?event->input[i]:0;
        permission[2]=(permission[2]&~(3u<<22))|(w_foresight_stage(source,event)<<22);
        return;
    }
    for(unsigned i=0;i<8;++i)permission[i]=0;
    permission[WI_ROLL.word]|=world_follow_stage(frame,100,10,0)<<WI_ROLL.shift;
    if(s->iai_armed_partners&ML_X)
        permission[WI_THRUST.word]|=world_follow_stage(frame,76,10,0)<<WI_THRUST.shift;
    if(s->counter_success){
        permission[WI_SHEATHE.word]|=world_follow_stage(frame,76,16,0)<<WI_SHEATHE.shift;
        unsigned stage=world_follow_stage(frame,76,10,0);
        if(s->iai_armed_partners&ML_A)permission[WI_FORESIGHT.word]|=stage<<WI_FORESIGHT.shift;
        if(!(s->iai_blocked_solo&ML_X))permission[WI_X.word]|=stage<<WI_X.shift;
        if(!(s->iai_blocked_solo&ML_R))permission[WI_R.word]|=stage<<WI_R.shift;
    }
}
static unsigned world_source_node(unsigned source)
{
    switch(source){
    case 1:return 0;case 101:return 11;case 102:return 10;case 103:return 7;
    case 104:return 1;case 105:return 4;case 106:return 12;case 107:return 3;
    case 108:return 2;case 109:return 5;case 110:return 6;case 112:return 23;
    case 127:return 9;case 128:return 8;case 140:return 28;case 170:return 45;
    case 174:return 57;case 307:return 64;
    case 308:case 309:case 310:case 311:return 65;
    default:return 0xffff;
    }
}
/* Only the post-small-Iai spirit opener has no GU counterpart. Resolve the
 * other World edges to stock commands so their follow-up context, hit rows,
 * gauge accounting and presentation all belong to the native handler. */
static WDecision world_route(WState *s,const WObservation *o,unsigned edge)
{
    const WGroundNode *n=w_ground_node(edge);
    if(!n)return (WDecision){0};
    int dest=n->kind==2?w_ground_select(edge,0):(int)edge;
    if(dest==28||dest==45||dest==61||(s->move==W_SMALL&&edge==82))
        return ground_enter(s,o,edge,0);
    if(!s->move){
        /* The two ordinary combo edges use GU's thrust and combo upper.
         * Spirit insertions share these poses but have different actions. */
        unsigned from=o->base.native_action, motion=o->base.motion;
        if(edge==1&&w_guild_spirit_opener(from,motion,o->previous_weapon_action))
            return (WDecision){.kind=W_NATIVE,.native_action=o->base.gauge>=15?16:17,.heading=o->base.heading};
        unsigned upper=(from==4&&motion==0x569&&edge==31)||(from==22&&(motion==0x569||motion==0x570)&&edge==3);
        unsigned thrust=(from==25||from==41)&&motion==0x56b&&edge==4;
        if((from==7||from==25||from==41)&&motion==0x56b&&edge==39)
            return (WDecision){.kind=W_NATIVE,.native_action=5,.heading=o->base.heading};
        if(upper||thrust)
            return (WDecision){.kind=W_NATIVE,.native_action=upper?25:4,.heading=o->base.heading};
        return (WDecision){0};
    }

    unsigned action;
    switch(dest){
    case 1:action=o->base.gauge>=15?16:17;break;
    case 2:action=26;break;
    case 3:case 38:action=25;break;
    case 4:action=4;break;
    case 5:action=30;break;
    case 7:action=8;break;
    /* Measured against camera-centered walking: GU 28 travels left, 29 right. */
    case 8:action=28;break;
    case 9:action=29;break;
    case 10:action=5;break;
    case 11:action=0;break;
    case 12:action=23;break;
    case 23:action=22;break;
    default:return (WDecision){0};
    }
    WDecision d=w_leave(s);d.kind=W_NATIVE;d.native_action=action;d.heading=o->base.heading;
    return d;
}

static WDecision world_recovery_roll(WState *s,const MLObservation *b)
{
    WDecision d=w_leave(s);d.kind=W_ROLL;
    if(world_attack_back(b)){
        d.slot=WORLD_BACK_ROLL_SLOT;
        d.heading=b->heading;
    }else d.heading=b->magnitude>=50?b->input_heading:b->heading;
    return d;
}

static WDecision world_reckoning_follow(WState *s,const WObservation *o,unsigned pressed)
{
    const MLObservation *b=&o->base;
    /* Rise atk_112 starts the MOT330 branch at F114 with a ten-frame buffer.
     * Use that recovery window for the added GU Roundslash as well. */
    const float frame=b->animation_frame;
    unsigned stage=o->grounded?world_follow_stage(frame,114,10,150):0;
    /* Honor an R pressed at the end of the window after its chord grace. */
    unsigned pending_r=o->grounded&&s->reckoning_follow==ML_R&&frame<=150+WORLD_CHORD_GRACE;
    if((!stage&&!pending_r)||(b->held&ML_B))s->reckoning_follow=0;
    else if(stage&&(b->held==ML_Y||b->held==ML_R)&&(pressed&b->held)){
        s->reckoning_follow=b->held;
        s->reckoning_r_frame=frame;
    }
    if(stage==3&&s->reckoning_follow==ML_Y){
        s->reckoning_follow=0;
        return (WDecision){.kind=W_PUT_AWAY};
    }
    if(s->reckoning_follow==ML_R&&b->gauge>=25&&frame>=114&&frame>=s->reckoning_r_frame+WORLD_CHORD_GRACE){
        s->reckoning_follow=0;
        return (WDecision){.kind=W_NATIVE,.native_action=30,.heading=b->heading};
    }
    return (WDecision){0};
}

static WDecision world_big_iai_follow(WState *s,const WObservation *o,unsigned pressed)
{
    const MLObservation *b=&o->base;
    if(!o->grounded)return (WDecision){0};
    unsigned put_away=world_follow_stage(b->animation_frame,76,10,150);
    if(!put_away)s->put_away_buffered=0;
    else if((pressed&ML_Y)&&b->held==ML_Y)s->put_away_buffered=1;

    WControls *controls=&s->controls;
    WCommands *commands=&controls->commands;
    /* A chord owns both buttons, including while it is still buffered. */
    if(w_command_has(commands->raw,24)||w_command_primary(commands,24)){
        w_command_clear_primary(commands,17);
        w_command_clear_primary(commands,39);
    }
    if(w_command_has(commands->raw,53)||w_command_primary(commands,53)){
        w_command_clear_primary(commands,25);
        w_command_clear_primary(commands,39);
    }
    if(w_command_has(commands->raw,60)||w_command_primary(commands,60))
        w_command_clear_primary(commands,39);
    if(w_controls_query(controls,24,1,0))return enter(s,o,W_SHEATHE,ML_R|ML_B);
    if(s->counter_success&&w_controls_query(controls,60,1,0))return enter(s,o,W_FORESIGHT,ML_R|ML_A);
    /* GU's thrust is the existing entry to Helm Breaker. */
    if(w_controls_query(controls,53,1,0))return enter(s,o,W_THRUST,ML_R|ML_X);
    if(o->base.gauge>=20&&w_controls_query(controls,39,1,0)){
        WDecision d=w_leave(s);d.kind=W_NATIVE;d.native_action=27;d.heading=b->heading;
        return d;
    }
    if(w_controls_query(controls,25,1,0))return world_route(s,o,11);
    if(put_away==3&&s->put_away_buffered){
        WDecision d=w_leave(s);d.kind=W_PUT_AWAY;return d;
    }
    if(w_controls_query(controls,17,1,0))return world_recovery_roll(s,b);
    if(b->motion_finished||b->animation_frame>=w_clip(s->source)->length-1){
        WDecision d=w_leave(s);d.kind=W_IDLE;return d;
    }
    return (WDecision){0};
}
/* Preserve a two-button tap across GU's native request hook and the
 * partner's delayed confirmation. Both physical buttons must overlap on
 * the same stock attack in a valid source window. */
static void world_guild_chords(WState *s,unsigned held,const uint32_t *permission)
{
    WControls *c=&s->controls;
    static const unsigned ids[]={53,60,24},partners[]={ML_X,ML_A,ML_B},buttons[]={0,1,3};
    for(unsigned i=0;i<3;++i){
        unsigned id=ids[i],mask=ML_R|partners[i];
        unsigned stage=w_command_stage(permission,id);
        if(!stage||(held&~mask)){
            if(s->gu_chord_wait==id)s->gu_chord_wait=0;
            continue;
        }
        if(held==mask&&(w_command_has(c->commands.raw,id)||w_command_has(c->commands.observed,id)))
            s->gu_chord_wait=id;
        if(s->gu_chord_wait!=id)continue;
        if(c->buttons[buttons[i]].pressed){
            w_command_collect(&c->commands,id,stage,0,0,0,0);
            w_command_promote(&c->commands,id);
        }
        if(!c->buttons[buttons[i]].waiting)s->gu_chord_wait=0;
    }
}
static WDecision world_command_step(WState *s,const WObservation *o,unsigned source,
                                    const WEvent *event,unsigned pressed)
{
    const MLObservation *b=&o->base;
    unsigned idle=!s->move&&(o->state==1||o->state==2)&&b->native_action<=5&&o->grounded;
    unsigned air_spirit=!s->move&&o->state==16&&
        ((((b->native_action>=42&&b->native_action<=47)||(b->native_action>=76&&b->native_action<=81))&&
          (b->motion==0x598||b->motion==0x568))||
         (b->native_action>=72&&b->native_action<=75&&
          ((b->motion>=0x599&&b->motion<=0x59c)||b->motion==0x568)));
    if(s->air_sheathe_sample!=b->sample_sequence){
        s->air_sheathe_sample=b->sample_sequence;
        if(!air_spirit)s->air_sheathe_buffer=0;
        else{
            if(b->simulation_delta>0&&b->simulation_delta<30)
                s->air_sheathe_buffer-=b->simulation_delta/60;
            if(!o->grounded&&b->held==(ML_R|ML_B)&&s->air_sheathe_held!=(ML_R|ML_B))
                s->air_sheathe_buffer=.35f;
        }
        s->air_sheathe_held=b->held;
    }
    if(ML_WORLD_STYLE_ENHANCED){
        if(!s->move&&o->state==16&&b->native_action==WORLD_RECKONING_ACTION&&b->motion==WORLD_RECKONING_MOTION)
            return world_reckoning_follow(s,o,pressed);
        s->reckoning_follow=0;
    }
    if(!s->move){
        source=w_guild_source(o->state,b->native_action,b->motion);
        if(!source||!o->grounded){s->controls=(WControls){0};s->gu_chord_wait=0;return (WDecision){0};}
        if(air_spirit&&(b->motion==0x568||b->motion==0x59a||b->motion==0x59c)&&b->animation_frame>=10&&b->animation_frame<60&&
           s->air_sheathe_buffer>0&&!(b->held&(ML_X|ML_A|ML_Y|ML_ZR))){
            s->air_sheathe_buffer=0;
            return enter(s,o,W_SHEATHE,ML_R|ML_B);
        }
        event=w_event(source,idle?0:b->animation_frame);
    }
    float frame=b->animation_frame;
    if(ML_WORLD_STYLE_ENHANCED&&s->move==W_BIG&&pressed){
        /* A delayed solo confirmation must not fire after a chord tap. */
        s->iai_blocked_solo&=~pressed;
        if((b->held&ML_R)&&(b->held&(ML_A|ML_B|ML_X)))s->iai_blocked_solo|=ML_R;
        if((b->held&ML_X)&&(b->held&(ML_A|ML_R)))s->iai_blocked_solo|=ML_X;
        /* The chord VM polls a waiting partner each update. Require a
         * physical overlap inside the recovery window. */
        if(world_follow_stage(frame,76,10,0)){
            if((pressed&(ML_R|ML_X))&&(b->held&(ML_R|ML_X))==(ML_R|ML_X))
                s->iai_armed_partners|=ML_X;
            if(s->counter_success&&(pressed&(ML_R|ML_A))&&b->held==(ML_R|ML_A))
                s->iai_armed_partners|=ML_A;
        }
    }
    uint32_t permission[8];world_input_permission(s,source,event,frame,permission);
    const float direction[4]={(float)b->input_heading,0,0,0};
    int delta=ml_heading_delta(b->input_heading,b->heading);
    /* Recorded GU input: positive quarter-turn is the hunter's left.
     * Original World motion128 is left,127 is right; legacy preview labels
     * had these reversed and must not define this command binding. */
    unsigned facing=delta>=8192&&delta<=24576?3:
        delta<=-8192&&delta>=-24576?1:delta>24576||delta< -24576?2:0;
    if(s->input_motion!=b->motion){w_controls_bind(&s->controls);s->input_motion=b->motion;s->gu_chord_wait=0;}
    /* A short R+B tap can end before the two Foresight cancel windows.
     * Latch only a fresh chord; the source timeline still decides entry. */
    unsigned sheath_edge=b->held==(ML_R|ML_B)&&
        !(s->controls.buttons[2].raw&&s->controls.buttons[3].raw);
    w_controls_tick(&s->controls,b->actor,b->epoch,b->sample_sequence,b->held,
        b->simulation_delta/60,permission,direction,b->magnitude>=50,facing,facing);
    if(!s->move)world_guild_chords(s,b->held,permission);
    else s->gu_chord_wait=0;
    if(s->main_source&&(b->motion_finished||frame>=w_clip(source)->length-1)){
        unsigned main=s->main_source;s->main_source=0;
        return ground_motion(s,main,source,frame,W_MOTION);
    }
    if(s->move==W_SHEATHE){
        if(event&&event->phase==1&&b->simulation_delta>0&&b->simulation_delta<30)
            s->hold_seconds+=b->simulation_delta/60;
        if(ML_WORLD_STYLE_ENHANCED){
            /* Special Sheathe has no R/X chord to disambiguate. Buffer the
             * physical edge and release on the F73 blade-close cue. */
            unsigned iai=pressed&~s->consumed;
            if(frame>=60&&!(b->held&(ML_A|ML_B|ML_Y|ML_ZR))){
                if(iai&ML_R)s->queued=W_BIG;
                else if(iai&ML_X)s->queued=W_SMALL;
            }
            if(frame>=WORLD_SHEATHE_CLOSE_FRAME&&s->queued)
                return enter(s,o,s->queued,s->queued==W_BIG?ML_R:ML_X);
        }else{
            if(w_controls_query(&s->controls,39,1,0))return enter(s,o,W_BIG,ML_R);
            if(w_controls_query(&s->controls,25,1,0))return enter(s,o,W_SMALL,ML_X);
        }
        if(s->hold_seconds>=3.5f)return enter(s,o,W_SHEATHE_END,0);
        return (WDecision){0};
    }
    if(s->move==W_THRUST&&s->hit_success&&o->rank>=1)return enter(s,o,W_RISE,0);
    if(s->move==W_RISE){
        /* The last displayed pose in the two original jump starts is F45.32.
         * GU binds the successor on the next engine update. */
        if(b->motion_finished||frame>=w_clip(source)->length-2)return enter(s,o,W_AIR,0);
        return (WDecision){0};
    }
    if(s->move==W_AIR){
        if(o->grounded&&frame>0)return enter(s,o,W_LAND,0);
        WGroundQuery air={.s=s,.o=o};
        int next=world_query_node(&air,56);
        if(next==58)return enter(s,o,W_DOWN,ML_R|ML_X|ML_A);
        /* The GU air pose has no slash collider; finish the released-input
         * route with an actual downward swing after the manual input window. */
        if(frame>=60)return enter(s,o,W_DOWN,0);
        return (WDecision){0};
    }
    if(s->move==W_DOWN){if(o->grounded&&frame>0)return enter(s,o,W_LAND,0);return (WDecision){0};}
    if(s->move==W_SHEATHE_END){
        if(b->motion_finished||frame>=w_clip(source)->length-1){WDecision d=w_leave(s);d.kind=W_IDLE;return d;}
        return (WDecision){0};
    }
    if(ML_WORLD_STYLE_ENHANCED&&s->move==W_BIG)return world_big_iai_follow(s,o,pressed);
    if(!s->move&&(source==149||source==154)){
        if(w_controls_query(&s->controls,24,1,0))return enter(s,o,W_SHEATHE,ML_R|ML_B);
        if(w_controls_query(&s->controls,60,1,0))
            return enter(s,o,W_FORESIGHT,ML_R|ML_A);
        if(w_controls_query(&s->controls,53,1,0))return enter(s,o,W_THRUST,ML_R|ML_X);
        return (WDecision){0};
    }
    unsigned node=s->move==W_GROUND?s->ground_node:world_source_node(source);
    if(node==0xffff||!o->grounded)return (WDecision){0};
    if(s->move==W_FORESIGHT){
        if(ML_WORLD_STYLE_ENHANCED){
            /* Y uses the Rise finisher window. R and X+A keep their original routes. */
            unsigned stage=world_follow_stage(frame,88,10,150);
            if(!stage)s->reckoning_buffered=0;
            else if(b->held==ML_Y&&(pressed&ML_Y))s->reckoning_buffered=1;
            if(stage==3&&s->reckoning_buffered&&s->counter_success&&s->hit_success&&b->gauge>=25){
                WDecision d=w_leave(s);
                d.kind=W_NATIVE;d.native_action=WORLD_RECKONING_ACTION;d.heading=b->heading;
                return d;
            }
        }
        if(sheath_edge&&frame>=13&&frame<90){
            s->queued=W_SHEATHE;s->queue_until=frame<47?47:160;
        }
        if(s->queued==W_SHEATHE){
            if(frame>=s->queue_until||(b->held&(ML_X|ML_A|ML_Y|ML_ZR)))s->queued=0;
            else if(input_stage(event,WI_SHEATHE)==3)return ground_enter(s,o,61,ML_R|ML_B);
        }
    }
    WGroundQuery query={.s=s,.o=o};
    /* Shared weapon-ground special-sheathe transition, before local links.
     * Its own TIML command permission still owns both buffering and entry. */
    if(node!=0&&node!=64&&node!=65&&world_property(&query,11))
        return ground_enter(s,o,61,ML_R|ML_B);
    int next=world_query_node(&query,node);
    if(next>=0){
        WDecision d=world_route(s,o,(unsigned)next);
        if(d.kind!=W_KEEP)return d;
    }
    if(s->move&&event&&((event->input[0]>>8)&3)==3&&w_controls_query(&s->controls,17,0,0))
        return world_recovery_roll(s,b);
    if(s->move&&(b->motion_finished||frame>=w_clip(source)->length-1)){
        WDecision d=w_leave(s);d.kind=W_IDLE;return d;
    }
    return (WDecision){0};
}
