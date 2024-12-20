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

                int64_t value_delta =  data_block[i] - expected_short  ;

                int64_t location_delta = location - last_location;

                last_location = location;

                printf("\nFail: Byte # 0x%010lX (%lu), Expected 0x%04X (%u), Actual 0x%04X (%u)\n",
                        location, location, expected_short, expected_short, data_block[i], data_block[i]);

                printf("address delta bytes %ld (0x%08lX), value delta bytes %ld (0x%08lX), samples %ld\n", 
                        location_delta, location_delta, value_delta, value_delta, value_delta / 4);

                for (int j = -5 ; j < 5; j++)
                {
                    printf("0x%08lX , 0x%0X (%d)\n", (location+j), data_block[(i+j)], data_block[(i+j)]); 
                }


                error_ctr++;

                if (error_ctr > 3)
                {
                    exit(-1);
                }
            }
            expected_short = data_block[i] + 1;
            if (expected_short == (max_data + 1))
            {
                expected_short = -(max_data + 1);
            }
        }
        ctr += shorts_read;
    }

    printf("ctr %ld, bytes %ld\n", ctr, ctr * 2);
    printf("Success\n");

}
