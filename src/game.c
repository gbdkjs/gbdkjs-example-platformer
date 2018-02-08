#include "game.h"
#include "assets.h"

void game_loop();
void win();

UBYTE prev_joy;
UBYTE time;
fixed player_x;
fixed player_y;
fixed player_vx;
fixed player_vy;
BYTE player_vx_dir;
BYTE player_vy_dir;
UBYTE running;
UBYTE hold_jump;

#define MIN_WALK_VEL 0x130
#define WALK_ACCELERATION 0x098
#define RUN_ACCELERATION 0xE4
#define RELEASE_DECELERATION 0xD0
#define SKIDDING_DECELERATION 0x1A0
#define MAX_WALK_SPEED 0xD00
#define MAX_RUN_SPEED 0x2900

#define JUMP_VELOCITY 0x4000
#define JUMP_HOLD 0x200
#define GRAVITY 0x700
#define MAX_FALL_SPEED 0x4800

void draw_player()
{
    if(player_vx.w == 0) {
        set_sprite_prop(0, 0);
        set_sprite_prop(1, 0);
        set_sprite_tile(0, 0);
        set_sprite_tile(1, 2);
    } else {
        if(player_vx_dir >= 0) {
            set_sprite_prop(0, 0);
            set_sprite_prop(1, 0);
            set_sprite_tile(0, 4 + (((time>>2)&2)<<1));
            set_sprite_tile(1, 6 + (((time>>2)&2)<<1));  
        } else {
            set_sprite_prop(0, S_FLIPX);
            set_sprite_prop(1, S_FLIPX);
            set_sprite_tile(0, 6 + (((time>>2)&2)<<1));  
            set_sprite_tile(1, 4 + (((time>>2)&2)<<1));
        }         
    }
}

void move_camera()
{
    SCX_REG = player_x.b.h < 80
                ? 0
                : player_x.b.h >= 175
                ? 95
                : player_x.b.h - 80;
    SCY_REG = player_y.b.h < 72
                ? 0
                : player_y.b.h >= 183
                ? 111
                : player_y.b.h - 72;
}

void move_player()
{
    move_sprite(0, player_x.b.h - SCX_REG, player_y.b.h - SCY_REG);  
    move_sprite(1, player_x.b.h + 8 - SCX_REG, player_y.b.h - SCY_REG);
}

int main()
{
    LCDC_REG = 0x67; // ???

    /* Set palettes */
    BGP_REG = OBP0_REG = 0xE4U;
    OBP1_REG = 0xD2U;

    set_interrupts(VBL_IFLAG | LCD_IFLAG);
    STAT_REG = 0x45;

    WX_REG = MAXWNDPOSX;
    WY_REG = MAXWNDPOSY;

    // Initialize the background
    set_bkg_data(0x00, 108, platformer_tileset);
    set_bkg_tiles(0, 0, 32, 32, platformer_tiles);
    set_sprite_data(0x00, 0x80, player_sprites);

    DISPLAY_ON;
    SHOW_SPRITES;

    player_x.b.h = 48;
    player_y.b.h = 224;

    running = TRUE;

    draw_player();
    move_player();
    
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(game_loop, 60, 1);
#else
    while(1)
    {
        game_loop();
    }
#endif
}

void game_loop()
{
    UBYTE i, t_x, t_y, below_tile, col_tile;
  
#ifdef __EMSCRIPTEN__    
    update_registers(SCX_REG, SCY_REG, WX_REG, WY_REG, LYC_REG, BGP_REG, OBP0_REG, OBP1_REG);
#endif
    wait_vbl_done();

    i = joypad();

    if(running) {

        // Velocity

        if(i & J_LEFT) {
            // if(player_vx.w <= 0) {
            if(player_vx_dir <= 0) {
                player_vx_dir = -1;
                if(i & J_B) {
                    player_vx.w += RUN_ACCELERATION; 
                } else {
                    player_vx.w += WALK_ACCELERATION; 
                }
            } else {
                // LOG("SKID LEFT\n");
                player_vx.w -= SKIDDING_DECELERATION;
                if(player_vx.w < MIN_WALK_VEL) {
                    player_vx.w = 0;
                    player_vx_dir = 0;
                }                
            }
        } else if(i & J_RIGHT) {
            // if(player_vx.w >= 0) {
            if(player_vx_dir >= 0) {
                player_vx_dir = 1;
                if(i & J_B) {
                    player_vx.w += RUN_ACCELERATION; 
                } else {
                    player_vx.w += WALK_ACCELERATION; 
                }                
            } else {
                player_vx.w -= SKIDDING_DECELERATION; 
                if(player_vx.w < MIN_WALK_VEL) {
                    player_vx.w = 0;
                    player_vx_dir = 0;
                }                               
            }      
        } else if(player_vx.w >= RELEASE_DECELERATION && player_vy.w == 0   ) {
            player_vx.w -= RELEASE_DECELERATION;
            if(player_vx.w < MIN_WALK_VEL) {
                player_vx.w = 0;
                player_vx_dir = 0;
            }            
        }

        if(player_vy_dir >= 0) {
            player_vy.w += GRAVITY;
        } else {
            if(hold_jump && player_vy.w >= JUMP_HOLD) {
                player_vy.w -= JUMP_HOLD;
            }
            else if(player_vy.w >= GRAVITY) {
                player_vy.w -= GRAVITY;
            } else {
                player_vy.w = 0;
                player_vy_dir = 1;
            }
        }

        if(!(i & J_B) && player_vx.w > MAX_WALK_SPEED) {
            player_vx.w = MAX_WALK_SPEED;
        } else if((i & J_B) && player_vx.w > MAX_RUN_SPEED) {
            player_vx.w = MAX_RUN_SPEED;
        }

        if(player_vy.w > MAX_FALL_SPEED) {
            player_vy.w = MAX_FALL_SPEED;
        }

        if(player_vx_dir > 0) {
            player_x.w += player_vx.w>>4;
        } else {
            player_x.w -= player_vx.w>>4;
        }

        if(player_vy_dir >= 0) {
            player_y.w += player_vy.w>>4;
        } else {
            player_y.w -= player_vy.w>>4;
        }        

        // Stage Bounds

        if(player_x.b.h < 8) {
            player_x.b.h = 8;
        } else if (player_x.b.h > 248) {
            player_x.b.h = 248;
        }

        if(player_y.b.h < 8) {
            player_y.b.h = 8;
            player_vy.w = 0;
        } else if (player_y.b.h > 248) {
            player_y.b.h = 248;
        }

        // Collision
        t_x = player_x.b.h>>3;
        t_y = player_y.b.h>>3;

        below_tile = platformer_tiles[(32 * t_y) + t_x];
        col_tile = platformer_tiles[(32 * (t_y-1)) + t_x];

        if(below_tile >= 0xb && player_vy_dir >= 0) {
            // LOG("HIT TILE %d\n", below_tile);
            player_y.b.h = t_y<<3;
            player_vy.w = 0; 
            if(!hold_jump && (i & J_A)) {
                player_vy.w = JUMP_VELOCITY;
                player_vy_dir = -1;
                hold_jump = TRUE;
            }                     
        }

        // Handle Jump Release
        if(!(i & J_A)) { 
            hold_jump = FALSE;
        }  

        // Reposition Player
        draw_player();
        move_camera();
        move_player();

        // Check for end state
        if(col_tile == 0xa && below_tile >= 0xb && player_vy.w == 0 ) {
            win();
        }

    }

    prev_joy = i;
    time++;
}

void win() {
    DISPLAY_OFF;
    HIDE_SPRITES;
    running = FALSE;
    set_bkg_data(0x00, 108, win_tileset);
    set_bkg_tiles(0, 0, 20, 18, win_tiles);
    SCX_REG = 0;
    SCY_REG = 0;
    DISPLAY_ON;    
}
