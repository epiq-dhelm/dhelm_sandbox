/**
 * @file testapp_server.c
 *
 * @brief
 * This provides a TCP server to make a sidekiq card into a sigannalyzer and a sig generator.
 * It provides basic funtions:
 *      - Start a continuous wave at a specified frequency and power
 *      - Start a sweeping wave over a range of frequency at one power
 *      - Stop all siggen transmissions
 *      - Do a peak search over a span, return power and frequency of highest signal
 *
 *
 * <pre>
 * Copyright 2014-2022 Epiq Solutions, All Rights Reserved
 * </pre>
 */

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <inttypes.h>
#include <pthread.h>


#include "siggen.h"
#include "sigann.h"
#include "arg_parser.h"
#include "utils_common.h"

#define MAX 80
#define IP "127.0.0.1"
#define PORT 10000
#define SA struct sockaddr
#define MAX_CLIENT_THREADS 5

/*
  struct client_thread_params {}
 */
struct client_thread_params
{
  pthread_t         client_thread; // thread responsible for transmitting data
  int               client_index;
  int               client_socket;
  bool              client_completed;
};

#define CLIENT_THREAD_PARAMS_INITIALIZER                     \
{                                                            \
  .client_thread                    = 0,                     \
  .client_index                     = -1,                    \
  .client_socket                    = 0,                     \
  .client_completed                 = true,                     \
}                         

/***** GLOBAL DATA *****/

/* running is written to true here and only here.
   Setting 'running' to false will cause the threads to close and the
   application to terminate.
*/
volatile sig_atomic_t g_running = 1;
extern volatile sig_atomic_t g_tone_thread_running;
extern volatile sig_atomic_t g_sweep_thread_running;
int signal_num = 0;
int connfd = 0;
int server_sock;
struct sockaddr_in cli;

uint8_t card = 0;

char * server_address = IP;
bool server_address_is_present = false;
uint32_t tcp_port = PORT;
bool port_is_present = false;

/* There is a separate structure for common radio data, RX, and TX */
struct radio_config rconfig = RADIO_CONFIG_INITIALIZER;
struct rx_radio_config rx_rconfig = RX_RADIO_CONFIG_INITIALIZER;
struct tx_radio_config tx_rconfig = TX_RADIO_CONFIG_INITIALIZER;

bool tx_running = false;

/* Insert the description of your app here, in short and long form */
static const char* p_help_short = "- Provides server portion of the testapp system";
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
    log_trace("in app_cleanup");

    signal_num = signum;
    g_running = 0;
    g_tone_thread_running = 0;
    g_sweep_thread_running = 0;

}

int send_response(int client_sock, char * response)
{

    log_trace("send_response");
    log_trace("in send_response response is %s", response);


    int len = send(client_sock, response, strlen(response), 0);

    log_info("send response len %d", len);

    if (len < 0)
        perror("send error:");

    return 0;
}

int process_startCW(int client_sock, char * cmdline)
{
    char * arg = NULL;
    uint32_t freq = 0;
    uint32_t span = 0;
    uint32_t power_level = 0;
    int32_t status = 0;

    log_trace("in process_startCW ");

    arg = strtok(NULL, " ");
    if (arg == NULL)
    {
        log_error( "not enough command arguments for StartCW ");
        send_response(client_sock, "FAILURE");
        return 1;
    }
    freq = atoi(arg);
    if (freq <= 0 || freq > 6000)
    {
        log_error( "startCW invalid freq parameter freq %d ", freq);
        send_response(client_sock, "FAILURE");
        return 1;
    }

    arg = strtok(NULL, " ");
    if (arg == NULL)
    {
        log_error( "not enough command arguments for StartCW ");
        send_response(client_sock, "FAILURE");
        return 1;
    }

    span = atoi(arg);
    if (span <= 0 || span > 60)
    {
        log_error( "startCW invalid span parameter span %d ", span);
        send_response(client_sock, "FAILURE");
        return 1;
    }

    arg = strtok(NULL, " ");
    if (arg == NULL)
    {
        log_error( "not enough command arguments for StartCW ");
        send_response(client_sock, "FAILURE");
        return 1;
    }

    power_level = atoi(arg);
    if (power_level < 0 || power_level > 9)
    {
        log_error( "startCW invalid power_level parameter power_level %d ", power_level);
        send_response(client_sock, "FAILURE");
        return 1;
    }

    if (tx_running == true)
    {
        status = stopGen(card, &rconfig, &tx_rconfig);
        if (status != 0)
        {

            send_response(client_sock, "FAILURE");
            return status;
        }
    }

    status = startCW(card, &rconfig, &tx_rconfig, freq, span, power_level);

    if (status != 0)
    {

        send_response(client_sock, "FAILURE");
        return status;
    }

    tx_running = true;

    send_response(client_sock, "SUCCESS");

    return status;
}

int process_stopGen(int client_sock, char * cmdline)
{
    int status = 0;

    log_trace("in process_stopGen ");

    status = stopGen(card, &rconfig, &tx_rconfig);
    if (status != 0)
    {

        send_response(client_sock, "FAILURE");
        return status;
    }

    tx_running = false;

    send_response(client_sock, "SUCCESS");
    return status;
}

int process_startSweep(int client_sock, char * cmdline)
{
    char * arg = NULL;
    uint32_t start_freq_MHz = 0;
    uint32_t power_level = 0;
    uint32_t steps = 0;
    uint32_t freq_step_MHz = 0;
    uint32_t step_time_ms = 0;
    uint32_t span_MHz = 0;

    int32_t status = 0;

    log_trace("in process_startSweep" );

    arg = strtok(NULL, " ");
    if (arg == NULL)
    {
        log_error( "not enough command arguments for processSweep ");
        send_response(client_sock, "FAILURE");
        return 1;
    }
    start_freq_MHz = atoi(arg);
    if (start_freq_MHz <= 0 || start_freq_MHz > 6000)
    {
        log_error( "processSweep invalid parameter start_freq_MHz %d ", start_freq_MHz);
        send_response(client_sock, "FAILURE");
        return 1;
    }

    arg = strtok(NULL, " ");
    if (arg == NULL)
    {
        log_error( "not enough command arguments for processSweep ");
        send_response(client_sock, "FAILURE");
        return 1;
    }
    power_level = atoi(arg);
    if (power_level < 0 || power_level > 9)
    {
        log_error( "processSweep invalid parameter power_level %d ", power_level);
        send_response(client_sock, "FAILURE");
        return 1;
    }

    arg = strtok(NULL, " ");
    if (arg == NULL)
    {
        log_error( "not enough command arguments for processSweep ");
        send_response(client_sock, "FAILURE");
        return 1;
    }
    steps = atoi(arg);
    if (steps <= 0 || steps > 100)
    {
        log_error( "processSweep invalid parameter steps %d ", steps);
        send_response(client_sock, "FAILURE");
        return 1;
    }

    arg = strtok(NULL, " ");
    if (arg == NULL)
    {
        log_error( "not enough command arguments for processSweep ");
        send_response(client_sock, "FAILURE");
        return 1;
    }
    freq_step_MHz = atoi(arg);
    if (freq_step_MHz <= 0 || freq_step_MHz > 100)
    {
        log_error( "processSweep invalid parameter freq_step_MHz %d ", freq_step_MHz);
        send_response(client_sock, "FAILURE");
        return 1;
    }

    arg = strtok(NULL, " ");
    if (arg == NULL)
    {
        log_error( "not enough command arguments for processSweep ");
        send_response(client_sock, "FAILURE");
        return 1;
    }

    step_time_ms = atoi(arg);
    if (step_time_ms <= 0 || step_time_ms > 6000)
    {
        log_error( "processSweep invalid parameter step_time_ms %d ", step_time_ms);
        send_response(client_sock, "FAILURE");
        return 1;
    }

    arg = strtok(NULL, " ");
    if (arg == NULL)
    {
        log_error( "not enough command arguments for processSweep ");
        send_response(client_sock, "FAILURE");
        return 1;
    }

    span_MHz = atoi(arg);
    if (span_MHz < 0 || span_MHz > 60)
    {
        log_error( "processSweep invalid parameter span_MHz %d ", span_MHz);
        send_response(client_sock, "FAILURE");
        return 1;
    }


    if (tx_running == true)
    {
        status = stopGen(card, &rconfig, &tx_rconfig);
        if (status != 0)
        {

            send_response(client_sock, "FAILURE");
            return status;
        }
    }

    status = startSweep(card, &rconfig, &tx_rconfig, start_freq_MHz, power_level, steps, freq_step_MHz, step_time_ms, span_MHz);

    if (status != 0)
    {

        send_response(client_sock, "FAILURE");
        return status;
    }

    tx_running = true;

    send_response(client_sock, "SUCCESS");

    return status;
}

int process_peakSearch(int client_sock, char * cmdline)
{
    char * arg = NULL;
    uint32_t freq = 0;
    uint32_t span = 0;
    int32_t status = 0;

    log_trace("in process_peakSearch ");

    arg = strtok(NULL, " ");
    if (arg == NULL)
    {
        log_error( "not enough command arguments for peakSearch ");
        send_response(client_sock, "FAILURE");
        return 1;
    }
    freq = atoi(arg);
    if (freq <= 0 || freq > 6000)
    {
        log_error( "peakSearch invalid freq parameter freq %d ", freq);
        send_response(client_sock, "FAILURE");
        return 1;
    }

    arg = strtok(NULL, " ");
    if (arg == NULL)
    {
        log_error( "not enough command arguments for peakSearch ");
        send_response(client_sock, "FAILURE");
        return 1;
    }

    span = atoi(arg);
    if (span <= 0 || span > 60)
    {
        log_error( "peakSearch invalid span parameter span %d ", span);
        send_response(client_sock, "FAILURE");
        return 1;
    }

    uint64_t peak_freq = 0;
    int32_t peak_power = -300;

    /* determine if we are already transmitting, then be careful about changing span */

    status = peakSearch(card, &rconfig, &rx_rconfig, freq, span, &peak_freq, &peak_power);
    if (status != 0)
    {

        send_response(client_sock, "FAILURE");
        return status;
    }



    log_debug("peak freq %lu, peak power %d", peak_freq, peak_power);

    char outline[90];
    sprintf(outline,"SUCCESS %" PRIu64 " %" PRIi32 "", peak_freq, peak_power);
    send_response(client_sock, outline);

    return status;
}
int process_getData(int client_sock, char * cmdline)
{
    char * arg = NULL;
    uint32_t freq = 0;
    uint32_t span = 0;
    int32_t status = 0;

    log_trace("in process_getData ");

    arg = strtok(NULL, " ");
    if (arg == NULL)
    {
        log_error( "not enough command arguments for getData ");
        send_response(client_sock, "FAILURE");
        return 1;
    }
    freq = atoi(arg);
    if (freq <= 0 || freq > 6000)
    {
        log_error( "getData invalid freq parameter freq %d ", freq);
        send_response(client_sock, "FAILURE");
        return 1;
    }

    arg = strtok(NULL, " ");
    if (arg == NULL)
    {
        log_error( "not enough command arguments for getData ");
        send_response(client_sock, "FAILURE");
        return 1;
    }

    span = atoi(arg);
    if (span <= 0 || span > 60)
    {
        log_error( "getData invalid span parameter span %d ", span);
        send_response(client_sock, "FAILURE");
        return 1;
    }

#define MAXLEN  100000

    /* determine if we are already transmitting, then be careful about changing span */
    double freq_array[SWEEPPOINTS];
    double power_array[SWEEPPOINTS];
    char freq_str[MAXLEN] = INIT_ARRAY(MAXLEN, 0);
    char power_str[MAXLEN] = INIT_ARRAY(MAXLEN, 0);
    char tmp_str[100] = INIT_ARRAY(100, 0);
    
    status = getData(card, &rconfig, &rx_rconfig, freq, span, freq_array, power_array);
    if (status != 0)
    {
        send_response(client_sock, "FAILURE");
        return status;
    }

    for(int i=0; i < SWEEPPOINTS; i++)
    {
        sprintf(tmp_str, " %f", freq_array[i]);
        strcat(freq_str, tmp_str);
        sprintf(tmp_str, " %3.1f", power_array[i]);
        strcat(power_str, tmp_str);
    }

    char outline[MAXLEN * 2 + 10];
    sprintf(outline,"SUCCESS %s %s ", freq_str, power_str );
    send_response(client_sock, outline);

    return status;
}

int process_setDebug(int client_sock, char * cmdline)
{
    char * arg = NULL;
    int32_t status = 0;
    char outline[100];

    log_trace("in process_setDebug ");

    arg = strtok(NULL, " ");
    if (arg == NULL)
    {
        log_error( "not enough command arguments for getData ");
        send_response(client_sock, "FAILURE");
        return 1;
    }

    if( 0 == strcasecmp(arg, "TRACE") )
    {
        log_set_level(LLOG_TRACE, false, false);

    }
    else if( 0 == strcasecmp(arg, "DEBUG") )
    {
        log_set_level(LLOG_DEBUG, false, false);

    }
    else if( 0 == strcasecmp(arg, "INFO") )
    {
        log_set_level(LLOG_INFO, false, false);

    }
    else if( 0 == strcasecmp(arg, "INFO") )
    {
        log_set_level(LLOG_INFO, false, false);

    }
    else if( 0 == strcasecmp(arg, "WARN") )
    {
        log_set_level(LLOG_WARN, false, false);

    }
    else if( 0 == strcasecmp(arg, "ERROR") )
    {
        log_set_level(LLOG_ERROR, false, false);

    }
    else if( 0 == strcasecmp(arg, "FATAL") )
    {
        log_set_level(LLOG_FATAL, false, false);

    }
    else
    {
        sprintf(outline, "FAILURE");
        send_response(client_sock, outline);
        return 0; 
    }

    sprintf(outline, "SUCCESS");
    send_response(client_sock, outline);

    return status;
}

int process_command(int client_sock)
{
    char buff[MAX];
    char *cmd_str = NULL; 
    char *cmd = NULL;
    int32_t len = 0;

    log_trace("in process_command ");


    bzero(buff, MAX);

    log_info("waiting for command...");

    // read the message from client and copy it in buffer
    len = recv(client_sock, buff, sizeof(buff), 0);

    if (len < 0)
    {
        perror("received  failed");
        return 1;
    }
    else if (len == 0)
    {
        /* this happens when the client disconnects, so indicate we are done */
        log_debug("client disconnected");
        return 1;
    }
    else
    {
        // print buffer which contains the client contents
        log_info("cmd rcvd: socket %d, len %d, %s", client_sock, len, buff);

        cmd_str = strdup(buff);


        if (cmd_str == NULL)
        {
            log_error( "Could not duplicate command line");
        }
        else
        {
            cmd = strtok(cmd_str, " ");

            if( 0 == strcasecmp(cmd, "STARTCW") )
            {
                process_startCW(client_sock, cmd_str);
            }
            else if( 0 == strcasecmp(cmd, "STOPGEN") )
            {
                process_stopGen(client_sock, cmd_str);
            }
            else if( 0 == strcasecmp(cmd, "STARTSWEEP") )
            {
                process_startSweep(client_sock, cmd_str);
            }
            else if( 0 == strcasecmp(cmd, "PEAKSEARCH") )
            {
                process_peakSearch(client_sock, cmd_str);
            }
            else if( 0 == strcasecmp(cmd, "GETDATA") )
            {
                process_getData(client_sock, cmd_str);
            }
            else if( 0 == strcasecmp(cmd, "SETDEBUG") )
            {
                process_setDebug(client_sock, cmd_str);
            }
            else
            {
                log_error( "invalid command %s ", cmd);
            }
        
        }

    }
    return 0;
} 

int wait_connection(int connfd)
{
    uint32_t len = 0;
    int client_sock = 0;

    log_trace("wait_connection ");
    len = sizeof(cli);
    log_info("waiting for client to connect");


    client_sock = accept(server_sock, (struct sockaddr*)&cli, &len);
    if (client_sock < 0) 
    {
        perror("accept failed");
        return 1;
    }
    else if (len == 0)
    {
        log_debug(" Received 0 len ");
        g_running = 0;
        return  1;
    }
    else
    {
        log_debug("Client connected ");
    }

    return client_sock;
}

int setup_socket(void)
{
    log_trace("setup_socket ");
    struct sockaddr_in servaddr;

    // socket create and verification
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) 
    {
        perror("Socket error");
        exit(1);
    }
    else
    {
        log_debug("Socket successfully created.. ");
    }

    /* setsockopt: Handy debugging trick that lets
   * us rerun the server immediately after we kill it;
   * otherwise we have to wait about 20 secs.
   * Eliminates "ERROR on binding: Address already in use" error.
   */
   int optval = 1;
   if(setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR,
	     (const void *)&optval , sizeof(int)) != 0)
   {
        perror("Sockopt error");
        exit(1);
   }
   else
   {
       log_debug("setsockopt successful ");
   }

    struct timeval tv;
    tv.tv_sec = 3 * 3600; // 3 minutes
    tv.tv_usec = 0;
    setsockopt(server_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(server_address);
    servaddr.sin_port = htons(tcp_port);

    // Binding newly created socket to given IP and verification
    
    if(bind(server_sock, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0)
    {
        perror("bind error");
        exit(1);
    }
    else
    {
        log_info("Bound to address %s, port number: %d ", server_address, tcp_port);
    }

    // Now server is ready to listen and verification
    if ((listen(server_sock, 5)) != 0) 
    {
        perror("listen error");
        exit(1);
    }
    else
    {
        log_debug("Server listening.. ");
    }


    
    return connfd;

}

/*****************************************************************************/
/** 

*/

static void* client_thread( void *params)
{
    int status = 0;
   
    log_trace("client_thread");
    struct client_thread_params *thread_params = params;
    thread_params->client_completed = false;

    while (g_running == true && status == 0)  
    {
        status = process_command(thread_params->client_socket);
    }

    log_debug("leaving thread");

    thread_params->client_completed = true;
    return (void *)(intptr_t)status;

}

/*****************************************************************************/
/** This is called every time a log is received.  We want to put the port
 *  number before every log.  So we can tell which server is logging.

      @param[in]  ev:     The structure containing all the log info

 */
void log_callback(log_Event *ev)
{
    fprintf(ev->udata, "[%d] ", tcp_port);

    /* now call the logging function to actually print out the log */
    stderr_callback(ev);

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
    int client_sock = 0;
    struct client_thread_params client_thread_array[MAX_CLIENT_THREADS] = INIT_ARRAY(5, CLIENT_THREAD_PARAMS_INITIALIZER);
    int index = 0;


    status = log_add_callback(&log_callback, stderr, LLOG_INFO); 
    if (status != 0)
    {
        printf("add log callback failed %d", status);
    }

    log_set_level(LLOG_INFO, false, false);

    log_set_quiet(true);

    /* always install a handler for proper cleanup */
    signal(SIGINT, app_cleanup);                                                                                                  

    /* arg_select is a bitmask of the common parameters used in this app */  
    g_cmd_line_selector.arg_select =    ARG_CARD_ID             | 
                                        ARG_SERIAL_NUM          |
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

        new_arg.p_long_flag     = "server-addr" ;
        new_arg.short_flag      = 'a';
        new_arg.p_info          = "IP address of this server";
        new_arg.p_label         = 0;
        new_arg.p_var           = &server_address; //give the address where you want the parameter
        new_arg.type            = STRING_VAR_TYPE;

        new_arg.required    = false;
        new_arg.p_is_set    = &server_address_is_present;  //give the address where you want the flag

        add_app_specific_args(args, &new_arg, &num_args);

        new_arg.p_long_flag     = "port" ;
        new_arg.short_flag      = 'p';
        new_arg.p_info          = "The TCP port of the server ";
        new_arg.p_label         = 0;
        new_arg.p_var           = &tcp_port; //give the address where you want the parameter
        new_arg.type            = UINT32_VAR_TYPE;

        new_arg.required    = false;
        new_arg.p_is_set    = &port_is_present;  //give the address where you want the flag

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

    /* always use async mode unless the user explicitly set them to something */
    if(g_cmd_line_args.threads_is_present == false)
    {
        g_cmd_line_args.threads_is_present = true;
        g_cmd_line_args.num_threads = 4;
    }

    /* print the arguments you have received and defaults 
     * These may have changed during map_arguments so print it here. */
    print_args(num_args, args);


    /* map command line arguments to radio config */
    status = map_arguments_to_radio_config((struct cmd_line_args *)&g_cmd_line_args, &rconfig);
    if ( status != 0 )
    {
        log_error( "Error: Failed map_arg_to_radio, status %" PRIi32 "  ", status);
        goto exit;
    }

    status = map_arguments_to_rx_radio_config((struct cmd_line_args *)&g_cmd_line_args, &rconfig, &rx_rconfig);
    if ( status != 0 )
    {
        log_error( "Error: Failed map_arg_to_rx_radio, status %" PRIi32 "  ", status);
        goto exit;
    }
    status = map_arguments_to_tx_radio_config((struct cmd_line_args *)&g_cmd_line_args, &rconfig, &tx_rconfig);
    if ( status != 0 )
    {
        log_error( "Error: Failed map_arg_to_tx_radio, status %" PRIi32 "  ", status);
        goto exit;
    }

    /* determine which card to use */
    card = g_cmd_line_args.card_id;


    log_debug("Info: initializing %" PRIu8 " card(s)... ", rconfig.num_cards);
    status = init_libsidekiq(skiq_xport_init_level_full, &rconfig);
    if (status != 0) {
        log_error( "Error: Failed radio init, status %" PRIi32 "  ", status);
        goto exit;
    }

    while (g_running == true)
    {
        int slot = 0;
        client_sock = 0;

        /* get our IP address up and listening */
        connfd = setup_socket();

        while (g_running == true)
        {
            /* wait for a new connection */
            client_sock = wait_connection(connfd);

            index = -1;

            /* see if any threads have exited */
            for (slot = 0; slot < MAX_CLIENT_THREADS; slot++)
            {
                if (client_thread_array[slot].client_index != -1)
                {
                    /* see if it has completed */
                    /* if not busy then it has exited and done */
                    if (client_thread_array[slot].client_completed == true)
                    {
                        log_debug("found a completed slot %d", slot);
                        client_thread_array[slot].client_index = -1;
                    }
                }
                else
                {
                    /* this is an empty slot so use this index */
                    index = slot;
                }
            }

            if (index == -1)
            {
                log_error("thread array is full");
            }
            else
            {
                log_debug("thread_array index is %d", index);
                client_thread_array[index].client_index = index;
                client_thread_array[index].client_socket = client_sock;

                status = pthread_create( &(client_thread_array[index].client_thread),
                  NULL, client_thread, &client_thread_array[index]);
            }
        }
    }

exit:
    /* done so exit */
    if (rconfig.skiq_initialized == true) {
        skiq_exit();
    }

    close(connfd);

    return status;
}

