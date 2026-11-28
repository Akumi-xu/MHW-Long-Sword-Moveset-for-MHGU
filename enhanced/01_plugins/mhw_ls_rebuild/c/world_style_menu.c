#include "world_style_menu.h"
#include "world_style_save.h"

/* Addresses relative to the retail GU main NSO. All UI calls here have only
 * integer/pointer arguments in r0-r3; there are no float or stack arguments. */
typedef uintptr_t Word;
typedef Word (*Call)(Word, Word, Word, Word);
typedef struct { void *target, *detour, *original, *context; void **cell; unsigned has_context; } Hook;
extern Hook forge_hook_create(void *, void *, void **);

static Word main_base, box_controller, preview_label;
static unsigned preview_slot, installed;
static Call original_open, original_tick, original_description;
static Call original_large, original_small, original_preview, original_label;

enum { WORLD_ROW = 6, WORLD_NODE = 0x4e, GUILD_NODE = 7 };
static Word word(Word obj, unsigned off) { return *(Word *)(obj + off); }
static unsigned char *byte_at(Word obj, unsigned off) { return (unsigned char *)(obj + off); }
static Word native(unsigned offset, Word a, Word b, Word c, Word d)
{
    return ((Call)(main_base + offset))(a, b, c, d);
}
static const char *world_name(void)
{
    return "世界风格";
}
static void set_text(Word ui, unsigned node, unsigned part, const char *value)
{
    native(0x56ea08, ui, node, part, (Word)value);
}
static unsigned is_world_choice(Word record)
{
    Word manager = word(main_base, 0x1885b58);
    return record && manager && native(0x275a30, manager, 0, 0, 0) == record
        && *byte_at(record, 1241) == 0 && world_style_selected();
}
static void label_current(Word menu)
{
    unsigned world = word(menu, 104) == 0 && world_style_selected();
    native(0x56e530, menu, WORLD_NODE, 1, world);
    if (world) {
        native(0x56e530, menu, GUILD_NODE, 1, 0);
        set_text(menu, 0x55, 1, world_name());
    }
}
static Word description(Word down, Word kind, Word row, Word page)
{
    if (!installed || !box_controller || down != word(box_controller, 8) ||
        kind != 0 || row != WORLD_ROW)
        return original_description(down, kind, row, page);
    /* Refresh the panel's native visibility and page state using a valid row.
     * The original has six-element description tables for this list. */
    Word result = original_description(down, kind, 0, page);
    set_text(down, 2, 2, world_name());
    set_text(down, 3, 2, "使用太刀时，可从攻击派生特殊纳刀与居合。");
    return result;
}
static void extend_list(Word box, unsigned restore_selection)
{
    Word menu = word(box, 4), list = word(menu, 88);
    Word cursor = box + 12;
    unsigned row = *byte_at(cursor, 13);
    native(0x54edec, cursor, WORLD_ROW + 1, 1, 0);
    *byte_at(cursor, 13) = restore_selection && word(menu, 104) == 0 && world_style_selected()
        ? WORLD_ROW : row;
    native(0x5c5500, list, WORLD_ROW + 1, 0, 0);
    set_text(menu, WORLD_NODE, 2, world_name());
    label_current(menu);
    native(0x548e24, box, 0, 0, 0);
}
static Word open_menu(Word box, Word menu, Word down, Word flags)
{
    Word result = original_open(box, menu, down, flags);
    if (installed && menu && down && word(box, 128) == 1) {
        box_controller = box;
        extend_list(box, 1);
    }
    return result;
}
static void begin_confirmation(Word box)
{
    *byte_at(box, 120) = 0;
    *(Word *)(box + 128) = 2;
    native(0x54ecc0, box + 12, 0, 0, 0);
    native(0x54ecc0, box + 84, 0, 0, 0);
    native(0x54f260, 0, 1, 0, 0);
    native(0x54ecc0, box + 12, 1, 0, 0);
    native(0x54ecc0, box + 84, 1, 0, 0);
}
static Word box_tick(Word box, Word b, Word c, Word d)
{
    if (!installed || box != box_controller)
        return original_tick(box, b, c, d);
    unsigned state = word(box, 128), row = *byte_at(box, 25);
    unsigned world_row = row == WORLD_ROW;
    if (state == 1) {
        Word pad = word(main_base, 0x188c670);
        if ((word(pad, 27644) & 0x2000) &&
            (world_row || (row == 0 && world_style_selected()))) {
            if (world_row && world_style_selected() && word(word(box, 4), 104) == 0) {
                *byte_at(box, 25) = 0;
                return original_tick(box, b, c, d);
            }
            begin_confirmation(box);
            return 1;
        }
    }
    /* Only the cursor uses 6. GU's apply path indexes a six-row table and
     * passes the result to the equipment, arts and actor dispatch routines. */
    if (state == 3 && world_row)
        *byte_at(box, 25) = 0;
    Word result = original_tick(box, b, c, d);
    unsigned next = word(box, 128);
    if (state == 3 && next == 4)
        world_style_select(world_row);
    if (next == 1 && next != state)
        extend_list(box, 0);
    if (next == 5 && next != state)
        label_current(word(box, 4));
    if (!next)
        box_controller = 0;
    return result;
}
static Word status_large(Word ui, Word record, Word detail, Word d)
{
    Word result = original_large(ui, record, detail, d);
    if (installed && is_world_choice(record)) {
        set_text(ui, 0x5c, 1, world_name());
        Word cursor = word(ui, 80);
        if (detail && cursor && *byte_at(cursor, 13) == 0) {
            set_text(ui, 0x14, 1, world_name());
            set_text(ui, 0x14, 2, "使用太刀时，可从攻击派生特殊纳刀与居合。");
        }
    }
    return result;
}
static Word status_small(Word ui, Word b, Word c, Word d)
{
    Word result = original_small(ui, b, c, d);
    if (installed && is_world_choice(word(ui, 76)))
        set_text(ui, 0x69, 2, world_name());
    return result;
}
static Word preview_bind(Word panel, Word slot, Word c, Word d)
{
    Word result = original_preview(panel, slot, c, d);
    preview_label = native(0x65cb3c, panel, 22, 0, 0);
    preview_slot = slot;
    /* My Set's expanded panel uses a formatted text line on child 21. */
    if (installed && word(panel, 840) && world_style_set_selected(slot)) {
        Word ui = native(0x65cb3c, panel, 21, 0, 0);
        if (ui && word(ui, 76) == panel + 3052 && *byte_at(panel + 3052, 1241) == 0)
            set_text(ui, 2, 3, world_name());
    }
    return result;
}
static Word preview_paint(Word ui, Word b, Word c, Word d)
{
    Word result = original_label(ui, b, c, d);
    if (installed && ui == preview_label && word(ui, 76) == 0 && world_style_set_selected(preview_slot))
        set_text(ui, 0, 3, world_name());
    return result;
}
static unsigned hook(unsigned offset, Call replacement, Call *original)
{
    forge_hook_create((void *)(main_base + offset), (void *)replacement, (void **)original);
    return *original != 0;
}
unsigned world_style_menu_install(uintptr_t base)
{
    if (installed) return 1;
    main_base = base;
    installed = hook(0x6394b8, description, &original_description)
        && hook(0x62fb78, status_large, &original_large)
        && hook(0x633450, status_small, &original_small)
        && hook(0x72d3d8, preview_bind, &original_preview)
        && hook(0x58a44c, preview_paint, &original_label)
        && hook(0x5490fc, box_tick, &original_tick)
        && hook(0x548b7c, open_menu, &original_open);
    return installed;
}
