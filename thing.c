#include <stdio.h>
//#include <conio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>

#define _ESC_ \x1b
#define _CSI_ \x9b
#define _CUU_ A
#define _CUD_ B
#define _CUF_ C
#define _CUB_ D

#define N_COLS 4
#define N_ROWS 4

int board[N_COLS ][ N_ROWS];

#define SYM_YEL "\x1b[33m"
#define SYM_GRN "\x1b[32m"
#define SYM_RED "\x1b[31m"
#define SYM_B_YEL "\x1b[93m"
#define SYM_B_GRN "\x1b[92m"
#define SYM_B_RED "\x1b[91m"

#define MAX_SYMBOL 17
#define SYM_LEGEND 0
#define SYM_COLOR 1

#define BORDER_COLOR "\x1b[37m"


typedef enum  { VK_NONE, VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, VK_QUIT } valid_key_t;

int g_score;

char* symbols[MAX_SYMBOL][2] = {
	{"   ", SYM_YEL }, // 2^0
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
	{"err", SYM_B_RED }
};

int move_up(int cells[N_COLS][N_ROWS]);
int move_down(int cells[N_COLS][N_ROWS]);
int move_left(int cells[N_COLS][N_ROWS]);
int move_right(int cells[N_COLS][N_ROWS]);
int getkey(void);
void handle_key_press(void);
void render(int cells[N_COLS][N_ROWS], int width, int height);
void restore_cursor(void);
int no_moves_left(void);
int insert_new_tile(void);
void debug_cell_print(void);


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

	printf("\x1b[%dD", width); // cursor left
	printf("\x1b[%dA", height); // cursor up


}

/************************************************

render(int* cells, int width, int height)

DIsplay the game board

************************************************/

void render(int cells[N_COLS][N_ROWS], int width, int height){

	int row,col,c;

	printf( BORDER_COLOR );
	printf("\nScore %d\n", g_score);

	//top row
	printf( BORDER_COLOR "\u2554\u2550");
	for(col = 0; col < width; col++){ printf("\u2550\u2550\u2550\u2550\u2550"); }
	printf("\u2550\u2557\n");

	for(row = 0; row < height; row++){
		//left wall
		printf( "\u2551 ");

		for(col = 0; col < width; col++){

			// cache ccell value & validate
			c = cells[col][row];
			if(c < 0){ c = 0; }
			if(c > MAX_SYMBOL){ c = MAX_SYMBOL; }

			printf( "%s", symbols[c][SYM_COLOR] );
			printf("%s", c ? "\u250C\u2500\u2500\u2500\u2510" : "     ");

		}

		//right wall / left wall
		printf(BORDER_COLOR " \u2551\n\u2551 ");

		for(col = 0; col < width; col++){

			// cache ccell value & validate
			c = cells[col][row];
			if(c < 0){ c = 0; }
			if(c > MAX_SYMBOL){ c = MAX_SYMBOL; }

			char* n = c ? "\u2502" : " ";
			printf( "%s", symbols[c][SYM_COLOR]);
			printf(	"%s%s%s", n, c ? symbols[c][SYM_LEGEND] : "   ", n);

		}

		//right wall / left wall
		printf(BORDER_COLOR " \u2551\n\u2551 ");

		for(col = 0; col < width; col++){

			// cache cell value & validate
			c = cells[col][row];
			if(c < 0){ c = 0; }
			if(c > MAX_SYMBOL){ c = MAX_SYMBOL; }

			printf( "%s", symbols[c][SYM_COLOR] );
			printf("%s", c ? "\u2514\u2500\u2500\u2500\u2518" : "     ");

		}

		//right wall : end of row
		printf(BORDER_COLOR " \u2551\n");
	}
	//bottom row
	printf("\u255A\u2550");
	for(col = 0; col < width; col++){ printf("\u2550\u2550\u2550\u2550\u2550"); }
	printf("\u2550\u255D");

	restore_cursor();
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
			if(board[col][ row] == board[col+1][row] ){ return 0; }
			// and below
			if(board[col][ row] == board[col][row+1] ){ return 0; }

		}
		//last column test below
		if(board[col][row] == board[col][row+1] ){ return 0; }
	}

	// last row
	for( col = 0; col < N_COLS-1; col++){
		// to the right
		if(board[col][row] == board[col+1][row] ){ return 0; }
	}
	printf("No moves left\n");
	return 1;

}



/***********************************************************************

 insert_new_tile()

 insert a new tile at a random position with probability 90% of a '2' and 10% of a '4'

 Return: the number of available empty spaces remaining

***********************************************************************/

int insert_new_tile(void){

	int empty_cells[16];
	int n_empties;

	int random = ((rand() % 10));
	int new_tile = (random != 0) ? 1 : 2;

	// list empty slots
	n_empties = 0;

	for(int col = 0; col < N_COLS ; col++){
		for(int row = 0; row < N_ROWS; row++){

			if(board[col][row] == 0){
				empty_cells[n_empties] = col + row * N_COLS;
				n_empties++;
			}
		}
	}
	// if list isn't empty, insert tile
	if( n_empties != 0){

		// drop new_tile into random slot
		random = ((rand() % n_empties));

		board[empty_cells[random] % N_COLS][empty_cells[random] / N_COLS] = new_tile;

		//printf("Inserting at %d, %d    (empties:%d)\n", random % 4, random / 4, n_empties);

	}

	// if inserted tile results in a deadlock, the game is also over

	if(n_empties == 1 &&  no_moves_left() ){

		//signal end of game
		printf("Died in no_moves_left\n");
		return 0;
	}
	return n_empties;
}


void debug_cell_print(void){
	for(int r = 0; r < N_ROWS; r++){

		// printf("\n%2d,%2d,%2d,%2d", cells[0][ r],cells[1][ r],cells[2][ r],cells[3][r]);
	}
	// printf("\n");
}

/******************************************************************************************/
/*
	move functions: int move_xxxx(int* cells)
	in : array of game cells. this will be modified if a legal move is made
	out: the number of cells moved. Returning zero indicates that no move was made

	All four move options are reflections in either ascending/descending order or rows vs columns

	Coded as separate functions for speed and laziness
*/



int move_down(int cells[N_COLS][N_ROWS]){

	int n_moved = 0;

	// check all columns in turn
	for( int col = 0; col < N_COLS; col++){

		int smush = 0;//we will only amalgamate cells once per column

		// check one fewer than total rows: last row cannot move
		for(int row = N_ROWS - 2; row >= 0; row--){

			// is cell not empty
			if(cells[col][ row] ){

				// printf("D:Non empty cell at(%d,%d)\n", col, row);debug_cell_print();

				// first check if there are empty cells in direction of motion
				int dest_row = row;
				while( dest_row < (N_ROWS-1) && !cells[col][ dest_row + 1]) {

					dest_row ++;
				}
				if(dest_row != row){
					//move cell
					// printf("D:moving (%d,%d) to (%d,%d)\n",col,row,col,dest_row);
					cells[col][ dest_row] = cells[col][ row];
					cells[col][ row] = 0;
					n_moved++;
				}

				//dest_row = row + 1;
				// if we did not yet smush in this column && there is a row beneath
				if( !smush && dest_row < N_ROWS - 1 ){
					// printf("D:smush test (%d,%d),(%d,%d)\n",col,dest_row,col,dest_row+1);

					if(cells[col][ dest_row] == cells[col][ dest_row + 1] ){
						// printf("D:smush to(%d,%d)\n", col, dest_row+1);
						smush++;
						cells[col][ dest_row + 1] ++; //increase by power of 2
						cells[col][ dest_row] = 0;
						n_moved++;
						g_score += 2 << cells[col][ dest_row + 1] ;
					}
				}

			}
		}

	}
	return n_moved;
}


int move_up(int cells[N_COLS][N_ROWS]){

	int n_moved = 0;

	// check all columns in turn
	for( int col = 0; col < N_COLS; col++){

		int smush = 0;//we will only amalgamate cells once per column

		// check one fewer than total rows: last row cannot move
		for(int row = 1; row < N_ROWS; row++){

			// is cell not empty
			if(cells[col][ row] ){

				// printf("U:Non empty cell at(%d,%d)\n", col, row);debug_cell_print();

				// first check if there are empty cells in direction of motion
				int dest_row = row;
				while( dest_row > 0 && !cells[col][ dest_row - 1]) {

					dest_row --;
				}
				if(dest_row != row){
					//move cell
					// printf("u:moving (%d,%d) to (%d,%d)\n",col,row,col,dest_row);
					cells[col][ dest_row] = cells[col][ row];
					cells[col][ row] = 0;
					n_moved++;
				}

				//dest_row = row + 1;
				// if we did not yet smush in this column && there is a row beneath
				if( !smush && dest_row > 0){
					// printf("U:smush test (%d,%d),(%d,%d)\n",col,dest_row,col,dest_row-1);

					if(cells[col][ dest_row] == cells[col][dest_row - 1] ){
						// printf("U:smush to(%d,%d)\n", col, dest_row-1);
						smush++;
						cells[col][ dest_row - 1] ++; //increase by power of 2
						cells[col][dest_row] = 0;
						n_moved++;
						g_score += 2 << cells[col][ dest_row - 1];
					}
				}

			}
		}

	}
	return n_moved;
}

int move_left(int cells[N_COLS][N_ROWS]){

	int n_moved = 0;

	// check all rows in turn
	for( int row = 0; row < N_ROWS; row++){

		int smush = 0;//we will only amalgamate cells once per row

		// check one fewer than total cols: last col cannot move
		for(int col = 1; col < N_COLS; col++){

			// is cell not empty
			if(cells[col][ row] ){

				// printf("L:Non empty cell at(%d,%d)\n", col, row);debug_cell_print();

				// first check if there are empty cells in direction of motion
				int dest_col = col;
				while( dest_col > 0 && !cells[dest_col - 1][ row]) {

					dest_col --;
				}
				if(dest_col != col){
					//move cell
					// printf("L:moving (%d,%d) to (%d,%d)\n",col,row,dest_col,row);
					cells[dest_col][ row] = cells[col][ row];
					cells[col][ row] = 0;
					n_moved++;
				}


				// if we did not yet smush in this row && there is a cell beneath
				if( !smush && dest_col > 0){
					// printf("L:smush test (%d,%d),(%d,%d)\n",dest_col,row,dest_col-1,row);

					if( cells[dest_col][ row] == cells[dest_col - 1][ row] ){
						// printf("L:smush to(%d,%d)\n", dest_col-1, row);
						smush++;
						cells[dest_col - 1][row] ++; //increase by power of 2
						cells[dest_col][row ] = 0;
						n_moved++;
						g_score += 2 << cells[dest_col - 1][row];
					}
				}

			}
		}

	}
	return n_moved;
}

int move_right(int cells[N_COLS][N_ROWS]){

	int n_moved = 0;

	// check all rows in turn
	for( int row = 0; row < N_ROWS; row++){

		int smush = 0;//we will only amalgamate cells once per row

		// check one fewer than total cols: last col cannot move
		for(int col = N_COLS-2; col >=0; col--){

			// is cell not empty
			if(cells[col][ row] ){

				// printf("R:Non empty cell at(%d,%d)\n", col, row);debug_cell_print();

				// first check if there are empty cells in direction of motion
				int dest_col = col;
				while( dest_col < (N_COLS-1) && !cells[dest_col + 1][ row]) {

					dest_col ++;
				}
				if(dest_col != col){
					//move cell
					// printf("R:moving (%d,%d) to (%d,%d)\n",col,row,dest_col,row);
					cells[dest_col][ row] = cells[col][ row];
					cells[col][ row] = 0;
					n_moved++;
				}


				// if we did not yet smush in this row && there is a cell beneath
				if( !smush && dest_col  < N_COLS - 1){
					// printf("R:smush test (%d,%d),(%d,%d)\n",dest_col, row, dest_col+1, row);

					if(cells[dest_col][ row] == cells[dest_col + 1][ row] ){
						// printf("R:smush to(%d,%d)\n", dest_col+1, row);
						smush++;
						cells[dest_col + 1][ row] ++; //increase by power of 2
						cells[dest_col][ row] = 0;
						n_moved++;
						debug_cell_print();
						g_score += 2 << cells[dest_col + 1][ row];
					}
				}

			}
		}

	}
	return n_moved;
}
/***************************************

handle_key_press()

Accept key presses and wait until a keypress results in a valid move, i.e. when one or more tiles changes position,
or two or more tiles amalgmate into one (or the user quits)


***************************************************/


void handle_key_press(void){
	//printf("KEY\n");

	int key, n_cells_moved = 0;

	valid_key_t valid_key = VK_NONE;

	//
	while( (n_cells_moved == 0) && !valid_key ){

		key = getkey();
		//n_cells_moved = 0;

		switch(key){

			case 0x1B:
			 getkey();
			 getkey();
			 break;

			case 'w':
			case 'W':
			valid_key = VK_UP;
			n_cells_moved = move_up( board );
			break;

			case 'a':
			case 'A':
			n_cells_moved = move_left( board );
			valid_key = VK_LEFT;
			break;

			case 's':
			case 'S':
			valid_key = VK_DOWN;
			n_cells_moved = move_down( board );
			break;

			case 'd':
			case 'D':
			n_cells_moved = move_right( board );
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
			printf("Ignoring -%c-\n", key);
			break;

		}//end switch
		// printf("KEY:%d, moves:%d\n",key, n_cells_moved);
	}//end while
	// printf("KEY exit:%d\n", n_cells_moved);
}

/*************************************************************************************************/



int main(int argc, char** argv){

	//RNG go
	srand( (unsigned)time(NULL));

	// test no_moves_left

	// while(1){

		// int r;

		// for(int c = 0; c < 16; c++){

			// board[c % N_COLS][c / N_COLS] = rand()%13 + 1;
		// }
		// r = rand()%16;

		////cells[r % N_COLS][r / N_COLS] = 0;

		// render( board, N_COLS, N_ROWS );

		// printf("Moves left? : %d\n", no_moves_left() );

		// getkey();
	// }

	// board initially contains 2 populated cells. Here's the first
	insert_new_tile();

	g_score = 0;

	for(;;){	// loop until game ends

		if( !insert_new_tile() ){

			// no remaining empty cells, so we must test if any valid moves remain

			break;
		}
		render( board, N_COLS, N_ROWS );

		//wait for valid key that results in some movement of a tile
		handle_key_press();

	}

	printf("\x1b[%dB", 1 + ( 3 * N_ROWS ) + 1);

	printf("\nGAME OVER!\n");
	//
	// a score would be nice
	//

	return 0;
}
