#include "includes.h"
#include "sprite_models.h"

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

sprite_id player;

void setup_game(){
    sprite_init( player, 0, 0, 7, 7, player_image);
    sprite_draw( &player);
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
                show_screen();
        }
    }


    return 0;
}

// DEBUG LINE
// draw_string(22, 24, "DEBUG", FG_COLOUR);