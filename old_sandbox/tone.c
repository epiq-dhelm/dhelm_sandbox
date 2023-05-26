
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



int main(void)
{

    int16_t word_ptr1[200];
    int16_t word_ptr2[200];
    double tone_offset1 = 2000000;
    double tone_offset2 = 4000000;
    double sample_rate = 20000000;

    uint32_t resolution = 12;
    uint32_t max_amplitude = (uint32_t)(pow(2.0f, resolution -1 ) / 2.0);

    printf("resolution %d, max_amplitude %d\n", resolution, max_amplitude);

    double nu1 = 2 * M_PI * (double)tone_offset1/(double)sample_rate ;
    double nu2 = 2 * M_PI * (double)tone_offset2/(double)sample_rate ;
    double A = max_amplitude / M_SQRT2;

    uint32_t block_size = 50;
    uint32_t sample_ctr = 0;

    /* loop through each sample */
    /* word_ptr is an int16_t array one for I and one for Q per sample */
    for (int j = 0; j < block_size; j+= 1)
    {
        /* insert I */
        word_ptr1[2 * j] = (int16_t)(cos(nu1 * ((1.0 * sample_ctr) )) * A);
        word_ptr2[2 * j] = (int16_t)(cos(nu2 * ((1.0 * sample_ctr) )) * A);

        /* insert Q */
        word_ptr1[2 * j + 1] = (int16_t)(sin(nu1 * ((1.0 * sample_ctr) )) * A);
        word_ptr2[2 * j + 1] = (int16_t)(sin(nu2 * ((1.0 * sample_ctr) )) * A);

        sample_ctr++;
    }


    for (int j = 0; j < block_size; j+= 1)
    {
        printf("1 I %5d    ",word_ptr1[2 * j]);
        printf("Q %5d\t ",word_ptr1[2 * j+1]);
        printf("2 I %5d    ",word_ptr2[2 * j]);
        printf("Q %5d\t ",word_ptr2[2 * j+1]);

        printf("diff I %5d    ", word_ptr2[2 * j] - word_ptr1[2 * j]); 
        printf("Q %5d\n", word_ptr2[2 * j+1] - word_ptr1[2 * j+1]); 
    }
    printf("\n");
}
