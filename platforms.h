#ifndef PLATFORMS_H_   /* Include guard */
#define PLATFORMS_H_
#include "includes.h"

#define sizeOfPlatforms 28

Sprite Platforms[sizeOfPlatforms];
// gs multiplier, alter to change gamespeed.
#define GS 1;
#define MAX_PS 10
float platformMultiplyer = 1;

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

void create_platforms( void ) {
    //memset(Platforms, 0, sizeof Platforms);
    int initX = 0, initY = 9;
    int deltaY = 0;
    int c = 0;
    platformMultiplyer = adc_read(1) / 80;
    if (platformMultiplyer > MAX_PS) platformMultiplyer = MAX_PS;
    //if (platformMultiplyer < 0.1) platformMultiplyer = 0;
    float speed = 0.05 * platformMultiplyer;
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

void change_platform_speed( float speed )
{
    int c = 0;
    for (int i = 0; i < 4; i++){
        for (int j = 0; j < 7; j++){
            Platforms[c].dx = 0.05 * speed;
            c++;
        }
        speed = -speed;
    }
}

void check_pot(){
    float current_pot = adc_read(1) / 80;

    if (platformMultiplyer != current_pot){
        if (current_pot > MAX_PS) current_pot = MAX_PS;
        change_platform_speed(current_pot);
    }
    platformMultiplyer = current_pot;
}


#endif 