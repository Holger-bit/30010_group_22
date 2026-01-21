#include "spil.h"
#include "startscreen.h"
#include "ansi.h"
#include "30010_io.h"
#include "Player_enemies.h"
#include "EX5.h"
#include "LCDpush.h"
#include <string.h>
#include "charset.h"
#include "gravity_astroide.h"
#include "timer.h"
#include "projectiles.h"
#include "enemy.h"

// --- GLOBALE VARIABLER ---
static Entity player = {47, 40, 6, 6, 0};
static bullet_t bullets[MAX_PROJECTILES];
static uint8_t next_bullet_idx = 0;

volatile uint8_t game_tick = 0;
uint16_t score = 0;

// Callback fra Timer 15
void spil_timer_callback(void) {
    game_tick = 1;
}

typedef enum {
    STATE_MENU,
    STATE_PLAYING,
    STATE_HELP
} State;

void run_game_manager(void) {
    State currentState = STATE_MENU;
    uint8_t selected = 0;
    char input;

    // --- HARDWARE INIT (Kører kun én gang) ---
    setupjoystick();
    setupLed();
    setLed(COLOUR_BLACK);
    lcd_init();
    timer15_init();
    timer15_setCallback(spil_timer_callback);

    maketitle();
    make_menu(selected);

    while (1) {
        input = uart_get_char();

        switch (currentState) {
            case STATE_MENU:
                if (input != 0) {
                    if (input == 'w') { selected = 0; make_menu(selected); }
                    else if (input == 's') { selected = 1; make_menu(selected); }
                    else if (input == 13 || input == 32) {
                        if (selected == 0) {
                            currentState = STATE_PLAYING;
                        } else {
                            clrscr();
                            currentState = STATE_HELP;
                        }
                    }
                }
                break;

            case STATE_PLAYING:
            {
                static uint8_t start = 0;

                static char hearts[] = "123";
                static char Bombs[] = "123"; // *****//laver en string som kan aendres
                static uint8_t buffer[512];
                static Bomb bombs[MAX_BOMBS] = {0};

                // TÆLLERE (Baseret på 10ms ticks)
                static uint32_t skud_taeller = 0;
                static uint32_t sten_taeller = 0;
                static uint32_t lcd_taeller = 0;

                // POWERUP VARIABLE
                static uint8_t powerup_ready = 0;
                static uint8_t powerup_active = 0;
                static uint8_t timer_offset = 0;
                static char timer_str[] = "01234567890123456789";
                static uint8_t powerup_timer_offset = 0;
                static const uint8_t timer_len = (uint8_t)(sizeof(timer_str) - 1);

                //BOMB VARIABLE
                static char bombtimer_str[] = "012"; // *****// 3 tegn
				static uint8_t bombtimer_active = 0;
				static uint8_t bombtimer_offset = 0;
				static const uint8_t bombtimer_len = (uint8_t)(sizeof(bombtimer_str) - 1);
				static uint8_t bomb_timer_offset = 0;
				static int bombs_left = 3; //antal bomber tilbage (til affyring)
				static uint32_t last_bomb_time = (uint32_t)(-4000); // soerger for jeg kan skyde i starten

				static uint32_t last_lcd_ms = 0;// *****
					uint32_t now = sys_millis();// *****




                // --- RESET VED HVER SPIL-START ---
                if (start == 0) {
                    player.x = 47; player.y = 40; player.tilt = 0;
                    score = 0;
                    strcpy(hearts, "123");
                    skud_taeller = 0; sten_taeller = 0; lcd_taeller = 0;
                    powerup_ready = 0; powerup_active = 0; timer_offset = 0;
                    init_enemies_session();
                    clrscr();
                    window(1, 1, 100, 50, " SPACE HAM ", 0);
                    // (drawPortal er fjernet herfra)
                    bullets_init(bullets, &next_bullet_idx);
                    makeplayer(&player, 0);
                    start = 1;
                    hearts[0] = '1'; hearts[1] = '2'; hearts[2] = '3'; hearts[3] = '\0';//*****
					Bombs[0]  = '1'; Bombs[1]  = '2'; Bombs[2]  = '3'; Bombs[3]  = '\0';//*****

                }

                // --- MOTOR (100 Hz) ---
                if (game_tick == 1) {
                    game_tick = 0;

                    uint8_t joy = readJoystick();
                    int8_t old_tilt = player.tilt;
                    int32_t old_x = player.x;

                    // --- 1. FJENDE LOGIK (Tællere og bevægelse) ---
                    enemy_tick();
                    uint8_t hits = enemy_update();

                    while (hits--) {
                        lcd_hearts_remove(hearts);
                    }

                    if (hearts[0] == '\0') {
                        // GAME OVER -> tilbage til menu
                        setLed(COLOUR_BLACK);
                        start = 0;

                        lcd_clear(buffer);      // ryd LCD
                        clrscr();               // ryd terminal

                        currentState = STATE_MENU;

                        maketitle();
                        make_menu(0);

                        // (valgfrit) reset ting så næste spil starter rent
                        init_enemies_session();
                        // reset bullets/bomber hvis du vil

                        break;  // vigtigt: hop ud af STATE_PLAYING i din switch
                    }





                    // 2. Powerup system
                    if (input == 'r' || input == 'R') {
                        powerup_ready = 1;
                        setLed(COLOUR_BLUE);
                    }
                    if (joy == 16 && powerup_ready && !powerup_active) {
                        powerup_ready = 0;
                        powerup_active = 1;
                        timer_offset = 0;
                        setLed(COLOUR_PINK);
                    }








                    if ((input == 'q' || input == 'Q') &&
                        bombs_left > 0 &&
                        !bombtimer_active)
                    {
                        if (bombs[0].active == 0)
                        {
                            bombtimer_active = 1;      // start cooldown / HUD timer
                            bombtimer_offset = 0;

                            bombs[0].active = 1;
                            bombs[0].x = player.x + 3;
                            bombs[0].y = player.y - 1;
                            bombs[0].vy = -1;
                            bombs[0].vx = player.tilt;

                            bombs[0].under_char = ' ';
                            bombs[0].fuse = 17;

                            bombs_left--;

                            lcd_bombs_remove(Bombs);
                        }
                    }













                    // 3. Navigation
                    if (joy == 4) player.tilt = -2;
                    else if (joy == 5) player.tilt = -1;
                    else if (joy == 8) player.tilt = 2;
                    else if (joy == 9) player.tilt = 1;
                    else player.tilt = 0;

                    if (input == 'a' && player.x > 3) player.x -= 2;
                    if (input == 'd' && player.x < (99 - player.w)) player.x += 2;

                    // 4. Skydning
                    skud_taeller++;
                    uint32_t cooldown = powerup_active ? 10 : 30;
                    if (skud_taeller >= cooldown) {
                        shoot(bullets, &next_bullet_idx, player.x + 3, player.y - 1, player.tilt, -1);
                        skud_taeller = 0;
                    }

                    // 5. Asterolde og Skud
                    updateProjectiles(bullets);
                    updateBombs(bombs, bullets);

                    sten_taeller++;
                    if (sten_taeller >= 3) {
                        updateAsteroid(&rocky);
                        sten_taeller = 0;
                    }

                    // 6. Raket
                    if (player.x != old_x || player.tilt != old_tilt) {
                        int32_t tx = player.x; int8_t tt = player.tilt;
                        player.x = old_x; player.tilt = old_tilt;
                        makeplayer(&player, 1);
                        player.x = tx; player.tilt = tt;
                    }
                    makeplayer(&player, 0);

                    // 7. LCD HUD (Hvert 100. tick = 1 sekund)

                    lcd_taeller++;
                    if (lcd_taeller >=100) {



                        // 2) byg nyt HUD bagefter
                        memset(buffer, 0x00, 512);
                        lcd_write_heart(buffer, hearts, 1);
                        lcd_write_score(buffer, 104, score);
                        lcd_write_bomb(buffer, Bombs, 1);

						if (powerup_ready) lcd_write_powerup(buffer, "1", 385);

						if (powerup_active) {
							lcd_write_timer(buffer, timer_str, 451, timer_offset);
							if (timer_offset < 19) timer_offset++;
							else {
								powerup_active = 0;
								setLed(COLOUR_BLACK);
							}
						}


						if (bombtimer_active)
						{
							lcd_write_timer_omvendt(buffer, bombtimer_str, 179, bombtimer_offset);

							if (bombtimer_offset < bombtimer_len)
							{
								bombtimer_offset++;
							}
							else
							{
								bombtimer_active = 0; // færdig
							}
						}





						lcd_push_buffer(buffer);
						lcd_taeller = 0;
					}




                    // 8. Tilbage til menu
                    if (input == 'b') {
                        setLed(COLOUR_BLACK);
                        start = 0;
                        lcd_clear(buffer);
                        currentState = STATE_MENU;
                        clrscr(); maketitle(); make_menu(0);


                        powerup_timer_offset = 0; // ***** //resetter timer til power-up
					    bomb_timer_offset = 0; // ***** //resetter timer til bomb
						last_bomb_time = (uint32_t)(-4000);

						bombtimer_active = 0;
						bombs_left = 3; // resetter antal bomber
						for (int i = 0; i < MAX_BOMBS; i++) {
								if (bombs[i].active) {
									gotoxy(bombs[i].x, bombs[i].y);
									printf(" ");          // <-- ryd bomben visuelt
								}
								bombs[i].active = 0;
								bombs[i].fuse = 0;
						}
						powerup_ready = 0;
						powerup_active = 0;










                        break;
                    }
                }
            }
            break;

            case STATE_HELP:



                        	gotoxy(10,9);
                            printf("CONTROLS");
                        	gotoxy(10,10);
                            printf("USE W AND S IN THE MENU");
                            gotoxy(10,11);
                            printf("PRESS ENTER TO SELECT");
                            gotoxy(10,12);
                            printf("PRESS B TO GO BACK");
                            gotoxy(10,13);
                            printf("PRESS A AND D TO MOVE IN GAME");
                            gotoxy(10,14);
                            printf("MOVE JOYSTICK TO AIM AND CENTER TO POWER UP");
                            gotoxy(10,15);
                            printf("GOOD LUCK!");




                            gotoxy(10,18);
                            printf("WHO WAS HAM?");
                            gotoxy(10,19);
                            printf("A CHIMPANZE! IN 1961 HE WAS WELCOMED BACK, TO");
                            gotoxy(10,20);
                            printf("EARTH, AFTER A TRIP TO SPACE, WITH AN APPLE!");

                            input = uart_get_char();
                            if (input == 'b') {
                            currentState = STATE_MENU;
                            maketitle();
                            make_menu(selected);
                            }
                            break;
        }
    }
}
