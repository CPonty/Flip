#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#define PROG_NAME "flip"
#define INSTRUCTIONS "Usage: flip load filename\n\
    or flip new dim [playerXtype] [playerOtype]"
/* Byte interval for expanding buffers */
#define BUFFER_INCREMENT 32
/* Option constants for board pathfinding */
#define WALK_VALIDATE 1
#define WALK_REPLACE 2

typedef unsigned char bool;

/* Game board */
typedef struct { 
    char ** s;      /* 2D string-array */
    unsigned int n; /* Side length */
    char * border;  /* Border string */
} boardType;

/* Integer pair */
typedef struct {
    int a, b;
} intPair;

/* Full game state */
typedef struct {
    int passes; /* if last turn was a pass */
    int pTypeO, pTypeX; /* player type: 0..2 */
    int scoreO, scoreX; /* player scores */
    char * filepath;    /* filepath last used */
    char whoseTurn;     /* current player: O,X */
    boardType board;    /* board state */
    boardType validMove;/* positions avilable to current player */
} gameType;

/* (x,y) movement vectors for all 8 paths from a tile */
const int vect[8][2] = { {-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, \
    {0, 1}, {1, -1}, {1,  0}, {1,  1}  };

/* ------------------------------------------------------------------------- */

/* 
    Function prototypes 
*/

/* Library import */
int isdigit (int c);

int main (int argc, char * argv[]);

/* Input & parsing */
void input_turn (gameType * game);
void parse_ini (int argc, char * argv[], gameType * game);
void parse_setup (int argc, char * argv[], gameType * game);
void parse_turn (int argc, char * arg1, char * arg2, gameType * game);

/* High-level gameplay */
void play (gameType * game);
void turn_decision(gameType * game);
void player_try_move (int x, int y, gameType * game);
void ai_turn (int playerType, gameType * game);

/* Boardgame engine */
bool board_walk(int x, int y, int dx, int dy, char tile, \
                boardType * board, int action);
bool move_valid (int x, int y, char tile, boardType * board);
void game_update_valid_moves (gameType * game);
void game_put_tile (int x, int y, gameType * game);

/* Game state, memory management and R/W */
void game_ini (gameType * game);
void game_set_fname (char * fname, gameType *game);
void game_load (char * fname, gameType * game);
void game_save (char * fname, gameType * game);
void game_next_player (gameType * game);
void game_update_scoring (gameType *game);
/**/
void board_ini (boardType * board, unsigned int size);
void board_cleanup (boardType *board);
void board_print (boardType * board);
bool board_missing_char (char c, boardType * board);
void board_free (boardType * board);

/* System messages & exit actions */
void sysMessage (int msgId, gameType *game);

/* String utilities */
bool string_is_numeric (char * s);
int string_find (char * s, char c);
void string_strip_nondigit (char * s);


/* ------------------------------------------------------------------------- */

int main (int argc, char * argv[]) {
    gameType game;
    
    game_ini(&game);
    parse_ini(argc, argv, &game);
    
    return 0;
}


/* ------------------------------------------------------------------------- */

/* Input & parsing */

void input_turn(gameType * game) {
    /*
        Read and split player input into arguments for the parser
    */
    int pos;
    int bufUsed = 0, bufSize = BUFFER_INCREMENT;
    char * buf, * arg1, * arg2, * temp, c;
    
    printf("Player (%c)> ", game->whoseTurn);
    
    /* Read stdin to buffer */
    buf = (char *) malloc(sizeof(char) * bufSize);
    while ( (c = getchar()) != '\n' && !feof(stdin) ) {
        bufUsed++;
        /* Expand memory if needed */
        if (bufSize <= bufUsed) {
            bufSize += BUFFER_INCREMENT;
            temp = (char *) malloc(sizeof(char) * bufSize);
            memcpy(temp, buf, sizeof(char) * bufSize-BUFFER_INCREMENT);
            buf = temp;
			free(temp);
        }
        buf[bufUsed-1] = c;
    }
    buf[bufUsed] = '\0';
    /* Catch EoF-only input */
    if ((bufUsed == 0) && (c != '\n')) {
        sysMessage(10, game);
    }
    
	if (buf[0]=='s') {
		
		/* Save file */
		parse_turn(1, buf, "", game);
		
	} else {
		
		/* Play a turn */
		pos = string_find(buf, ' ');
		if (pos > 0) {
			/* Two arguments */
			arg1 = (char *) malloc( sizeof(char) * (pos+1) );
			arg2 = (char *) malloc( sizeof(char) * (bufUsed-pos) );
			memcpy( arg1, &buf[0], sizeof(char) * pos);
			memcpy( arg2, &buf[pos+1], sizeof(char) * (bufUsed-pos-1));
			arg1[pos] = '\0';
			arg2[strlen(buf)-pos-1] = '\0';
			/* Send to parser and free memory */
			parse_turn(2, arg1, arg2, game);
			free(arg1);
			free(arg2);
		}
	}
	free(buf);
}

void parse_ini (int argc, char * argv[], gameType * game) {
    /* 
     Decide on action to take on startup
     */
    if (argc == 1) {
        /* No parameters */
        sysMessage(1, game);
        
    } else if (!strcmp(argv[1], "new") && (argc > 2) && (argc <= 5)) {
        /* Start new game */
        parse_setup(argc, argv, game);
        
    } else if (!strcmp(argv[1], "load") && (argc == 3)) {
        /* Load a game */
        game_load(argv[2], game);
        play(game);
        
    } else {
        /* Wrong parameters */
        sysMessage(11, game);
    }
}

void parse_setup (int argc, char * argv[], gameType * game) {
    /*
     Process arguments from main() for starting a game
     */
    int a, b = 0, c = 0, i;
	
	/* Strip non-integer characters after the last argument */
	if (argc == 4) {
		string_strip_nondigit(argv[3]);
	} else if (argc == 5) {
		string_strip_nondigit(argv[4]);
	}
	/* Check arguments are positive integers */
    for (i = 2; i < argc; i++) {
        if (!string_is_numeric(argv[i])) {
            if (i == 2) {
                sysMessage(5, game);
            } else {
                sysMessage(6, game);
            }
        }
    } 
    /* get parameters */
	if (argc >= 4) {
		b = atoi(argv[3]);
		if (argc == 5) {
			c = atoi(argv[4]);
		}
	}
	a = atoi(argv[2]);
    if (a <= 3) {
        /* Boardsize is invalid */
        sysMessage(5, game);
        
    } else if ( (b > 2) || (c > 2) || (b < 0) || (c < 0) ) {
        /* Player selection is invalid */
        sysMessage(6, game);
        
    } else {
        /* Valid game start condition! */
        board_ini(&game->board, a);
        board_ini(&game->validMove, a);
        game->pTypeX = b;
        game->pTypeO = c;
        play(game);
    }
}

void parse_turn (int argc, char * arg1, char * arg2, gameType * game) {
    /*
     Decide on an action to take with player input: 
     make a move, save a file or exit
     */
    int a, b;
    char * fname;
    
    /* Save */
    if (argc == 1) {
        
        /* Check filename given */
        /*if (strlen(arg1) == 1) {
            sysMessage(9, game);
        }*/
        
        /* Strip out first character for filename & save */
        fname = (char *) malloc(strlen(arg1));
        fname = strncpy(fname, &arg1[1], strlen(arg1) - 1);
        fname[strlen(arg1)] = '\0';
        game_save(fname, game);
        free(fname);
    }
    /* Place piece */
    else if (argc == 2) {
        
        /* Check positive integers given */
		string_strip_nondigit(arg2);
        if (!( string_is_numeric(arg1) && string_is_numeric(arg2) )) {
            return;
        }
        /* Check integers did not overflow */
        a = atoi(arg1);
        b = atoi(arg2);
        if ((a == INT_MAX) || (b == INT_MAX) || (a < 0) || (b < 0)) {
            return;
        }
        
        /* Perform the move */
        player_try_move(a, b, game);
    }
}


/* ------------------------------------------------------------------------- */

/* High-level gameplay */

void play (gameType * game) {
    /*
        Gameplay loop
    */
    
    board_print(&game->board);
    while (1) {
        turn_decision(game);
    }
}

void turn_decision (gameType * game) {
    /*
        Make a decision on what to do in a turn, based on game state
    */
    char player;
    
    /* Refresh background information */
    player = game->whoseTurn;
    game_update_valid_moves(game);
    game_update_scoring(game);
    
    /* Board is full: end the game */
    if (board_missing_char('.', &game->board)) {
        sysMessage(2, game);
    }
    
    /* Player has no move options: pass */
    else if (board_missing_char(player, &game->validMove)) {

        printf("%c passes.\n", game->whoseTurn);
        game_next_player(game);
        (game->passes)++;
        
        /* Both players passed: end the game */
        if (game->passes > 1) {
            sysMessage(3, game);
        }
        return;
    }
    
    /* AI player: place a tile */
    else if ((player == 'O') && ((game->pTypeO) != 0)) {
        ai_turn(game->pTypeO, game);
    } else if ((player == 'X') && ((game->pTypeX) != 0)) {
        ai_turn(game->pTypeX, game);
    }
    
    /* Human player: input prompt */
    else {
        input_turn(game);
    }   
}

void player_try_move (int x, int y, gameType * game) {
    /*
        Check if a player's choice is available and execute
    */
    
    /* Check position is within board */
    if ( (x > (game->board).n-1) || (y > (game->board).n-1) ) {
        return;
    }
    
    /* Check position is a valid move */
    if ( (game->validMove).s[x][y] != game->whoseTurn ) {
        return;
    }
        
    /* Place tile, display & prepare for the next player */
    game_put_tile(x, y, game);
    board_print(&game->board);
    game_next_player(game);
    game->passes = 0;
}

void ai_turn (int playerType, gameType * game) {
    /*
        Parse through the valid moves using one of the AI types
     */
    int x, y, dy, size;
    size = (game->validMove).n;
    
    /* Choose an AI search pattern */
    if (playerType == 1) {
        x = 0;
        y = 0;
        dy = 1;
    } else {
        x = size-1;
        y = size-1;
        dy = -1;
    }
    
    /* Find an available position */
    while (1) {
        /* is this position a valid move? */
        if ((game->validMove).s[x][y] == game->whoseTurn) {
            break;
        }
        y += dy;
        /* walked off ends of board - wrap around */
        if ((y < 0)) {
            y = size-1;
            x--;
        } else if (y >= size) {
            y = 0;
            x++;
        }
        /* walked off top/bottom of board - suspend; debug problem */
        if ((x < 0) || (x >= size)) {
            sysMessage(0, game);
        }
    }
    /* Place tile, display & prepare for the next player */
    game_put_tile(x, y, game);
    printf("Player %c moves at %d %d.\n", game->whoseTurn, x, y);
    board_print(&game->board);
    game_next_player(game);
    game->passes = 0;
}


/* ------------------------------------------------------------------------- */

/* Boardgame engine */

bool board_walk(int x, int y, int dx, int dy, char tile, \
                boardType * board, int action) {
    /*
        Combination of actions when moving along path (dx,dy) on the board
            WALK_VALIDATE:  Return if a valid path to another tile exists
            WALK_REPLACE:   Flip the other players' tiles where possible
                             (assumes path already validated)
    */
    int i, j, enemy_pieces = 0;
    
    i = x;
    j = y;  
    /* Walk along board */
    while (1) {
        /* Move along the path! */
        i += dx;
        j += dy;
        /* Exit condition: walked off board*/
        if ((i < 0) || (j < 0) || (i >= board->n) || (j >= board->n)) {
            /*printf("off board\n");*/
            return 0;
        }
        /* Exit condition: walked to empty cell */
        else if ((board->s)[i][j] == '.') {
            /*printf("empty cell\n");*/
            return 0;
        }
        /* Action: found enemy piece */
        else if ((board->s)[i][j] != tile) {
            /*printf("found enemy piece!\n");*/
            enemy_pieces++;
            if (action == WALK_REPLACE) {
                (board->s)[i][j] = tile;
            }
        }
        /* Action: found player piece */
        else if ((board->s)[i][j] == tile) {
            /*printf("found friendly piece\n");*/
            /* only if this is not the first iteration! */
            
            return enemy_pieces;
            /* This works as a true/false expression, returning true if:
                WALK_VALIDATE:  the path has enemy pieces and is valid
                WALK_REPLACE:   replacements have been made */
        }
    }
}

bool move_valid (int x, int y, char tile, boardType * board) {
    /*
        Return whether the position is a valid move for the given tile
    */  
    int i, dx, dy;
    
    /* Exit if position is taken */
    if ((board->s)[x][y] != '.') {
        return 0;
    }
    /* Check for any links with same-player tiles to validate move */
    for (i = 0; i < 8; i++) {
        dx = vect[i][0];
        dy = vect[i][1];
        if (board_walk(x, y, dx, dy, tile, board, WALK_VALIDATE)) {
            return 1;
        }
    }
    /* No linked tiles found */
    return 0;
}

void game_update_valid_moves (gameType * game) {
    /*
        Refresh the array of valid moves for the current player
    */
    int i, j;
    /* wipe old moves */
    board_cleanup(&game->validMove);
    /* loop over all board positions */
    for (i  = 0; i < (game->validMove).n; i++) {
        for (j = 0; j < (game->validMove).n; j++) {
            /* write a character if position is a current valid move */
            if (move_valid(i, j, game->whoseTurn, &game->board)) {
                (game->validMove).s[i][j] = game->whoseTurn;
            }
        }
    }
}

void game_put_tile (int x, int y, gameType * game) {
    /*
        Execute a player's turn & place tiles appropriately
    */
    int i, dx, dy;
    char tile;
    
    /* Place centre tile */
    tile = game->whoseTurn;
    (game->board).s[x][y] = tile;
    /* Replace tiles now bounded by this players' pieces */
    for (i = 0; i < 8; i++) {
        dx = vect[i][0];
        dy = vect[i][1];
        if (board_walk(x, y, dx, dy, tile, &game->board, WALK_VALIDATE)) {
            board_walk(x, y, dx, dy, tile, &game->board, WALK_REPLACE);
        }
    }
}


/* ------------------------------------------------------------------------- */

/* Game state, memory management and R/W */

void game_ini (gameType * game) {
    /* 
        Apply initial game state; board unchanged
    */
    game->scoreO = 0;
    game->scoreX = 0;
    game->pTypeO = -1;
    game->pTypeX = -1;
    game->whoseTurn = 'O';
    game->filepath = (char *) malloc(sizeof(char));
    game->filepath[0] = '\0';
    game->passes = 0;
}

void game_set_fname (char * fname, gameType *game) {
    /* 
        Set the referrable filepath for load, save & sysMessage
    */
    free(game->filepath);
    game->filepath = (char *) malloc((strlen(fname)+1) * sizeof(char));
    game->filepath[strlen(fname)] = '\0';
    strcpy(game->filepath, fname);
}

void game_load (char * fname, gameType * game) {
    /*
        Try to load & apply a gamestate from the file
            Assumes gameplay has not yet started
    */
    FILE * f;
    int i;
    char validator[5] = {'\0'};
    game_set_fname(fname, game);
    
    /* Check file readable */
    if (strlen(fname) == 0) {
        sysMessage(7, game); 
    }
    f = fopen(fname, "r");
    if (f == NULL) {
        sysMessage(7, game);
    }
    /* Simple 'file-is-a-flip-save-file' check */
    fread(validator, 1, sizeof(char) * strlen(PROG_NAME), f);
    if ((strcmp(validator, PROG_NAME))) {
        sysMessage(7, game);
    }
    
    /* Read new data */
    fread(&game->passes, 1, sizeof(int), f);
    fread(&game->pTypeO, 1, sizeof(int), f);
    fread(&game->pTypeX, 1, sizeof(int), f);
    fread(&game->whoseTurn, 1, sizeof(char), f);
    fread(&(game->board).n, 1, sizeof(int), f);
    /* board/validMove state */
    (game->validMove).n = (game->board).n;
    board_ini(&game->board, (game->board).n);
    board_ini(&game->validMove, (game->validMove).n);
    for (i = 0; i < (game->board).n; i++) {
        fread((game->board).s[i], 1, sizeof(char) * (game->board).n, f);
    }
    fclose(f);
}

void game_save (char * fname, gameType * game) {
/*
    Try to write state-dependent parts of '*game' to file
 
    'flip'
    passes
    pType1
    pType2
    whoseTurn
    board.n
    board.s[0]
    ...
    board.s[n-1]
*/
    FILE * f;
    int i;
    char * validator = PROG_NAME;
    game_set_fname(fname, game);
    
    /* Check file writable */
    if (strlen(fname) == 0) {
        sysMessage(9, game);
        return;
    }
    f = fopen(fname, "w");
    if (f == NULL) {
        sysMessage(8, game);
        return;
    }
    /*Write validation code, variables & board array*/
    fwrite(validator, 1, sizeof(char) * strlen(PROG_NAME), f);
    fwrite(&game->passes, 1, sizeof(int), f);
    fwrite(&game->pTypeO, 1, sizeof(int), f);
    fwrite(&game->pTypeX, 1, sizeof(int), f);
    fwrite(&game->whoseTurn, 1, sizeof(char), f);
    fwrite(&(game->board).n, 1, sizeof(int), f);
    for (i = 0; i < (game->board).n; i++) {
        fwrite((game->board).s[i], 1, sizeof(char) * (game->board).n, f);
    }
    fclose(f);
    sysMessage(4, game);
}

void game_next_player (gameType * game) {
    /*
        Swap players
    */
    if (game->whoseTurn == 'X') {
        game->whoseTurn = 'O';
    } else {
        game->whoseTurn = 'X';
    }   
}

void game_update_scoring (gameType * game) {
    /* 
        Read the score into the game struct
    */
    int i, j;
    game->scoreO = 0;
    game->scoreX = 0;
    /* Loop over board */
    for (i = 0; i < (game->board).n; i++) {
        for (j = 0; j < (game->board).n; j++) {
            /* Increment player 1/2 score */
            switch ( (game->board).s[i][j] ) {
                case 'O':
                    game->scoreO++;
                    break;
                case 'X':
                    game->scoreX++;
                    break;
            }
        }
    }
}

void board_ini (boardType * board, unsigned int size) {
    /* 
        Allocate memory for the board & write default values
    */
    int i, midPos;
    board->n = size;
    /* 2d string array */
    board->s = (char **) malloc( (board->n) * sizeof(char *) );
    for (i = 0; i < size; i++) {
         /* allocate/write each line */
        board->s[i] = (char *) malloc( (size+1) * sizeof(char) );
        memset( board->s[i], '.', (size) * sizeof(char) );
        board->s[i][size] = '\0';
    }
    /* fill in border string */
    board->border = (char*) malloc( (size+1) * sizeof(char) );
    memset( board->border, '-', (size) * sizeof(char) );
    board->border[size] = '\0';
    /* put starting positions on the board */
    midPos = (board->n - 1)/2;
    board->s[midPos][midPos] = 'O';
    board->s[midPos+1][midPos] = 'X';
    board->s[midPos][midPos+1] = 'X';
    board->s[midPos+1][midPos+1] = 'O';
    return;
}

void board_cleanup (boardType * board) {
    /*
        Writes '.' characters to the entire board
    */
    int i;
    for (i = 0; i < (board->n); i++) {
        memset( board->s[i], '.', (board->n) * sizeof(char) );
    }
}

void board_print (boardType * board) {
    int i;
    /* 
        Write graphical board representation to stdout
    */
    printf("+%s+\n", board->border);
    for (i = 0; i < (board->n); i++) {
        printf("|%s|\n", board->s[i]);
    }
    printf("+%s+\n", board->border);
}

bool board_missing_char (char c, boardType * board) {
    /*
        Return whether there are no free spots remaining
    */
    int i, j;
    for (i = 0; i < (board->n); i++) {
        for (j = 0; j < (board->n); j++) {
            if ((board->s)[i][j] == c) {
                return 0;
            }
        }
    }
    return 1;
}

void board_free (boardType * board) {
    /* 
     Clear memory used by board 
     */
    int i;
    for (i = 0; i < (board->n); i++) {
        free(board->s[i]);
    }
    free(board->s);
}


/* ------------------------------------------------------------------------- */

/* System messages & exit actions */

void sysMessage (int msgId, gameType * game) {
/*
    Execute a packaged action matching the ID:
        Write a message to stdout and (optionally) exit with return value
 
    0 : Debug exit
    1 : Program is started with 0 params
    2 : Game over (Board is full)
    3 : Game over (Board not full) 
    4 : Game saved
    5 : Board size is not an integer greater than 3
    6 : Player type is not 0 or 1 or 2
    7 : Unable to load from the specified file
    8 : Unable to save game
    9 : s command given with no filename
    10: End of input before end of game
    11: Program is started with other invalid combination of params
         * other constants error
*/
    switch (msgId) {
        case 0:
            printf("Termination for debugging\n");
            exit(-1);
            break;
        case 1:
            printf("%s\n", INSTRUCTIONS);
            exit(1);
            break;
        case 2:
            printf("Game Over - O=%d X=%d.\n", game->scoreO, game->scoreX);
            exit(0);
            break;
        case 3:
            printf("Game Over - O=%d X=%d.\n", game->scoreO, game->scoreX);
            exit(0);
            break;
        case 4:
            printf("Game saved.\n");
            exit(0);
            break;
        case 5:
            printf("Invalid board dimension.\n");
            exit(2);
            break;
        case 6:
            printf("Invalid player type.\n");
            exit(3);
            break;
        case 7:
            printf("Error loading board.\n");
            exit(4);
            break;
        case 8:
            printf("Unable to write to %s.\n", game->filepath);
            break;
        case 9:
            printf("Please give a filename.\n");
            break;
        case 10:
            printf("End of input from Player %c.\n", game->whoseTurn);
            exit(5);
            break;
        case 11:
            printf("%s\n", INSTRUCTIONS);
            exit(1);
            break;
        default:
            printf("ERROR: Program called sysMsg() with invalid params\n");
            exit(-1);
            break;
    }
}


/* ------------------------------------------------------------------------- */

/* String utilities */

bool string_is_numeric (char * s) {
    /*
        Return wether all characters are digits
    */
    int i = 0;
	if (s[0] == '\0') {
		return 0;
	}
    while (s[i] != '\0') {
        if ((s[i]>'9') || (s[i]<'0')) {
            return 0;
        }
        i++;
    }
    return 1;
}

int string_find (char * s, char c) {
	/*
		Return the first position of c in the string, or -1
	*/
	int i;
	for (i = 0; i < strlen(s); i++) {
		if (s[i] == c) {
			return i;
		}
	}
	return -1;
}

void string_strip_nondigit (char * s) {
	/*
		Replace the first nondigit character with a string terminator
	*/
	int i = -1;
	while (++i < strlen(s)) {
		if ((s[i]>'9') || (s[i]<'0')) {
			s[i] = '\0';
			return;
		}
	}
}


/* ------------------------------------------------------------------------- */
