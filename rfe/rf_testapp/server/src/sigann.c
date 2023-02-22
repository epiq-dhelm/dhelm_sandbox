/**
 * @file sigann.c
 *
 * 
 * <pre>
 * Copyright 2014-2022 Epiq Solutions, All Rights Reserved
 * </pre>
 */


/***** INCLUDES *****/
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <complex.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
#include <math.h>


#include <fcntl.h>
#include <pthread.h>

#include "sidekiq_api.h"
#include "sigann.h"
#include "nsfft.h"

#include "arg_parser.h"
#include "utils_common.h"


#define FFT_LEN 65536

extern volatile sig_atomic_t g_running;
bool g_rx_running = false;
uint32_t    num_blocks_to_acquire = FFT_LEN / (SKIQ_MAX_RX_BLOCK_SIZE_IN_WORDS - SKIQ_RX_HEADER_SIZE_IN_WORDS) + 1;
int16_t     *data_ptr;
bool        skiq_initialized;
int         logging_num = 0; //gives logging_handler a way to print out multiple lines of logs


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
/** gets the power array
 * 
    @param p_rconfig: the main radio config pointer
    @param p_rx_rconfig: the RX radio config pointer
    @return void
*/
void fft_data(struct radio_config *p_rconfig, struct rx_radio_config *p_rx_rconfig, 
        double *data_freq_array, double *data_power_array) 
{
    double      freq_array[FFT_LEN] = INIT_ARRAY(FFT_LEN, 0);
    double      power_array[FFT_LEN] = INIT_ARRAY(FFT_LEN, 0); 
    int counter = 0;
    float nsfft_in[FFT_LEN * 2];
    float nsfft_out[FFT_LEN * 2];
    int16_t *tmp_ptr = (int16_t *)data_ptr;
    int i;

    log_trace("fft_data");

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
    for(i = 0; i < FFT_LEN; i++)
    {
        freq_array[i] = (p_rx_rconfig->freq - p_rconfig->bandwidth/2.0)  +  
            i * ((p_rconfig->bandwidth)/(double)FFT_LEN);
        /* get into MHz */
        freq_array[i] /= 1000000;
    }


    uint32_t pointsPerFreqBin = FFT_LEN / SWEEPPOINTS;
    double freq_temp = 0;

    log_debug("pointsPerFreqBin %d", pointsPerFreqBin);
    
    /* determine the max power in bins that go into one power bin */
    for(i = 0; i < SWEEPPOINTS; i++)
    {
        /* get max value for each frequency bin */
        double  powermax_tmp = -300;
        for(int j = i * pointsPerFreqBin; j < pointsPerFreqBin * (i + 1); j++)
        {
            if(power_array[j] > powermax_tmp)
            {
                powermax_tmp = power_array[j];
                freq_temp = freq_array[j];
            }
        }

        /* get power to display for each displayed freq bin */
        data_power_array[i] = powermax_tmp;
        data_freq_array[i] = freq_temp;


    }


}
/******************************************************************************/
/** calculates the FFT
 * 
    @param p_rconfig: the main radio config pointer
    @param p_rx_rconfig: the RX radio config pointer
    @return void
*/
void calc_fft(struct radio_config *p_rconfig, struct rx_radio_config *p_rx_rconfig, 
        uint64_t *peak_freq, int32_t *peak_power)
{
    double      freq_array[FFT_LEN] = INIT_ARRAY(FFT_LEN, 0);
    double      power_array[FFT_LEN] = INIT_ARRAY(FFT_LEN, 0); 
    int counter = 0;
    float nsfft_in[FFT_LEN * 2];
    float nsfft_out[FFT_LEN * 2];
    int16_t *tmp_ptr = (int16_t *)data_ptr;
    int i;

    log_trace("calc_fft");

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
    for(i = 0; i < FFT_LEN; i++)
    {


        freq_array[i] = (p_rx_rconfig->freq - p_rconfig->bandwidth/2.0)  +  
            i * ((p_rconfig->bandwidth)/(double)FFT_LEN);
    }

    int32_t tmp_power = -300;
    uint64_t tmp_freq = 0;
    double tmp_fpower = -300;
    double tmp_ffreq = 0;

    /* find the peak */
    for(i = 0; i < FFT_LEN; i++)
    {

        if (power_array[i] > tmp_fpower)
        {
            tmp_power =  (int32_t)power_array[i];
            tmp_fpower =  power_array[i];

            tmp_freq = (uint64_t)freq_array[i];
            tmp_ffreq = freq_array[i];
        }
    }

    log_debug("in calc_fft, freq %" PRIu64 ", power %" PRIi32 "", tmp_freq, tmp_power);
    log_debug("in calc_fft, ffreq %f, power %f", tmp_ffreq, tmp_fpower);

    *peak_power = tmp_power;
    *peak_freq =  tmp_freq;
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

    log_trace("get_data");

    card = p_rconfig->cards[0];
    hdl = p_rx_rconfig->handles[card][0];

    /*
        Tell the receiver to start streaming samples to the host; these samples will be read
        into this program in the loop below
    */ 
    status = skiq_start_rx_streaming(card, hdl );
    if ( status != 0 )
    {
        log_error("Error: failed to starting streaming samples, status %" PRIi32 " ",
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
        log_info("Info: finished with error(s)! status %d ", status);
    }

    /* Tell the receiver to stop streaming sample data */
    tmp_status = skiq_stop_rx_streaming(card, hdl);
    if ( tmp_status != 0 )
    {
        log_warn("Warning: failed to stop streaming (status = %" PRIi32 "); continuing... ",
            tmp_status);
    }
    logging_num = 0;
    return tmp_status;
}


int32_t peakSearch(                             uint8_t card,
                                                struct radio_config *p_rconfig,
                                                struct rx_radio_config *p_rx_rconfig ,
                                                uint64_t center_freq,
                                                uint32_t span,
                                                uint64_t *peak_freq,
                                                int32_t *peak_power)
{
    int status = 0;

    log_trace("in peakSearch");

    /* if the server is not running, exit */
    if (g_running == 0)
    {
       return -1; 
    }

    /* configure card with correct frequency and span */
    span = span * 1000000;
    p_rconfig->bandwidth = span ;

    /* make the sample rate 20% larger than the span */
    p_rconfig->sample_rate = span + (span * 0.2);

    status = configure_radio(p_rconfig->cards[0], p_rconfig);
    if (status != 0) 
    {
        log_error("Error: Failed radio configure, card %" 
                PRIi32 " status %" PRIi32 "  ", card, status);
        return status;
    }

    p_rx_rconfig->freq = (center_freq * 1000000) ;
    p_rx_rconfig->gain = 10;
    p_rx_rconfig->gain_manual = true;


    status = configure_rx_radio(p_rconfig->cards[0], p_rconfig, p_rx_rconfig);
    if (status != 0) 
    {
        log_error("Error: Failed rx_radio configure, card %" 
                PRIi32 " status %" PRIi32 "  ", card, status);
        return status;
    }

    dump_rconfig( p_rconfig, p_rx_rconfig, NULL);

    g_rx_running = true;

    /* allocate space for IQ data */
    data_ptr = malloc(FFT_LEN * 2 * sizeof(int16_t));
    if (data_ptr == NULL)
    {
        log_error("Error: didn't successfully allocate %" PRIu64 " bytes to hold"
                 " unpacked iq ", (uint64_t)(FFT_LEN * 2 * sizeof(int16_t)));
         return -1;
    }

    memset(data_ptr, 0, FFT_LEN * 2 * sizeof(int16_t));

    /* get the data from the radio */
    status = get_data(p_rconfig, p_rx_rconfig);
    if (status != 0)
    {
        return status;
    }


    *peak_power = -300;

    /* calculate the fft from the data */
    calc_fft(p_rconfig, p_rx_rconfig, peak_freq, peak_power);
    log_debug("in peakSearch, peak_freq %" PRIu64 ", peakpower %" PRIi32 "", *peak_freq, *peak_power);

    
return 0;
}

int32_t getData(                                uint8_t card,
                                                struct radio_config *p_rconfig,
                                                struct rx_radio_config *p_rx_rconfig ,
                                                uint64_t center_freq,
                                                uint32_t span,
                                                double *freq_array,
                                                double *power_array)
{
    int status = 0;

    log_trace("in getData ");

    /* if the server is not running, exit */
    if (g_running == 0)
    {
       return -1; 
    }

    /* if nothing has changed then don't reconfigure */
    if (p_rconfig->bandwidth/ 1000000 != span || p_rx_rconfig->freq / 1000000 != center_freq)
    { 
        /* configure card with correct frequency and span */
        span = span * 1000000;
        p_rconfig->bandwidth = span ;

        /* make the sample rate 20% larger than the span */
        p_rconfig->sample_rate = span + (span * 0.2);

        status = configure_radio(p_rconfig->cards[0], p_rconfig);
        if (status != 0) 
        {
            log_error("Error: Failed radio configure, card %" 
                    PRIi32 " status %" PRIi32 "  ", card, status);
            return status;
        }

        p_rx_rconfig->freq = (center_freq * 1000000) ;
        p_rx_rconfig->gain = 10;
        p_rx_rconfig->gain_manual = true;


        status = configure_rx_radio(p_rconfig->cards[0], p_rconfig, p_rx_rconfig);
        if (status != 0) 
        {
            log_error("Error: Failed rx_radio configure, card %" 
                    PRIi32 " status %" PRIi32 "  ", card, status);
            return status;
        }
    }

    g_rx_running = true;

    /* allocate space for IQ data */
    data_ptr = malloc(FFT_LEN * 2 * sizeof(int16_t));
    if (data_ptr == NULL)
    {
        log_error("Error: didn't successfully allocate %" PRIu64 " bytes to hold"
                 " unpacked iq ", (uint64_t)(FFT_LEN * 2 * sizeof(int16_t)));
         return -1;
    }

    memset(data_ptr, 0, FFT_LEN * 2 * sizeof(int16_t));

    /* get the data from the radio */
    status = get_data(p_rconfig, p_rx_rconfig);
    if (status != 0)
    {
        return status;
    }

    /* calculate the fft from the data */
    fft_data(p_rconfig, p_rx_rconfig, freq_array, power_array);
    
return 0;
}
