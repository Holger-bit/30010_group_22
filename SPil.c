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

uint16_t score = 0;

void run_game_manager(void) {


	//Declarations and initialization
	uint8_t start = 0;

	Entity player = {47, 40, 6, 6, 0};
    Asteroid rocky = {2, 15, 1};
	bullet_t bullets[MAX_PROJECTILES];
	uint8_t next_bullet_idx = 0;
    State currentState = STATE_MENU;
    uint8_t selected = 0;
    char hearts[4];
    char Bombs[4];

    char input;

    // TÆLLERE (Baseret på 10ms ticks)
    uint32_t skud_taeller;
    uint32_t sten_taeller;
	uint32_t lcd_taeller;

    uint8_t powerup_ready;
    uint8_t powerup_active;
    uint8_t timer_offset;

    //BOMB VARIABLE
    char bombtimer_str[4]; // 3 tegn + escape
	uint8_t bombtimer_active;
	uint8_t bombtimer_offset;
	uint8_t bombtimer_len;
	int bombs_left; //antal bomber tilbage (til affyring)
    uint32_t cooldown;

    // hardware
    setupjoystick();
    setupLed();
    setLed(COLOUR_BLACK);
    lcd_init();

    char timer_str[11];
    uint8_t buffer[512]= {};
    Bomb bombs[MAX_BOMBS] = {0};

    // lav menuen når man starter programmet
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
                // --- RESET VED HVER SPIL-START ---
                if (start == 0) {
                    player.x = 47; player.y = 40; player.tilt = 0;
                    score = 0;


                    skud_taeller = 0; sten_taeller = 0; lcd_taeller = 0;
                    powerup_ready = 0; powerup_active = 0; timer_offset = 0;
                    init_enemies_session();
                    clrscr();
                    window(1, 1, 100, 50, " SPACE HAM ", 0);
                    bullets_init(bullets, &next_bullet_idx);
                    makeplayer(&player, 0);
                    start = 1;

                    strcpy(hearts, "123");
                    hearts[0] = '1'; hearts[1] = '2'; hearts[2] = '3'; hearts[3] = '\0';

                    strcpy(Bombs, "123");
					//Bombs[0]  = '1'; Bombs[1]  = '2'; Bombs[2]  = '3'; Bombs[3]  = '\0';

	                //BOMB VARIABLE
					strcpy(bombtimer_str, "123");
					//bombtimer_str[0]= '1';bombtimer_str[0]= '2';bombtimer_str[0]= '3';bombtimer_str[0]= '\0';
					bombtimer_active = 0;
					bombtimer_offset = 0;
					bombtimer_len = (uint8_t)(sizeof(bombtimer_str) - 1);
					bombs_left = 3; //antal bomber tilbage (til affyring)
					strcpy(timer_str, "0123456789");



                }

                // --- MOTOR (100 Hz) ---
                if (gtimerIRQFLAG == 1) {
                	gtimerIRQFLAG = 0;

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


                    if (powerup_active == 1) {
                        cooldown = 10;  // Hvis powerup er aktiv, skal vi kun vente 10 ticks (skyder hurtigt)
                    } else {
                        cooldown = 30;  // Ellers skal vi vente 30 ticks (skyder langsomt)
                    }

                    skud_taeller++;
                    if (skud_taeller >= cooldown) {
                        shoot(bullets, &next_bullet_idx, player.x + 3, player.y - 1, player.tilt, -1);
                        skud_taeller = 0;
                    }

                    // 5. Asterolde og Skud
                    updateProjectiles(bullets,&rocky);
                    updateBombs(bombs, bullets);

                    sten_taeller++;

                    if (sten_taeller >= 15) {
                        // Hver 15. gang flytter vi den
                        updateAsteroid(&rocky);
                        sten_taeller = 0;
                    } else {
                        // Alle de andre gange tegner vi den bare (uden at flytte/slette)
                        // Det fixer de huller som skuddene laver, uden at det flimrer
                        drawAsteroid(&rocky);
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
							lcd_draw_timer(buffer, timer_str, 451, timer_offset);
							if (timer_offset < 19) timer_offset++;
							else {
								powerup_active = 0;
								setLed(COLOUR_BLACK);
							}
						}


						if (bombtimer_active)
						{
							lcd_draw_timer_omvendt(buffer, bombtimer_str, 179, bombtimer_offset);

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
