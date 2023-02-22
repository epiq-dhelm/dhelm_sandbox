/**
 * @file app_template.c
 *
 * This is a template for how to use the "utils_common" helper functions
 *
 * @brief
 *
 * <pre>
 * Copyright 2014-2022 Epiq Solutions, All Rights Reserved
 * </pre>
 */

#include <signal.h>
#include <inttypes.h>
#include <unistd.h>

#include "arg_parser.h"
#include "utils_common.h"

/***** GLOBAL DATA *****/

/* running is written to true here and only here.
   Setting 'running' to false will cause the threads to close and the
   application to terminate.
*/
volatile sig_atomic_t g_running = 1;
int signal_num = 0;

/* Insert the description of your app here, in short and long form */
static const char* p_help_short = "- Short form of app description";
char   help_inc_defaults[MAX_LONG_STRING];

/* The text for the defaults will be added by a common function later */
static const char* p_help_long = "\
   Long form of app description\n\
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
    uint32_t internal_arg;
    bool internal_arg_is_present;
    uint32_t num_args = 0;
    int32_t status = 0;
    int i;


    /* always install a handler for proper cleanup */
    signal(SIGINT, app_cleanup);                                                                                                  

    /* arg_select is a bitmask of the common parameters used in this app */  
    g_cmd_line_selector.arg_select =    ARG_CARD_ID             | 
                                        ARG_SERIAL_NUM          |
                                        ARG_SAMPLE_RATE         |
                                        ARG_BANDWIDTH           | 
                                        ARG_SAMPLE_ORDER_QI     |
                                        ARG_PACKED              |
                                        ARG_REPEAT              |
                                        ARG_RX_HDL              |
                                        ARG_RX_RF_PORT          | 
                                        ARG_RX_FREQ             |
                                        ARG_RX_GAIN             |
                                        ARG_RX_PATH             |
                                        ARG_RX_PAYLOAD_SAMPLES  |
                                        ARG_TRIGGER_SRC         |
                                        ARG_PPS_SOURCE          |
                                        ARG_SETTLE_TIME         |
                                        ARG_INCLUDE_META        |
                                        ARG_USE_COUNTER         |
                                        ARG_RX_BLOCKING         |
                                        ARG_DISABLE_DC_CORR     |
                                        ARG_TX_HDL              |
                                        ARG_TX_RF_PORT          |
                                        ARG_TX_FREQ             |
                                        ARG_ATTENUATION         |
                                        ARG_BLOCK_SIZE          |
                                        ARG_TX_FILE_PATH        |
                                        ARG_TIMESTAMP_BASE      |
                                        ARG_TIMESTAMP_VALUE     |
                                        ARG_USE_LATE_TIMESTAMP  |
                                        ARG_THREADS             |
                                        ARG_THREAD_PRIORITY 
                                        ; 

    /* arg_required is a bitmask of the common command line parameters that must be present 
     * or it will error out 
     */
    g_cmd_line_selector.arg_required = ARG_NO_ARG;
      
    /* Initialize all the common command line parameters used in this app
     */  
    initialize_application_args(&g_cmd_line_selector, args, &g_cmd_line_args, &num_args);

    {
        /* if your app needs to change the common command line variable or create new ones, 
         * This is how you do it.
         */

        struct application_argument new_arg;

        new_arg.p_long_flag     = "internal-arg" ;
        new_arg.short_flag      = 'i';
        new_arg.p_info          = "Example of internal arg";
        new_arg.p_label         = "N";
        new_arg.p_var           = &internal_arg; //give the address where you want the parameter
        new_arg.type            = UINT32_VAR_TYPE;

        new_arg.required    = true;
        new_arg.p_is_set    = &internal_arg_is_present;  //give the address where you want the flag

        add_app_specific_args(args, &new_arg, &num_args);

    }

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

    /* There is a separate structure for common radio data, RX, and TX */
    struct radio_config rconfig = RADIO_CONFIG_INITIALIZER;
    struct rx_radio_config rx_rconfig = RX_RADIO_CONFIG_INITIALIZER;
    struct tx_radio_config tx_rconfig = TX_RADIO_CONFIG_INITIALIZER;

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
    status = map_arguments_to_tx_radio_config((struct cmd_line_args *)&g_cmd_line_args, &rconfig, &tx_rconfig);
    if ( status != 0 )
    {
        fprintf(stderr, "Error: Failed map_arg_to_tx_radio, status %" PRIi32 " \n", status);
        goto exit;
    }



    printf("Info: initializing %" PRIu8 " card(s)...\n", rconfig.num_cards);
    status = init_libsidekiq(skiq_xport_init_level_full, &rconfig);
    if (status != 0) {
        fprintf(stderr, "Error: Failed radio init, status %" PRIi32 " \n", status);
        goto exit;
    }


    printf("Info: configuring %" PRIu8 " card(s)...\n", rconfig.num_cards);
    /* perform some initialization for all of the cards */
    for ( i = 0; (i < rconfig.num_cards) && (status == 0); i++ )
    {
        /* configure radio expects the actual card ID, not the index
           This function does not error out if the requested 
           sample rate and bandwidth is not attained.
           The application will need to do that if needed */
        status = configure_radio(rconfig.cards[i],&rconfig);
        if (status != 0) 
        {
            fprintf(stderr, "Error: Failed radio configure, card %" 
                    PRIi32 " status %" PRIi32 " \n", i, status);
            goto exit;
        }
        status = configure_rx_radio(rconfig.cards[i], &rconfig, &rx_rconfig);
        if (status != 0) 
        {
            fprintf(stderr, "Error: Failed rx_radio configure, card %" 
                    PRIi32 " status %" PRIi32 " \n", i, status);
            goto exit;
        }
        status = configure_tx_radio(rconfig.cards[i], &rconfig, &tx_rconfig);
        if (status != 0) 
        {
            fprintf(stderr, "Error: Failed tx_radio configure, card %" 
                    PRIi32 " status %" PRIi32 " \n", i, status);
            goto exit;
        }
    }

    /* debugging info */ 
    dump_rconfig(&rconfig, &rx_rconfig, &tx_rconfig);

    if( status == 0 )
    {
        /* add application here */
       
        for (  i = 0; (i < rconfig.num_cards) && (status == 0); i++ )
        {
            uint8_t card = rconfig.cards[i];
            int j;

            for ( j = 0; j < rx_rconfig.nr_handles[card]; j++)
            {
                skiq_rx_hdl_t hdl = rx_rconfig.handles[card][j];

                status = skiq_start_rx_streaming( card, hdl);
                if ( status == 0 )
                {
                    printf("Info: card %" PRIu8 " starting Rx handle %u \n", 
                              card, (uint8_t)rx_rconfig.handles[card][0]);
                }
                else
                {
                    fprintf(stderr,"Error: card %" PRIu8 " receive streaming failed to start with status code %" PRIi32 "\n",
                          card, status);
                    break;
                }
            }
        }

        sleep(4);

        for (  i = 0; (i < rconfig.num_cards) && (status == 0); i++ )
        {
            uint8_t card = rconfig.cards[i];
            int j;

            for ( j = 0; j < rx_rconfig.nr_handles[card]; j++)
            {
                skiq_rx_hdl_t hdl = rx_rconfig.handles[card][j];

                status = skiq_stop_rx_streaming( card, hdl);
                if ( status == 0 )
                {
                    printf("Info: card %" PRIu8 " stopping %u Rx handle(s) \n", 
                              card, (uint8_t)rx_rconfig.handles[card][0]);
                }
                else
                {
                    fprintf(stderr,"Error: card %" PRIu8 " receive streaming failed to stop with status code %" PRIi32 "\n",
                          card, status);
                    break;
                }
            }
        }
    }

exit:
    /* done so exit */
    if (rconfig.skiq_initialized == true) {
        skiq_exit();
    }

    return status;
}

