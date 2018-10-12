#include "includes.h"


Sprite player;

// Game states
// TODO move these to single byte.
bool gameOver = false;
bool gamePause = false;
bool gravity;
bool playerCollision = true;
bool treasureMove = true;
//TODO put these in struct
float pot1Value; 
uint8_t score;
unint8_t livesRemaining;
uint16_t startTime; 


void move_player(){
    if ( BIT_VALUE(PIND, 0) && player.x < LCD_X - 5 && playerCollision)
    {
        if (player.dx < 0.5)
        {
            player.dx += 0.1;
        }        
    }
    else if (BIT_VALUE(PINB, 1) && player.x > 0 && playerCollision)
    {
        if (player.dx > -0.5)
        {
            player.dx -= 0.1;
        }
    }
    else if (BIT_VALUE(PIND, 1) && playerCollision)
    {
        player.y -= 1;
        // Jumping with horizontal velocity moves further than
        // falling with horizontal velocity
        if (player.dy != 0)
        { 
            player.dy *= 1.5;
        }
    }
    // move downwards, for testing only //TODO remove this
    else if (BIT_VALUE(PINB, 7) && playerCollision)
    {
        player.y += 1;
        // Jumping with horizontal velocity moves further than
        // falling with horizontal velocity
        if (player.dy != 0)
        { 
            player.dy *= 1.5;
        }
    }
    else if (BIT_VALUE(PINF, 5)){ // TODO move this to seperate function (?)
        // TODO debounce this
        treasureMove = !treasureMove;
    }
    else 
    {
        if (player.dx > 0.0){
            player.dx -= 0.1;
        }
        if (player.dx < 0.0){
            player.dx += 0.1;
        }
    }
}

void init_buttons(){
    CLEAR_BIT( DDRF, 5 ); // button 2 / right / sw3
    CLEAR_BIT( DDRF, 6 ); // button 1/ left / sw2
    CLEAR_BIT( DDRD, 1 ); // joystick up
    CLEAR_BIT( DDRB, 7 ); // joystick down
    CLEAR_BIT( DDRB, 1 ); // joystick left 
    CLEAR_BIT( DDRD, 0 ); // joystick right
    CLEAR_BIT( DDRB, 0 ); // joystick centre click
    // TODO LED's
}

void setup_start(){
    set_clock_speed(CPU_8MHz);
    lcd_init(LCD_DEFAULT_CONTRAST);
    init_buttons();
    adc_init(); //init pot1
    // TODO change this to ascii per asci_font.h
    draw_string(8, 16, "Nicholas Kress", FG_COLOUR);
    draw_string(22, 24, "n9467688", FG_COLOUR);
    show_screen();
}

void setup_game(){
    create_platforms();
    draw_platforms();
    sprite_init( &player, 0, 0, 5, 4, player_image);
    sprite_draw( &player);
    sprite_init( &treasure, LCD_X / 2, LCD_Y - 5, 5, 3, chest_image);
    treasure.dx = 0.1 * GS;
    sprite_draw( &treasure);
}

void draw_all(){
    sprite_draw(&player);
    sprite_draw(&treasure);
    draw_platforms();
}

void gamePauseScreen()
{
    clear_screen();

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
    while(!game_exit)
    {
        while(!gameOver)
        {   
            while(!gamePause)
            {
                clear_screen();
                check_pot();
                move_player();
                if (treasureMove) sprite_step(&treasure);
                sprite_step(&player);
                auto_move_platforms();
                move_treasure();
                draw_all();
                show_screen();
            }
            //gamePauseScreen();    
        }
        // gameOverScreen();
    }


    return 0;
}

// DEBUG LINE
// draw_string(22, 24, "DEBUG", FG_COLOUR);