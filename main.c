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
#include <cab202_adc.h>
#include "usb_serial.h" // SERIAL


//TODO remove unused libraries
//TODO convert int to uint8_t where appropriate.
// TODO put any const in progmem

#define MY_LCD_CONTRAST 0x44

// gs multiplier, alter to change gamespeed.
#define GS 1.1;
#define MAX_PSPEED 10
#define sizeOfPlatforms 29

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
bool playerFalling = false;
bool playerCollision = true;
bool treasureMove = true;
bool playerMovingRight = false;
bool playerMovingLeft = false;
bool playerJumping = false;
bool Flash = true;
bool stopTime = false;
//TODO put these in struct
float pot1Value; 
float platformMultiplyer = 1;
uint8_t current_block;
uint8_t Score = 0;
uint8_t LivesRemaining;
uint16_t secondsPast; 
uint8_t food_in_inventory = 5;

//DEBUGGING
bool starting_platform = true;

#define playerSpawnX 4
#define ZOMBIE_SPEED 1.2

Sprite treasure;
Sprite player;
Sprite startingBlock;
Sprite Platforms[sizeOfPlatforms];
Sprite Zombies[5]; // for all zombies if zombie.dy = 0; stop flashing
Sprite Food[5];

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
volatile uint32_t seed_overflow_counter = 0;


ISR(TIMER0_OVF_vect){
    seed_overflow_counter++;
    uint8_t mask = 0b00000011;
    uint8_t downB_mask = 0b00001111;
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
    downB_counter = ((downB_counter << 1) & downB_mask) | BIT_IS_SET(PINB, 7);
    if (downB_counter == downB_mask) { downB_pressed = 1; } 
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
    if (!stopTime){
	    time_overflow_counter++;
    }
}



// FLASH MEMORY STORAGE; 
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

uint8_t food_image[3] = {
    0b01000000,
    0b11100000,
    0b01000000,
};

uint8_t zombie_image[3] = {
    0b11100000,
    0b01000000,
    0b10100000,
};

uint8_t bad_image[4] = { // Shouldn't be stored in progmem, used too often.
    0b10101010, 0b10100000,
    0b11111111, 0b11110000
};

int count_zombies( void ){
    int count = 0;
    for (int i = 0; i < 5; i++){
        if (Zombies[i].y > 0 && Zombies[i].y < LCD_Y ){
            count++;
        }
    }
    return count;
}

/* Code from topic 10 - USB serial examples
**	Transmits a string via usb_serial.
*/
void usb_serial_send(char * message) {
	// Cast to avoid "error: pointer targets in passing argument 1 
	//	of 'usb_serial_write' differ in signedness"
	usb_serial_write((uint8_t *) message, strlen(message));
}

//TODO change to array? 
//TODO format minutes/seconds
// store each seperately to save space.
const unsigned char serialText_GameStart[44] PROGMEM = "Game Start - player positon: %.0f , %.0f \r\n";
const unsigned char serialText_Death[57] PROGMEM = "Death - reason: %s, lives: %d, score: %d, time: %d:%d \r\n";
const unsigned char serialText_Respawn[40] PROGMEM = "Respawn - player position: %.0f, %.0f\r\n";
const unsigned char serialText_ZombiesAppear[72] PROGMEM = "Zombies Appeared - num_Zombies: %d, time: %d:%d, lives: %d, score: %d\r\n";
const unsigned char serialText_ZombieFood[79] PROGMEM = "Zombie Ate Food - num_Zombies_Remaining: %d, Food_Remaining: %d, time: %d:%d\r\n";
const unsigned char serialText_ChestCollide[86] PROGMEM = "Treasure Collected - Score: %d, Lives: %d, time: %d:%d, player_position: %.0f, %.0f\r\n";
const unsigned char serialText_Pause[81] PROGMEM = "Pause - Lives: %d, Score: %d, time: %d:%d, num_Zombies: %d, Food_Remaining: %d\r\n";
const unsigned char serialText_GameOver[81] PROGMEM = "Game Over - Lives: 0, Score: %d, time: %d:%d, Zombies_Fed: %d\r\n";

void serial_comms ( uint8_t event, char* death_type ){
    char output[86]; // allocate max for largest text //TODO check max
    uint8_t minutes = (secondsPast /60) % 60;
    uint8_t seconds = secondsPast % 60;
    //TODO format minutes/seconds
    if (event == 1){ // Game Start
        sprintf(output, (char*)load_rom_string(serialText_GameStart), player.x, player.y);
    }
    else if (event == 2){ // Player Death
        sprintf(output, (char*)load_rom_string(serialText_Death), 
                                                death_type, LivesRemaining, Score, minutes, seconds);
    }
    else if (event == 3){ //Player Respawn
        sprintf(output, (char*)load_rom_string(serialText_Respawn), player.x, player.y);
    }
    else if (event == 4){ // Zombies Appear
        //TODO zombies appear
        sprintf(output, (char*)load_rom_string(serialText_ZombiesAppear), 5, minutes, seconds, LivesRemaining, Score );
    }
    else if (event == 5){ //Zombie eats food
        // TODO num zombies
        sprintf(output, (char*)load_rom_string(serialText_ZombieFood), count_zombies(), food_in_inventory, minutes, seconds);
    }
    else if (event == 6){ //Chest Collide
        sprintf(output, (char*)load_rom_string(serialText_ChestCollide), Score, LivesRemaining, minutes, seconds, player.x, player.y);
    }
    else if (event == 7){ // Pause Button
        //TODO num_Zombies
        sprintf(output, (char*)load_rom_string(serialText_Pause), LivesRemaining, Score, minutes, seconds, 5, food_in_inventory);
    }
    else if (event == 8){ //Game Over
        //TODO zombies fed
        sprintf(output, (char*)load_rom_string(serialText_GameOver), Score, minutes, seconds, 0);
    }
    
    usb_serial_send(output);

}

//from zdj
bool sprites_collide(Sprite s1, Sprite s2, uint8_t offset)
{
    int top1 = round(s1.y);
    int bottom1 = top1 + s1.height - 1 + offset;
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

int get_current_platform(Sprite s)
{
    int sl = (int)round(s.x);
    int sr = sl + s.width - 1;
    int sy = (int)round(s.y);

    for(int i = 0; i < sizeOfPlatforms; i++)
    {
        sprite_id p = &Platforms[i];
        int pl = (int)round(p->x);
        int pr = pl + p->width - 1;
        int py = (int)round(p->y);

        if(sr >= pl && sl <= pr && sy == py - s.height)
        {
            return i;
        }
    } 

    return -1;
}

//FOOD
void init_food ( void ){
    for (uint8_t i = 0; i < 5; i++){
        // Initially store food off screen and in_visible
        sprite_init(&Food[i], 0, LCD_Y + 5, 3, 3, food_image);
        Food[i].is_visible = false; 
    }
}

void drop_food ( void ){
    if (food_in_inventory > 0){
        Food[food_in_inventory -1].x = player.x;
        Food[food_in_inventory -1].y = player.y + 1;
        Food[food_in_inventory -1].dx = Platforms[current_block].dx;
        Food[food_in_inventory -1].is_visible = true;
        food_in_inventory--;
    }
}

void draw_food (){
    for (uint8_t i = 0; i < 5; i++){
        Food[i].x += Food[i].dx;
        if (Food[i].x + 10 < 0){ // Screen LHS
                Food[i].x = LCD_X - 1;
            }
        else if(Food[i].x > LCD_X - 1){ // Screen RHS
            Food[i].x = 0 - 8;
        }
        sprite_draw(&Food[i]);
    }
}

void update_food_speed(){
    for (uint8_t i = 0; i < 5; i++){ // Gravity
        for (uint8_t j = 0; j < sizeOfPlatforms; j++){
            if (sprites_collide(Food[i], Platforms[j], 1)){
                Food[i].dx = Platforms[j].dx;
            }
        }
    }    
};

//ZOMBIES
void init_zombies(){
    for (uint8_t i = 0; i < 5; i++){\
        //Store zombies off screen at start
        sprite_init(&Zombies[i], LCD_X - 5 - ((LCD_X / 5) * i), -10, 3, 3, zombie_image);
    }
}

void draw_zombies(){
    for (uint8_t i = 0; i < 5; i++){
        Zombies[i].x += Zombies[i].dx;
        Zombies[i].y += Zombies[i].dy;
        sprite_draw(&Zombies[i]);
    }
}

bool sent_serial = false;

void drop_zombies(){
    if (secondsPast == 3){
        for (uint8_t i = 0; i < 5; i++){
            Zombies[i].dy = 0.25;
            Zombies[i].y = 0;
        }
        if (!sent_serial){
            serial_comms(4, NULL);
            sent_serial = true;
        }
    }
}

//TODO fix score

//13 e) when zombie collides with food
void zombie_eat(){
    for (int i = 0; i < 5; i++){
        for (int j = food_in_inventory; j > 0 ;j--){
            if (sprites_collide(Zombies[i], Food[j], 0)){
                Zombies[i].y = -10;
                Score += 10;
                Food[j].y = LCD_Y + 5;
                food_in_inventory += 1;
                serial_comms(5, NULL);
            }
        }
    }
}

void zombie_movement(){
    // todo add if only on screen
    for (uint8_t i = 0; i < 5; i++){ // Gravity
        int plat = get_current_platform(Zombies[i]);
        // do not check once zombie is on block
        if (Zombies[i].dy != 0){
            if (plat != -1){
                Zombies[i].dy = 0;
                if (Platforms[plat].dx == 0){
                    Zombies[i].dx = 0.3;
                }
                else{
                    Zombies[i].dx = Platforms[plat].dx * ZOMBIE_SPEED;
                }
                /**
                else if (Platforms[plat].dx > 0){
                    Zombies[i].dx = Platforms[plat].dx + 0.2; 
                }
                else if (Platforms[plat].dx < 0){
                    Zombies[i].dx = Platforms[plat].dx - 0.2;
                }**/
            }
        }

        // loop to opposite side of screen
        if (Zombies[i].x + 10 < 0){ // Screen LHS
                Zombies[i].x = LCD_X - 1;
            }
        else if(Zombies[i].x > LCD_X - 1){ // Screen RHS
            Zombies[i].x = 0 - 8;
        }
        
        if (Zombies[i].dy == 0 && Zombies[i].x > 0 && Zombies[i].x < LCD_X - 1 && Zombies[i].y > 0){
            int offset = 0;
            double cdx = Zombies[i].dx;
            if (cdx < 0) { offset = -2; }
            else if ( cdx > 0){ offset = 2; }

            Zombies[i].x += offset; //for fall test
            int plat2 = get_current_platform(Zombies[i]);
            Zombies[i].x -= offset; //reset x
            if (plat2 == - 1){
                cdx = -cdx;
            }
            if (cdx != Zombies[i].dx ){
                Zombies[i].x -= Zombies[i].dx;
                Zombies[i].dx = cdx;
            }
        }

    }
}

void die ( char * death_type ); // so compiler can see die function

//TODO order functions properly/ in call order

void player_zombie_collide(){
    for (int i = 0; i < 5; i++){
        if (sprites_collide(Zombies[i], player, 0)){
            die("Player hit Zombie");
            //TODO check this long char string wont break serial serial_comms
            // TODO store string in progmem.
        }
    }
}

void process_zombies(){
    drop_zombies();
    zombie_movement();
    zombie_eat();
    //player_zombie_collide();//TODO debugging, re-enable.
}

// Returns seconds past - code used from AMS Topic 9 ex 2
double get_current_time (){
    double time = ( time_overflow_counter * 65536.0 + TCNT1) * 1 / 125000;
    return time;
}


void led_flash(){
    int time = (( time_overflow_counter * 65536.0 + TCNT1) * 1 / 125000)*8;
    uint8_t zombiesStopped = 0;
    for (uint8_t i = 0; i < 5; i++){
        if (Zombies[i].dy == 0){
            zombiesStopped++;
        }
    }
    if(time%2==0){
        SET_BIT(PORTB, 2);
        SET_BIT(PORTB, 3);
    }
    else{
        CLEAR_BIT(PORTB, 2);
        CLEAR_BIT(PORTB, 3);
    }
    
    if (secondsPast > 4 && zombiesStopped == 5){
        CLEAR_BIT(PORTB, 2);
        CLEAR_BIT(PORTB, 3);
        Flash = false;
    }
}

//Sprite Control
void sprite_step( sprite_id sprite){
    sprite->x += sprite->dx;
    sprite->y += sprite->dy;
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
        type = bad_image; //load_rom_bitmap(bad_image, 4);
    }
    else{
        type = NULL;
    }
    return type;
}

void create_platforms( void ) {
    int initX = 0, initY = 8;
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
        sprite_init(&Platforms[c], 0, 4, 10, 1, block_image);
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

void change_platform_speed( float speed )
{
    speed *= GS;
    int c = 0;
    for (int i = 0; i < 4; i++){
        for (int j = 0; j < 7; j++){
            Platforms[c].dx = 0.05 * speed;
            c++;
        }
        speed = -speed;
    }
    for (int j = 0; j < 5; j++){
        Zombies[j].dx = Platforms[get_current_platform(Zombies[j])].dx * ZOMBIE_SPEED;
    }
    update_food_speed();
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
    // TODO clean up/fix
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
        playerFalling = true;
        player.dy = 0.1;
    }
    
    //TODO what is this???
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
    float c = 0.8;
    float b = 1.0;
    for (uint8_t i = 0; i < 8; i++){
        if (i == 8){ b = 0, c = 0; }
        backlight_pwm(initBacklightValue * c);
        lcd_init(contrast * b);
        _delay_ms(100);
        c -= 0.1;
        b -= 0.05;
    }
}

// cycle contrast and led up to MAX with PWM
void screen_fade_up( uint16_t initBacklightValue, uint8_t contrast){
    float c = 0.0;
    float b = 0.6;
    for (uint8_t i = 0; i < 8; i++){
        if (i == 7) { c = 1, b = 1; }
        backlight_pwm(initBacklightValue * c);
        lcd_init(contrast * b);
        _delay_ms(100);
        c += 0.1;
        b += 0.05;
    }
}

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
void die ( char * death_type )
{   
    LivesRemaining--;
    if (LivesRemaining != 0){
        if (strcmp(death_type, "chest_collide") != 0){
            serial_comms(2, death_type);
        }
        else{
            serial_comms(6, NULL);
        }
        screen_fade_down( ADC_MAX, MY_LCD_CONTRAST );
        player.is_visible = 0;
        //reset player variables
        player.dx = 0;
        player.dy = 0;

        player.x = playerSpawnX;
        player.y = 0;
        player.is_visible = 1;
        init_food();
        food_in_inventory = 5;
        show_screen();
        screen_fade_up( ADC_MAX, MY_LCD_CONTRAST );
        serial_comms(3, NULL);
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
    if(current_block != new_block)
    {
        Score++;
    }
    current_block = new_block;
}

// Checks collision between two sprites on a pixel level
bool pixel_level_collision( Sprite *s1, Sprite *s2 )
{       // Uses code from AMS wk5.
        // Only check bottom of player model, stops player getting stuck in blocks.
    uint8_t y = round(s1->y + 3); // add line 1 ie 5 pixel line

    //for (uint8_t y = s1->y + 3; y < s1->y + s1->height + 3; y++){
        for (uint8_t x = s1->x; x < s1->x + s1->width; x++){       
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
        //}
    }
    return false;
}

void update_player_speed( uint8_t platformNumber ){
    uint8_t i = platformNumber;
    float moveSpeed = 0.3;
    float struggleSpeed = 0.6;
    if (playerFalling){
        playerMovingLeft = false;
        playerMovingRight = false;
        player.dx = Platforms[i].dx;
        playerFalling = false;
    }

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

    //condition if platforms stopped moving
    if (Platforms[i].dx == 0 && playerMovingLeft){
        player.dx = -moveSpeed;
    }
    else if (Platforms[i].dx == 0 && playerMovingRight){
        player.dx = moveSpeed;
    }
    else if (Platforms[i].dx == 0 && !playerMovingLeft && !playerMovingRight){
        player.dx = 0;
    }
}

// Checks for collision between the player and each block 'Platforms' array
void platforms_collide( void ) 
{
    bool output = false;
    uint8_t c = 0;
    int current_platform = get_current_platform(player);
    if (current_platform == -1){
        playerCollision = false;
    }
    else if(Platforms[current_platform].bitmap == bad_image){
        die("bad_platform");
        output = true;
    }
    else
    {   // Die if any part of current block is off screen
        if(Platforms[current_platform].x > LCD_X - Platforms[current_platform].width ||
            Platforms[current_platform].x < 0){
            die("block_off_screen");
        }
        // Update player speed so that player moves with platform on
        update_player_speed(current_platform);
        playerJumping = false;
        output = true;
        if (c == 0){
            c = current_platform;
        }
        if (output == true){
            increase_score(c);
            playerCollision = true;
        }
        else{
            playerCollision = false;
        }
    }
}

// Cause the player to die if moved out of bounds to left, right or bottom of screen
void check_out_of_bounds( void )
{
    if (player.y >= LCD_Y + 6 || 
        player.x < 0 || 
        player.x + player.width > LCD_X)
        {
        die("player_off_screen");
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

// Checks for player collision with chest, hides chest upon collision
void chest_collide( void )
{ 
    bool collide = sprites_collide(player, treasure, 0);
    if (collide){
        treasure.x = LCD_X + 6;
        LivesRemaining += 3;
        die("chest_collide");
    }
}

bool check_serial (int character){
    if ( usb_serial_available() ) {
		int c = usb_serial_getchar();
		if ( c == character ) return true;
	}
    return false;
}

uint8_t downB_prevState;
uint8_t rightB_prevState; 
uint8_t leftB_prevState;

void control_player(){
    // move right
    if ( (rightB_pressed != rightB_prevState && player.x < LCD_X - 5 && playerCollision))
    {   
        rightB_prevState = rightB_pressed;
        static uint16_t rightCount;
        rightCount++;
        if (rightCount % 2 == 0){
            if (playerMovingLeft){
                playerMovingLeft = false;
            }
            else{
                playerMovingRight = true;
            }
        }
    }
    //move left
    else if ((leftB_pressed != leftB_prevState && player.x > 0 && playerCollision))
    {
        leftB_prevState = leftB_pressed;
        static uint16_t leftCount;
        leftCount++;
        if (leftCount % 2 == 0){
            if (playerMovingRight){
                playerMovingRight = false;
            }
            else{
                playerMovingLeft = true;
            }
        }
    }
    // jump
    else if ((upB_pressed && playerCollision))
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
    
    else if ( downB_pressed != downB_prevState && playerCollision) {
		downB_prevState = downB_pressed;
        static uint16_t downCount;
        downCount++;
        if (downCount % 2 == 0){
		    drop_food();
        }
	}
    else if (pause_pressed){
        serial_comms(7, NULL);
        _delay_ms(200);
        gamePause = true;
    }

    if ( usb_serial_available() ) {
		int c = usb_serial_getchar();
		if ( c == 'd' && player.x < LCD_X - 5 && playerCollision){
            if (playerMovingLeft){
            playerMovingLeft = false;
            }
            else{
                playerMovingRight = true;
            }
        }
        else if (c == 'a' && player.x > 0 && playerCollision){
            if (playerMovingRight){
            playerMovingRight = false;
            }
            else{
                playerMovingLeft = true;
            }
        }
        else if ( c == 'w' && playerCollision){
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
        else if ( c == 't'){
            treasureMove = !treasureMove;
        }
        else if ( c == 's' && playerCollision){
            drop_food();
        }
        else if ( c == 'p'){
            serial_comms(7, NULL);
            _delay_ms(200);
            gamePause = true;
        }
	}
    
    // keeps Horizontal jumping in line.
    else 
    {
        if (player.dx > 0.0 && !playerCollision){
            player.dx -= 0.1;
        }
        if (player.dx < 0.0 && !playerCollision){
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

void setup_main(){
    set_clock_speed(CPU_8MHz);
    lcd_init(MY_LCD_CONTRAST);
    init_buttons();
    adc_init(); //init pot1
    draw_string(8, 16, "Nicholas Kress", FG_COLOUR);
    draw_string(22, 24, "n9467688", FG_COLOUR);
    usb_init(); // SERIAL
    show_screen();
    setup_timers();
}

void setup_game(){
    srand(time_overflow_counter); //TODO properly seed this
    LivesRemaining = 10; //TODO set to 10
    memset(Platforms, 0, sizeOfPlatforms*sizeof(Platforms[0]));
    create_platforms();
    draw_platforms();
    sprite_init( &player, playerSpawnX, 0, 5, 4, player_image);
    sprite_draw( &player);
    sprite_init( &treasure, LCD_X / 2, LCD_Y - 5, 5, 3, chest_image);
    init_food();
    init_zombies();
    treasure.dx = 0.2 * GS;
    sprite_draw( &treasure);
    serial_comms(1, NULL);
    playerMovingLeft = false;
    playerMovingRight = false;
    time_overflow_counter = 0;

}

// draw sprites in order of overlap - sprites draw last on top.
void draw_all(){
    draw_platforms();
    sprite_draw(&treasure);
    sprite_draw(&player); 
    draw_food();
    draw_zombies();
    draw_line( 0, 0, 0, LCD_Y, FG_COLOUR);
    draw_line( LCD_X-1, 0, LCD_X-1, LCD_Y, FG_COLOUR );
}

void game_pause_screen()
{
    if (LivesRemaining == 0){
        serial_comms(8, NULL);
        gameOver = true;
    }
    stopTime = true;
    clear_screen();
    uint8_t minutes = (secondsPast /60) % 60;
    uint8_t seconds = secondsPast % 60;
    char lives[15];
    char score[11];
    char counter[10]; 
    char food[10];
    char zombies[13];

    sprintf(lives, "%d lives left", LivesRemaining);
    sprintf(score, "Score: %d", Score);
    sprintf(food, "Food: %d", food_in_inventory);
    sprintf(counter, "%02u:%02u", minutes, seconds);
    sprintf(zombies, "Zombies: %d", count_zombies());

    uint8_t sy = 4;
    draw_string(10,sy, lives, FG_COLOUR);
    draw_string(20,sy+8, score, FG_COLOUR);
    draw_string(24,sy+16, food, FG_COLOUR);
    draw_string(16,sy+24, zombies, FG_COLOUR);
    draw_string(28,sy+32, counter, FG_COLOUR);
    show_screen();
    if (pause_pressed || check_serial('p')){
        _delay_ms(200);
        gamePause = false;
        stopTime = false;
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
    if (switchR_pressed || check_serial('r')){
        setup_game();
        Score = 0;
        gamePause = false;
        gameOver = false;
        
    }
    if (switchL_pressed || check_serial('q')){
        gameExit = true;
    }
}

int main ( void ){
    setup_main();
    // Wait for player to press button
    while(1){
        if (BIT_VALUE(PINF, 6)){
            clear_screen();
            show_screen();
            break;
        }
        if ( usb_serial_available() ) { //SERIAL
            int c = usb_serial_getchar();
            if ( c == 's'){
                clear_screen();
                show_screen();
                break;
            };
	}
        _delay_ms(100);
    }

    while(!gameExit)
    {   
        setup_game(); //TODO fix score here.
        while(!gameOver)
        {   
            
            while(!gamePause)
            {
                clear_screen();
                //long left_adc = adc_read(0);
                //backlight_pwm(ADC_MAX - left_adc);
                process_zombies();
                check_out_of_bounds();
                platforms_collide();
                check_pot();
                gravity();
                control_player();
                if (treasureMove) sprite_step(&treasure);
                sprite_step(&player);
                chest_collide();
                auto_move_platforms();
                auto_move_treasure();
                draw_all();
                //DEBUG
                secondsPast = get_current_time();
                if (Flash){ led_flash(); }
                show_screen();
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