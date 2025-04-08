#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>

#define _ESC_ \x1b
#define _CSI_ \x9b
#define _CUU_ A
#define _CUD_ B
#define _CUF_ C
#define _CUB_ D

#define N_COLS 4
#define N_ROWS 4

#define ESC "\x1b"

#define SYM_YEL "\x1b[33m"
#define SYM_GRN "\x1b[32m"
#define SYM_RED "\x1b[31m"
#define SYM_B_YEL "\x1b[93m"
#define SYM_B_GRN "\x1b[92m"
#define SYM_B_RED "\x1b[91m"
#define SYM_WHITE "\x1b[37m"

#define MAX_SYMBOL 19
#define SYM_LEGEND 0
#define SYM_COLOR 1

#define BORDER_COLOR SYM_WHITE


typedef enum  { VK_NONE, VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, VK_QUIT } valid_key_t;

struct game{

   int     score;
   uint8_t board[N_COLS ][ N_ROWS];
   bool    won;
   int     max_cell; // player's progress, highest tile reached
   int     width;    // N_COLS
   int     height;   // N_ROWS
   FILE*   logfile;  // fh for logging
   
}game;

// flag used to indicate if cell was previously amalgamated
// also does double duty to indicate tile has changed
// or that tile is newly spawned
// This all in an attempt to make up for lack of animation
#define SMUSHED (1<<7)

char* symbols[MAX_SYMBOL][2] = {
	{"   ", SYM_YEL }, 
	{" 2 ", SYM_YEL },
	{" 4 ", SYM_YEL },
	{" 8 ", SYM_GRN },
	{" 16", SYM_GRN },
	{" 32", SYM_GRN },
	{" 64", SYM_RED },
	{"128", SYM_RED },
	{"256", SYM_RED },
	{"512", SYM_B_YEL },
	{"1K ", SYM_B_YEL },
	{"2K ", SYM_B_YEL },
	{"4K ", SYM_B_GRN },
	{"8K ", SYM_B_GRN },
	{"16K", SYM_B_GRN },
	{"32K", SYM_B_RED },
	{"64K", SYM_B_RED },
	{" \u221e ", SYM_B_RED }, /* infinity symbol: v. unlikely for a human to reach 128K */
	{"err", SYM_B_RED }
};

// flag bit to identify cells to be printed inverse
#define INVERT (1 << 7)

int move_up(void);
int move_down(void);
int move_left(void);
int move_right(void);
int getkey(void);
int handle_key_press(void);
void render();
void restore_cursor(void);
int no_moves_left(void);
int insert_new_tile(void);
void debug_cell_print(void);
void disable_cursor(void);

#define f_out stdout

int ffsprintf(FILE* fh, const char* fmt, ...){

	static char persist_buffer[1024];
	int ret;
	va_list args;
	va_start( args, fmt );
	/*check return!*/vsnprintf(persist_buffer,1024, fmt, args );
	ret = fprintf(fh, "%s", persist_buffer );
	if(NULL != game.logfile){ 
		ret = fprintf(game.logfile, "%s", persist_buffer); //fmt, args );
		fflush(game.logfile);
	}
	va_end( args );
	fflush(fh);
	return ret;
}


/* termios code influenced by
   "Build your own text editor"
   https://viewsourcecode.org/snaptoken/kilo/index.html
*/
struct termios original_termios; // Store original terminal settings

int die(const char* s){

    perror(s);
    exit(1);
}

void enable_raw_mode() {
    struct termios raw;
    if( -1 == tcgetattr(STDIN_FILENO, &original_termios)){ die("tcgetattr"); } // Save original settings
    raw = original_termios; // Copy original settings to modify
    raw.c_iflag &= (BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN );//| ISIG); // keep ctrl+c as an option

    //timeout?
    raw.c_cc[VMIN] = 0; //minimum bytes to read
    raw.c_cc[VTIME] = 1; // tenths of a second to wait for a char

    if(-1 == tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)){ die("tcsetattr raw"); };

	disable_cursor();
}

void disable_raw_mode() {
    
    if(-1 == tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios)){ die("tcsetattr original"); }; // Restore original settings
}

int getkey(void) {
    int character;
    struct termios orig_term_attr;
    struct termios new_term_attr;

    /* set the terminal to raw mode */
    tcgetattr(fileno(stdin), &orig_term_attr);
    memcpy(&new_term_attr, &orig_term_attr, sizeof(struct termios));
    new_term_attr.c_lflag &= (unsigned)~(ECHO|ICANON);
    new_term_attr.c_cc[VTIME] = 0;
    new_term_attr.c_cc[VMIN] = 0;
    tcsetattr(fileno(stdin), TCSANOW, &new_term_attr);

    /* read a character from the stdin stream without blocking */
    /*   returns EOF (-1) if no character is available */
    character = fgetc(stdin);

    /* restore the original terminal attributes */
    tcsetattr(fileno(stdin), TCSANOW, &orig_term_attr);

    return character;
}

void restore_cursor(void){

	int width = 2 + ( 5 * N_COLS ) + 2;
	int height = 1 + ( 3 * N_ROWS ) + 1 +1; // zero backs up 1 line

	ffsprintf(f_out, "\x1b[%dD", width); // cursor left
	ffsprintf(f_out, "\x1b[%dA", height); // cursor up


}

void disable_cursor(void){

	ffsprintf(f_out, "\x1B[?25l");
}

void cursor_to(int x, int y){
	ffsprintf(f_out, ESC "[%d;%dH", x, y); // origin is at 1,1


}

/************************************************

render(void)

DIsplay the game board

************************************************/

void render(void){

	int row,col,c;
	bool inv = false;

	ffsprintf(f_out, ESC "[2J"); // clear screen

	cursor_to(1,1);

	//top row
	ffsprintf(f_out,  BORDER_COLOR "\u2554\u2550");
	for(col = 0; col < game.width; col++){ ffsprintf(f_out, "\u2550\u2550\u2550\u2550\u2550"); }
	ffsprintf(f_out, "\u2550\u2557");
	//score line
	cursor_to(2, 1);
	ffsprintf(f_out,  BORDER_COLOR );
	//ffsprintf(f_out, "\u2551" ESC "[7mScore: %d" ESC "[m", game.score); cursor_to(2, 23);ffsprintf(f_out, " \u2551");
	ffsprintf(f_out, "\u2551 Score: %d", game.score); cursor_to(2, 23);ffsprintf(f_out, " \u2551");
	
	// separator
	cursor_to(3,1);
	ffsprintf(f_out,  BORDER_COLOR "\u2560\u2550");
	for(col = 0; col < game.width; col++){ ffsprintf(f_out, "\u2550\u2550\u2550\u2550\u2550"); }
	ffsprintf(f_out, "\u2550\u2563");
	
	cursor_to(4,1);
	for(row = 0; row < game.height; row++){
		//left wall
		ffsprintf(f_out,  "\u2551 ");

		for(col = 0; col < game.width; col++){

			// cache ccell value & validate
			c = game.board[col][row];
			inv = c & INVERT;
			c &= ~INVERT;
			
			if(c < 0){ c = 0; }
			if(c > MAX_SYMBOL){ c = MAX_SYMBOL; }

			ffsprintf(f_out, "%s", symbols[c][SYM_COLOR] );
			//if(inv){
			//	ffsprintf(f_out, ESC "[7m%s" ESC "[m", c ? "\u250C\u2500\u2500\u2500\u2510" : "     " );
			//}else{
				ffsprintf(f_out, "%s", c ? "\u250C\u2500\u2500\u2500\u2510" : "     " );
			//}

		}

		//right wall / left wall
		ffsprintf(f_out, BORDER_COLOR " \u2551\n\r\u2551 ");

		for(col = 0; col < game.width; col++){

			// cache ccell value & validate
			c = game.board[col][row];
			inv = c & INVERT;
			c &= ~INVERT;
			
			if(c < 0){ c = 0; }
			if(c > MAX_SYMBOL){ c = MAX_SYMBOL; }

			char* n = c ? "\u2502" : " ";
			ffsprintf(f_out,  "%s", symbols[c][SYM_COLOR]);
			// if(inv){
			// 	ffsprintf(f_out, 	ESC "[7m%s%s%s" ESC "[m", n, c ? symbols[c][SYM_LEGEND] : "   ", n);
			// }else{
			// 	ffsprintf(f_out, 	"%s%s%s", n, c ? symbols[c][SYM_LEGEND] : "   ", n);
			// }

			if(inv){
				ffsprintf(f_out, 	"%s" ESC "[7m%s" ESC "[m%s%s", n, c ? symbols[c][SYM_LEGEND] : "   ", symbols[c][SYM_COLOR], n);
			}else{
				ffsprintf(f_out, 	"%s%s%s", n, c ? symbols[c][SYM_LEGEND] : "   ", n);
			}

		}

		//right wall / left wall
		ffsprintf(f_out, BORDER_COLOR " \u2551\n\r\u2551 ");

		for(col = 0; col < game.width; col++){

			// cache cell value & validate
			c = game.board[col][row];
			inv = c & INVERT;
			c &= ~INVERT;
			game.board[col][row] &= ~INVERT;
			if(c < 0){ c = 0; }
			if(c > MAX_SYMBOL){ c = MAX_SYMBOL; }

			ffsprintf(f_out,  "%s" , symbols[c][SYM_COLOR] );
			//if(inv){
			//	ffsprintf(f_out, 	ESC "[7m%s" ESC "[m",  c ? "\u2514\u2500\u2500\u2500\u2518" : "     ");
			//}else{
				ffsprintf(f_out, "%s", c ? "\u2514\u2500\u2500\u2500\u2518" : "     ");
			//}

		}

		//right wall : end of row
		ffsprintf(f_out, BORDER_COLOR " \u2551\n\r");
	}
	//bottom row
	ffsprintf(f_out, BORDER_COLOR "\u255A\u2550");
	for(col = 0; col < game.width; col++){ ffsprintf(f_out, "\u2550\u2550\u2550\u2550\u2550"); }
	ffsprintf(f_out, "\u2550\u255D");

	// for cell in board: uninvert
	// for(col = 0; col < game.width; col++){
	// 	for(row = 0; row < game.height; row++){
	// 		game.board[col][row] &= ~INVERT;
	// 	}
	// }

	fflush(stdout);

	//restore_cursor();
}

/***********************************************************************

 no_moves_left()

 If the game board is full and no adjacent tiles have the same value, there are no legal moves remaining

***********************************************************************/

int no_moves_left(void){

	// for each cell, testing the cell to the right and the cell below
	// will cover all the cells
	// For the rightmost cell in each row, we only need to test the cell beneath
	// for the last row, we do not need to test the cells beneath
	int row,col;
	for( row = 0; row < N_ROWS-1; row++){

		for( col = 0; col < N_COLS-1; col++){
			// to the right
			if( (~SMUSHED & game.board[col][ row]) == (~SMUSHED & game.board[col+1][row])){ return 0; }
			// and below
			if( (~SMUSHED & game.board[col][ row]) == (~SMUSHED & game.board[col][row+1]) ){ return 0; }

		}
		//TODO: BUG HERE? col value implied
		//last column test below
		if((~SMUSHED & game.board[col][row]) == (~SMUSHED & game.board[col][row+1]) ){ return 0; }
	}

	// last row
	for( col = 0; col < N_COLS-1; col++){
		// to the right
		if((~SMUSHED & game.board[col][row]) == (~SMUSHED & game.board[col+1][row]) ){ return 0; }
	}
	//ffsprintf(f_out, "No moves left\n");
	return 1;

}



/***********************************************************************

 insert_new_tile()

 insert a new tile at a random position with probability 90% of a '2' and 10% of a '4'

 Return: the number of available empty spaces remaining

***********************************************************************/

int insert_new_tile(void){

	int empty_cells[N_ROWS*N_COLS];
	int n_empties;

	int random = ((rand() % 10));
	int new_tile = (random != 0) ? 1 : 2;

	// list empty slots
	n_empties = 0;

	for(int col = 0; col < N_COLS ; col++){
		for(int row = 0; row < N_ROWS; row++){

			if(game.board[col][row] == 0){
				empty_cells[n_empties] = col + row * N_COLS;
				n_empties++;
			}
		}
	}
	// if list isn't empty, insert tile
	if( n_empties != 0){

		// drop new_tile into random slot
		random = ((rand() % n_empties));

		game.board[empty_cells[random] % N_COLS][empty_cells[random] / N_COLS] = new_tile | INVERT;

		--n_empties;

		ffsprintf(f_out, "Inserting at %d, %d    (empties:%d)\n", random % 4, random / 4, n_empties);

	}else{ // TODO:  logic?

	// if inserted tile results in a deadlock, the game is also over

	//if(no_moves_left() &&  n_empties == 0){
		if(no_moves_left())
		//signal end of game
		ffsprintf(f_out, "Died in no_moves_left\n");
		return 0;
	}
	return n_empties;
}


void debug_cell_print(void){
	for(int r = 0; r < N_ROWS; r++){

		 ffsprintf(f_out, "\n%2d,%2d,%2d,%2d", game.board[0][ r],game.board[1][ r],game.board[2][ r],game.board[3][r]);
	}
	// ffsprintf(f_out, "\n");
}

/******************************************************************************************/


typedef struct move_result{

    bool     moved;
    uint32_t points;
    
}move_result;



// process a 'vector' of cell values
//
// return: did we move?
//         did the score change? 

int remove_gaps(uint8_t** c, size_t sz){

	int moved = 0;
	for( int i = 1; i < sz; i++){
	   
	   if(!*c[i])continue;
	   int j = i;
	   while(j && !*c[j-1]){
		  *c[j-1] = *c[j];   // move down ...
		  *c[j] = 0;        // from here
		  j--;   
		  moved++;      
	   }
	}
	return moved;
 }

 move_result traverse( uint8_t** c, size_t sz){
 
	move_result  r = {.moved = false, .points = 0};
 
	int moves = remove_gaps(c, sz);
 
	for( int i = 1; i < sz; i++){
 
	   if(!*c[i])continue; // skip blank
 
	   if(*c[i] == *c[i-1]){         //smush  
		  moves++; // a smush is a move       
		  *c[i-1] = *c[i-1] + 1;     // tile value doubles
		  if(*c[i-1] == 11){ game.won = true; /* hate the 'magic number' 2048 = 2^11*/}
		  r.points += (1 << *c[i-1]);
		  *c[i-1] |= SMUSHED;        // flag as new & prevent re-smushing
		  *c[i] = 0;                 // remove smusher
		  moves += remove_gaps(c, sz);
	   }
 
	}
	r.moved = moves ? true : false;
	return r;
 }

int move_down(void){

   uint8_t* pcells[N_ROWS];
   int n_moved = 0;
   
   for( int col = 0; col < N_COLS; col++){
   
      for(int row = 0; row < N_ROWS; row++){

		game.board[col][N_ROWS - row - 1] &= ~SMUSHED;
      
         pcells[row] = &game.board[col][N_ROWS - row - 1];
      }
   
      move_result mr = traverse( (uint8_t**)&pcells, sizeof(pcells)/sizeof(pcells[0]) );
      
      game.score += mr.points;
      n_moved += mr.moved ? 1 : 0;
   }
   return n_moved;
}



int move_up(void){

	uint8_t* pcells[N_ROWS];
	int n_moved = 0;
	
	for( int col = 0; col < N_COLS; col++){
	
	   for(int row = 0; row < N_ROWS; row++){
 
		 game.board[col][row] &= ~SMUSHED;
	   
		  pcells[row] = &game.board[col][row];
	   }	
	   move_result mr = traverse( (uint8_t**)&pcells, sizeof(pcells)/sizeof(pcells[0]) );
	   
	   game.score += mr.points;
	   n_moved += mr.moved ? 1 : 0;
	}
	return n_moved;
 }
 
 int move_left(void){

	uint8_t* pcells[N_ROWS];
	int n_moved = 0;

	for(int row = 0; row < N_ROWS; row++){

		for( int col = 0; col < N_COLS; col++){
	
		 game.board[col][row] &= ~SMUSHED;
	   
		  pcells[col] = &game.board[col][row];
	   }	
	   move_result mr = traverse( (uint8_t**)&pcells, sizeof(pcells)/sizeof(pcells[0]) );
	   
	   game.score += mr.points;
	   n_moved += mr.moved ? 1 : 0;
	}
	return n_moved;
 }

 int move_right(void){

	uint8_t* pcells[N_ROWS];
	int n_moved = 0;

	for(int row = 0; row < N_ROWS; row++){

		for( int col = 0; col < N_COLS; col++){
	
		 game.board[N_COLS - col - 1][row] &= ~SMUSHED;
	   
		  pcells[col] = &game.board[N_COLS - col - 1][row];
	   }	
	   move_result mr = traverse( (uint8_t**)&pcells, sizeof(pcells)/sizeof(pcells[0]) );
	   
	   game.score += mr.points;
	   n_moved += mr.moved ? 1 : 0;
	}
	return n_moved;
 }


char read_key(void){

    int nread;
    char c;
    while(( nread = read(STDIN_FILENO, &c, 1)) != 1){
        if( nread == -1 && errno != EAGAIN) die("read");
    }
	// xlate cursor keys to wasd
    if( c == '\x1B'){
        char seq[2];
        if(read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1B';
        if(read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1B';

        if(seq[0] == '[' ){

            switch(seq[1]){
                case 'A': return 'w'; /* short cct switch */
                case 'B': return 's';
                case 'C': return 'd';
                case 'D': return 'a';
            }
        }
        // malformed ESC seq
        return '\x1B';
    }else{
        //not ESC: default behavior
        return c;
    }
}

/***************************************

handle_key_press()

Accept key presses and wait until a keypress results in a valid move, i.e. when one or more tiles changes position,
or two or more tiles amalgmate into one (or the user quits)


***************************************************/


int handle_key_press(void){

	int key, n_cells_moved = 0;

	valid_key_t valid_key = VK_NONE; 
	
	while( (n_cells_moved == 0) && !valid_key ){

		//key = getkey();
		key = read_key();
		//n_cells_moved = 0;

		switch(key){

			case 0x1B:   /*  \x1B */
			 getkey();   /* [ */
			 getkey();   /* A|B|C|D */
			 ffsprintf(f_out, "Unwanted getkey\n\r");
			 break;

			case 'w':
			case 'W':
			valid_key = VK_UP;
			n_cells_moved = move_up();
			break;

			case 'a':
			case 'A':
			n_cells_moved = move_left();
			valid_key = VK_LEFT;
			break;

			case 's':
			case 'S':
			valid_key = VK_DOWN;
			n_cells_moved = move_down();
			break;

			case 'd':
			case 'D':
			n_cells_moved = move_right();
			valid_key = VK_RIGHT;

			break;

			case 'Q':
			case 'q':
			valid_key = VK_QUIT;
				exit(0);
			break;

			default:
			valid_key = VK_NONE;
			n_cells_moved = 0;
			ffsprintf(f_out, "Ignoring -%c-\n", key);
			break;

		}//end switch
		 ffsprintf(f_out, "KEY:%d, moves:%d\n",key, n_cells_moved);
	}//end while
	// ffsprintf(f_out, "KEY exit:%d\n", n_cells_moved);
	return n_cells_moved;
}

void cleanup_and_exit() {
	//ffsprintf(f_out, ESC "[2J"); // clear screen
    disable_raw_mode();
	restore_cursor();
   	ffsprintf(f_out, "\x1b[999C\x1b[999B");  //move to bottom of screen (implausably large x and y)
   	ffsprintf(f_out, "\nGoodbye!\n");
	if(game.logfile) fclose(game.logfile);

    exit(0); // Terminate the program
}

/*************************************************************************************************/


char* game_over = "\n\r\
  ____    _    __  __ _____  \n\r\
 / ___|  / \\  |  \\/  | ____| \n\r\
| |  _  / _ \\ | |\\/| |  _|   \n\r\
| |_| |/ ___ \\| |  | | |___  \n\r\
 \\____/_/   \\_\\_|  |_|_____| \n\r\
   _____     _______ ____   \n\r\
  / _ \\ \\   / / ____|  _ \\  \n\r\
 | | | \\ \\ / /|  _| | |_) | \n\r\
 | |_| |\\ V / | |___|  _ <  \n\r\
  \\___/  \\_/  |_____|_| \\_\\ \n\r\
";

char* title = "\
  ____   ___  _  _    ___  \n\r\
 |___ \\ / _ \\| || |  ( _ ) \n\r\
   __) | | | | || |_ / _ \\ \n\r\
  / __/| |_| |__   _| (_) |\n\r\
 |_____|\\___/   |_|  \\___/ \n\r\
";

char * you_win = "\
  __   _____  _   _ \n\r\
  \\ \\ / / _ \\| | | |\n\r\
   \\ V / | | | | | |\n\r\
    | || |_| | |_| |\n\r\
    |_| \\___/ \\___/ \n\r\
                  \n\r\
          _____ _   _ \n\r\
\\ \\      / /_ _| \\ | |\n\r\
 \\ \\ /\\ / / | ||  \\| |\n\r\
  \\ V  V /  | || |\\  |\n\r\
   \\_/\\_/  |___|_| \\_|\n\r\
";

void endgame(void){

	ffsprintf(f_out, "%s", you_win);
	ffsprintf(f_out, "Would you like to try for 4096 and beyond?");

	int resp = read_key();
	if(resp == 'N' || resp == 'n'){ 
		ffsprintf(f_out, "Thanks for playing. Goodbye!");
		exit(0); 
	}
}

int play_2048(void){
	
	game.score = 0, game.won = false, game.max_cell = 0, game.width = N_COLS, game.height = N_ROWS;

	bool once = true;
	
	// show splash
	ffsprintf(f_out, "%s\n\nDo you want to play a game?\n", title);
	
	// wait for ANY key
	int resp = read_key();
	if(resp == 'N' || resp == 'n'){ return 0; }


	// board initially contains 2 populated cells. 
	insert_new_tile();
	insert_new_tile();

	while(1){	// loop until game ends

		render();

		if( handle_key_press() > 0){ //got a keypress that results in some movement of a tile

			if(once && game.won){
				once = false;
				endgame();
			}
			
			if(!insert_new_tile()){

				// no remaining empty cells, so we must test if any valid moves remain

				if(no_moves_left()) {
					render();// show final state	
					goto game_over; // exit game loop
				}
			}
		}
		/*else{
			ffsprintf(f_out, "no move possible\n");
		}*/
	}
game_over:
	ffsprintf(f_out, "%s", game_over);

	return 0;
}
void consider_options(int argc, char** argv){ // quik and dirty adding logging option

	FILE* f = fopen( argv[1], "w" ); //( argv[1], O_WRONLY | O_CREAT );
	game.logfile = f;
}
int main(int argc, char** argv){

	if(argc > 1) consider_options(argc, argv);

	//RNG go
	srandom( (unsigned)time(NULL));

	enable_raw_mode();
	atexit(cleanup_and_exit); // Register cleanup function

	play_2048();	

	return 0;
}
