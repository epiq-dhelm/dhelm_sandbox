/**
 * @file plot_spectrum.c
 *
 * This is a spectrum analyzer for the receive of the configured card.
 * It works in a normal bash window by using ncurses
 *
 * @brief
 *
 * <pre>
 * Copyright 2014-2022 Epiq Solutions, All Rights Reserved
 * </pre>
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <complex.h>
#include <sidekiq_api.h>
#include <inttypes.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <arg_parser.h>
#include <curses.h>
#include "nsfft.h"
#include "utils_common.h"

#define FFT_LEN 32768
#define TOP_BUFFER 0        // top buffer (space) in window
#define BOTTOM_BUFFER 4     // bottom buffer (space) in window
#define HORZ_BUFFER 3       // left and right buffer (space) in window
#define START_FREQ_BUFFER 6 // How far over to start frequency (x-axis) ticks
#define MAX_POWER_BINS 60
#define LOCAL_DEFAULT_SAMPLE_RATE 20000000
#define MAX_STR 200

/***** GLOBAL DATA *****/

/* running is written to true here and only here.
   Setting 'running' to false will cause the threads to close and the
   application to terminate.
*/
volatile sig_atomic_t g_running = 1;
int         signal_num = 0;
uint32_t    num_blocks_to_acquire = FFT_LEN / (SKIQ_MAX_RX_BLOCK_SIZE_IN_WORDS - SKIQ_RX_HEADER_SIZE_IN_WORDS) + 1;
int16_t     *data_ptr;
bool        skiq_initialized;
double      freq_array[FFT_LEN] = INIT_ARRAY(FFT_LEN, 0);
double      power_array[FFT_LEN] = INIT_ARRAY(FFT_LEN, 0); 
int         logging_num = 0; //gives logging_handler a way to print out multiple lines of logs

//needed for handling window resizing
int         numFreqBins = 0;
int         numPowerBins = 0;

/* Insert the description of your app here, in short and long form */
static const char* p_help_short = "- Plot the received signal in the terminal";
char   help_inc_defaults[MAX_LONG_STRING];

/* The text for the defaults will be added by a common function later */
static const char* p_help_long = "\
This plots the received signal inside the terminal using ncurses.\n\
\n\
\n\
Defaults:\n\
";


/****************************** APPLICATION **********************************/
/*****************************************************************************/

/*****************************************************************************/
/** This is the cleanup handler to ensure that the app properly exits and
    does the needed cleanup if it ends unexpectedly.

    @param[in] signum: the signal number that occurred

    @return void
*/
void app_cleanup(int signum)
{

    signal_num = signum;
    g_running = false;

}

/****************************** APPLICATION **********************************/
/*****************************************************************************/

void swap(double *v1, double *v2)
{
    double tmp = *v1;
    *v1 = *v2;
    *v2 = tmp;
}

/******************************************************************************/
/** This takes the result of the fft and splits it so the lo_freq is the 
 *  center frequency
 *
 *  The new array is placed back into the passed in array
 *
 *  @param data : The array that will be split
 *  @param count: how many items are in the array
 *
 *  @return : void
 */
void fftshift(double *data, int count)
{
    int k = 0;
    int c = (int) floor((float)count/2);
    // For odd and for even numbers of element use different algorithm
    if (count % 2 == 0)
    {
        for (k = 0; k < c; k++)
            swap(&data[k], &data[k+c]);
    }
    else
    {
        double tmp = data[0];
        for (k = 0; k < c; k++)
        {
            data[k] = data[c + k + 1];
            data[c + k + 1] = data[k + 1];
        }
        data[c] = tmp;
    }
}


/******************************************************************************/
/** This takes data in and plots it using ncurses.
 *
 *  Normally there are more FFT bins than FreqBins based upon the size of the 
 *  window.  So this needs to resize the display based upon the window size.
 *
 *  If DEBUG is set, there are two more lines of text at the bottom to help
 *  with debugging

    @param p_rconfig: the main radio config pointer
    @param p_rx_rconfig: the RX radio config pointer

    @return void
*/


void plot_data(struct radio_config *p_rconfig, struct rx_radio_config *p_rx_rconfig)
{
    int pointsPerFreqBin;
    double powermax_tmp;
    double maxPower[1000];
    char str[MAX_STR];
    double power_min = -120;
    double power_max = 10;
    uint32_t freq_max = 0;
    uint32_t freq_min = 0;
    uint32_t freq_ticks = 0;

    /* determine the span of the displayed plot */
    freq_max = p_rx_rconfig->freq + (p_rconfig->sample_rate / 2);
    freq_min = p_rx_rconfig->freq - (p_rconfig->sample_rate / 2);

    /* calculate the number of freq and power bins based upon the window size 
     * use the various BUFFER numbers to put space around the plot */
    numFreqBins = COLS - (HORZ_BUFFER * 2) - START_FREQ_BUFFER;
    numPowerBins = LINES - (TOP_BUFFER + BOTTOM_BUFFER);

     /* There are always more FFT bins (points) than displayed bins.
     * Determine how many FFT bins (points) need to be in each displayed bin */
    pointsPerFreqBin = FFT_LEN / numFreqBins;

    int max_power = -300;
    int max_index = 0;
    int i,j;

    /* Determine the max value in the FFT to get the power number for that displayed freq bin */
    for(i = 0; i < numFreqBins; i++)
    {
        /* get max value for each frequency bin */
        powermax_tmp = -300;
        for(j = i * pointsPerFreqBin; j < pointsPerFreqBin * (i + 1); j++)
        {
            if(power_array[j] > powermax_tmp)
            {
                powermax_tmp = power_array[j];
            }
        }
   
        /* get power to display for each displayed freq bin */ 
        maxPower[i] = powermax_tmp;

        /* Get the overall max_power and index for the whole plot */
        if (powermax_tmp > max_power)
        {
            max_power = powermax_tmp;
            max_index = i;
        }
    }

    /* Add  the user_interface menu to the screen */
    snprintf(str, MAX_STR, "press [f], [b], [r], or [g] to change values...");
    mvaddstr(TOP_BUFFER , 10, str);  // mvaddstr(y, x)

    /* display the current config in MHz */
    snprintf(str, MAX_STR, "[f] Frequency:\t%.3f MHz", (double)p_rx_rconfig->freq/1000000);
    mvaddstr(TOP_BUFFER + 1, 10, str);
    snprintf(str, MAX_STR, "[b] Bandwidth:\t%.3f MHz", (double)p_rconfig->bandwidth/1000000);
    mvaddstr(TOP_BUFFER + 2, 10, str);
    snprintf(str, MAX_STR, "[r] Sample Rate:\t%.3f MHz", (double)p_rconfig->sample_rate/1000000);
    mvaddstr(TOP_BUFFER + 3, 10, str);
    snprintf(str, MAX_STR, "ctrl+c to exit");
    mvaddstr(TOP_BUFFER + 0, COLS-15, str);
    if(p_rx_rconfig->gain_manual == true)
    {
        snprintf(str, MAX_STR, "[g] Gain:\t\t%" PRIu8 , p_rx_rconfig->gain);
    }
    else
    {
        snprintf(str, MAX_STR, "[g] Gain:\t\t(auto)");
    }
    mvaddstr(4, 10, str);

    /* add the axis titles */
    /* its hard to determine where to put the power title so it doesn't interfere with the axis ticks */ 
    mvaddstr(16, 0, "Power");       
    mvaddstr(17, 0, "[dBfs]");
    mvaddstr(LINES-1, (numFreqBins/2 + 5), "Frequency [GHz]");

#define POWER_DIV 3     // how many columns between each power tick 
#define POWER_X 4       // x axis location of the start of the power tick text

    /* add each power axis tick 
     * We need to divide the actual range into even tick values */
    for(i = 0; i < numPowerBins / POWER_DIV; i ++)
    {
        snprintf(str, MAX_STR, "%.0f", power_max - (power_max - power_min)/numPowerBins * POWER_DIV * i);
        mvaddstr(i * POWER_DIV, POWER_X, str);
    }

    /* display the frequency axis numbers */
#define FREQ_DIV  8         // how many bins between displayed tick
#define FREQ_X    9         // how far on the x axis to start the display
#define FREQ_Y    LINES - 3 // How far up (y axis) to display the tick

    /* determine how many Hz are in each displayed bin */
    freq_ticks = (freq_max - freq_min) / numFreqBins;

    /* add each freq axis ticks */
    for(i = 0; i < (numFreqBins / FREQ_DIV) + 1; i ++)
    {
        /* calculate the frequency to display based upon the number of bins displayed */
        double freq = ((double)(freq_min + (freq_ticks * FREQ_DIV * i))/1000000000);

        /* get a four digit number rounded up 0.0000 MHz */
        double temp_freq = freq * 10000;
        uint32_t int_freq = round(temp_freq);
        freq = (double) int_freq / 10000;

        /* display the indicator "|" from the number to the bin  */
        mvaddstr(FREQ_Y - 1, FREQ_X + (i * FREQ_DIV), "|");

        /* display the frequency number */
        snprintf(str, MAX_STR, "%.4lf", freq);
        mvaddstr(FREQ_Y, FREQ_X + (i * FREQ_DIV), str);
    }

    /* if we are in debug mode, display a line of debug info */
#ifdef DEBUG
    snprintf(str, MAX_STR, "numFreqBins %d, pointsPerFreqBin %d, umPowerBins %d, freq_ticks %d, " 
            "COLS %d, LINES %d, max_power %d , max_index %d, calc freq %0.4f" ,
            numFreqBins, pointsPerFreqBin, numPowerBins, freq_ticks, COLS, LINES, 
            max_power, max_index, max_freq); 

    mvaddstr(LINES-2, 0, str);
#endif

    /* calculate the maximum frequency based upon how many freq Hz are in a bin */
    double max_freq = (double)(freq_min + (max_index * freq_ticks))/1000000000;
    double freq_to_plot = 0;
    double power_to_plot = 0;

    /* plot the data */
    for(i = 0; i < numFreqBins; i++)
    {
        /* calculate position for data to be located in the terminal */
        freq_to_plot = START_FREQ_BUFFER  + HORZ_BUFFER + i;
        power_to_plot = LINES - BOTTOM_BUFFER - (numPowerBins * ((maxPower[i] - power_min) 
                    / (power_max - power_min)));

        double frac = modf(power_to_plot, &power_to_plot);

        //add smaller characters for more Y axis definition
        char peak = '|';

        if(frac >= 0.85)
        {
            peak = '|';
        }
        if(frac >= 0.7)
        {
            peak = '!';
        }
        else if (frac >= 0.5)
        {
            peak = ':';
        }
        else if (frac >= 0.2)
        {
            peak = '.';
        }
        else{
            peak = ' ';
        }

        mvvline(power_to_plot+1, freq_to_plot, '|', LINES - power_to_plot - BOTTOM_BUFFER - 1);
        mvaddch(power_to_plot, freq_to_plot, peak);

        /* if we are in debug mode display another line of info */
#ifdef DEBUG 
        if (i==max_index)
        {
            snprintf(str, MAX_STR, "power_to_plot %f, freq_to_plot %f height %f, power %f ", 
                    power_to_plot, freq_to_plot, (LINES - power_to_plot - BOTTOM_BUFFER - 1), 
                    maxPower[i]);
            mvaddstr(LINES-1, 0, str);
        }
#endif

    }

    /* the overall peak values */
    freq_to_plot = START_FREQ_BUFFER  + HORZ_BUFFER + max_index;
    power_to_plot = LINES - BOTTOM_BUFFER - 
        (numPowerBins * ((max_power - power_min) / (power_max - power_min)));

    
#define PEAK_LEN 21     // how far left to start the peak line if we need to move it left

    /* normally display 1 COL to the right of the peak line */
    int32_t location = 1;

    /* but if too far right place it PEAK_LEN COL to the left of the peak line */
    if ((freq_to_plot + PEAK_LEN) > numFreqBins)
    {
        location = -PEAK_LEN;
    }

    snprintf(str, MAX_STR, " %6.4f GHz %d dBfs", max_freq, max_power);
    mvaddstr(power_to_plot, freq_to_plot + location, str);
}


/******************************************************************************/
/** Gets data from the card
 * 
    @param p_rconfig: the main radio config pointer
    @param p_rx_rconfig: the RX radio config pointer

    @return status
*/
int32_t get_data(struct radio_config *p_rconfig, struct rx_radio_config *p_rx_rconfig)
{
    int32_t status = 0;
    int32_t tmp_status = 0;
    uint8_t card = 0;
    skiq_rx_hdl_t hdl = skiq_rx_hdl_end;
    skiq_rx_hdl_t rcvd_hdl = skiq_rx_hdl_end;
    uint32_t curr_block = 0;
    uint32_t data_len   = 0;
    skiq_rx_block_t* p_rx_block = NULL;
    int16_t *running_ptr = data_ptr;

    card = p_rconfig->cards[0];
    hdl = p_rx_rconfig->handles[card][0];

    /*
        Tell the receiver to start streaming samples to the host; these samples will be read
        into this program in the loop below
    */ 
    status = skiq_start_rx_streaming(card, hdl );
    if ( status != 0 )
    {
        fprintf(stderr, "Error: failed to starting streaming samples, status %" PRIi32 "\n",
            status);
        return status ;
    }

    /* loop getting blocks */
    while( (curr_block < num_blocks_to_acquire) && (g_running==true) )
    {
        /* Receive a packet of sample data, data_len is in bytes */
        status = skiq_receive(card, &rcvd_hdl, &p_rx_block, &data_len);
        if( status == 0)
        {
            /*
                `skiq_receive()` returns the handle that the sample data was
                received on
                don't do anything unless it was a valid status and the handle
                indicates the data is from the Rx interface we're interested in
            */
            if(rcvd_hdl == hdl)
            {
                /* watch out for bytes, ints or words */
                int16_t *tmp_ptr = (int16_t *) p_rx_block->data;
                uint32_t tmp_len = data_len - SKIQ_RX_HEADER_SIZE_IN_BYTES;

                /* determine how much data (bytes) we already have placed into the buffer */
                uint32_t diff = running_ptr - data_ptr;

                /* see if we have enough space in the buffer to place the new data */
                if((diff + (tmp_len / 2)) > (FFT_LEN * 2))
                {
                    /* we have less than the received amount of space left, only copy till full */
                    tmp_len = (FFT_LEN * 4) - diff * 2;
                }
                
                /* copy the block into our memory */
                memcpy(running_ptr, tmp_ptr, tmp_len);

                /* update the pointer pointing to where we are in the data buffer */
                running_ptr += tmp_len / 2;
               
                /* move on to next block */ 
                curr_block++;
            }
        }
        else if ( status != skiq_rx_status_no_data )
        {
            /*
                Failed to read the sample data - see the values for skiq_rx_status_t
                to understand the failure code.

                `skiq_rx_status_no_data` is the exception as it indicates that the
                card timed out waiting for sample data, which can be completely
                normal when polling the card at slower sample rates.
            */
            continue;
        }
    }

    if(status != 0)
    {
        printf("Info: finished with error(s)! status %d\n", status);
    }

    /* Tell the receiver to stop streaming sample data */
    tmp_status = skiq_stop_rx_streaming(card, hdl);
    if ( tmp_status != 0 )
    {
        printf("Warning: failed to stop streaming (status = %" PRIi32 "); continuing...\n",
            tmp_status);
    }
    logging_num = 0;
    return tmp_status;
}


/******************************************************************************/
/** calculates the FFT
 * 
    @param p_rconfig: the main radio config pointer
    @param p_rx_rconfig: the RX radio config pointer
    @return void
*/
void calc_fft(struct radio_config *p_rconfig, struct rx_radio_config *p_rx_rconfig)
{
    int counter = 0;
    float nsfft_in[FFT_LEN * 2];
    float nsfft_out[FFT_LEN * 2];
    int16_t *tmp_ptr = (int16_t *)data_ptr;
    int i;

    /* copy data into a float array for nsftt */
    for (i = 0; i < FFT_LEN; i += 1)
    {
        nsfft_in[2 * i] = (float) tmp_ptr[2*i] / 2047;
        nsfft_in[2* i + 1] = (float) tmp_ptr[2*i + 1] / 2047;
    }

    complex double s1[FFT_LEN];

    /* execute the nsfft */
    Nsfft *nsfft = new_Nsfft(FFT_LEN, false);
    exec_Nsfft(nsfft, nsfft_in, nsfft_out);

    /* convert the fft output to a single index power array */
    counter = 0;
    for (i = 0; i < FFT_LEN; i += 1)
    {
        s1[i] = (I * nsfft_out[counter + 1] + (nsfft_out[counter])) / FFT_LEN;
        power_array[i] = 20*log10(cabs(s1[i]));
        counter += 2;
    }

    /* shift the data to have lo_freq at center */
    fftshift(power_array, FFT_LEN);

    /* release memory reserved for the FFT */
    delete_Nsfft(nsfft);

    //creating frequency axis data
    for(i = 0; i < FFT_LEN; i ++)
    {
        freq_array[i] = (p_rx_rconfig->freq - p_rconfig->bandwidth/2.0)  +  
            i * ((p_rconfig->bandwidth)/(double)FFT_LEN);
    }
}

/******************************************************************************/
/** initializes curses
 * 
    @return void
*/
void init_curses(void)
{
    initscr();
    
    /* so pressing of a character doesn't show up on the screen */
    noecho();  

    /* prevents ncurses from waiting for a getch() */
    nodelay(stdscr, TRUE); 


    clear();
    refresh();
}

/******************************************************************************/
/** gets user input for radio configuration
 *  various sleeps are for letting the user read an output that might appear
 * 
    @param p_rconfig: the main radio config pointer
    @param p_rx_rconfig: the RX radio config pointer

    @return void
*/
void user_input(struct radio_config *p_rconfig, struct rx_radio_config *p_rx_rconfig)
{
    int32_t status = 0;
    uint8_t card = p_rconfig->cards[0]; 
    skiq_rx_hdl_t hdl = p_rx_rconfig->handles[card][0];

    /* get the character the user hit */
    char config_char = getch();

    /* if it is a valid character process it */
    if(config_char == 'f' || config_char == 'b' || config_char == 'r' || config_char == 'g' )
    {
        /* create the config window if a valid key is pressed */
        /* newwin(height of window, width of window, Y value where to place, X value where to place) */
        WINDOW * config_win = newwin(5, 45, LINES/2, COLS/2 - 45/4);

        /* put the new window on top of the old window */
        wrefresh(config_win);

        /* draw a box around the window using the default character */
        box(config_win, 0, 0);

        char input[20] = "";

        /* enable echo so users can see themselves type */
        echo();

        switch(config_char)
        {
            /* handle frequency change */
            case 'f' : ;
                uint64_t tmp_freq = p_rx_rconfig->freq;

                mvwprintw(config_win, 1, 1, "Enter new frequency:");
                mvwprintw(config_win, 2, 1, "(accepts e notation eg. 1e6 = 1,000,000)");

                /* get the user input */
                mvwgetnstr(config_win, 1, 22, input, 20);


                /* determine if they used the exponential format */
                if (strchr(input, 'e') != NULL)
                {
                    tmp_freq = (uint64_t) atof(input);
                }
                else
                {
                    tmp_freq = (uint64_t) atoll(input);
                }

                /* attempt to set the value received */
                status = skiq_write_rx_LO_freq(card, hdl, tmp_freq);
                if(status != 0)
                {
                    status = 0;

                    /* erase contents of box and display error message */
                    werase(config_win);
                    box(config_win, 0, 0);
                    mvwprintw(config_win, 1, 1, "Can't change frequency to %" PRIu64 " Hz", tmp_freq);
                    wrefresh(config_win);

                    /* wait so the user can read it */
                    sleep(2);
                }
                else
                {
                    /* refresh original window */
                    wrefresh(config_win);

                    /* store the new frequency configured */
                    p_rx_rconfig->freq = tmp_freq;
                }
                break;

            case 'b' : ;
                /* change bandwidth */
                uint32_t tmp_band = p_rconfig->bandwidth;


                mvwprintw(config_win, 1, 1, "Enter new bandwidth:");
                mvwprintw(config_win, 2, 1, "(accepts e notation eg. 1e6 = 1,000,000)");

                /* get user input */
                mvwgetnstr(config_win, 1, 22, input, 20);
                if (strchr(input, 'e') != NULL)
                {
                    tmp_band = (uint32_t) atof(input);
                }
                else{
                    tmp_band = (uint32_t) atoi(input);
                }
                
                status = skiq_write_rx_sample_rate_and_bandwidth(card, hdl, p_rconfig->sample_rate, tmp_band);
                if(status != 0)
                {
                    sleep(1);
                    werase(config_win);
                    box(config_win, 0, 0);
                    mvwprintw(config_win, 1, 1, "Can't change bandwidth to %" PRIu32" Hz", tmp_band);
                    wrefresh(config_win);
                    sleep(2);
                }
                else
                {
                    /* refresh original window */
                    wrefresh(config_win);

                    /* update new bandwidth */
                    p_rconfig->bandwidth = tmp_band;
                }
                break;

            case 'r' : ;
                uint32_t tmp_samp = p_rconfig->sample_rate;
                mvwprintw(config_win, 1, 1, "Enter new sample rate:");
                mvwprintw(config_win, 2, 1, "(accepts e notation eg. 1e6 = 1,000,000)");

                /* get user input */
                mvwgetnstr(config_win, 1, 24, input, 20);
                if (strchr(input, 'e') != NULL)
                {
                    tmp_samp = (uint32_t) atof(input);
                }
                else{
                    tmp_samp = (uint32_t) atoi(input);
                }

                /* attempt to write it */
                status = skiq_write_rx_sample_rate_and_bandwidth(card, hdl, tmp_samp, p_rconfig->bandwidth);
                if(status != 0)
                {
                    status = 0;
                    werase(config_win);
                    box(config_win, 0, 0);
                    mvwprintw(config_win, 1, 1, "Can't change sample rate to %" PRIu32" Hz", tmp_samp);
                    wrefresh(config_win);
                    sleep(2);
                }
                else
                {
                    /* refresh original window */
                    wrefresh(config_win);

                    /* update new sample rate */
                    p_rconfig->sample_rate = tmp_samp;
                }
                break;

            case 'g' : ;
                uint8_t tmp_gain = p_rx_rconfig->gain;

                mvwprintw(config_win, 1, 1, "Enter new gain:");
                mvwprintw(config_win, 2, 1, "(accepts 'auto' or range 0-255)");

                /* get user input */
                mvwgetnstr(config_win, 1, 17, input, 20);

                /* determine if they gave us an integer */
                char * res_ptr = input;
                long int int_input = strtol(input, &res_ptr, 10);

                /* check to see if it is not a number */
                if (res_ptr == input)
                {
                    /* it is not a number, so see if it is "auto" */
                    if (strncasecmp(input, "auto", 20) == 0)
                    {
                        status = skiq_write_rx_gain_mode(card, hdl, skiq_rx_gain_auto);
                        if(status != 0)
                        {
                            sleep(1);
                            werase(config_win);
                            box(config_win, 0, 0);
                            mvwprintw(config_win, 1, 1, "can't set gain mode");
                            wrefresh(config_win);
                            sleep(2);
                        }
                        else
                        {
                            /* refresh original window */
                            wrefresh(config_win);

                            /* it automatic so we are done */
                            p_rx_rconfig->gain_manual = false;
                            goto fin;
                        }
                    }
                    else
                    {
                        werase(config_win);
                        box(config_win, 0, 0);
                        mvwprintw(config_win, 1, 1, "invalid value can't set gain");
                        wrefresh(config_win);
                        sleep(2);
                        goto fin;
                    }

                }
                else
                {
                    /* there is a number, it may have text after it */

                    /* determine if the value is valid */
                    if (int_input > 255)
                    {
                        werase(config_win);
                        box(config_win, 0, 0);
                        mvwprintw(config_win, 1, 1, "invalid value can't set gain");
                        wrefresh(config_win);
                        sleep(2);
                        goto fin;
                    }

                    /* its manual they typed in a value */
                    tmp_gain = (uint8_t) int_input;

                    /* attempt to set to manual mode */
                    status = skiq_write_rx_gain_mode(card, hdl, skiq_rx_gain_manual);
                    if(status != 0)
                    {
                        sleep(1);
                        werase(config_win);
                        box(config_win, 0, 0);
                        mvwprintw(config_win, 1, 1, "can't set gain mode");
                        wrefresh(config_win);
                        sleep(2);
                    }
                    else
                    {
                        p_rx_rconfig->gain_manual = true;
                    }
                }
    
                /* attempt to set the gain value */
                status = skiq_write_rx_gain(card, hdl, tmp_gain);
                if(status != 0)
                {
                    werase(config_win);
                    box(config_win, 0, 0);
                    mvwprintw(config_win, 1, 1, "can't set gain");
                    wrefresh(config_win);
                    sleep(2);
                }
                else
                {
                    /* refresh original window */
                    wrefresh(config_win);

                    /* set the new gain value */
                    p_rx_rconfig->gain = tmp_gain;
                }
                break;

        }
fin:
        /* turn echo off again */
        noecho();

        /* get rid of the config window */
        werase(config_win);
        wrefresh(config_win);

        erase();
        refresh();
        logging_num = 0;

    }
}

/******************************************************************************/
/** This is the custom logging handler.  If there were custom handling
    required for logging messages, it should be handled here.

    @param signum: the signal number that occurred
    @return void
*/
void logging_handler( int32_t priority, const char *message )
{
    if(logging_num == 0)
    {
        erase();
    }

    /* print the log to the screen */
    mvwprintw(stdscr, logging_num, 0, "status: %"PRIi32" | %s", priority, message);
    refresh();

    /* sleep for half a second */
    usleep(500 * 1000);

    logging_num++;
}



/*****************************************************************************/
/** This is the main function 

      @param[in] argc:    the # of arguments from the command line
      @param[in] *argv:   a vector of ascii string arguments from the command line

      @return int:        ERROR_COMMAND_LINE
 */

int main( int argc, char *argv[])
{
    struct application_argument args[MAX_ARGS];
    struct cmd_line_args     g_cmd_line_args = COMMAND_LINE_ARGS_INITIALIZER;
    struct cmd_line_selector g_cmd_line_selector;
    uint32_t num_args = 0;
    int32_t status = 0;

    /* There is a separate structure for common radio data, RX, and TX */
    struct radio_config rconfig = RADIO_CONFIG_INITIALIZER;
    struct rx_radio_config rx_rconfig = RX_RADIO_CONFIG_INITIALIZER;
     

    /* always install a handler for proper cleanup */
    signal(SIGINT, app_cleanup);                                                                                                  

    /* arg_select is a bitmask of the common parameters used in this app */  
    g_cmd_line_selector.arg_select =    ARG_CARD_ID             | 
                                        ARG_SERIAL_NUM          |
                                        ARG_SAMPLE_RATE         |
                                        ARG_BANDWIDTH           | 
                                        ARG_RX_HDL              |
                                        ARG_RX_FREQ             |
                                        ARG_REPEAT              |
                                        ARG_RX_GAIN             
                                        ; 

    /* arg_required is a bitmask of the common command line parameters that must be present 
     * or it will error out 
     */
    g_cmd_line_selector.arg_required = ARG_NO_ARG;
      
    /* Initialize all the common command line parameters used in this app */
    initialize_application_args(&g_cmd_line_selector, args, &g_cmd_line_args, &num_args);

    /* start with a default of 20 MHz sample rate and 18 MHZ bandwidth */
    g_cmd_line_args.sample_rate = LOCAL_DEFAULT_SAMPLE_RATE;


    /* add the defaults to the long help string */
    /* any internal application arguments added to args[] above will */
    /* automatically have the default value (what is in the variable) put */
    /* into the default help string */
    initialize_help_string(args, num_args, p_help_long, help_inc_defaults);

    /************************ parse command line ******************************/
    status = arg_parser(argc, argv, p_help_short, help_inc_defaults, args); 
    if( status != 0 )
    {
        perror("Command Line ");
        arg_parser_print_help(argv[0], p_help_short, help_inc_defaults, args);
        status = ERROR_COMMAND_LINE;
        goto exit;
    }

    /* print the arguments you have received and defaults 
     * These may have changed during map_arguments so print it here. */
    print_args(num_args, args);


    /* map command line arguments to radio config */
    status = map_arguments_to_radio_config((struct cmd_line_args *)&g_cmd_line_args, &rconfig);
    if ( status != 0 )
    {
        fprintf(stderr, "Error: Failed map_arg_to_radio, status %" PRIi32 " \n", status);
        goto exit;
    }

    status = map_arguments_to_rx_radio_config((struct cmd_line_args *)&g_cmd_line_args, &rconfig, &rx_rconfig);
    if ( status != 0 )
    {
        fprintf(stderr, "Error: Failed map_arg_to_rx_radio, status %" PRIi32 " \n", status);
        goto exit;
    }

    /* Only can work with one card, so use the card number given (or default) */
    uint8_t card = rconfig.cards[0];

    printf("Info: initializing %" PRIu8 " card(s)...\n", rconfig.num_cards);
    status = init_libsidekiq(skiq_xport_init_level_full, &rconfig);
    if (status != 0) {
        fprintf(stderr, "Error: Failed radio init, status %" PRIi32 " \n", status);
        goto exit;
    }

    printf("Info: configuring %" PRIu8 " card ...\n", card);

    /* configure radio expects the actual card ID, not the index
       This function does not error out if the requested 
       sample rate and bandwidth is not attained.
       The application will need to do that if needed */
    status = configure_radio(card, &rconfig);
    if (status != 0) 
    {
        fprintf(stderr, "Error: Failed radio configure, card %" 
                PRIi32 " status %" PRIi32 " \n", card, status);
        goto exit;
    }

    status = configure_rx_radio(card, &rconfig, &rx_rconfig);
    if (status != 0) 
    {
        fprintf(stderr, "Error: Failed rx_radio configure, card %" 
                PRIi32 " status %" PRIi32 " \n", card, status);
        goto exit;
    }

#ifdef DEBUG
    /* debugging info */ 
    dump_rconfig(&rconfig, &rx_rconfig, NULL);
#endif


    /* allocate space for IQ data */
    data_ptr = malloc(FFT_LEN * 2 * sizeof(int16_t));
    if (data_ptr == NULL)
    {
        printf("Error: didn't successfully allocate %" PRIu64 " bytes to hold"
                 " unpacked iq\n", (uint64_t)(FFT_LEN * 2 * sizeof(int16_t)));
        goto exit;
    }
    memset(data_ptr, 0, FFT_LEN * 2 * sizeof(int16_t));

    logging_num = 0;
    skiq_register_logging(logging_handler);


    /* Initializes the ncurses interface and must be used before any other ncurses function call. */
    init_curses();

    /* Saves the current terminal state. */
    savetty();

    /* Clears the screen completely without setting blanks.  Note: ncurses_clear() 
     * clears the screen without setting blanks, which have the current background rendition. 
     * To clear screen with blanks, use ncurses_erase().
     */
    clear();


    /* makes the cursor not visible */
    curs_set(0);

    uint32_t ctr = 0;
    while(g_running && status == 0)
    {
        /* 100ms delay */
        usleep(100000);

        /* get the data from the radio */
        status = get_data(&rconfig, &rx_rconfig); 

        /* calculate the fft from the data */
        calc_fft(&rconfig, &rx_rconfig);

        /* Fills the terminal screen with blanks.  Created blanks have the current background rendition, 
         * set by ncurses_bkgd().
         */
        erase();

        /* puts all the data on the screen */
        plot_data(&rconfig, &rx_rconfig);

        /* put the data onto the screen */
        refresh();

        /* if the screen has changed size clear screen */
        if (numFreqBins != (COLS - (HORZ_BUFFER * 2) - START_FREQ_BUFFER) ||  
                    (numPowerBins != LINES - (TOP_BUFFER + BOTTOM_BUFFER)))
        {
            erase();
            refresh();
        }

        /* go see if the user hit a key and handle it */
        user_input(&rconfig, &rx_rconfig);

        /* put the new data on the screen */
        refresh();

        ctr++;
        if (ctr == 10 && g_cmd_line_args.repeat_is_present == true)
        {
            /* determine if it is time to stop */
            if( g_cmd_line_args.repeat > 0 )
            {
                g_running = (--g_cmd_line_args.repeat == 0) ? 0 : 1;
            }

            ctr = 0;

        } 

    }

    /* clear the screen */
    erase();
    refresh();

exit:
    /* Disable (release) the card so that another application may use it */
    status = skiq_disable_cards(rconfig.cards, rconfig.num_cards);
    if( status != 0 )
    {
        printf("Warning: failed to disable card(s) (status = %" PRIi32 "); should be resolved"
            " with skiq_exit() call but possible resource leak...\n", status);
    }

    logging_num = 0;
    /* done so exit */
    if (rconfig.skiq_initialized == true) {
        skiq_exit();
    }

    /* reset the terminal settings before we started */
    resetty();

    /* end ncurses */
    endwin();
    return status;
}

