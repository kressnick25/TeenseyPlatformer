#include <avr/io.h>
#include <util/delay.h>
#include <cpu_speed.h>
#include <macros.h>
#include <sprite.h>
#include <ascii_font.h>
#include <ram_utils.h>
#include <graphics.h>
#include <lcd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <lcd_model.h>
#include <string.h>
#include <stdio.h>

//TODO remove unused libraries
//TODO convert int to uint8_t where appropriate.

// gs multiplier, alter to change gamespeed.
#define GS 1.1;
#define MAX_PSPEED 10
#define sizeOfPlatforms 28

// Game states
// TODO move these to single byte.
bool gameOver = false;
bool gamePause = false;
bool gameExit = false;
//bool gravity;
bool playerCollision = true;
bool treasureMove = true;
//TODO put these in struct
float pot1Value; 
float platformMultiplyer = 1;
uint8_t old_block;
uint8_t Score = 0;
uint8_t LivesRemaining = 10;
uint16_t startTime = 1000; 

Sprite treasure;
Sprite player;
Sprite Platforms[sizeOfPlatforms];

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
    0b01110000
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
    0b10101010, 0b10100000
};

//Sprite Control
void sprite_step( sprite_id sprite){
    sprite->x += sprite->dx;
    sprite->y += sprite->dy;
}

/*
**	Initialize and enable ADC with pre-scaler 128.
**
**	Assuming CPU speed is 8MHz, sets the ADC clock to a frequency of
**	8000000/128 = 62500Hz. Normal conversion takes 13 ADC clock cycles,
**	or 0.000208 seconds. The first conversion will be slower, due to
**	need to initialise ADC circuit.
*/
void adc_init() {
	// ADC Enable and pre-scaler of 128: ref table 24-5 in datasheet
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

/*
**	Do single conversion to read value of designated ADC pin combination.
**
**	Input:
**	channel - A 6-bit value which specifies the ADC input channel and gain
**			  selection. Refer table 24-4 in datasheet.
**
**	On TeensyPewPew, channel should be 0, 1, or 4.
**	0 = Pot0
**	1 = Pot1
**	4 = Broken-out Pin F4.
*/
uint16_t adc_read(uint8_t channel) {
	// Select AVcc voltage reference and pin combination.
	// Low 5 bits of channel spec go in ADMUX(MUX4:0)
	// 5th bit of channel spec goes in ADCSRB(MUX5).
	ADMUX = (channel & ((1 << 5) - 1)) | (1 << REFS0);
	ADCSRB = (channel & (1 << 5));

	// Start single conversion by setting ADSC bit in ADCSRA
	ADCSRA |= (1 << ADSC);

	// Wait for ADSC bit to clear, signalling conversion complete.
	while ( ADCSRA & (1 << ADSC) ) {}

	// Result now available.
	return ADC;
}

// Returns a random double floating point number between two values
double rand_number(double min, double max)
{ 
    double range = (max - min);
    double div = RAND_MAX / range;
    return min + (rand() / div);
}

uint8_t* choose_platform_type ( void )
{
    int i = rand_number(0,8);
    uint8_t* type;
    if (i <= 2){
        type = block_image;
    }
    else if (i == 3){
        type = bad_image;
    }
    else{
        type = NULL;
    }
    return type;
}

void create_platforms( void ) {
    //TODO enable this on death to change platforms
    //memset(Platforms, 0, sizeof Platforms); 
    int initX = 0, initY = 7;
    int deltaY = 0;
    int c = 0;
    platformMultiplyer = adc_read(1) / 80;
    if (platformMultiplyer > MAX_PSPEED) platformMultiplyer = MAX_PSPEED;
    float speed = 0.05 * platformMultiplyer;
    
    for (int i = 0; i < 4; i++)
    {
        int deltaX = 0;
        speed = -speed; // alternate direction
        for (int j = 0; j < 7; j++)
        {
            uint8_t* bitmap = choose_platform_type();
            if (bitmap != NULL) // bitmap != Null
            {
                sprite_init(&Platforms[c], initX + deltaX, initY + deltaY, 
                                                10, 2, bitmap);
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
            sprite_draw(&Platforms[j]);
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
                Platforms[i].x = 0 - 8;
            }
        //}
    }
}

void change_platform_speed( float speed ) //TODO fix platforms reversing
{
    int c = 0;
    for (int i = 0; i < 4; i++){
        for (int j = 0; j < 7; j++){
            Platforms[c].dx = 0.05 * speed * GS;
            c++;
        }
        speed = -speed;
    }
}

void check_pot(){
    float current_pot = adc_read(1) / 80;

    if (platformMultiplyer != current_pot){
        if (current_pot > MAX_PSPEED) current_pot = MAX_PSPEED;
        change_platform_speed(current_pot);
    }
    platformMultiplyer = current_pot;
}

void gravity( void )
{
    // When on block, kill velocity
    if (playerCollision)
    {
       player.dy = 0;
   }
    // When jumping, half velocity each step
    else if(player.dy <= -0.01)
    {
        player.dy *= 0.3;
    }
    // When falling (negative velocity) double velocity
    else if (player.dy > 0 && player.dy < 0.8)
    {
        player.dy *= 1.5;
    }
    // When jump peak reached, flip velocity to negative
    else if(player.dy > -0.2 && player.dy < 0)
    {
         player.dy = -player.dy;
    }
    // Else give player falling velocity
    else if(!playerCollision)// && player.dy == 0.0)
    {
        player.dy = 0.1;
    }
    /**
    // Apply accumulated 'momentum' to player speed when off block.
    if(!playerCollision){
        player->dx = momentum;
    }**/
}

// Called when player collides with forbidden block or moves out of bounds
// Pauses the game momentarily, resets player to safe block in starting row
void die ( void )
{   
    //timer_pause(1000); //TODO
    _delay_ms(1000);
    player.is_visible = 0;
    //create_platforms(); //TODO
    //reset player variables
    player.dx = 0;
    player.dy = 0;
    //choose safe platform to move to
    //sprite_id safe_block = choose_safe_block();
    //sprite_move_to(player, safe_block->x, safe_block->y - 3);
    player.x = 20;
    player.y = 0;
    player.is_visible = 1;
    LivesRemaining--;
    if (LivesRemaining == 0)
    {
        gamePause = true;
    }
}

// Increases score only when player moves to or lands on a new safe block
void increase_score(int new_block)
{
    if(old_block != new_block)
    {
        Score++;
    }
    old_block = new_block;
}

// Checks collision between two sprites on a pixel level
bool pixel_level_collision( Sprite *s1, Sprite *s2 )
{       // Uses code from AMS wk5.
        // Only check bottom of player model, stops player getting stuck in blocks.
    int y = round(s1->y + 4);
    //int xcor[5] = {0, 4, 1, 2, 3};
    //int ycor[5] = {1, 1, 4, 4, 4};
    //for (int y = s1->y; y < s1->y + s1->height; y++){
        for (int x = s1->x; x < s1->x + s1->width; x++){
    //for (int y = 0; y < 5; y++){
        //for (int x = 0; x < 5; x++){        
            // Get relevant values of from each sprite
            int sx1 = x - round(s1->x);
            int sy1 = y - ceil(s1->y);
            int offset1 = sy1 * s1->width + sx1;

            int sx2 = x - round(s2->x);
            int sy2 = y - round(s2->y);
            int offset2 = sy2 * s2->width + sx2;
            
            // If opaque at both points, collisio has occured
            if (0 <= sx1 && sx1 < s1->width &&
                0 <= sy2 && sy1 < s1->height &&
                s1->bitmap[offset1] != '0')
                {
                if (0 <= sx2 && sx2 < s2->width &&
                    0 <= sy2 && sy2 < s2->height && 
                    s2->bitmap[offset2] != '0')
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
        increase_score(c);
    }
    playerCollision = output;
    return output;
}

// Cause the player to die if moved out of bounds to left, right or bottom of screen
void check_out_of_bounds( void )
{
    if (player.y >= LCD_Y + 6 || 
        player.x + player.width < 0 || 
        player.x > LCD_X)
        {
        die();
    }     
}

// Moves chest along bottom of screen
void move_treasure()
{
    // Uses code from ZDJ Topic 04
    // Toggle chest movement when 't' pressed.
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

// Checks for player collision with chest, hides chest upon collision
void chest_collide( void )
{ 
    // only do if player close proximity to chest, saves cpu time when not needed.
    if (player.y > LCD_X - 10){ //TODO narrow this down.
        bool collide = pixel_level_collision(&player, &treasure);
        if (collide)
        {
            LivesRemaining += 3;
            //hide_chest_timer = create_timer(2000);
            //sprite_hide(chest);
            //TODO destory treasure sprite
            //move treasure off screen, cant collide

            die();
        }
    }
}

void move_player(){
    if ( BIT_VALUE(PIND, 0) && player.x < LCD_X - 5 && playerCollision)
    {
        if (player.dx < 0.7)
        {
            player.dx += 0.5;
        }        
    }
    else if (BIT_VALUE(PINB, 1) && player.x > 0 && playerCollision)
    {
        if (player.dx > -0.7)
        {
            player.dx -= 0.5;
        }
    }
    else if (BIT_VALUE(PIND, 1) && playerCollision)
    {
        player.y -= 3;
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
    else if (BIT_VALUE(PINB, 0)){ // TODO move to seperate function
        gamePause = true; //TODO debounce this
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
    
    // Backlight
    SET_BIT(DDRC, 7);
    SET_BIT(PORTC, 7);

}

void setup_start(){
    srand(100); //TODO proper timer seed.
    set_clock_speed(CPU_8MHz);
    lcd_init(0x44);
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
    sprite_init( &player, 20, 0, 5, 4, player_image);
    sprite_draw( &player);
    sprite_init( &treasure, LCD_X / 2, LCD_Y - 5, 5, 3, chest_image);
    treasure.dx = 0.1 * GS;
    sprite_draw( &treasure);
}

void draw_all(){
    sprite_draw(&player);
    sprite_draw(&treasure);
    draw_platforms();
    draw_line( 0, 0, 0, LCD_Y, FG_COLOUR);
    draw_line( LCD_X-1, 0, LCD_X-1, LCD_Y, FG_COLOUR );
}

void game_pause_screen()
{
    if (LivesRemaining == 0){
        gameOver = true;
    }
    clear_screen();
    uint16_t secondsPast = startTime; // TODO minus current time
    uint8_t minutes = (secondsPast /60) % 60;
    uint8_t seconds = secondsPast % 60;
    char lives[15];
    char score[11];
    char counter[10]; 

    sprintf(lives, "%d lives left", LivesRemaining);
    sprintf(score, "Score: %d", Score);
    sprintf(counter, "%02u:%02u", minutes, seconds);
    
    draw_string(10,16, lives, FG_COLOUR);
    draw_string(20,24, score, FG_COLOUR);
    draw_string(28,32, counter, FG_COLOUR);
    show_screen();
    if (BIT_VALUE(PINB, 0)){ //TODO debounce this
        gamePause = false;
    }
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

    while(!gameExit)
    {
        while(!gameOver)
        {   
            while(!gamePause)
            {
                clear_screen();
                check_out_of_bounds();
                platforms_collide();
                check_pot();
                gravity();
                move_player();
                if (treasureMove) sprite_step(&treasure);
                sprite_step(&player);
                auto_move_platforms();
                move_treasure();
                draw_all();
                show_screen();
                _delay_ms(0);
            }
            game_pause_screen();  
        }
        // gameOverScreen();
        clear_screen();
        show_screen();
    }
    return 0;
}

// DEBUG LINE
// draw_string(22, 24, "DEBUG", FG_COLOUR);