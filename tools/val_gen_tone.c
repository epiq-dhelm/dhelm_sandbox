
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

#define MIN_BLOCKS 10

int64_t max_num_blocks = 0;
int64_t max_total_samples = 0;
int32_t max_sample_rate = 0;
uint32_t test = 0;

/* Function to calculate the greatest common divisor (GCD) */
static uint32_t gcd(uint32_t a, uint32_t b) 
{
    while (b != 0) 
    {
        uint32_t temp = b;

        b = a % b;
        a = temp;
    }

    return a;
}

/* Function to calculate the least common multiple (LCM) */
static uint32_t lcm(uint32_t a, uint32_t b) 
{
    return (a / gcd(a, b)) * b;
}

/*****************************************************************************/
/** @brief When generating a tone, we must make sure the blocks end on a sample
 * that doesn't cause a discontinuity.  To do that we must find the LCM of the 
 * wavelength and block_size.
 *
 *  @param[in] frequency    - LO frequency being used  
 *  @param[in] sample_rate  - sample rate being used
 *  @param[in] block_size   - block_size being used
 *
    @return: num_blocks     - required number of blocks to insert the tone samples into 
*/
static uint32_t calculate_blocks(uint32_t frequency, uint32_t sample_rate, uint32_t block_size)
{
    uint32_t total_samples = 0;
    uint32_t num_blocks = 0;


    /* Calculate wavelength in samples */
    uint32_t wavelength = (uint32_t)(sample_rate / frequency);

    /* Calculate LCM of wavelength and block size */
    total_samples = lcm(wavelength, block_size);

    /* Calculate number of blocks based on total number of samples */
    num_blocks = total_samples / block_size;

    if (total_samples > max_total_samples)
    {
        printf("test %d, tone_freq %u, sample_rate %u, " 
                "block_size %u, \nwavelength %u, total_samples %u, num_blocks %u\n",
                test, frequency, sample_rate, block_size, 
                wavelength, total_samples, num_blocks);

        max_num_blocks = num_blocks;
        max_total_samples = total_samples;
        max_sample_rate = sample_rate;
    }

    return num_blocks;
}

uint32_t random_between(uint32_t min, uint32_t max) 
{
    return min + rand() % (max - min + 1);
}

#define MIN_SAMPLE_RATE 100000
#define MAX_SAMPLE_RATE 500000000
#define MIN_TONE_FREQ 100000
#define MAX_TONE_FREQ 20000000
#define MAX_TONE_FREQ_INC 1000000

int main(int argc, char *argv[])
{
    uint32_t block_size_in_words = 0;
    uint32_t block_sizes[] = {2044, 4092, 8188, 16380, 32764, 65532};

//    int size = sizeof(block_sizes) / sizeof(block_sizes[0]);
    int size = 256;
    uint32_t sample_rate_inc = 0;
    uint32_t tone_freq_inc = 0;

    srand(time(NULL));

//    for (int i = 0; i < size; i++)
    for (int i = 4; i < size; i++)
    {
//        block_size_in_words = block_sizes[i];
        block_size_in_words = 256 * i - 4;
        tone_freq_inc = random_between(MIN_TONE_FREQ, MAX_TONE_FREQ_INC);

        for (uint32_t tone_freq_hz = MIN_TONE_FREQ; 
                tone_freq_hz < MAX_TONE_FREQ; 
                tone_freq_hz = tone_freq_hz + tone_freq_inc )
        {
            sample_rate_inc = random_between(MIN_SAMPLE_RATE, MAX_SAMPLE_RATE);
//            printf("test: %d, tone_freq_inc %u, tone_freq_hz: %u, sample_rate_inc: %u, block_size_in_words %u\n", 
 //                   test, tone_freq_inc, tone_freq_hz, sample_rate_inc, block_size_in_words);

            for (uint32_t sample_rate = MIN_SAMPLE_RATE; 
                    sample_rate < MAX_SAMPLE_RATE; 
                    sample_rate = sample_rate + sample_rate_inc)
            {
                if (tone_freq_hz <= (sample_rate / 2))
                {

                    uint32_t num_blocks = calculate_blocks(tone_freq_hz, sample_rate, block_size_in_words);

                    test++;
                }
            }
        }
    }

    printf("tests: %d, max_sample_rate %u, max_num_blocks: %ld, max_total_samples %ld, max_total_bytes %lu\n", 
            test, max_sample_rate, max_num_blocks, max_total_samples, max_total_samples * 4);

}
