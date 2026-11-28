#pragma once
#include <stdint.h>
#include "world_ground_graph.h"

static inline const WGroundNode *w_ground_node(unsigned id)
{
    for(unsigned i=0;i<sizeof(world_ground_nodes)/sizeof(*world_ground_nodes);++i)
        if(world_ground_nodes[i].id==id)return world_ground_nodes+i;
    return 0;
}
static inline unsigned w_ground_condition(const WGroundEdge *edge,uint64_t properties)
{
    unsigned stack[32],top=0;
    for(unsigned i=0;i<edge->count;++i){
        WGroundTerm term=world_ground_terms[edge->offset+i];
        if(term.op<=1){
            if(top>=32)return 0;
            stack[top++]=term.op?(unsigned)((properties>>term.value)&1):term.value;
        }else{
            if(term.value>top)return 0;
            unsigned result=term.op==2;
            for(unsigned n=0;n<term.value;++n){
                unsigned value=stack[--top];
                if(term.op==2)result&=value;else result|=value;
            }
            stack[top++]=result;
        }
    }
    return top==1&&stack[0];
}
/* Return the original helper node, never skip a source LinkMotion process. */
static inline int w_ground_select(unsigned node,uint64_t properties)
{
    const WGroundNode *n=w_ground_node(node);if(!n)return -1;
    for(unsigned i=0;i<n->count;++i){
        const WGroundEdge *edge=world_ground_edges+n->offset+i;
        if(w_ground_condition(edge,properties))return (int)edge->target;
    }
    return -1;
}
