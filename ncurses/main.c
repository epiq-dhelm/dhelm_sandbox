
#include<ncurses.h>
#include<stdlib.h>
#include <unistd.h>
#include <time.h>

//--------------------------------------------------------
// FUNCTION PROTOTYPES
//--------------------------------------------------------
void printing();
void moving_and_sleeping();
void colouring();

//--------------------------------------------------------
// FUNCTION main
//--------------------------------------------------------
int main(void)
{
    initscr();

    // addstr("-----------------\n| codedrome.com |\n| ncurses Demo  |\n-----------------\n\n");
    refresh();

    //printing();

    //moving_and_sleeping();

    //colouring();
    srand(time(NULL));
    int height = 60;
    int length = 150;

    int change;
    //building height array
    int heightarr[length];

    //init first value
    heightarr[0] = rand() % height;

    for(int i = 1; i < length; i++){
        change = (rand() % 3) - 1;
        heightarr[i] = heightarr[i-1] + change;
    }

    nodelay(stdscr, TRUE);
    int x = 0;


    while (x < 40){
        erase();

        //iterating through every column
        for(int i = 0; i < length; i++){
            // move(height, i);

            //setting char at correct height
            // for(int j = 0; j < heightarr[i]; j++){
            //     move(height-j, i);
            //     addch('#');
            // }

            mvaddch(height - heightarr[i], i, '#');

            //change height of column
            change = (rand() % 3) - 1;
            heightarr[i] = heightarr[i] + change;

            refresh();
        }
        usleep(300000);
        x++;
    }


    mvaddstr(height + 1, 0, "\npress any key to exit...");
    // refresh();

    getch();

    endwin();
    

    return EXIT_SUCCESS;
}
