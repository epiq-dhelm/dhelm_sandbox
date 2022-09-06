
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <complex.h>
#include <inttypes.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <strings.h>


int main(void)
{
    char * str[4] = {"100", "alltext", "100text", "text100"};

    for (int i = 0; i < 4; i++)
    {
        char * resptr = str[i];
        char * instr = str[i];


        long int int_input = strtol(instr, &resptr, 10);
        printf("curr string: %s, int_input: %ld, instr %p, resptr %p \n", instr, int_input, instr, resptr);


        if ( instr == resptr)
        {
            printf("starts with text\n");
        }
        else if ( strncasecmp(resptr, "", 2))
        {
            printf("has number then text\n");
        }
        else
        {
            printf("number only %ld, resptr %s,  instr %s \n\n", int_input, resptr, instr);
        }
    }

    
}
