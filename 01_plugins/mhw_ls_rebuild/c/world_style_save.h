#ifndef WORLD_STYLE_SAVE_H
#define WORLD_STYLE_SAVE_H

#include <stdint.h>

/* Selection belongs to the currently loaded character. Gameplay also checks
 * the local hunter, native backing style, and equipped weapon. */
unsigned world_style_selected(void);
void world_style_select(unsigned enabled);
unsigned world_style_set_selected(unsigned slot);
unsigned world_style_save_install(uintptr_t base);

#endif
