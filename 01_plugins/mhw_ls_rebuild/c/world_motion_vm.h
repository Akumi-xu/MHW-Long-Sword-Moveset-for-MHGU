#pragma once
#include "world_motion_links.h"

/* 1c007f0: ordered exact previous-motion/bank match, then Default.
 * A positive previous-frame limit is inclusive. */
static inline const WMotionLink *w_motion_link(unsigned target,unsigned previous,
                                              unsigned bank,float frame)
{
    const WMotionLink *fallback=0;
    for(unsigned i=0;i<sizeof(world_motion_links)/sizeof(*world_motion_links);++i){
        const WMotionLink *r=world_motion_links+i;
        if(r->target!=target)continue;
        if(r->bank==W_BANK_DEFAULT){if(!fallback)fallback=r;continue;}
        if(r->previous==previous&&r->bank==bank&&(!(r->limit>0)||frame<=r->limit))return r;
    }
    return fallback;
}
