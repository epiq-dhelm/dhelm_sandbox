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

    uint64_t delta = 0;   
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

        int16_t expected_short = data_block[0];

        //validate samples
        for (int i = 0; i < shorts_read; i++)
        {
            if (data_block[i] != expected_short)
            {
                uint64_t location = (ctr + i) * 2;

                delta = location - delta;

                printf("Fail: Byte # 0x%010lX, Expected 0x%04X, Actual 0x%04X, delta %lu (0x%08lX), samples %lu\n", 
                        location, expected_short, data_block[i], delta, delta, delta / 4); 

                exit(1);
            }
            expected_short = data_block[i] + 1;
            if (expected_short == (MAX_DATA + 1))
            {
                expected_short = -(MAX_DATA + 1);
            }
        }
        ctr += shorts_read;
    }

    printf("ctr %ld\n", ctr);
    printf("Success\n");

}
