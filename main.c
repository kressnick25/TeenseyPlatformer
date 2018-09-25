#include "includes.h"


Sprite player;


void init_buttons(){
    CLEAR_BIT( DDRF, 6 );
}

void setup_start(){
    set_clock_speed(CPU_8MHz);
    lcd_init(LCD_DEFAULT_CONTRAST);
    init_buttons();

    draw_string(8, 16, "Nicholas Kress", FG_COLOUR);
    draw_string(22, 24, "n9467688", FG_COLOUR);
    show_screen();
}



void setup_game(){
    create_platforms();
    draw_platforms();
    sprite_init( &player, 0, 0, 5, 4, player_image);
    sprite_draw( &player);
    sprite_init( &treasure, 0, LCD_Y - 5, 5, 3, chest_image);
    treasure.dx = 0.1 * GS;
    sprite_draw( &treasure);
}

void draw_all(){
    sprite_draw(&player);
    sprite_draw(&treasure);
    draw_platforms();
}

int main ( void ){
    setup_start();
    // Wait for player to press button
    while(1){
        if (BIT_VALUE(PINF, 6)){
            clear_screen();
            show_screen();
            break;
        }
        _delay_ms(100);
    }


    setup_game();

    bool game_exit = false;
    while(!game_exit){
        bool game_exit = false;
        while(!game_exit){
                clear_screen();
                sprite_step(&treasure);
                auto_move_platforms();
                //move_treasure();
                draw_all();
                show_screen();
        }
    }


    return 0;
}

// DEBUG LINE
// draw_string(22, 24, "DEBUG", FG_COLOUR);