/**
 * MiniFrogger
 * ./obj-x86_64-linux-gnu/mt128.elf ./atmega128_minifrog.axf
 */

#undef F_CPU
#define F_CPU 16000000
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega128");

#define	__AVR_ATmega128__	1
#include <avr/io.h>
#include <stdio.h>


// GENERAL INIT - USED BY ALMOST EVERYTHING ----------------------------------

static void port_init() {
	PORTA = 0b00011111;	DDRA = 0b01000000; // buttons & led
	PORTB = 0b00000000;	DDRB = 0b00000000;
	PORTC = 0b00000000;	DDRC = 0b11110111; // lcd
	PORTD = 0b11000000;	DDRD = 0b00001000;
	PORTE = 0b00100000;	DDRE = 0b00110000; // buzzer
	PORTF = 0b00000000;	DDRF = 0b00000000;
	PORTG = 0b00000000;	DDRG = 0b00000000;
}

// TIMER-BASED RANDOM NUMBER GENERATOR ---------------------------------------

static void rnd_init() {
	TCCR0 |= (1  << CS00);	// Timer 0 no prescaling (@FCPU)
	TCNT0 = 0; 				// init counter
}

// generate a value between 0 and max
static int rnd_gen(int max) {
	return TCNT0 % max;
}

// BUTTON HANDLING -----------------------------------------------------------

#define BUTTON_NONE		0
#define BUTTON_CENTER   1
#define BUTTON_LEFT		2
#define BUTTON_RIGHT	3
#define BUTTON_UP		4
static int button_accept = 1;

static int button_pressed() {
	// up
	if (!(PINA & 0b00000001) & button_accept) { // check state of button 1 and value of button_accept
		button_accept = 0; // button is pressed
		return BUTTON_UP;
	}

	// left
	if (!(PINA & 0b00000010) & button_accept) { // check state of button 2 and value of button_accept
		button_accept = 0; // button is pressed
		return BUTTON_LEFT;
	}

	// center
	if (!(PINA & 0b00000100) & button_accept) { // check state of button 3 and value of button_accept
		button_accept = 0; // button is pressed
		return BUTTON_CENTER;
	}

	// right
	if (!(PINA & 0b00001000) & button_accept) { // check state of button 4 and value of button_accept
		button_accept = 0; // button is pressed
		return BUTTON_RIGHT;
	}

	return BUTTON_NONE;
}

static void button_unlock() {
	//check state of all buttons
	if (
		((PINA & 0b00000001)
		|(PINA & 0b00000010)
		|(PINA & 0b00000100)
		|(PINA & 0b00001000)
		|(PINA & 0b00010000)) == 31)
	button_accept = 1; //if all buttons are released button_accept gets value 1
}

// LCD HELPERS ---------------------------------------------------------------

#define	CLR_DISP		0x00000001
#define	DISP_ON			0x0000000C
#define	DISP_OFF		0x00000008
#define	DD_RAM_ADDR		0x00000080
#define	DD_RAM_ADDR2	0x000000C0
#define CG_RAM_ADDR		0x00000040

#define CHARMAP_SIZE 4

static void lcd_delay(unsigned int b) {
	volatile unsigned int a = b;
	while (a)
		a--;
}

static void lcd_pulse() {
	PORTC = PORTC | 0b00000100;	//set E to high
	lcd_delay(1400); 			//delay ~110ms
	PORTC = PORTC & 0b11111011;	//set E to low
}

static void lcd_send(int command, unsigned char a) {
	unsigned char data;

	data = 0b00001111 | a;					//get high 4 bits
	PORTC = (PORTC | 0b11110000) & data;	//set D4-D7
	if (command)
		PORTC = PORTC & 0b11111110;			//set RS port to 0 -> display set to command mode
	else
		PORTC = PORTC | 0b00000001;			//set RS port to 1 -> display set to data mode
	lcd_pulse();							//pulse to set D4-D7 bits

	data = a<<4;							//get low 4 bits
	PORTC = (PORTC & 0b00001111) | data;	//set D4-D7
	if (command)
		PORTC = PORTC & 0b11111110;			//set RS port to 0 -> display set to command mode
	else
		PORTC = PORTC | 0b00000001;			//set RS port to 1 -> display set to data mode
	lcd_pulse();							//pulse to set d4-d7 bits
}

static void lcd_send_command(unsigned char a) {
	lcd_send(1, a);
}

static void lcd_send_data(unsigned char a) {
	lcd_send(0, a);
}

static void lcd_create_custom_chars() {
	unsigned char charmap[CHARMAP_SIZE][8] = {
		{ 0b00000, 0b01000, 0b10101, 0b00010, 0b01000, 0b10101, 0b00010, 0b00000 },
		{ 0b00000, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b00000 },
		{ 0b00000, 0b01010, 0b10101, 0b11111, 0b11111, 0b10001, 0b01110, 0b00000 },
		{ 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b11111, 0b11111 }
	};

	for (int c = 0; c < CHARMAP_SIZE; ++c) {
		lcd_send_command(CG_RAM_ADDR + (c + 1) * 8);
		for (int r = 0; r < 8; r++) {
			lcd_send_data(charmap[c][r]);
		}
	}
}

static void lcd_init() {
	//LCD initialization
	//step by step (from Gosho) - from DATASHEET

	PORTC = PORTC & 0b11111110;

	lcd_delay(10000);

	PORTC = 0b00110000;				//set D4, D5 port to 1
	lcd_pulse();					//high->low to E port (pulse)
	lcd_delay(1000);

	PORTC = 0b00110000;				//set D4, D5 port to 1
	lcd_pulse();					//high->low to E port (pulse)
	lcd_delay(1000);

	PORTC = 0b00110000;				//set D4, D5 port to 1
	lcd_pulse();					//high->low to E port (pulse)
	lcd_delay(1000);

	PORTC = 0b00100000;				//set D4 to 0, D5 port to 1
	lcd_pulse();					//high->low to E port (pulse)

	lcd_send_command(0x28); // function set: 4 bits interface, 2 display lines, 5x8 font
	lcd_send_command(DISP_OFF); // display off, cursor off, blinking off
	lcd_send_command(CLR_DISP); // clear display
	lcd_send_command(0x06); // entry mode set: cursor increments, display does not shift

	lcd_send_command(DISP_ON);		// Turn ON Display
	lcd_send_command(CLR_DISP);		// Clear Display
	
	lcd_create_custom_chars();
}

static void lcd_send_text(char *str) {
	while (*str)
		lcd_send_data(*str++);
}

static void lcd_send_line1(char *str) {
	lcd_send_command(DD_RAM_ADDR);
	lcd_send_text(str);
}

static void lcd_send_line2(char *str) {
	lcd_send_command(DD_RAM_ADDR2);
	lcd_send_text(str);
}

// GAME ----------------------------------------------------------------------

#define LCD_COLS 					16
#define MAP_ROWS 					12
#define LOGS_PER_ROW 				3
#define GAP_BETWEEN_LOGS 			3
#define NUM_LANE_TYPES 				9
#define QUICK_BONUS_MAX 			50
#define QUICK_BONUS_DECAY_TICKS 	50

typedef struct {
	int start_c, length, direction, speed, tick;
} log_t;

typedef struct { 
	int c, r;
} frog_t;

typedef struct {
    int length, speed;
} lane_type_t;

static log_t logs[MAP_ROWS][LOGS_PER_ROW];
static frog_t frog;
static const lane_type_t lane_types[NUM_LANE_TYPES] = {
	{5, 10},
	{5, 8},
	{5, 6},
	{4, 10},
	{4, 8},
	{4, 6},
	{3, 10},
	{3, 8},
	{3, 6}
};

static int top_view = MAP_ROWS - 2;
static int dir_change = 0;
static int lane_sequence[MAP_ROWS];
static int total_row_length[MAP_ROWS];
static int score;
static int game_ticks;

static inline int clamp_x(int x) {
	// clamp the frog to screen bounds
	
	// left side
	if (x < 0) {
		return 0;
	}
	// right side
	if (x >= LCD_COLS) {
		return LCD_COLS - 1;
	}
	// return the same x number if its in bounds
	return x;
}

static void generate_lane_sequence() {
	// pick a type for each water lane
    for (int i = 1; i < MAP_ROWS - 1; i++) {
        lane_sequence[i] = rnd_gen(NUM_LANE_TYPES);
    }
}

static void generate_lane(int r) {
	// create a lane of water
	
	// get the lane type from the sequence
	const lane_type_t *T = &lane_types[lane_sequence[r]];
	
	// based on previous water lane direction, change the current one's to the opposite
	int lane_dir;
	if (!dir_change) {
		// right flow
		lane_dir = 1;
		dir_change = 1;
	}
	else {
		// left flow
		lane_dir = -1;
		dir_change = 0;
	}
    
    int pos = (lane_dir == 1) ? 0 : LCD_COLS-1;  // initial log starts from the left side if the lane direction is right and vice versa
    
    for (int i = 0; i < LOGS_PER_ROW; i++) {
        logs[r][i].length = T->length;  // from lane type
        logs[r][i].direction = lane_dir;
        logs[r][i].speed = T->speed;  // from lane type
        logs[r][i].tick = 0;
		logs[r][i].start_c = pos;
        
        // following log appears next to previous log with a gap between them
		if (lane_dir == 1) {
			// right flow -> next log placed to the right
            pos += T->length + GAP_BETWEEN_LOGS;
		}
        else {
			// left flow -> next log placed to the left
            pos -= T->length + GAP_BETWEEN_LOGS;
		}
    }
	
	// calculate the total length of lane, this is used for wrapping logs around
	int total_length = 0;
	for (int i = 0; i < LOGS_PER_ROW; i++) {
		total_length += logs[r][i].length;
	}
	total_length += GAP_BETWEEN_LOGS * LOGS_PER_ROW;

	total_row_length[r] = total_length;
}

static void logs_init() {
	// initialize water lanes with logs from top to bottom
	generate_lane_sequence();
	for (int i = 1; i < MAP_ROWS - 1; i++) {
		generate_lane(i);
	}
}

static void logs_update() {
	// update the logs positions
	for (int i = 1; i < MAP_ROWS - 1; i++) {
		for (int j = 0; j < LOGS_PER_ROW; j++) {
			log_t *L = &logs[i][j];
			// if the log's tick is greater than or equal its speed, then move it
			if (++L->tick >= L->speed) {
				L->start_c += L->direction;
				// if the log is moving right and is out of bounds, wrap it around
				if (L->direction == 1 && L->start_c >= LCD_COLS) {
					L->start_c -= total_row_length[i];
				} else if (L->start_c <= -L->length) {
					L->start_c += total_row_length[i];  // log is moving left and is out of bounds, wrap it around
				}
				L->tick = 0;  // reset log tick
			}
		}
	}
}

static log_t* log_under_frog(int r, int c, int *log_moved) {
	// check if frog is standing on a log in row r at column c
	// returns the log the frog stands on, returns 0 when the frog is not on a log
    for (int i = 0; i < LOGS_PER_ROW; i++) {
        log_t *L = &logs[r][i];
		// loop through the tiles of the log
        for (int j = 0; j < L->length; j++) {
            // check if column c is one of the tiles of the log, return the log if true
			if (L->start_c + j == c) {
				*log_moved = (L->tick == 0);  // check if log moved this tick
                return L;
            }
        }
		// edge case: frog standing on the edge of a log is momentarily not on log after the log moved
		// edge here means the leftmost tile if the log is going right and vice versa
		if (L->tick == 0) {
            int edge_x = (L->direction == 1) ? L->start_c - 1 : L->start_c + L->length;
            // return the log and set log_moved to 1 if frog is supposed to be on the edge of the log
			if (edge_x == c) {
                *log_moved = 1;
                return L;
            }
        }
    }
    return 0;
}

static int compute_bonus(int ticks) {
	// compute a game tick based bonus for quick completion
    int decay_steps = ticks / QUICK_BONUS_DECAY_TICKS;
    int bonus = QUICK_BONUS_MAX - decay_steps;
    if (bonus < 0) {
		bonus = 0;
	}
    return bonus;
}

static void lcd_draw() {
	// draw visible lanes
	char buf[2][17];
	for (int row = 0; row < 2; row++) {
		int mapr = top_view + row;
		for (int i = 0; i < LCD_COLS; i++) {
			// top and bottom lanes are ground
			if (mapr == 0 || mapr == MAP_ROWS - 1) {
				buf[row][i] = 4;  // ground char
			}
			// every other lane is a water lane
			else {
				buf[row][i] = 1;  // water char
			}
		}
		// draw logs, we only check water lanes
		if (mapr > 0 && mapr < MAP_ROWS - 1) {
			for (int i = 0; i < LOGS_PER_ROW; i++) {
				log_t *L = &logs[mapr][i];
				for (int j = 0; j < L->length; j++) {
					// draw each tile of the log if they are visible
					if (L->start_c + j >= 0 && L->start_c + j < LCD_COLS) {
						buf[row][L->start_c + j] = 2;  // log char
					}
				}
			}
		}
		// draw frog if it's on this row
		if (frog.r == mapr) {
			buf[row][frog.c] = 3;  // frog char
		}
	}
	
	lcd_send_line1(buf[0]);
	lcd_send_line2(buf[1]);
}

// GAME MAIN LOOP ------------------------------------------------------------

int main() {
	port_init();
	lcd_init();
	rnd_init();
	
	// start screen
	lcd_send_line1("  MiniFrogger   ");
	lcd_send_line2("  Press center  ");
	int start_screen = 1;
	
	// program loop
	while (1) {
		// this is here to prevent double center button press after a win or loss
		if (start_screen) {
			// press center to start
			while (button_pressed() != BUTTON_CENTER) {
				button_unlock();
			}
			start_screen = 0;
		}

		frog.r = MAP_ROWS - 1;  // put the frog to the bottom lane
		frog.c = LCD_COLS / 2;  // put the frog to the middle of the lane
		
		top_view = MAP_ROWS - 2;  // top row shows bottom water lane

		dir_change = rnd_gen(2);  // random initial direction that determines the flow of the first water lane
		logs_init();  // new lane sequence for each game
		
		int game_over = 0;
		int win = 0;
		score = 0;
		game_ticks = 0;

		// game loop
		while (!game_over && !win) {
			logs_update();
		
			// check frog if its in a water lane
			if (frog.r > 0 && frog.r < MAP_ROWS - 1) {
				int log_moved = 0;
				log_t *L = log_under_frog(frog.r, frog.c, &log_moved);  // get the log the frog is on
				if (L) {
					if (log_moved) {
						// we move the frog with the log
						frog.c += L->direction;
						// check bounds, if the frog is outside -> game over
						if (frog.c < 0 || frog.c >= LCD_COLS) {
							game_over = 1; 
							break;
						}
					}
				} else {
					// frog is not on a log, therefore is in the water -> game over
					game_over = 1; 
					break;
				}
			}

			int b = button_pressed();
			if (b != BUTTON_NONE) {
				// move frog to the left if BUTTON_LEFT is pressed, keep it in bounds
				if (b == BUTTON_LEFT) {
					frog.c = clamp_x(frog.c - 1);
				}
				// move frog to the right if BUTTON_RIGHT is pressed, keep it in bounds
				else if (b == BUTTON_RIGHT) {
					frog.c = clamp_x(frog.c + 1);
				}
				// move frog forward if BUTTON_UP is pressed, increase score
				else if (b == BUTTON_UP) {
					if (frog.r > 0) {
						frog.r--;
						score++;
					}
				} 
				// move frog backward if BUTTON_CENTER is pressed, decrease score
				else if (b == BUTTON_CENTER) {
					if (frog.r < MAP_ROWS - 1) {
						frog.r++;
						score--;
					}
				}
			}
			
			// for scrolling view
			if (frog.r == top_view) {
				// if frog moved up
				top_view--;
			} else if (frog.r > top_view + 1) {
				// if frog moved down
				top_view++;
			}
			
			// frog reached top row -> victory
			if (frog.r == 0) {
                win = 1;
                break;
            }
			
			lcd_draw();
			button_unlock();  // to allow frog movement
			game_ticks++;
		}

		char buf[17];
		if (win) {
			// compute win bonus based on game ticks
			int bonus = compute_bonus(game_ticks);
			score += bonus;
			
			// win screen
			lcd_send_line1("    YOU WIN!    ");
			snprintf(buf, sizeof(buf), "  Score:%d +%d  ", score, bonus);
			lcd_send_line2(buf);
		} else if (game_over) {
			// game over screen
			lcd_send_line1("   GAME OVER!   ");
			snprintf(buf, sizeof(buf), "   Score: %d     ", score);
			lcd_send_line2(buf);
		}
		
		// press BUTTON_CENTER for new game
		while (button_pressed() != BUTTON_CENTER) {
			button_unlock();
		}
	}
}
