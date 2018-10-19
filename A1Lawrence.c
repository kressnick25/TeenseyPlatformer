#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include <cab202_graphics.h>
#include <cab202_sprites.h>
#include <cab202_timers.h>

#define DELAY_MS (20)
#define INIT_LIVES (10)
#define BORDER_CHAR ('~')
#define STATUS_HEIGHT (5)
#define MAX_PLATS (160)
#define PLAT_WIDTH (7)
#define COL_WIDTH (PLAT_WIDTH + 5)
#define ROW_HEIGHT (PLAT_HEIGHT + HERO_HEIGHT + 4)
#define PLAT_HEIGHT (2)
#define OCCUPIED_PERCENT (70)
#define MAX_WIDTH (1000)
#define FORB_PERCENT (25)
#define TREASURE_WIDTH (5) 
#define TREASURE_HEIGHT (2)

char treasure_img[] = 
",___,"
"|___|";
Sprite treasure;

#define HERO_WIDTH (3)
#define HERO_HEIGHT (3)
char hero_img[] = 
" O "
"/|\\"
"/ \\";
Sprite hero;

Sprite platforms[MAX_PLATS];
int num_plats;
bool is_safe[MAX_PLATS];
char safe_img[MAX_WIDTH];
char forb_img[MAX_WIDTH];

int score;
int lives;
double start_time;

bool game_over = false;

int get_col(sprite_id s)
{
    return (int)round(s->x) / COL_WIDTH;
}

int get_row(sprite_id s)
{
    return ((int)round(s->x) + PLAT_HEIGHT - STATUS_HEIGHT) / ROW_HEIGHT - 1;
}

void place_hero_on_start_block()
{
    sprite_id starting_blocks[MAX_PLATS];
    int num_starts = 0;

    for(int i = 0; i < num_plats; i++)
    {
        sprite_id plat = &platforms[i];

        if(is_safe[i] && get_row(plat) == 0)
        {
            starting_blocks[num_starts] = plat;
            num_starts++;
        }
    }

    int start_index = num_starts > 1 ? rand() % num_starts : 0;
    sprite_id start_plat = starting_blocks[start_index];
    hero.x = start_plat->x + rand() % (start_plat->width - hero.width);
    hero.y = start_plat->y - hero.height;
}

void setup_player()
{
    sprite_init(&hero, 10, 10, HERO_WIDTH, HERO_HEIGHT, hero_img);
    hero.dy = 0.3;
    place_hero_on_start_block();
}

void setup_platforms()
{
    int w, h;
    get_screen_size(&w, &h);

    for(int i = 0; i < MAX_WIDTH; i++)
    {
        safe_img[i] = '=';
        forb_img[i] = 'x';
    }

    int cols = w / COL_WIDTH;
    int rows = (h - STATUS_HEIGHT) / ROW_HEIGHT;

    int wanted = rows * cols * OCCUPIED_PERCENT / 100;
    int out_of = rows * cols;

    num_plats = 0;

    int safe_per_col[MAX_PLATS] = {0};

    for(int row = 0; row < rows; row++)
    {
        for(int col = 0; col < cols; col++)
        {
            if(num_plats >= MAX_PLATS)
            {
                break;
            }

            double p = (double)rand() / RAND_MAX;

            if(p <= (double)wanted / out_of)
            {
                wanted--;

                sprite_id plat = &platforms[num_plats];
                int x = col * (w - COL_WIDTH) / (cols - 1) + rand() % (COL_WIDTH - PLAT_WIDTH);
                int y = STATUS_HEIGHT + (row + 1) * ROW_HEIGHT - PLAT_HEIGHT;

                sprite_init(plat, x, y, PLAT_WIDTH, PLAT_HEIGHT, safe_img);

                is_safe[num_plats] = true;
                safe_per_col[col]++;
                num_plats++;            
            }

            out_of--;
        }
    }

    int num_forbidden = num_plats * FORB_PERCENT / 100;

    if(num_forbidden < 2)
    {
        num_forbidden = 2;
    }

    for(int i = 0; i < num_forbidden; i++)
    {
#define MAX_TRIALS (1000)

        for(int trial = 0; trial < MAX_TRIALS; i++)
        {
            int plat_index = 1 + rand() % (num_plats - 1);
            sprite_id plat = &platforms[plat_index];
            int col = get_col(plat);

            if(safe_per_col[col] > 1)
            {
                is_safe[plat_index] = false;
                safe_per_col[col]--;
                plat->bitmap = forb_img;
                break;
            }
        }
    }
}

void setup_treasure()
{
    sprite_init(&treasure, 0, screen_height() - TREASURE_HEIGHT, TREASURE_WIDTH, TREASURE_HEIGHT, treasure_img);
    treasure.dx = 0.3;
    treasure.dy = 0;
}

void setup_game()
{
    score = 0;
    lives = INIT_LIVES;
    start_time = get_current_time();

    setup_platforms();
    setup_player();
    setup_treasure();
}

void draw_status()
{
    int w = screen_width();

    draw_line(0, 0, w - 1, 0, BORDER_CHAR);
    draw_line(0, STATUS_HEIGHT - 1, w - 1, STATUS_HEIGHT - 1, BORDER_CHAR);

    for(int i = 1; i < STATUS_HEIGHT - 1; i++)
    {
        draw_line(0, i, w - 1, i, ' ');
    }

    int time = (int)(get_current_time() - start_time);

    draw_formatted(10, STATUS_HEIGHT / 2, "Lives: %2d       Time: %2d:%2d       Score: %d", lives, time / 60, time % 60, score);
}

void draw()
{
    clear_screen();
    sprite_draw(&treasure);
    sprite_draw(&hero);

    for(int i = 0; i < num_plats; i++)
    {
        sprite_draw(&platforms[i]);
    }

    draw_status();
    show_screen();
}

int get_current_platform(sprite_id s)
{
    int sl = (int)round(s->x);
    int sr = sl + s->width - 1;
    int sy = (int)round(s->y);

    for(int plat = 0; plat < num_plats; plat++)
    {
        sprite_id p = &platforms[plat];
        int pl = (int)round(p->x);
        int pr = pl + p->width - 1;
        int py = (int)round(p->y);

        if(sr >= pl && sl <= pr && sy == py - s->height)
        {
            return plat;
        }
    } 

    return -1;
}

bool play_again()
{
     clear_screen();
     draw_formatted(10, 10, "Game Over!");
     draw_formatted(10, 11, "Score: %d", score);
     
     int time = (int)(get_current_time() - start_time);
     draw_formatted(10, 12, "Elapsed time: %02d:%02d", time / 60, time % 60);

     draw_formatted(10, 15, "Press 'r' to play again, or 'q' to quit.");

     show_screen();

     int key = get_char();

     while(key != 'r' && key != 'q')
     {
         key = get_char();
     }

     return 'r' == key;
}

bool sprites_collide(sprite_id s1, sprite_id s2)
{
    int top1 = round(sprite_y(s1));
    int bottom1 = top1 + sprite_height(s1) - 1;
    int left1 = round(sprite_x(s1));
    int right1 = left1 + sprite_width(s1) - 1;

    int top2 = round(sprite_y(s2));
    int bottom2 = top2 + sprite_height(s2) - 1;
    int left2 = round(sprite_x(s2));
    int right2 = left2 + sprite_width(s2) - 1;

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

void process()
{
    int key = get_char();
    int plat = get_current_platform(&hero);

    static bool falling = false;

    bool died = false;

    if(plat >= 0)
    {
        if(is_safe[plat])
        {
            if(falling)
            {
                score++;
            }

            falling = false;

            if(key == 'a')
            {
                hero.x--;
            }
            else if(key == 'd')
            {
                hero.x++;
            }
        }
        else
        {
            died = true;
        }
    }
    else
    {
        falling = true;
        hero.y += hero.dy;
    }

    int h1 = (int)round(hero.x);
    int hr = h1 + hero.width - 1;
    int ht = (int)round(hero.y);
    int hb = ht + hero.height - 1;

    if(h1 < 0 || hr >= screen_width() || hb >= screen_height())
    {
        died = true;
    }

    static bool treasure_paused = false;

    if(key == 't')
    {
        treasure_paused = !treasure_paused;
    }

    if(!treasure_paused)
    {
        treasure.x += treasure.dx;
        int tl = (int)round(treasure.x);
        int tr = tl + treasure.width - 1;

        if(tl < 0 || tr >= screen_width())
        {
            treasure.x -= treasure.dx;
            treasure.dx = -treasure.dx;
        }
    }

    if(sprites_collide(&treasure, &hero))
    {
        falling = false;
        treasure.y = -1000;
        lives += 2;
        setup_player();
    }

    if(died)
    {
        falling = false;
        lives--;

        if(lives > 0)
        {
            setup_player();
        }
        else
        {
            if(play_again())
            {
                setup_game();
            }
            else
            {
                game_over = true;
                return;
            }
        }
    }

    draw();
}

int main()
{
    srand(time(NULL));
    setup_screen();
    setup_game();

    while(!game_over)
    {
        process();
        timer_pause(DELAY_MS);
    }
}