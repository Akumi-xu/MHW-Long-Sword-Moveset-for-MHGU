/* World build 421810 input VM. Offsets are verified against 11edc90;
 * command collection/promotion against 1230990 and 12308d0.
 * This module does not manufacture permissions before a TIML buffer window.
 */
#pragma once
#include <stdint.h>
#include "world_command_bits.h"

typedef struct {
    uint32_t reserved[2];
    uint8_t raw, previous, edge, padding;
    float wait_seconds, remaining;
    int32_t frames, wait_frames;
    float age;
    uint8_t waiting, waiting_edge, held, pressed, released, tail[3];
} WButton;

static inline void w_button_tick(WButton *b, float seconds)
{
    b->pressed=b->released=b->waiting_edge=b->edge=0;
    if(b->raw&&!b->previous)b->edge=1;
    if(b->held){
        if(b->raw)b->age+=seconds;
        else {b->released=1;b->held=0;}
    }else if(!b->waiting&&b->raw){
        b->remaining=b->wait_seconds;b->frames=b->wait_frames;
        b->waiting=b->waiting_edge=1;b->age=0;
    }
    if(b->waiting){
        if(b->remaining<=0&&b->frames<=0){
            b->waiting=0;b->held=b->pressed=1;
        }
        --b->frames;b->remaining-=seconds;
    }
    b->previous=b->raw;
}

/* 137 native command IDs, not the unrelated TIML bit indices. */
typedef struct {
    uint32_t observed[5], raw[5], executable[5], raw_executable[5];
    uint32_t buffered[5], secondary[5], secondary_buffered[5];
    float direction[4], buffered_direction[4];
} WCommands;

static inline void w_command_collect(WCommands *c,unsigned command,
                                     unsigned permission,unsigned global,
                                     unsigned secondary_permission,
                                     unsigned secondary_global,unsigned layer)
{
    if(command>=137)return;
    unsigned word=command/32;uint32_t bit=1u<<(command%32);
    if(((permission|global)&1)&&!(c->executable[word]&bit)){
        c->buffered[word]|=bit;
        for(unsigned i=0;i<3;++i)c->buffered_direction[i]=c->direction[i];
        c->buffered_direction[3]=0;
    }
    if(layer&&!(c->secondary[word]&bit)&&((secondary_permission|secondary_global)&1)){
        c->secondary_buffered[word]|=bit;
        for(unsigned i=0;i<3;++i)c->buffered_direction[i]=c->direction[i];
        c->buffered_direction[3]=0;
    }
    if((permission|global)&2){
        c->raw_executable[word]|=bit;
        for(unsigned i=0;i<3;++i)c->buffered_direction[i]=c->direction[i];
        c->buffered_direction[3]=0;
    }
    c->raw[word]|=bit;
}

static inline void w_command_promote(WCommands *c,unsigned command)
{
    if(command>=137)return;
    unsigned word=command/32;uint32_t bit=1u<<(command%32);
    c->observed[word]|=bit;
    if(c->buffered[word]&bit){c->buffered[word]&=~bit;c->executable[word]|=bit;}
    if(c->secondary_buffered[word]&bit){
        c->secondary_buffered[word]&=~bit;c->secondary[word]|=bit;
    }
}

typedef struct {uint32_t tested[5],other[5],queried[5];} WCommandQueries;
enum {W_QUERY_LEVEL,W_QUERY_EDGE,W_QUERY_INVERSE_EDGE};
/* Native122fe60 /122fdb0 /122fcd0. 'allowed' is1230000's secondary
 * layer gate; 'execute' is1230070's current primary TIML permission. */
static inline unsigned w_command_query(const WCommands *c,WCommandQueries *q,
    unsigned id,unsigned kind,unsigned mode,unsigned allowed,unsigned execute)
{
    if(id>=137)return 0;
    unsigned word=id/32;uint32_t bit=1u<<(id%32);
    if(kind==W_QUERY_INVERSE_EDGE)return allowed&&execute&&!(c->observed[word]&bit);
    q->tested[word]|=bit;
    if(!allowed)return 0;
    q->queried[word]|=bit;
    unsigned observed=(c->observed[word]&bit)!=0;
    if(kind==W_QUERY_LEVEL){
        if(mode)return !observed;
        return observed||((c->executable[word]|c->secondary[word])&bit)!=0;
    }
    if((c->raw_executable[word]&bit)&&observed)return 1;
    return execute&&(observed||((c->executable[word]|c->secondary[word])&bit)!=0);
}

static inline unsigned w_permission_bit(const uint32_t *p,int bit)
{return bit>=0&&bit<256?((p[(unsigned)bit/32]>>((unsigned)bit%32))&1u):0;}
static inline unsigned w_command_stage(const uint32_t *permission,unsigned id)
{
    int bit=world_command_bits[id];
    return w_permission_bit(permission,bit-1)|(w_permission_bit(permission,bit)<<1);
}

/* Native1230230; aliases share a permission and are consumed together.
 * Primary and secondary action-layer changes have distinct clearing rules. */
static inline void w_command_prune(WCommands *c,WCommandQueries *q,uint32_t *previous,
    const uint32_t *primary,const uint32_t *secondary,unsigned force,
    unsigned changed_primary,unsigned changed_secondary)
{
    if(force){
        for(unsigned i=0;i<5;++i)c->buffered[i]=c->executable[i]=c->secondary_buffered[i]=c->secondary[i]=0;
    }else if(changed_primary||changed_secondary){
        uint32_t tested_permissions[8]={0};
        for(unsigned id=0;id<137;++id){
            int p=world_command_bits[id]-1;if(p<0)continue;
            unsigned word=id/32;uint32_t bit=1u<<(id%32);
            unsigned allowed=w_permission_bit(primary,p)||w_permission_bit(secondary,p);
            if(changed_primary&&!changed_secondary)allowed|=w_permission_bit(primary,202);
            if(!allowed){
                if(changed_primary){c->buffered[word]&=~bit;c->executable[word]&=~bit;}
                if(changed_secondary){c->secondary_buffered[word]&=~bit;c->secondary[word]&=~bit;}
            }
            if((q->queried[word]|(changed_primary?q->other[word]:0))&bit)
                tested_permissions[(unsigned)p/32]|=1u<<((unsigned)p%32);
        }
        for(unsigned id=0;id<137;++id){
            int p=world_command_bits[id]-1;if(!w_permission_bit(tested_permissions,p))continue;
            unsigned word=id/32;uint32_t bit=1u<<(id%32);
            if(changed_secondary){c->buffered[word]&=~bit;c->executable[word]&=~bit;}
            if(changed_primary){c->secondary_buffered[word]&=~bit;c->secondary[word]&=~bit;}
        }
    }
    for(unsigned i=0;i<5;++i){previous[i]=q->queried[i];q->queried[i]=0;}
}

static inline unsigned w_command_has(const uint32_t *bits,unsigned id)
{return id<137&&(bits[id/32]&(1u<<(id%32)))!=0;}
static inline unsigned w_command_primary(const WCommands *c,unsigned id)
{return w_command_has(c->buffered,id)||w_command_has(c->executable,id)||
        w_command_has(c->raw_executable,id)||w_command_has(c->observed,id);}
static inline void w_command_clear_primary(WCommands *c,unsigned id)
{
    if(id>=137)return;
    unsigned word=id/32;uint32_t bit=~(1u<<(id%32));
    c->buffered[word]&=bit;c->executable[word]&=bit;
    c->raw_executable[word]&=bit;c->observed[word]&=bit;
}
static inline void w_command_clear_secondary(WCommands *c,unsigned id)
{
    if(id>=137)return;
    unsigned word=id/32;uint32_t bit=~(1u<<(id%32));
    c->secondary_buffered[word]&=bit;c->secondary[word]&=bit;
}
/* Native122f4f0, including all seven directional variants. A queried chord
 * consumes the corresponding solo commands; directional queries similarly
 * consume their generic counterpart. Raw input edges remain untouched. */
static inline void w_command_conflicts(WCommands *c,const WCommandQueries *q,const uint32_t *previous)
{
    static const unsigned groups[3][4]={{25,46,53,67},{32,46,60,67},{39,53,60,67}};
    for(unsigned group=0;group<3;++group){
        for(unsigned partner=1;partner<4;++partner){
            unsigned chord=groups[group][partner];
            if(w_command_has(c->observed,chord)||w_command_has(c->raw_executable,chord)){
                if(w_command_has(previous,chord))
                    for(unsigned i=0;i<7;++i)w_command_clear_primary(c,groups[group][0]+i);
                if(w_command_has(q->tested,chord))
                    for(unsigned i=0;i<7;++i)w_command_clear_secondary(c,groups[group][0]+i);
            }
        }
    }
    for(unsigned base=10;base<=13;base+=3){
        if(!w_command_primary(c,base))continue;
        for(unsigned n=1;n<=2;++n){
            unsigned id=base+n;
            if(w_command_has(q->tested,id)&&(w_command_has(c->raw,id)||
               w_command_has(c->observed,id)||w_command_has(c->raw_executable,id)))
                w_command_clear_primary(c,base);
        }
    }
    static const unsigned bases[]={25,32,39,46,53,60,67};
    for(unsigned b=0;b<7;++b){
        unsigned base=bases[b];
        if(w_command_primary(c,base)){
            for(unsigned n=1;n<=5;++n)
                if(w_command_has(previous,base+n)&&
                   (w_command_has(c->buffered,base+n)||w_command_has(c->executable,base+n)))
                    w_command_clear_primary(c,base);
        }
        if(w_command_has(c->secondary,base)||w_command_has(c->secondary_buffered,base)){
            for(unsigned n=1;n<=5;++n)
                if(w_command_has(q->tested,base+n)&&
                   (w_command_has(c->buffered,base+n)||w_command_has(c->executable,base+n)))
                    w_command_clear_secondary(c,base);
        }
    }
}
