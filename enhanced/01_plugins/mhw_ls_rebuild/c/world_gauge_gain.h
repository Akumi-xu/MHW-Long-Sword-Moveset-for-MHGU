#ifndef WORLD_GAUGE_GAIN_H
#define WORLD_GAUGE_GAIN_H

static unsigned world_normal_gauge_action(unsigned state,unsigned action)
{
    return state==16&&(action<=14||action==22||action==25||action==41||
        (action>=32&&action<=34)||(action>=63&&action<=65)||(action>=69&&action<=71));
}

static int world_normal_gauge_gain(int gain,unsigned *tenths)
{
    if(gain<=0||gain>100)return gain;
    /* Carry the extra tenth across hits instead of rounding each hit away. */
    unsigned extra=*tenths+(unsigned)gain;
    *tenths=extra%10;
    return gain+(int)(extra/10);
}
#endif
