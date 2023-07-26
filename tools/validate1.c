#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>
#include <time.h>


#define MAX_BLOCK  (1018 * 2)
#define RX_RESOLUTION 12
#define MAX_DATA ((1 << (RX_RESOLUTION-1)) - 1)

int main(int argc, char *argv[])
{
    FILE *fileptr;
    fileptr = fopen(argv[1], "rb");
    long shortslen;
    int16_t data_block[MAX_BLOCK];
    long ctr = 0;

    if( fileptr == NULL )
    {
        fprintf(stderr, "Error: unable to open input file %s\n", argv[1]);
        exit(1);
    }

    fseek(fileptr, 0, SEEK_END);
    shortslen = (ftell(fileptr) / 2); 
    rewind(fileptr);
   
    long last_location = 0;
    while (ctr < shortslen)
    {
//        printf("ctr %ld\n", ctr);
        // read a block of samples
        long shorts_read = fread( data_block, sizeof(int16_t), MAX_BLOCK, fileptr );

        int result = ferror(fileptr);
        if ( 0 != result )
        {
            fprintf(stderr,
                "Error: unable to read from file (result code %d)\n",
                result);
            exit(1);
        }


        int16_t expected_short = 1;

        //validate samples
        for (int i = 1; i < shorts_read; i++)
        {
            if (data_block[i] != expected_short)
            {
//                printf("Fail: Byte # %016lX, Expected %04X, Actual %04X\n", ctr*2, expected_short, data_block[i]); 
                long location = (ctr*2) + (i*2);              

                printf("byte # %016lX, counter %ld, delta (bytes) %ld \n", location, ctr + i, location - last_location);
                last_location = location;
            }
        }
        ctr += shorts_read;
    }

    printf("Success\n");

}
