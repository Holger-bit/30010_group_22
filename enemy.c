#include "enemy.h"
#include "ansi.h"
#include <stdio.h>
#include "joystick.h"
#include "levels.h"
#include "game_manager.h"

static enemy_t enemies[MAX_ENEMIES];
static uint8_t enemy_count = 0;
volatile uint8_t enemy_start = 0;
volatile uint8_t enemy_step = 0;

#define GRID_SLOTS 10
#define SLOT_WIDTH 10
#define START_X 2
#define START_Y 2
#define WAVES 5
#define NO_SPAWN 255

typedef struct {
    uint8_t delay[GRID_SLOTS*WAVES];   // spawn timing per column
    uint8_t spawned[GRID_SLOTS*WAVES]; // has this slot spawned yet?
} wave_t;


static wave_t current_wave = {
    .delay   = {NO_SPAWN, 0,  1, 		NO_SPAWN, NO_SPAWN, NO_SPAWN, NO_SPAWN, 5, 		  2,  		NO_SPAWN,
    			7,		  3,  NO_SPAWN, 8, 		  NO_SPAWN, 6, 		  9, 		NO_SPAWN, 6,  		4,
				10, 	  6,  NO_SPAWN, 12, 	  7, 		NO_SPAWN, 14, 		NO_SPAWN, 10, 		9,
				NO_SPAWN, 9,  10, 		NO_SPAWN, NO_SPAWN, 15, 	  NO_SPAWN, 16, 	  15, 		NO_SPAWN,
				17, 	  12, 18, 		NO_SPAWN, 12, 		NO_SPAWN, 17, 		19, 	  NO_SPAWN, 13},
    .spawned = {0}
};


void moveEnemies(void) {
	for (uint8_t i = 0; i < enemy_count; i++) {
		if (enemies[i].alive) {
			enemies[i].y++;
			enemy_step = 1;
		}
	}
}

void printEnemy(uint8_t x, uint8_t y, uint8_t enemy_type) {
	switch (enemy_type){
	case 1:
		gotoxy(x, y);
		printf(" /\\_/\\ ");
		gotoxy(x, y+1);
		printf("( 0 0 )");
		gotoxy(x, y+2);
		printf("=(_Y_)=");
		gotoxy(x, y+3);
		printf("  V-V  ");
		break;
	}
}

void clearEnemy(uint8_t x, uint8_t y) {
	gotoxy(x, y);
	printf("       ");
	gotoxy(x, y+1);
	printf("       ");
	gotoxy(x, y+2);
	printf("       ");
	gotoxy(x, y+3);
	printf("       ");
}


void spawnEnemyAtColumn(uint8_t col)
{
    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].alive) {

            enemies[i].x = START_X + col * SLOT_WIDTH;
            enemies[i].y = START_Y;
            enemies[i].alive = 1;

            if (i >= enemy_count)
                enemy_count = i + 1;

            enemy_start = 1;
            return;
        }
    }
}


void killEnemy(enemy_t *dying_enemy) {
	clearEnemy(dying_enemy->x, dying_enemy->y);
	dying_enemy->alive = 0;
	dying_enemy->x = 0;
	dying_enemy->y = 0;
	lvlup();
}

void detectCollision(void) {
	for (uint8_t i = 0; i < enemy_count; i++) {
		if (enemies[i].y > 44) {
			killEnemy(&enemies[i]);
		}
	}
}


void enemy_tick(void)
{
    static uint16_t spawn_timer = 0;
    static uint16_t move_counter = 0;

    spawn_timer++;

    /* ---- Wave spawner ---- */
    for (uint8_t i = 0; i < GRID_SLOTS*WAVES; i++) {
        if (current_wave.delay[i] == NO_SPAWN)
            continue;
        if (!current_wave.spawned[i] &&
            spawn_timer >= current_wave.delay[i] * 200) {

            spawnEnemyAtColumn(i % GRID_SLOTS);
            current_wave.spawned[i] = 1;
        }
    }

    /* ---- Enemy movement ---- */
    if (++move_counter >= 75) {
        enemy_step = 1;
        move_counter = 0;
    }
}


void enemy(void)
{
    static uint8_t j_old = 0;
    static uint8_t kill_index = 0;

    uint8_t j = readJoystick();

    if (enemy_step) {

        /* Move */
        for (uint8_t i = 0; i < enemy_count; i++) {
            if (enemies[i].alive) {
                clearEnemy(enemies[i].x, enemies[i].y);
                enemies[i].y++;
            }
        }

        /* Collisions */
        detectCollision();

        /* Render */
        for (uint8_t i = 0; i < enemy_count; i++) {
            if (enemies[i].alive) {
                printEnemy(enemies[i].x, enemies[i].y, 1);
            }
        }

        enemy_step = 0;
    }

    if (enemy_start) {
        printEnemy(enemies[enemy_count-1].x,
                   enemies[enemy_count-1].y, 1);
        enemy_start = 0;
    }

    /* Kill with joystick */
    if (j != j_old && (j & (1 << 4))) {
        if (kill_index < enemy_count) {
            killEnemy(&enemies[kill_index++]);
        }
    }

    j_old = j;
}

