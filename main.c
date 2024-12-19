#include "xparameters.h"
#include "xil_types.h"
#include "xil_printf.h"
#include "sleep.h"
#include "PmodOLED.h"
#include "PmodKYPD.h"
#include <stdio.h>
#include <stdlib.h>
#include "xtime_l.h"
#include <sys/time.h>
#include <time.h>

PmodOLED oled;
PmodKYPD keypad;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define PADDLE_HEIGHT 2
#define BALL_SIZE 3
#define BRICK_WIDTH 8
#define BRICK_HEIGHT 3
#define ROWS 3
#define COLS 15

#define OLED_IIC_ADDRESS 0x3C
#define GPIO_BASEADDR  0x40000000
#define SPI_BASEADDR   0x40010000

#define DEFAULT_KEYTABLE "0FED789C456B123A"

// Game state variables
int paddle_x = 54;
int paddle_y = 30;
int ball_x = SCREEN_WIDTH / 2, ball_y = SCREEN_HEIGHT / 2;
int ball_dx = 1, ball_dy = -1;
int bricks[ROWS][COLS];
int gameOver = 0;
int bricks_left = ROWS * COLS;
int gameDelay = 30000;
int PADDLE_WIDTH = 20;
int paddleSpeed = 5;

void game_over();
void game_won();
void reset();

void initializeOLED() {
	OLED_Begin(&oled, XPAR_PMODOLED_0_AXI_LITE_GPIO_BASEADDR, XPAR_PMODOLED_0_AXI_LITE_SPI_BASEADDR, 0x0, 0x0);
	OLED_ClearBuffer(&oled);
	OLED_Update(&oled);
}

void initializeKeypad() {
	KYPD_begin(&keypad, XPAR_PMODKYPD_0_AXI_LITE_GPIO_BASEADDR);
	KYPD_loadKeyTable(&keypad, (u8*) DEFAULT_KEYTABLE);
}

void init_game() {
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            bricks[i][j] = 1;
        }
    }
}

void drawRect(PmodOLED *InstancePtr, int x1, int y1, int x2, int y2) {
    x1 = GrphClampXco(x1);
    y1 = GrphClampYco(y1);
    x2 = GrphClampXco(x2);
    y2 = GrphClampYco(y2);

    for (int x = x1; x < x2-1; x++) {
    	for (int y = y1; y < y2; y++) {
    		OLED_MoveTo(InstancePtr, x, y);
    		OLED_LineTo(InstancePtr, x+1, y);
    	}
    }
}


void draw_game() {
    OLED_ClearBuffer(&oled);

    drawRect(&oled, paddle_x, paddle_y, paddle_x + PADDLE_WIDTH, paddle_y + PADDLE_HEIGHT);

    drawRect(&oled, ball_x, ball_y, ball_x + BALL_SIZE, ball_y + BALL_SIZE);

    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            if (bricks[i][j]) {
                int brick_x = j * BRICK_WIDTH;
                int brick_y = i * BRICK_HEIGHT;
                drawRect(&oled, brick_x, brick_y,
                              brick_x + BRICK_WIDTH, brick_y + BRICK_HEIGHT);
            }
        }
    }

    OLED_Update(&oled);
}

void update_ball() {
    ball_x += ball_dx;
    ball_y += ball_dy;

    if (ball_x <= 0) {
		ball_x = 0;
		ball_dx = -ball_dx;
	} else if (ball_x + BALL_SIZE >= SCREEN_WIDTH) {
		ball_x = SCREEN_WIDTH - BALL_SIZE;
		ball_dx = -ball_dx;
	}
    if (ball_y <= 0) {
    	ball_y = 0;
        ball_dy = -ball_dy;
    }

    if (ball_y + BALL_SIZE >= SCREEN_HEIGHT - PADDLE_HEIGHT - 1 &&
        ball_x + BALL_SIZE >= paddle_x && ball_x <= paddle_x + PADDLE_WIDTH) {
        ball_dy = -ball_dy;
    }

    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            if (bricks[i][j]) {
                int brick_x = j * BRICK_WIDTH;
                int brick_y = i * BRICK_HEIGHT;
                if (ball_x + BALL_SIZE >= brick_x && ball_x <= brick_x + BRICK_WIDTH &&
                    ball_y + BALL_SIZE >= brick_y && ball_y <= brick_y + BRICK_HEIGHT) {
                    bricks[i][j] = 0;
                    bricks_left -= 1;
                    ball_dy = -ball_dy;
                    if (ball_x + BALL_SIZE <= brick_x || ball_x >= brick_x + BRICK_WIDTH) {
						ball_dx = -ball_dx;
					}
                }
            }
        }
    }
    if (bricks_left <= 0) {
    	gameOver = 1;
    	game_won();
    }

    if (ball_y + BALL_SIZE >= SCREEN_HEIGHT) {
    	gameOver = 1;
        game_over();
    }
    usleep(gameDelay);
}

void update_paddle() {
    u16 keyStates = KYPD_getKeyStates(&keypad);
    u8 key;

    if (KYPD_getKeyPressed(&keypad, keyStates, &key) == KYPD_SINGLE_KEY) {
        if (key == 'D') {
            paddle_x += paddleSpeed;
            if (paddle_x + PADDLE_WIDTH > SCREEN_WIDTH)
                paddle_x = SCREEN_WIDTH - PADDLE_WIDTH;
        } else if (key == 'E') {
            paddle_x -= paddleSpeed;
            if (paddle_x < 0)
                paddle_x = 0;
        }
    }
}

void reset(int harder) {
	int r = 0;
    gameOver = 0;
    bricks_left = ROWS*COLS;
    ball_x = SCREEN_WIDTH / 2;
    ball_y = SCREEN_HEIGHT / 2;
    paddle_x = 54;
    paddle_y = 30;
    ball_dy = -1;
    ball_dx = 1;
	if (harder) {
		r = rand() % 3;
		if (r == 0) {
			if (gameDelay != 0) {
				gameDelay -= 5000;
			}
			OLED_ClearBuffer(&oled);
			OLED_SetCursor(&oled, 0, 1);
			OLED_PutString(&oled, "Ball will be faster!");
			sleep(2);
		} else if (r == 1) {
			if (PADDLE_WIDTH > 10) {
				PADDLE_WIDTH -= 2;
			}
			OLED_ClearBuffer(&oled);
			OLED_SetCursor(&oled, 0, 1);
			OLED_PutString(&oled, "Paddle will be smaller!");
			sleep(2);
		} else if (r == 2) {
			if (paddleSpeed > 3) {
				paddleSpeed -= 2;
			}
			OLED_ClearBuffer(&oled);
			OLED_SetCursor(&oled, 0, 1);
			OLED_PutString(&oled, "Paddle will be slower!");
			sleep(2);
		} else {
			if (paddleSpeed < 7) {
				paddleSpeed += 2;
			}
			OLED_ClearBuffer(&oled);
			OLED_SetCursor(&oled, 0, 1);
			OLED_PutString(&oled, "Paddle will be faster!");
			sleep(2);
		}
		r = rand() % 1;
		if (r == 0) {
			ball_x += rand() % 10;
		} else {
			ball_x -= rand() % 10;
		}
	}
}

void game() {
	OLED_ClearBuffer(&oled);
    OLED_SetCursor(&oled, 3, 0);
    OLED_PutString(&oled, "Breakout!");
    OLED_SetCursor(&oled, 2, 1);
    OLED_PutString(&oled, "C to begin!");
    OLED_SetCursor(&oled, 3, 2);
    OLED_PutString(&oled, "Controls:");
    OLED_SetCursor(&oled, 0, 3);
    OLED_PutString(&oled, "E: Left D: Right");
    OLED_Update(&oled);
    u8 key;
	while (1) {
		u16 keyStates = KYPD_getKeyStates(&keypad);
		if (KYPD_getKeyPressed(&keypad, keyStates, &key) == KYPD_SINGLE_KEY) {
			if (key == 'C') {
				break;
			}
		}
	}
	OLED_ClearBuffer(&oled);
	OLED_SetCursor(&oled, 3, 1);
	OLED_PutString(&oled, "Game Begin!");
    sleep(2);
    init_game();
    while (!gameOver) {
        draw_game();
        update_paddle();
        update_ball();
    }
}

void game_won() {
	OLED_ClearBuffer(&oled);
	OLED_SetCursor(&oled, 3, 0);
	OLED_PutString(&oled, "You won!!");
	OLED_SetCursor(&oled, 0, 1);
	OLED_PutString(&oled, "C: Play again");
	OLED_SetCursor(&oled, 0, 2);
	OLED_PutString(&oled, "9: Up difficulty");
	OLED_SetCursor(&oled, 0, 3);
	OLED_PutString(&oled, "F: exit");
	OLED_Update(&oled);
	u8 key;
	while (gameOver) {
		u16 keyStates = KYPD_getKeyStates(&keypad);
		if (KYPD_getKeyPressed(&keypad, keyStates, &key) == KYPD_SINGLE_KEY) {
			if (key == 'C') {
				reset(0);
				break;
			} else if (key == '9') {
				reset(1);
				break;
			} else if (key == 'F') {
				OLED_ClearBuffer(&oled);
				OLED_SetCursor(&oled, 3, 1);
				OLED_PutString(&oled, "Good bye!");
				OLED_Update(&oled);
				exit(0);
			}
		}
	}
	game();
}

void game_over() {
	OLED_ClearBuffer(&oled);
	OLED_SetCursor(&oled, 3, 0);
	OLED_PutString(&oled, "Game Over!");
	OLED_SetCursor(&oled, 0, 1);
	OLED_PutString(&oled, "C: Try again");
	OLED_SetCursor(&oled, 0, 2);
	OLED_PutString(&oled, "F: Exit");
	OLED_Update(&oled);
	u8 key;
	while (gameOver) {
		u16 keyStates = KYPD_getKeyStates(&keypad);
		if (KYPD_getKeyPressed(&keypad, keyStates, &key) == KYPD_SINGLE_KEY) {
			if (key == 'C') {
				reset(0);
				break;
			} else if (key == 'F') {
				OLED_ClearBuffer(&oled);
				OLED_SetCursor(&oled, 3, 1);
				OLED_PutString(&oled, "Good bye!");
				OLED_Update(&oled);
				exit(0);
			}
		}
	}
	game();
}

int main() {
	initializeOLED();
	initializeKeypad();
	game();
    return 0;
}

