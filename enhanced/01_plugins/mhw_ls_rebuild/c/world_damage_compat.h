#ifndef WORLD_DAMAGE_COMPAT_H
#define WORLD_DAMAGE_COMPAT_H
#include <stdint.h>

static int world_hook_head_matches(const uint32_t *detour,const uint32_t *head,unsigned count)
{
    if(!detour)return 0;
    for(unsigned i=0;i<count;++i)if(detour[i]!=head[i])return 0;
    return 1;
}

static int world_player_damage_entry_matches(const uint32_t *entry, const uint32_t *detour)
{
    if(entry[0]==0xe92d4ff0u&&entry[1]==0xe28db01cu)return 1;
    /* cb_gp_cancel A0851FE4: retain its GP retry through Forge's trampoline. */
    static const uint32_t cb[]={0xe92d48f0u,0xe28db010u,0xe24dd030u,0xe59f6164u,
        0xe1a04000u,0xe08f6006u,0xe5965024u,0xeb00244bu};
    /* mhgu_escape 09721C3D: preserve its charge and native evade handler. */
    static const uint32_t escape[]={0xe92d4ff0u,0xe28db01cu,0xe24dd00cu,0xe59f51dcu,
        0xe1a04003u,0xe1a09002u,0xe1a0a001u,0xe08f5005u,0xe1a07000u,0xe595803cu,
        0xeb000ef4u,0xe5950020u};
    return entry[0]==0xe51ff004u&&(world_hook_head_matches(detour,cb,sizeof(cb)/sizeof(*cb))||
        world_hook_head_matches(detour,escape,sizeof(escape)/sizeof(*escape)));
}

static int world_floor_entry_matches(const uint32_t *entry,const uint32_t *detour)
{
    if(entry[0]==0xe92d4df0u&&entry[1]==0xe28db018u)return 1;
    /* Rise Drink v1.3 18E58944 shares the native floor query. */
    static const uint32_t drink[]={0xe92d4ff0u,0xe24dd004u,0xed2d8b06u,0xe24dd010u,
        0xe1a08003u,0xe1a06002u,0xe1a07001u,0xe1a04000u,0xebfff54bu,0xe3500000u,
        0x0a00000au,0xe59fc57cu};
    return entry[0]==0xe51ff004u&&world_hook_head_matches(detour,drink,sizeof(drink)/sizeof(*drink));
}

static int world_damage_entry_matches(const uint32_t *entry, const uint32_t *detour)
{
    if (entry[0] == 0xe92d41f0u && entry[1] == 0xe24dd010u) return 1;
    /* Both supplied damage displays retain the native hit through Forge. */
    static const uint32_t display[]={0xe92d4ff0u,0xe28db01cu,0xe24dd044u,
        0xe58d001cu,0xe1a06002u,0xe59f0268u};
    static const uint32_t ff14[]={0xe92d4ff0u,0xe28db01cu,0xe24dd01cu,0xe1a06000u,
        0xe59f0108u,0xe1a07003u,0xe1a04002u,0xe08f0000u,0xe1a05001u,0xe5900030u,
        0xe58d0018u,0xeb0019f7u};
    return entry[0]==0xe51ff004u&&(world_hook_head_matches(detour,display,sizeof(display)/sizeof(*display))||
        world_hook_head_matches(detour,ff14,sizeof(ff14)/sizeof(*ff14)));
}
#endif
