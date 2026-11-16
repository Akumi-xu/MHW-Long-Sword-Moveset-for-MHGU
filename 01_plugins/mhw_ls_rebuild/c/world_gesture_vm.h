#pragma once
#include "world_input_vm.h"
typedef struct {uint32_t raw[5],pressed[5];} WGestures;
static inline void w_gesture_bit(uint32_t *bits,unsigned id){if(id<137)bits[id/32]|=1u<<(id%32);}
static inline void w_gesture_direction(uint32_t *bits,unsigned id,unsigned stick,unsigned direction)
{
    if(!stick){w_gesture_bit(bits,id+1);return;}
    w_gesture_bit(bits,id+2);
    static const unsigned variants[]={3,6,4,5};
    if(direction<4)w_gesture_bit(bits,id+variants[direction]);
}
/* Native1231d57..1232442, with directional helpers1233390/1232f20.
 * A chord is promoted by its partner's matured pulse while R is waiting or
 * held. A short released tap remains a waiting button, as in11edc90.
 * There is no extra delay applied when a command reaches an execute window. */
static inline WGestures w_gestures(const WButton *x,const WButton *a,
    const WButton *r,const WButton *b,unsigned stick,unsigned direction,unsigned roll_direction)
{
    WGestures g={0};
    if(b->edge){w_gesture_bit(g.raw,17);w_gesture_direction(g.raw,17,stick,roll_direction);}
    if(b->pressed){w_gesture_bit(g.pressed,17);w_gesture_direction(g.pressed,17,stick,roll_direction);}
    if(r->waiting||r->held){
        if(b->waiting||b->edge)w_gesture_bit(g.raw,24);
        if(b->pressed)w_gesture_bit(g.pressed,24);
    }
    if((x->waiting||x->pressed)&&(a->waiting||a->pressed)){
        w_gesture_bit(g.raw,46);w_gesture_direction(g.raw,46,stick,direction);
        w_gesture_bit(g.pressed,46);w_gesture_direction(g.pressed,46,stick,direction);
    }
    const WButton *buttons[]={x,a,r};static const unsigned ids[]={25,32,39};
    for(unsigned i=0;i<3;++i){
        if(buttons[i]->edge){w_gesture_bit(g.raw,ids[i]);w_gesture_direction(g.raw,ids[i],stick,direction);}
        if(buttons[i]->pressed){w_gesture_bit(g.pressed,ids[i]);w_gesture_direction(g.pressed,ids[i],stick,direction);}
    }
    if(r->waiting||r->held){
        const WButton *partners[]={x,a};static const unsigned chords[]={53,60};
        for(unsigned i=0;i<2;++i){
            if(partners[i]->waiting||partners[i]->edge){
                w_gesture_bit(g.raw,chords[i]);w_gesture_direction(g.raw,chords[i],stick,direction);
            }
            if(partners[i]->pressed){
                w_gesture_bit(g.pressed,chords[i]);w_gesture_direction(g.pressed,chords[i],stick,direction);
            }
        }
        if((x->waiting||x->pressed)&&(a->waiting||a->pressed)){
            w_gesture_bit(g.raw,67);w_gesture_direction(g.raw,67,stick,direction);
            w_gesture_bit(g.pressed,67);w_gesture_direction(g.pressed,67,stick,direction);
        }
    }
    return g;
}
