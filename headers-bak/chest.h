#ifndef _CHEST_H_
#define _CHEST_H_
#include "includes.h"

Sprite treasure;

// Moves chest along bottom of screen
void move_treasure()
{
    // Uses code from ZDJ Topic 04
    // Toggle chest movement when 't' pressed.
    /**if (key == 't')
    {
        stop_chest = !stop_chest;
    }
    if (!stop_chest)
    {
        sprite_step(chest);
        animate_chest();
    }**/
    // Change direction when wall hit
    int cx = round(treasure.x);
    double cdx = treasure.dx;
    if ( cx == 0 || cx == LCD_X - treasure.width){
        cdx = -cdx;
    }
    if ( cdx != treasure.dx){
        treasure.x -= treasure.dx;
        treasure.dx = cdx;
    }
}

#endif