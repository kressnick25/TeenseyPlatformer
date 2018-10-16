#ifndef COLLISION_H_   /* Include guard */
#define COLLISION_H_
#include "includes.h"

Sprite player; //TODO move this

// Called when player collides with forbidden block or moves out of bounds
// Pauses the game momentarily, resets player to safe block in starting row
void die ( void )
{   
    //timer_pause(1000); //TODO
    player.is_visible = 0;
    //create_platforms(); //TODO
    //reset player variables
    player.dx = 0;
    player.dy = 0;
    //choose safe platform to move to
    //sprite_id safe_block = choose_safe_block();
    //sprite_move_to(player, safe_block->x, safe_block->y - 3);
    player.x = 0;
    player.y = 0;
    player.is_visible = 1;
    /**livesRemaining--; //TODO
    if (livesRemaining == 0)
    {
        //gameOver = true;
    }**/
}

// Checks collision between two sprites on a pixel level
bool pixel_level_collision( Sprite *s1, Sprite *s2 )
{       // Uses code from AMS wk5.
        // Only check bottom of player model, stops player getting stuck in blocks.
    int y = round(s1->y + 2);
    for (int x = s1->x; x < s1->x + s1->width; x++){
        // Get relevant values of from each sprite
        int sx1 = x - round(s1->x);
        int sy1 = y - ceil(s1->y);
        int offset1 = sy1 * s1->width + sx1;

        int sx2 = x - round(s2->x);
        int sy2 = y - round(s2->y-1);
        int offset2 = sy2 * s2->width + sx2;
        
        // If opaque at both points, collisio has occured
        if (0 <= sx1 && sx1 < s1->width &&
            0 <= sy2 && sy1 < s1->height &&
            s1->bitmap[offset1] != ' ')
            {
            if (0 <= sx2 && sx2 < s2->width &&
                0 <= sy2 && sy2 < s2->height && 
                s2->bitmap[offset2] != ' ')
                {
                return true;
            }
        }
    }
    return false;
}

// Checks for collision between the player and each block 'Platforms' array
bool platforms_collide( void ) 
{
    bool output = false;
    uint8_t c = 0;
    for (int i = 0; i < sizeOfPlatforms; i++){
        //if(Platforms[i] != NULL)    // Do not check empty platforms //TODO
        //{ 
            bool collide = pixel_level_collision(&player, &Platforms[i]);
            if (collide)
            {
                if(Platforms[i].bitmap == bad_image){
                    die();
                    output = true;
                    break;
                }
                else
                {   // Die if any part of current block is off screen
                    if(Platforms[i].x > LCD_X - Platforms[i].width ||
                        Platforms[i].x < 0){
                        die();
                    }
                    // Update player speed so that player moves with platform on
                    player.dx = Platforms[i].dx;
                    output = true;
                    if (c == 0){
                        c = i;
                    }
                }
            }
        //} 
    }
    if (output == true){
        //increase_score(c); //TODO
    }
    return output;
}

// Cause the player to die if moved out of bounds to left, right or bottom of screen
void check_out_of_bounds( void )
{
    if (player.y >= LCD_Y + 6 || 
        player.x + player.width < 0 || 
        player.x > LCD_X)
        {
        //die();
    }     
}
#endif 