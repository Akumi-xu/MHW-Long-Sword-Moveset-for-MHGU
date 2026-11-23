#include "world_style_save.h"
#include <stddef.h>

enum {
    EQUIP_BOX = 0x1856fe8, PLAYER_MANAGER = 0x1885b58, LOCAL_ACTOR = 0x2772c4,
    SETS = 254028, SET_SIZE = 136, SET_COUNT = 40,
    EQUIPMENT_IDS = 42, CURRENT_TAG = 98, NATIVE_STYLE = 130, SET_TAG = 135,
    WORLD_TAG = 0x574c, WORLD_SET = 0x57
};

/* The per-character read/write functions 14b0b0/14b1c4 copy these 40 rows
 * as one 5440-byte block. 14709c copies each defined field by hand: the
 * decoration words end at 97, the arts start at 100, and flags end at 134.
 * The current choice occupies padding in row 0 even when no set is registered.
 * Only the row marker follows a set when it is swapped, cleared or replaced.
 * Both native style bytes (in the hunter and in each row) stay in GU's range. */
static uintptr_t game;
typedef unsigned char *Box;
typedef uintptr_t (*Reset)(Box);
typedef uintptr_t (*OneSet)(Box,unsigned);
typedef uintptr_t (*SwapSets)(Box,unsigned,unsigned,unsigned);
static Reset original_reset;
static OneSet original_register, original_apply, original_clear;
static SwapSets original_swap;
static unsigned ready;

static Box current_box(void)
{
    return ready ? *(Box *)(game + EQUIP_BOX) : NULL;
}

static unsigned char *row(Box box, unsigned slot)
{
    return box + SETS + slot * SET_SIZE;
}

unsigned world_style_selected(void)
{
    Box box = current_box();
    return box && *(uint16_t *)(row(box,0) + CURRENT_TAG) == WORLD_TAG;
}

void world_style_select(unsigned enabled)
{
    Box box = current_box();
    if (!box) return;
    uint16_t *tag = (uint16_t *)(row(box,0) + CURRENT_TAG);
    if (enabled || *tag == WORLD_TAG) *tag = enabled ? WORLD_TAG : 0;
}

unsigned world_style_set_selected(unsigned slot)
{
    Box box = current_box();
    if (!box || slot >= SET_COUNT) return 0;
    unsigned char *record = row(box,slot);
    return record[NATIVE_STYLE] == 0 && record[SET_TAG] == WORLD_SET;
}

static void mark_set(unsigned char *record, unsigned world)
{
    if (world || record[SET_TAG] == WORLD_SET) record[SET_TAG] = world ? WORLD_SET : 0;
}

static uintptr_t register_set(Box box,unsigned slot)
{
    uintptr_t result = original_register(box,slot);
    if (result == 1 && box == current_box() && slot < SET_COUNT) {
        unsigned char *record = row(box,slot);
        mark_set(record,record[NATIVE_STYLE] == 0 && world_style_selected());
    }
    return result;
}

static uintptr_t apply_set(Box box,unsigned slot)
{
    /* 146560 returns several unrelated values. Its only early exits are the
     * index, seven empty equipment IDs, and a missing local actor. */
    unsigned applies = 0;
    if (box == current_box() && box && slot < SET_COUNT) {
        const uint16_t *ids = (const uint16_t *)(row(box,slot) + EQUIPMENT_IDS);
        for (unsigned i = 0; i < 7; ++i) {
            if (ids[i] != 0xffff) {
                uintptr_t mgr = *(uintptr_t *)(game + PLAYER_MANAGER);
                applies = mgr && ((uintptr_t (*)(uintptr_t))(game + LOCAL_ACTOR))(mgr);
                break;
            }
        }
    }
    uintptr_t result = original_apply(box,slot);
    if (applies) world_style_select(world_style_set_selected(slot));
    return result;
}

static uintptr_t clear_set(Box box,unsigned slot)
{
    uintptr_t result = original_clear(box,slot);
    if (box == current_box() && box && slot < SET_COUNT) mark_set(row(box,slot),0);
    return result;
}

static uintptr_t swap_sets(Box box,unsigned cat,unsigned left,unsigned right)
{
    uintptr_t result = original_swap(box,cat,left,right);
    if (box == current_box() && box && !cat && left < SET_COUNT && right < SET_COUNT) {
        unsigned char *a = row(box,left), *b = row(box,right);
        unsigned left_world = a[SET_TAG] == WORLD_SET, right_world = b[SET_TAG] == WORLD_SET;
        mark_set(a,right_world);
        mark_set(b,left_world);
    }
    return result;
}

static uintptr_t reset_box(Box box)
{
    uintptr_t result = original_reset(box);
    if (box == current_box() && box) {
        world_style_select(0);
        for (unsigned i = 0; i < SET_COUNT; ++i) mark_set(row(box,i),0);
    }
    return result;
}

typedef struct {
    void *target,*detour,*original,*context;
    void **cell;
    unsigned char has_context;
} ForgeHook;
extern ForgeHook forge_hook_create(void *,void *,void **);

unsigned world_style_save_install(uintptr_t base)
{
    if (ready) return 1;
    game = base;
    const struct {
        unsigned entry, first, second;
        void *hook;
        void **original;
    } hooks[] = {
        {0x141b70,0xe92d4ff0,0xe24dd004,reset_box,(void **)&original_reset},
        {0x146cb8,0xe92d4ff0,0xe24dd014,register_set,(void **)&original_register},
        {0x146560,0xe92d4ff0,0xe24ddf6f,apply_set,(void **)&original_apply},
        {0x147250,0xe92d4070,0xe1a05001,clear_set,(void **)&original_clear},
        {0x1480b4,0xe92d4bf0,0xe24dde11,swap_sets,(void **)&original_swap}
    };
    for (unsigned i = 0; i < sizeof(hooks)/sizeof(*hooks); ++i) {
        const uint32_t *p = (const uint32_t *)(game + hooks[i].entry);
        if (p[0] != hooks[i].first || p[1] != hooks[i].second) return 0;
    }
    for (unsigned i = 0; i < sizeof(hooks)/sizeof(*hooks); ++i) {
        forge_hook_create((void *)(game + hooks[i].entry),hooks[i].hook,hooks[i].original);
        if (!*hooks[i].original) return 0;
    }
    ready = 1;
    return 1;
}
