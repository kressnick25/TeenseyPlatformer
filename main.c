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
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <math.h>

//TODO remove unused libraries
//TODO convert int to uint8_t where appropriate.

#define MY_LCD_CONTRAST 0x44

// gs multiplier, alter to change gamespeed.
#define GS 1.1;
#define MAX_PSPEED 10
#define sizeOfPlatforms 28

//PWM
#define BIT(x) (1 << (x))
#define OVERFLOW_TOP (1023)
#define ADC_MAX (1023)

// Game states
// TODO move these to single byte.
bool gameOver = false;
bool gamePause = false;
bool gameExit = false;
//bool gravity;
bool playerCollision = true;
bool treasureMove = true;
bool playerMovingRight = false;
bool playerMovingLeft = true;
bool playerJumping = false;
//TODO put these in struct
float pot1Value; 
float platformMultiplyer = 1;
uint8_t old_block;
uint8_t Score = 0;
uint8_t LivesRemaining;
uint16_t secondsPast; 

//DEBUGGING
bool starting_platform = false;

#define playerSpawnX 20

Sprite treasure;
Sprite player;
Sprite startingBlock;
Sprite Platforms[sizeOfPlatforms]; //TODO will storing in progmem affect performance
Sprite Zombies[5]; // for all zombies if zombie.dy = 0; stop flashing

//debounce //TODO put in array
volatile uint8_t pause_counter = 0b00000000;
volatile uint8_t leftB_counter = 0b00000000;
volatile uint8_t rightB_counter = 0b00000000;
volatile uint8_t upB_counter = 0b00000000;
volatile uint8_t downB_counter = 0b00000000;
volatile uint8_t switchL_counter = 0b00000000;
volatile uint8_t switchR_counter = 0b00000000;
// store as single uint8_t
volatile uint8_t pause_pressed;
volatile uint8_t leftB_pressed;
volatile uint8_t rightB_pressed;
volatile uint8_t upB_pressed;
volatile uint8_t downB_pressed;
volatile uint8_t switchL_pressed;
volatile uint8_t switchR_pressed;

//static uint8_t prevState = 0;

// seconds timer
volatile uint32_t time_overflow_counter = 0;
volatile uint32_t led_overflow_counter = 0;

ISR(TIMER0_OVF_vect){
    uint8_t mask = 0b00000011;

    //pause button
    pause_counter = ((pause_counter << 1) & mask) | BIT_IS_SET(PINB, 0);
    if (pause_counter == mask){ pause_pressed = 1; } 
    else if (pause_counter == 0){ pause_pressed = 0; }
    //left joystick
    leftB_counter = ((leftB_counter << 1) & mask) | BIT_IS_SET(PINB, 1);
    if (leftB_counter == mask){ leftB_pressed = 1; } 
    else if (leftB_counter == 0){ leftB_pressed = 0; }
    //right joystick
    rightB_counter = ((rightB_counter << 1) & mask) | BIT_IS_SET(PIND, 0);
    if (rightB_counter == mask){ rightB_pressed = 1; } 
    else if (rightB_counter == 0){ rightB_pressed = 0; }
    //up joystick
    upB_counter = ((upB_counter << 1) & mask) | BIT_IS_SET(PIND, 1);
    if (upB_counter == mask){ upB_pressed = 1; } 
    else if (upB_counter == 0){ upB_pressed = 0; }
    //down joystick
    downB_counter = ((downB_counter << 1) & mask) | BIT_IS_SET(PINB, 0);
    if (downB_counter == mask) { downB_pressed = 1; } 
    else if (downB_counter == 0){ downB_pressed = 0; }
    //left switch
    switchL_counter = ((switchL_counter << 1) & mask) | BIT_IS_SET(PINF, 6);
    if (switchL_counter == mask){ switchL_pressed = 1; } 
    else if (switchL_counter == 0){ switchL_pressed = 0; }
    //right switch
    switchR_counter = ((switchR_counter << 1) & mask) | BIT_IS_SET(PINF, 5);
    if (switchR_counter == mask){ switchR_pressed = 1; } 
    else if (switchR_counter == 0){ switchR_pressed = 0; }

}

ISR(TIMER1_OVF_vect) {
	time_overflow_counter++;
}


// FLASH MEMORY STORAGE;
const PROGMEM uint8_t player_image[4] = {
    0b00100000,
    0b11111000,
    0b01010000,
    0b01110000
};

const PROGMEM uint8_t chest_image[3] = {
    0b01010000,
    0b10101000,
    0b01110000,
};

const PROGMEM uint8_t block_image[4] = {
    0b11111111, 0b11000000,
    0b11111111, 0b11000000  
};

uint8_t bad_image[4] = { // Shouldn't be stored in progmem, used too often.
    0b10101010, 0b10100000,
    0b10101010, 0b10100000
};

// Returns seconds past - code used from AMS Topic 9 ex 2
double get_current_time (){
    double time = ( time_overflow_counter * 65536.0 + TCNT1) * 1 / 125000;
    return time;
}
void get_led_flash (){
    int time = (( time_overflow_counter * 65536.0 + TCNT1) * 1 / 125000)*8;
    if(time%2==0){
        //&& zombies.dx != 0); TODO
        SET_BIT(PORTB, 2);
        SET_BIT(PORTB, 3);
    }
    else{
        CLEAR_BIT(PORTB, 2);
        CLEAR_BIT(PORTB, 3);
    }
}
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
        type = load_rom_bitmap(block_image, 4);
    }
    else if (i == 3){
        type = bad_image; //load_rom_bitmap(bad_image, 4);
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
        speed = -speed; // alternate direction
        deltaY += 10;
    }
    if (starting_platform){
        sprite_init(&Platforms[c], 0, 4, 10, 1, load_rom_bitmap(block_image, 2));
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
    int k = 0;
    if (starting_platform){
        k = sizeOfPlatforms - 1;
    }
    else{
        k = sizeOfPlatforms;
    }
    for (int i = 0; i < k; i ++){
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
    else if (player.dy > 0 && player.dy < 0.9)
    {
        player.dy += 0.2;
        if (player.dy > 1.0){
            player.dy = 1.0;
        }
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
    
    if (!playerCollision && playerMovingLeft && !playerJumping){
        player.dx = -0.5;
    }
    else if (!playerCollision && playerMovingRight && !playerJumping){
        player.dx = 0.5;
    }
}

void backlight_pwm(int duty_cycle){
    // Set the TOP value for the timer overflow comparator to 1023, 
	//		yielding a cycle of 1024 ticks per overflow.
	TC4H = OVERFLOW_TOP >> 8;
	OCR4C = OVERFLOW_TOP & 0xff;
	// Use OC4A for PWM. Remember to set DDR for C7.
	TCCR4A = BIT(COM4A1) | BIT(PWM4A);
	SET_BIT(DDRC, 7);
	// Set pre-scale to "no pre-scale" 
	TCCR4B = BIT(CS42) | BIT(CS41) | BIT(CS40);
    // Select Fast PWM
	TCCR4D = 0;
    //SET DUTY CYCLE
    // Set bits 8 and 9 of Output Compare Register 4A.
	TC4H = duty_cycle >> 8;

	// Set bits 0..7 of Output Compare Register 4A.
	OCR4A = duty_cycle & 0xff;
}

// cycle contrast and led to 0 with PWM
void screen_fade_down( uint16_t initBacklightValue, uint8_t contrast ){
    float c = 0.75;
    float b = 0.9;
    for (uint8_t i = 0; i < 4; i++){
        if (i == 3){ b = 0, c = 0; }
        backlight_pwm(initBacklightValue * c);
        lcd_init(contrast * b);
        _delay_ms(300);
        c -= 0.25;
        b -= 0.1;
    }
}

// cycle contrast and led up to MAX with PWM
void screen_fade_up( uint16_t initBacklightValue, uint8_t contrast){
    float c = 0.25;
    float b = 0.7;
    for (uint8_t i = 0; i < 4; i++){
        backlight_pwm(initBacklightValue * c);
        lcd_init(contrast * b);
        _delay_ms(300);
        c += 0.25;
        b += 0.1;
    }
}


//TODO get help
void animate_death ( ){
    for (int i = 0; i < 7; i++){
        lcd_position(0, i);
        for (int j = 0; j < 84; j++){
            lcd_write(LCD_D, 0b01010101);
        }
        _delay_ms(500); 
    }
}

// Called when player collides with forbidden block or moves out of bounds
// Pauses the game momentarily, resets player to safe block in starting row
void die ( void )
{   
    //timer_pause(1000); //TODO
    //_delay_ms(1000);
    LivesRemaining--;
    if (LivesRemaining != 0){
        screen_fade_down( ADC_MAX, MY_LCD_CONTRAST );
        player.is_visible = 0;
        //create_platforms(); //TODO
        //reset player variables
        player.dx = 0;
        player.dy = 0;
        //choose safe platform to move to
        //sprite_id safe_block = choose_safe_block();
        //sprite_move_to(player, safe_block->x, safe_block->y - 3);
        player.x = playerSpawnX;
        player.y = 0;
        player.is_visible = 1;
        show_screen();
        screen_fade_up( ADC_MAX, MY_LCD_CONTRAST );
    }
    
    if (LivesRemaining == 0)
    {   
        animate_death();
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
    uint8_t y = round(s1->y + 3); // add line 1 ie 5 pixel line
    //int xcor[5] = {0, 4, 1, 2, 3};
    //int ycor[5] = {1, 1, 4, 4, 4};
    //for (uint8_t y = s1->y; y < s1->y + s1->height; y++){
        for (uint8_t x = s1->x; x < s1->x + s1->width; x++){
    //for (int y = 0; y < 5; y++){
        //for (int x = 0; x < 5; x++){        
            // Get relevant values of from each sprite
            uint8_t sx1 = x - round(s1->x);
            uint8_t sy1 = y - ceil(s1->y);
            uint8_t offset1 = sy1 * s1->width + sx1;

            uint8_t sx2 = x - round(s2->x);
            uint8_t sy2 = y - round(s2->y-1);
            uint8_t offset2 = sy2 * s2->width + sx2;
            
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
    //}
    return false;
}

void update_player_speed( uint8_t platformNumber ){
    uint8_t i = platformNumber;
    float moveSpeed = 0.3;
    float struggleSpeed = 0.6;
    if (playerMovingLeft && Platforms[i].dx < 0){
        player.dx = -moveSpeed + Platforms[i].dx;
    }
    else if (playerMovingRight && Platforms[i].dx > 0){
        player.dx = moveSpeed + Platforms[i].dx;
    }
    else if (playerMovingLeft && Platforms[i].dx > 0){
        player.dx = -struggleSpeed + Platforms[i].dx;
    }
    else if (playerMovingRight && Platforms[i].dx < 0){
        player.dx = struggleSpeed + Platforms[i].dx;
    }
    else if (!playerMovingLeft && !playerMovingRight){
        player.dx = Platforms[i].dx;
    }
}

// Checks for collision between the player and each block 'Platforms' array
bool platforms_collide( void ) 
{
    bool output = false;
    uint8_t c = 0;
    // TODO check closest platform to player only.
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
                    update_player_speed(i);
                    playerJumping = false;
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
void auto_move_treasure()
{
    // Uses code from ZDJ Topic 04
    // Toggle chest movement when 't' pause_pressed.
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

//from zdj
bool sprites_collide(Sprite s1, Sprite s2)
{
    int top1 = round(s1.y);
    int bottom1 = top1 + s1.height - 1;
    int left1 = round(s1.x);
    int right1 = left1 + s1.width - 1;

    int top2 = round(s2.y);
    int bottom2 = top2 + s2.height - 1;
    int left2 = round(s2.x);
    int right2 = left2 + s2.width - 1;

    if(top1 > bottom2)
    {
        return false;
    }
    else if(top2 > bottom1)
    {
        return false;
    }
    else if(right1 < left2)
    {
        return false;
    }
    else if(right2 < left1)
    {
        return false;
    }
    else{
        return true;
    }

    //Alternatively
    // return !( (top > bottom2) || (top2 > bottom1) || (right1 < left2) || (right2 < left1));
}

// Checks for player collision with chest, hides chest upon collision
void chest_collide( void )
{ 
    bool collide = sprites_collide(player, treasure);
    if (collide){
        treasure.x = LCD_X + 6;
        die();
    }
}

void move_player(){
    // move right
    if ( rightB_pressed && player.x < LCD_X - 5 && playerCollision)
    {   
        if (playerMovingLeft){
            playerMovingLeft = false;
        }
        else{
            playerMovingRight = true;
        }
    }
    //move left
    else if (leftB_pressed && player.x > 0 && playerCollision)
    {
        if (playerMovingRight){
            playerMovingRight = false;
        }
        else{
            playerMovingLeft = true;
        }
    }
    // jump
    else if (upB_pressed && playerCollision)
    {   
        playerJumping = true;
        player.y -= 5;
        // Horizontal movement when jumping
        if (playerMovingLeft){
            player.dx = -1.7;
        }
        else if (playerMovingRight){
            player.dx = 1.7;
        }
    }
    //right switch treasure
    else if (switchR_pressed){ // TODO move this to seperate function (?)
        treasureMove = !treasureMove;
    }
    /**
    else if ( pause_pressed != prevState ) {
		prevState = pause_pressed;
		gamePause = !gamePause;
	}**/
    else if (pause_pressed){
        _delay_ms(200);
        gamePause = true;
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
    // LED's
    SET_BIT(DDRB, 2);
    SET_BIT(DDRB, 3);
    // Backlight
    SET_BIT(DDRC, 7);
    SET_BIT(PORTC, 7);


}

void setup_timers(){
   // setup debounce timers
    TCCR0A = 0;
    TCCR0B = 4;
    TIMSK0 = 1; //Enable timer overflow interrupt for Timer 0.
    // Timer for get current time
    TCCR1A = 0;
    TCCR1B = 3;
    TIMSK1 = 1;//	(b) Enable timer overflow for Timer 1.

    sei(); //Turn on interrupts. 
}

void setup_start(){
    srand(100); //TODO proper timer seed.
    set_clock_speed(CPU_8MHz);
    lcd_init(MY_LCD_CONTRAST);
    init_buttons();
    adc_init(); //init pot1
    // TODO change this to ascii per asci_font.h
    draw_string(8, 16, "Nicholas Kress", FG_COLOUR);
    draw_string(22, 24, "n9467688", FG_COLOUR);
    show_screen();
    setup_timers();
}

void setup_game(){
    LivesRemaining = 10;
    Score = 0;
    time_overflow_counter = 0;
    //TODO time = 0
    memset(Platforms, 0, sizeOfPlatforms*sizeof(Platforms[0]));
    create_platforms();
    draw_platforms();
    sprite_init( &player, playerSpawnX, 0, 5, 4, load_rom_bitmap(player_image, 4));
    sprite_draw( &player);
    sprite_init( &treasure, LCD_X / 2, LCD_Y - 5, 5, 3, load_rom_bitmap(chest_image, 3));
    
    treasure.dx = 0.2 * GS;
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
    uint8_t minutes = (secondsPast /60) % 60;
    uint8_t seconds = secondsPast % 60;
    char lives[15];
    char score[11];
    char counter[10]; 

    sprintf(lives, "%d lives left", LivesRemaining);
    sprintf(score, "Score: %d", Score);
    sprintf(counter, "%02u:%02u", minutes, seconds);
    uint8_t sy = 12;
    draw_string(10,sy, lives, FG_COLOUR);
    draw_string(20,sy+8, score, FG_COLOUR);
    draw_string(28,sy+16, counter, FG_COLOUR);
    show_screen();
    if (pause_pressed){
        _delay_ms(200);
        gamePause = false;
    }
}

void gameOverScreen()
{
    clear_screen();
    uint8_t minutes = (secondsPast /60) % 60;
    uint8_t seconds = secondsPast % 60;
    char score[11];
    char counter[10]; 

    sprintf(score, "Score: %d", Score);
    sprintf(counter, "%02u:%02u", minutes, seconds);
    
    uint8_t sy = 12;
    draw_string(18,sy, "Game Over", FG_COLOUR);
    draw_string(20,sy+8, score, FG_COLOUR);
    draw_string(28,sy+16, counter, FG_COLOUR);
    show_screen();
    if (switchR_pressed){
        setup_game();
        gamePause = false;
        gameOver = false;
        
    }
    if (switchL_pressed){
        gameExit = true;
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

    setup_game(); // move inside game loop

    while(!gameExit)
    {   
        
        while(!gameOver)
        {   
            while(!gamePause)
            {
                clear_screen();
                //long left_adc = adc_read(0);
                //backlight_pwm(ADC_MAX - left_adc);
                check_out_of_bounds();
                platforms_collide();
                check_pot();
                gravity();
                move_player();
                if (treasureMove) sprite_step(&treasure);
                sprite_step(&player);
                chest_collide(); //TODO not working
                auto_move_platforms();
                auto_move_treasure();
                draw_all();
                //DEBUG
                secondsPast = get_current_time();
                get_led_flash();
                show_screen();
                _delay_ms(10);
            }
            game_pause_screen();  
        }
        gameOverScreen();
    }
    clear_screen();
    draw_string(20, 16, "n9467688", FG_COLOUR);
    show_screen();
    return 0;
}

// DEBUG LINE
// draw_string(22, 24, "DEBUG", FG_COLOUR);