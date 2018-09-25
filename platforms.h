#ifndef PLATFORMS_H_   /* Include guard */
#define PLATFORMS_H_
#include "includes.h"

#define sizeOfPlatforms 28

Sprite Platforms[sizeOfPlatforms];
// gs multiplier, alter to change gamespeed.
float GS = 1;

void create_platforms( void ) {
    //memset(Platforms, 0, sizeof Platforms);
    int initX = 0, initY = 9;
    int deltaY = 0;
    int c = 0;
    float speed = 0.1 * GS;
    for (int i = 0; i < 4; i++)
    {
        int deltaX = 0;
        speed = -speed; // alternate direction
        for (int j = 0; j < 7; j++)
        {
            if (true) // bitmap != Null
            {
                sprite_init(&Platforms[c], initX + deltaX, initY + deltaY, 
                                                10, 2, block_image);
                Platforms[c].dx = speed;
                deltaX += 12;
            }
            else{
                // If not drawing a block, add space between blocks.
                deltaX += 12;
            }
            c++; 
        }
        deltaY += 10;
    }
}

void draw_platforms( void )
{
    for (int j = 0; j < sizeOfPlatforms; j++){
        //if (Platforms[j] != NULL){ 
            sprite_draw(&Platforms[j]);
        //}
    }
}

void auto_move_platforms( void )
{
    for (int i = 0; i < sizeOfPlatforms; i ++){
        //if (Platforms[i] != NULL){ // Do not attempt to call empty value
            sprite_step(&Platforms[i]);
            if (Platforms[i].x + 10 < 0){ // Screen LHS
                Platforms[i].x = LCD_X - 1;
            }
            else if(Platforms[i].x > LCD_X - 1){ // Screen RHS
                Platforms[i].x = 0 - 10;
            }
        //}
    }
}
#endif 