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

int main(int argc, char *argv[])
{
    FILE *fileptr;
    fileptr = fopen(argv[1], "rb");
    int max_data = 2047;
    long shortslen;
    long blockslen;
    long byteslen;
    int16_t data_block[MAX_BLOCK];
    long ctr = 0;
    int retcode = 0;

    printf("file name: %s\n", argv[1]);
    if (argc > 2)
    {
        max_data = atoi(argv[2]);
    }

    if( fileptr == NULL )
    {
        fprintf(stderr, "Error: unable to open input file %s\n", argv[1]);
        exit(1);
    }

    fseek(fileptr, 0, SEEK_END);
    byteslen = ftell(fileptr);
    shortslen = (byteslen / 2);
    blockslen = shortslen /MAX_BLOCK;
    printf("byteslen %ld, shortslen %ld, blocks %ld\n\n", byteslen, shortslen, blockslen); 
    rewind(fileptr);

    int64_t delta = 0;   
    int error_ctr = 0;
    int64_t  last_location = 0;

    int16_t max_value = 0;
    int16_t min_value = 0;
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

#define THRESHOLD 10000
    
        //validate samples
        for (int i = 0; i < shorts_read; i++)
        {
            if (data_block[i] > THRESHOLD || data_block[i] < -THRESHOLD)
            {
                if (retcode == 0)
                {
                    printf("error index %d, value %d\n", i, data_block[i]);
                }
                retcode = -1;
            }
            if (data_block[i] > max_value)
            {
                max_value = data_block[i];
            }
            if (data_block[i] <  min_value)
            {
                min_value = data_block[i];
            }

        }

        ctr += shorts_read;
    }
    printf("ctr %ld, bytes %ld\n", ctr, ctr * 2);
    printf("max_value %d, min_value %d\n", max_value, min_value);
    if (retcode != 0)
    {
        printf(" Failure!");
    }

    return retcode;
}
