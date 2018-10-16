#ifndef SPRITE_CONTROL_H_   /* Include guard */
#define SPRITE_CONTOL_H_
#include "includes.h"

void sprite_step( sprite_id sprite){
    sprite->x += sprite->dx;
    sprite->y += sprite->dy;
}

#endif 