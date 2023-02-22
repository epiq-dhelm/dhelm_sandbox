/**
 * @file siggen.c
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
#include "siggen.h"
#include "arg_parser.h"
#include "utils_common.h"

#define DEFAULT_BLOCK_SIZE   65532

#define TOTAL_TX_BLOCKS      50
#define DEFAULT_SAMPLE_RATE  20000000
#define DEFAULT_TONE         1000
#define NUM_LOOP_DOT         5
#define MAX_POWER_LEVELS     10



/* Parameters passed to threads
*/
struct tone_thread_params
{
  pthread_t                   tone_thread; // thread responsible for transmitting data
  struct radio_config*  p_rconfig;
  uint8_t                     card; 
  volatile bool               init_complete;  // flag indicating that the thread has completed init
  uint32_t                    tone;
  uint32_t                    sample_rate;
  skiq_tx_hdl_t               hdl;
};

#define TONE_THREAD_PARAMS_INITIALIZER                           \
{                                                           \
  .tone_thread                    = 0,                      \
  .p_rconfig                      = NULL,                   \
  .card                           = UINT8_MAX,              \
  .init_complete                  = false,                  \
  .tone                           = DEFAULT_TONE,           \
  .sample_rate                    = DEFAULT_SAMPLE_RATE,    \
  .hdl                            = skiq_tx_hdl_A1          \
}                                                           \


struct sweep_thread_params
{
    pthread_t                   sweep_thread; // thread responsible for transmitting data
    struct radio_config*        p_rconfig;
    struct tx_radio_config*     p_tx_rconfig;
    uint8_t                     card; 
    volatile bool               init_complete;  // flag indicating that the thread has completed init
    uint32_t start_freq_MHz;
    uint32_t power_level;
    uint32_t steps;
    uint32_t freq_step_MHz;
    uint32_t step_time_ms;
    uint32_t span_MHz;
};

#define SWEEP_THREAD_PARAMS_INITIALIZER                     \
{                                                           \
  .sweep_thread                   = 0,                      \
  .p_rconfig                      = NULL,                   \
  .p_tx_rconfig                   = NULL,                   \
  .card                           = UINT8_MAX,              \
  .init_complete                  = false,                  \
  .start_freq_MHz                 = 0,                      \
  .power_level                    = 0,                      \
  .steps                          = 0,                      \
  .freq_step_MHz                  = 0,                      \
  .step_time_ms                   = 0,                      \
  .span_MHz                       = 0                       \
}                                                           \

/***** GLOBAL DATA *****/
extern volatile sig_atomic_t g_running;
bool g_tone_thread_running = false;  
bool g_sweep_thread_running = false;  
struct tone_thread_params g_tone_thread_parameters = TONE_THREAD_PARAMS_INITIALIZER;
struct sweep_thread_params g_sweep_thread_parameters = SWEEP_THREAD_PARAMS_INITIALIZER;

//  static pthread_cond_t           g_sync_start = PTHREAD_COND_INITIALIZER;  // used to sync threads
//  static pthread_mutex_t          g_sync_lock = PTHREAD_MUTEX_INITIALIZER; // used to sync thread
bool tx_streaming = false;
skiq_tx_block_t **p_tx_blocks = NULL; /* reference to an array of transmit block references */
uint32_t block_size = DEFAULT_BLOCK_SIZE;
uint32_t max_amplitude = 0;
uint16_t attenuation_max = 0;
uint16_t attenuation_min = 0;

/* mutex to protect updates to the tx buffer */
pthread_mutex_t tx_buf_mutex = PTHREAD_MUTEX_INITIALIZER;
// mutex and condition variable to signal when the tx queue may have room available
pthread_mutex_t space_avail_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t space_avail_cond = PTHREAD_COND_INITIALIZER;
uint32_t complete_count=0;



/*****************************************************************************/
/** This is the callback function for once the data has completed being sent.
    There is no guarantee that the complete callback will be in the order that
    the data was sent, this function just increments the completion count and
    signals the main thread that there is space available to send more packets.

    @param status status of the transmit packet completed
    @param p_block reference to the completed transmit block
    @param p_user reference to the user data
    @return void
*/
void tx_complete( int32_t status, skiq_tx_block_t *p_data, void *p_user )
{
    if( status != 0 && status != -2)
    {
        fprintf(stderr, "Error: packet %" PRIu32 " failed with status %d\n",
                complete_count, status);
    }

    // increment the packet completed count
    complete_count++;

    // signal to the other thread that there may be space available now that a
    // packet send has completed
    pthread_mutex_lock( &space_avail_mutex );
    pthread_cond_signal(&space_avail_cond);
    pthread_mutex_unlock( &space_avail_mutex );

}


static int32_t get_card_params(int card, skiq_tx_hdl_t hdl, uint32_t *max_amplitude, const char **type)
{
    skiq_param_t param;
    int32_t status = 0;
    uint8_t resolution;

    log_trace("in get_card_params ");

    status = skiq_read_parameters(card, &param);
    if (status != 0)
    {
        /* let the calling function report to the user. */
        return status;
    }

    resolution = param.tx_param[hdl].iq_resolution;
    attenuation_max = param.tx_param[hdl].atten_quarter_db_max;
    attenuation_min = param.tx_param[hdl].atten_quarter_db_min;


    *max_amplitude = (uint32_t)(pow(2.0f, resolution -1 ) / 2.0);

    *type =  skiq_part_string(param.card_param.part_type);

    return status;

}

/*****************************************************************************/
/** This function will initialize the TX buffer pool with I/Q values of the
 ** predefined tone

    @param none
    @return: status
*/
static int32_t init_tx_buffer(uint32_t tone_offset, uint32_t sample_rate)
{
    int32_t status = 0;
    uint32_t i = 0;
    uint32_t j = 0;
    int16_t * word_ptr;
    uint32_t tot_blocks;
    uint32_t sample_ctr = 0;

    log_trace ("init_tx_buffer");
    log_debug("offset %d, sample_rate %d, max_amplitude %d ", tone_offset, sample_rate, max_amplitude);

    tot_blocks = TOTAL_TX_BLOCKS;



    // allocate the buffer of block pointers
    p_tx_blocks = calloc( tot_blocks, sizeof( skiq_tx_block_t* ) );
    if ( p_tx_blocks == NULL )
    {
        log_error( "Error: unable to allocate %u bytes to hold transmit"
                " block descriptors ",
                (uint32_t)(tot_blocks * sizeof( skiq_tx_block_t* )));
        status = -1;
        goto finished;
    }
  
    /* define nu and amplitude for the tone */ 
    float nu = 2 * M_PI * (float)tone_offset/(float)sample_rate ;
    float A = max_amplitude / M_SQRT2;

    /* insert the tone into the I/Q */
    for (i = 0; i < tot_blocks; i++)
    {
        /* allocate a transmit block by number of samples */
        p_tx_blocks[i] = skiq_tx_block_allocate( block_size );

        if ( p_tx_blocks[i] == NULL )
        {
            log_error( "Error: unable to allocate transmit block data ");
            status = -2;
            goto finished;
        }

        word_ptr = p_tx_blocks[i]->data;

        /* loop through each sample */
        /* word_ptr is an int16_t array one for I and one for Q per sample */ 
        for (j = 0; j < block_size; j+= 1)
        {
            /* insert I */
            word_ptr[2 * j] = (int16_t)(cos(nu * ((1.0 * sample_ctr) )) * A);

            /* insert Q */
            word_ptr[2 * j + 1] = (int16_t)(sin(nu * ((1.0 * sample_ctr) )) * A);
            sample_ctr++;
        }
    }

finished:
    if (status != 0)
    {
        if (NULL != p_tx_blocks)
        {
            /* i is the index of the last block allocated */
            for (j = 0; j < i; j++)
            {
                skiq_tx_block_free(p_tx_blocks[j]);
            }

            free(p_tx_blocks);
            p_tx_blocks = NULL;
        }
    }

    return status;
}
static void *tx_tone(void *params)
{
    int status = 0 ;
    uint32_t curr_block=0;
    int32_t tmp_status=0;
    uint32_t errors=0;
    uint32_t tot_errors=0;
    uint32_t j = 0;
    bool tx_streaming = false;
    uint32_t num_blocks = TOTAL_TX_BLOCKS;
    uint64_t xmit_ctr = 0;


    struct tone_thread_params *p_tone_thread_params = params;

    log_trace("in tx_tone");


    // initialize the transmit buffer
    status = init_tx_buffer(p_tone_thread_params->tone, p_tone_thread_params->sample_rate);
    if (status != 0)
    {
        goto cleanup;
    }

    // enable the Tx streaming
    status = skiq_start_tx_streaming(p_tone_thread_params->card, p_tone_thread_params->hdl);
    if ( status != 0 )
    {
        log_error( "Error: unable to start streaming (result code %"
                PRIi32 ") ", status);
        goto cleanup;
    }

    tx_streaming = true;

    log_debug("Info: successfully started streaming ");

    uint64_t loop_ctr = 0;
    while (g_tone_thread_running)
    {
        if (loop_ctr % 50 == 0)
        {
            log_info("Transmitting Tone...");
        }
        loop_ctr++;

        // transmit a block at a time
        while( (curr_block < num_blocks) && (g_tone_thread_running==true) )
        {
            // transmit the data
            status = skiq_transmit(p_tone_thread_params->card, p_tone_thread_params->hdl, 
                    p_tx_blocks[curr_block], NULL );
            if( status == SKIQ_TX_ASYNC_SEND_QUEUE_FULL )
            {

                // if there's no space left to send, wait until there should be space available
                if( g_tone_thread_running == true )
                {
                    pthread_mutex_lock( &space_avail_mutex );
                    pthread_cond_wait( &space_avail_cond, &space_avail_mutex );
                        pthread_mutex_unlock( &space_avail_mutex );
                }
            }
            else if ( status != 0 )
            {
                log_error( "Error: failed to transmit data (result code %" PRIi32 ") ", status);
                goto cleanup;
            }
            curr_block++;
        }

        // see how many underruns we had 
        status = skiq_read_tx_num_underruns(p_tone_thread_params->card, 
                p_tone_thread_params->hdl, &errors);
        if (status != 0)
        {
            log_error( "Error: failed to receive underruns (result code %" PRIi32 ") ", status);
            goto cleanup;
        }

        // errors returned are the number since last starting of transmit, so only report
        // when it has changed
        if (tot_errors != errors)
        {
            log_debug(" Info: total number of tx underruns is %u ", errors);

            /* the FPGA does not clear this so every call will return the last */
            tot_errors = errors;
        }
        
        /* give some visible indication to the user that we are transmitting */
        if ( xmit_ctr % NUM_LOOP_DOT == 0)
        {
            //log_debug(". ");
        }

        curr_block = 0;
        xmit_ctr++;
    }

cleanup:
    // disable streaming, cleanup and shutdown
    if (tx_streaming)
    {
        // disable streaming, cleanup and shutdown
        tmp_status = skiq_stop_tx_streaming(p_tone_thread_params->card, p_tone_thread_params->hdl);
        if (tmp_status != 0)
        {
            log_error( "Warning: failed to stop tx streaming (result code %" PRIi32 ") ", tmp_status);
        }

        log_debug("Info: shutting down after %" PRIu64 " transmit loops total underruns %" PRIu32 " ", 
                xmit_ctr, tot_errors);
        tx_streaming = false;
    }

    if (NULL != p_tx_blocks)
    {

        for (j = 0; j < num_blocks; j++)
        {
            skiq_tx_block_free(p_tx_blocks[j]);
        }


        free(p_tx_blocks);

        p_tx_blocks = NULL;
    }


    return (void *)(intptr_t)status;
}


int32_t startCW(                                uint8_t card,
                                                struct radio_config *p_rconfig,
                                                struct tx_radio_config *p_tx_rconfig ,
                                                uint64_t freq_MHz,
                                                uint32_t span_MHz,
                                                uint32_t power_level)
{
    int status = 0;
    const char * card_type = "none";
    uint32_t tone_offset = 1000000;
    skiq_tx_hdl_t hdl = skiq_tx_hdl_A1;
    uint32_t span;

    log_trace("in startCW");

    /* if the server is not running, exit */
    if (g_running == 0)
    {
       return -1; 
    }

    /* if the tone thread is already running stop it*/
    if (g_tone_thread_running != 0)
    {
        /* get everything to stop */
        intptr_t thread_status;

        g_tone_thread_running = false;

        /* wait till threads done */
        pthread_join(g_tone_thread_parameters.tone_thread, (void *)&thread_status); 
    }

    /* configure card with correct frequency and bandwidth */
    span = span_MHz * 1000000;
    p_rconfig->bandwidth = span ;

    /* make the sample rate 20% larger than the span */
    p_rconfig->sample_rate = span + (span * 0.2);

    /* get the card into I/Q order mode */
    p_rconfig->sample_order_iq = skiq_iq_order_iq;

    status = configure_radio(p_rconfig->cards[0], p_rconfig);
    if (status != 0) 
    {
        log_error( "Error: Failed radio configure, card %" 
                PRIi32 " status %" PRIi32 "  ", card, status);
        return status;
    }

    /* determine how many bits of resolution the card type has */
    /* that determines the maximum amplitude of the signal */
    /* this must be done after skiq_init and before initializing the buffers */
    status = get_card_params(p_rconfig->cards[0], hdl, &max_amplitude, &card_type);
    if (status != 0)
    {
        log_error( "Error: unable to access card parameters (status % " 
                PRIi32 " ", status); 
        return status;
    }
    /* it would be good to not transmit on the center frequency, so lets drop the 
     * center frequency to below the desired tone by "tone_offset" amount.
     * Then generate a tone of tone_offset amount */
    p_tx_rconfig->freq = (freq_MHz * 1000000) - tone_offset;
    p_tx_rconfig->block_size_in_words = block_size;
    log_debug("freq %ld, span %d, tone offset %d ", p_tx_rconfig->freq, span, tone_offset);

    /* convert power to attenuation */
    int diff = attenuation_max -attenuation_min;

    /* there are 10 different power levels allowed 
     * So divide the range of the power into 10 levels
     * Set the attenuation based on the power level sent in */
    int att_per_level = diff/MAX_POWER_LEVELS;

    log_debug("att max %d, att min %d, diff %d, dB range %d, att_per_level %d ", 
            attenuation_max, attenuation_min, diff, diff / 4, att_per_level);

    /* 0 is the maximum attenuation, so the quietest */
    p_tx_rconfig->attenuation = attenuation_max + (-att_per_level * power_level);
   

    status = configure_tx_radio(p_rconfig->cards[0], p_rconfig, p_tx_rconfig);
    if (status != 0) 
    {
        log_error( "Error: Failed tx_radio configure, card %" 
                PRIi32 " status %" PRIi32 "  ", card, status);
        return status;
    }

    dump_rconfig( p_rconfig, NULL, p_tx_rconfig);

    if (p_tx_rconfig->num_threads > 1)
    {
        // register the callback
        log_trace("skiq_register_tx_complete_callback");
        status = skiq_register_tx_complete_callback( card, &tx_complete );
        if( status != 0 )
        {
            log_error("Error: unable to register transmit completion callback"
                    " (status = %" PRIi32 ")", status);
            return status;
        }
    }


    g_tone_thread_parameters.card = card;
    g_tone_thread_parameters.tone = tone_offset ;
    g_tone_thread_parameters.sample_rate = p_rconfig->sample_rate;
    g_tone_thread_parameters.hdl = hdl;

    /* start the tx_tone thread */
    status = pthread_create( &(g_tone_thread_parameters.tone_thread), 
              NULL, tx_tone, &g_tone_thread_parameters);
    if( status != 0 )
    {
        g_tone_thread_running = false; // Tell all the threads to terminate
    }

    g_tone_thread_running = true;

    return 0;
}

int32_t stopGen(                                uint8_t card,
                                                struct radio_config *p_rconfig,
                                                struct tx_radio_config *p_tx_rconfig)
{
    bool tone_thread = false;
    bool sweep_thread = false;
    int ret = 0;

    log_trace("in stopGen ");

    intptr_t tone_status;
    intptr_t sweep_status;

    if (g_tone_thread_running)
    {
        g_tone_thread_running = false;
        tone_thread = true;
    }
    if (g_sweep_thread_running)
    {
        g_sweep_thread_running = false;
        sweep_thread = true;
    }

    /* wait till threads done */
    if (tone_thread == true)
    {
        ret = pthread_join(g_tone_thread_parameters.tone_thread, (void *)&tone_status); 
        if (ret != 0) 
        {
            log_error("tone pthread_join failed with ret %d", ret);
            return ret;
        }
        if (tone_status != 0 )
        {
            log_error("tone_status failed with status %ld", tone_status);
            return tone_status;
        }

    }

    if (sweep_thread == true)
    {
        ret = pthread_join(g_sweep_thread_parameters.sweep_thread, (void *)&sweep_status); 
        if (ret != 0) 
        {
            log_error("sweep pthread_join failed with ret %d", ret);
            return ret;
        }
        if (tone_status != 0 )
        {
            log_error("sweep_status failed with status %ld", tone_status);
            return tone_status;
        }
    }

    return 0;
}

static void* tx_sweep( void *params)
{
    int status = 0;
    struct sweep_thread_params *p_sweep_thread_params = params;
    uint64_t step_time_usec = p_sweep_thread_params->step_time_ms * 1000;
    uint32_t stop_freq_MHz = (p_sweep_thread_params->steps * p_sweep_thread_params->freq_step_MHz) + p_sweep_thread_params->start_freq_MHz; 
    uint32_t curr_freq_MHz = p_sweep_thread_params->start_freq_MHz;

    log_trace("sweepThread");


    log_debug("stop_freq_MHz %d, g_sweep_thread_running %d", stop_freq_MHz, g_sweep_thread_running);
    while (g_sweep_thread_running == true)
    {
        log_info("Running Sweep: curr_freq_MHz %d", curr_freq_MHz);

        status = startCW(p_sweep_thread_params->card, p_sweep_thread_params->p_rconfig, 
                p_sweep_thread_params->p_tx_rconfig, curr_freq_MHz,
                p_sweep_thread_params->span_MHz, p_sweep_thread_params->power_level);
        if (status != 0)
        {
            goto done;
        }

        log_debug("usleep time %ld ", step_time_usec);
        usleep(step_time_usec);

        curr_freq_MHz += p_sweep_thread_params->freq_step_MHz; 

        if (curr_freq_MHz > stop_freq_MHz)
        {
           curr_freq_MHz = p_sweep_thread_params->start_freq_MHz;
        }
 

    }

done:

    return (void *)(intptr_t)status;
}


int32_t startSweep(                             uint8_t card,
                                                struct radio_config *p_rconfig,
                                                struct tx_radio_config *p_tx_rconfig ,
                                                uint32_t start_freq_MHz,
                                                uint32_t power_level,
                                                uint32_t steps,
                                                uint32_t freq_step_MHz,
                                                uint32_t step_time_ms,
                                                uint32_t span_MHz)
{
    log_trace("startSweep");
    int status = 0;

    log_debug("start_freq %d, power_level %d, steps %d, frq_step_MHz %d, step_time_ms %d, span %d", 
            start_freq_MHz, power_level, steps, freq_step_MHz, step_time_ms, span_MHz); 


    /* if the server is not running, exit */
    if (g_running == 0)
    {
       return -1; 
    }

    if (g_sweep_thread_running != 0)
    {
        /* get everything to stop */
        intptr_t thread_status;

        g_sweep_thread_running = false;

        /* wait till threads done */
        pthread_join(g_sweep_thread_parameters.sweep_thread, (void *)&thread_status); 
    }

    g_sweep_thread_parameters.card = card;
    g_sweep_thread_parameters.p_rconfig = p_rconfig;
    g_sweep_thread_parameters.p_tx_rconfig = p_tx_rconfig;
    g_sweep_thread_parameters.start_freq_MHz = start_freq_MHz;
    g_sweep_thread_parameters.power_level = power_level;
    g_sweep_thread_parameters.steps = steps;
    g_sweep_thread_parameters.freq_step_MHz = freq_step_MHz;
    g_sweep_thread_parameters.step_time_ms = step_time_ms;
    g_sweep_thread_parameters.span_MHz = span_MHz;

    /* start the tx_sweep thread */
    status = pthread_create( &(g_sweep_thread_parameters.sweep_thread), 
              NULL, tx_sweep, &g_sweep_thread_parameters);
    if( status != 0 )
    {
        g_sweep_thread_running = false; 
    }

    g_sweep_thread_running = true;
   
    
    return status;
}

