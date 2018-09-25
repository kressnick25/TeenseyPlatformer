#ifndef SPRITE_MODELS_H_   /* Include guard */
#define SPRITE_MODELS_H_
#include "includes.h"

/**uint8_t player_image[7] = {
    0b11101110,
    0b10111010,
    0b11010110,
    0b01111100,
    0b11010110,
    0b10111010,
    0b11101110
};**/

uint8_t player_image[4] = {
    0b00100000,
    0b11111000,
    0b01010000,
    0b11111000
};

uint8_t chest_image[3] = {
    0b01010000,
    0b10101000,
    0b01110000,
};

uint8_t block_image[4] = {
    0b11111111, 0b11000000,
    0b11111111, 0b11000000  
};

uint8_t bad_image[4] = {
    0b10101010, 0b10100000,
    0b11111111, 0b11100000
};

#endif 