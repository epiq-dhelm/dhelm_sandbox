/**
 * @file utils_common.c
 *
 * @brief Every test_app has command line parameters that are common across many of them.
 * The goal is to provide common code for all apps to avoid having to individually handle those parameters
 * and then configure the radio.  Every test_app is doing that differently.
 *
 * This code is intended to provide common code to handle the command line parameters, 
 * map those to radio configuration and then configure the radio.
 *
 * As more test_apps use this code, the number of "common command line arguments" are intended to grow.
 *
 * This also allows the common code to handle errors in a common way, so the look and feel of the app is
 * consistent.
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

#include <fcntl.h>
#include <pthread.h>
#if (defined __MINGW32__)
#define OUTPUT_PATH_MAX                     260
#else
#include <linux/limits.h>
#define OUTPUT_PATH_MAX                     PATH_MAX
#endif

#define MAX_CALLBACKS 32

#include "sidekiq_api.h"
#include "arg_parser.h"
#include "utils_common.h"
  
typedef struct {
  log_LogFn fn;
  void *udata;
  int level;
} Callback; 

/*************************** LOGGING VARIABLES  *****************************/
#define CALLBACK_INITIALIZER                \
{                                           \
    .udata      = NULL,                     \
    .level      = 0,                        \
    .fn         = NULL,                     \
}                                           \


static struct {
  void *udata;
  log_LockFn lock;
  int level;
  bool quiet;
  Callback callbacks[MAX_CALLBACKS];
  bool use_color;
  bool use_time;
} L = {.udata = NULL, .lock = NULL, .level = LLOG_DEBUG, .quiet = false, 
        .callbacks = INIT_ARRAY(MAX_CALLBACKS, CALLBACK_INITIALIZER), 
        .use_color = false, .use_time = false};

;
static const char *level_strings[] = {
  "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

static const char *level_colors[] = {
  "\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"
};

/*************************** LOGGING FUNCTIONS *****************************/

/*****************************************************************************/
/** @brief The function called to print to stderr 
*/
void stderr_callback(log_Event *ev) 
{
  char buf[16];
  int len = 7;
  char tmpstr[90] ;

  /* Get the level string that we need to print */
  sprintf(tmpstr, "<%s>", level_strings[ev->level]);


  /* Get the TIME string that we need to print */
  buf[strftime(buf, sizeof(buf), "%H:%M:%S", ev->time)] = '\0';

  /* Process Color output or not */
  if (L.use_color == true)
  {
      /* print out the level string getting the next print aligned 
       * even if the level is different character length */
      fprintf(
        ev->udata, "%s%-*s\x1b[0m ",
        level_colors[ev->level], len, tmpstr);

      /* print out the variable string given by the user */
      vfprintf(ev->udata, ev->fmt, ev->ap);


      /* if necessary print the TIME FILE:LINE */
      if (ev->level >= LLOG_ERROR || L.use_time == true)
      {
          fprintf(
            ev->udata, " (%s: \x1b[90m%s:%d\x1b[0m)",
            buf, ev->file, ev->line);
      }
  }
  else
  {
      /* print out the level string getting the next print aligned 
       * even if the level is different character length */
      fprintf(ev->udata, "%-*s ", len, tmpstr);

      /* print out the variable string given by the user */
      vfprintf(ev->udata, ev->fmt, ev->ap);

      /* if necessary print the TIME FILE:LINE */
      if (ev->level >= LLOG_ERROR || L.use_time == true)
      {
          fprintf(
            ev->udata, " (%s %s:%d) ",
            buf, ev->file, ev->line);
      }
  }

  /* print out the new line character */
  fprintf(ev->udata, "\n");

  fflush(ev->udata);
}


/*****************************************************************************/
/** @brief  Write the event to the file
*/
static void file_callback(log_Event *ev) 
{
  char buf[64];

  buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ev->time)] = '\0';
  fprintf(
    ev->udata, "%s %-5s %s:%d: ",
    buf, level_strings[ev->level], ev->file, ev->line);
  vfprintf(ev->udata, ev->fmt, ev->ap);
  fprintf(ev->udata, "\n");
  fflush(ev->udata);
}


/*****************************************************************************/
/** @brief call the users' lock function indicating lock
*/
static void lock(void)   
{

  if (L.lock) { L.lock(true, L.udata); }
}

/*****************************************************************************/
/** @brief call the users' lock function indicating unlock
*/

static void unlock(void) 
{
  if (L.lock) { L.lock(false, L.udata); }
}


/*****************************************************************************/
/** @brief Return the log_level_string based upon the level
*/
const char* log_level_string(int level) 
{
  return level_strings[level];
}

/*****************************************************************************/
/** @brief Initialize the event being logged 
*/
static void init_event(log_Event *ev, void *udata) 
{
  if (!ev->time) {
    time_t t = time(NULL);
    ev->time = localtime(&t);
  }
  ev->udata = udata;
}


/*****************************************************************************/
/** @brief  Set the logging parameters for the parameters that control stderr logging
*/
void log_set_level(int level, bool use_color, bool use_time) 
{
  L.level = level;
  L.use_color = use_color;
  L.use_time = use_time;
}

/*****************************************************************************/
/** @brief Quiet-mode can be enabled by passing true to the log_set_quiet() function.
 *  While this mode is enabled the library will not output anything to stderr,
 *  but will continue to write to files and callbacks if any are set.
*/
void log_set_quiet(bool enable) {
  L.quiet = enable;
}

/*****************************************************************************/
/** @brief One or more file pointers where the log will be written can be provided to the library 
 *   by using the log_add_fp() function.  Any messages below the give level are ignored. 
*/
int log_add_fp(FILE *fp, int level) 
{
  /* add the passed in fp as the udata for the file callback */
  return log_add_callback(file_callback, fp, level);
}

/*****************************************************************************/
/** @brief If the log will be written to from multiple threads a lock function can be set.
 *  When called the function is passed the boolean true if the lock should be acquired or
 *  false if the lock should be released and the given udata value.
*/
void log_set_lock(log_LockFn fn, void *udata) 
{
  L.lock = fn;
  L.udata = udata;
}

/*****************************************************************************/
/** @brief One or more callback functions which are called with the log data can be provided to
 * the library by using the log_add_callback() function.
 * A callback function is passed a log_Event structure containing the line number,
 * filename, fmt string, va printf va_list, level and the given udata.

*/
int log_add_callback(log_LogFn fn, void *udata, int level) 
{
  for (int i = 0; i < MAX_CALLBACKS; i++) 
  {
    if (!L.callbacks[i].fn) 
    {
      L.callbacks[i] = (Callback) { fn, udata, level };
      return 0;
    }
  }
  return -1;
}


/*****************************************************************************/
/** @briefThe library provides 6 function-like macros for logging:

 * log_trace(const char *fmt, ...);
 * log_debug(const char *fmt, ...);
 * log_info(const char *fmt, ...);
 * log_warn(const char *fmt, ...);
 * log_error(const char *fmt, ...);
 * log_fatal(const char *fmt, ...);
 * Each function takes a printf format string followed by additional arguments:
 * log_trace("Hello %s", "world") 
*/

void log_log(int level, const char *file, int line, const char *fmt, ...) 
{
  log_Event ev = {
    .fmt   = fmt,
    .file  = file,
    .line  = line,
    .level = level,
  };

  /* call the users' lock function */
  lock();

  /* process stderr output */
  if (!L.quiet && level >= L.level) 
  {
    /* level is high enough and not quiet */
    init_event(&ev, stderr);

    /* process the passed in fmt to get to a string */
    va_start(ev.ap, fmt);
    
    /* print out the event */
    stderr_callback(&ev);
    va_end(ev.ap);
  }

  /* loop through all the callbacks registered.  Including print to file */
  for (int i = 0; i < MAX_CALLBACKS && L.callbacks[i].fn; i++) 
  {
    Callback *cb = &L.callbacks[i];

    /* determine if level is high enough  */
    if (level >= L.level) 
    {
      init_event(&ev, cb->udata);
      va_start(ev.ap, fmt);

      /* call the callback function */
      cb->fn(&ev);
      va_end(ev.ap);
    }
  }

  /* call the users' lock function, indicating unlock */
  unlock();
}


/*************************** COMMAND LINE ARG FUNCTIONS *****************************/
/*****************************************************************************/
/** @brief
    Add a command line parameter to the args list

*/
uint32_t add_command_line_param(uint64_t selector,
                                struct cmd_line_selector *p_cmd_line_selector,
                                struct application_argument args[], 
                                struct application_argument *new_arg,
                                uint32_t *p_num_args, 
                                void * p_parameter)
{
    uint32_t num_args = *p_num_args;

    log_trace("add_command_line_param");

    if (num_args + 1 >= MAX_ARGS)
    {
        log_error("ERROR: Too many command line arguments requested %" PRIu32 , num_args + 1);
        return 1;
    }

    args[num_args] = *new_arg;

    if (p_cmd_line_selector->arg_required & selector){
        if ( args[num_args].type == BOOL_VAR_TYPE) {
            log_error("ERROR: A boolean type cannot be required");
            return 1;
        }    
        args[num_args].required    = true; 
    } else {
        args[num_args].required    = false; 
    }

    args[num_args].p_is_set    = p_parameter;

    num_args++;

    *p_num_args = num_args;

    return 0;
}


/*****************************************************************************/
/** @brief
    Initialize the application arg array info that is passed into arg_parser
    based upon the passed in cmd_line_selector bit array


*/
void initialize_application_args(struct cmd_line_selector *p_cmd_line_selector, 
                                 struct application_argument args[], 
                                 struct cmd_line_args *g_cmd_line_args,
                                 uint32_t *p_num_args)
{
    uint32_t ctr = 0;
    struct application_argument new_arg;
    uint32_t status;


    log_trace("initialize_application_args");

    if (p_cmd_line_selector->arg_select & ARG_CARD_ID){
        new_arg.p_long_flag     = "card";
        new_arg.short_flag      = 'c';
        new_arg.p_info          = "Use specified Sidekiq card"; 
        new_arg.p_label         = "ID";
        new_arg.p_var           = &(g_cmd_line_args->card_id);
        new_arg.type            = UINT8_VAR_TYPE; 

        status = add_command_line_param(ARG_CARD_ID, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         &(g_cmd_line_args->card_is_present)); 
        if (status != 0) {
            return;
        }
    }

    if (p_cmd_line_selector->arg_select & ARG_SERIAL_NUM){
        new_arg.p_long_flag     = "serial";
        new_arg.short_flag      = 'S';
        new_arg.p_info          = "Specify Sidekiq by serial number";
        new_arg.p_label         = "SERNUM";
        new_arg.p_var           = &(g_cmd_line_args->p_serial);
        new_arg.type            = STRING_VAR_TYPE; 

        status = add_command_line_param(ARG_SERIAL_NUM, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         &(g_cmd_line_args->p_serial_is_present)); 
        if (status != 0) {
            return;
        }
    }
    if (p_cmd_line_selector->arg_select & ARG_SAMPLE_RATE){
        new_arg.p_long_flag     = "rate";
        new_arg.short_flag      = 'r';
        new_arg.p_info          = "Sample rate in Hertz";
        new_arg.p_label         = "Hz";
        new_arg.p_var           = &(g_cmd_line_args->sample_rate);
        new_arg.type            = UINT32_VAR_TYPE; 

        status = add_command_line_param(ARG_SAMPLE_RATE, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         &(g_cmd_line_args->rate_is_present)); 
        if (status != 0) {
            return;
        }
    }

    if (p_cmd_line_selector->arg_select & ARG_BANDWIDTH){
        new_arg.p_long_flag     = "bandwidth";
        new_arg.short_flag      = 'b'; 
        new_arg.p_info          = "Bandwidth in Hertz";
        new_arg.p_label         = "Hz";
        new_arg.p_var           = &(g_cmd_line_args->bandwidth);
        new_arg.type            = UINT32_VAR_TYPE; 

        status = add_command_line_param(ARG_BANDWIDTH, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         &(g_cmd_line_args->bw_is_present)); 
        if (status != 0) {
            return;
        }
    }


    if (p_cmd_line_selector->arg_select & ARG_REPEAT){
        new_arg.p_long_flag     = "repeat";
        new_arg.short_flag      = 0;
        new_arg.p_info          = "How many times to repeat test (default is forever)";
        new_arg.p_label         = "N";
        new_arg.p_var           = &(g_cmd_line_args->repeat);
        new_arg.type            = UINT32_VAR_TYPE ;

        status = add_command_line_param(ARG_REPEAT, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         &(g_cmd_line_args->repeat_is_present)); 
        if (status != 0) {
            return;
        }
    }
    if (p_cmd_line_selector->arg_select & ARG_SAMPLE_ORDER_QI){
        new_arg.p_long_flag     = "sample-order-qi";
        new_arg.short_flag      = 0;
        new_arg.p_info          = "If specified, set the received sample order to\n" 
                                  "\t\t\t\t\t\t\tI then Q (the default is Q then I)";
        new_arg.p_label         = NULL;
        new_arg.p_var           = &(g_cmd_line_args->sample_order_qi);
        new_arg.type            = BOOL_VAR_TYPE ;

        status = add_command_line_param(ARG_SAMPLE_ORDER_QI, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         NULL);
        if (status != 0) {
            return;
        }
    }

    /* RX command line args */
    if (p_cmd_line_selector->arg_select & ARG_RX_HDL){
        new_arg.p_long_flag     = "rx-handle";
        new_arg.short_flag      = 0;
        new_arg.p_info          = "Rx handle to use";
        new_arg.p_label         = "['A1','A2','B1','B2','C1','D1','ALL']";
        new_arg.p_var           = &(g_cmd_line_args->p_rx_hdl);
        new_arg.type            = STRING_VAR_TYPE; 

        status = add_command_line_param(ARG_RX_HDL, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         &(g_cmd_line_args->rx_hdl_is_present)); 
        if (status != 0) {
            return;
        }
    }

    if (p_cmd_line_selector->arg_select & ARG_RX_RF_PORT){
        new_arg.p_long_flag     = "rx-rf-port";
        new_arg.short_flag      = 0;
        new_arg.p_info          = "RX RF port 'JN' where N is the number of the RF port \n" 
                                  "\t\t\t\t\t\t\t(configure ability dependent on product)";
        new_arg.p_label         = "'JN'";
        new_arg.p_var           = &(g_cmd_line_args->rx_rf_port);
        new_arg.type            = STRING_VAR_TYPE; 

        status = add_command_line_param(ARG_RX_RF_PORT, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         &(g_cmd_line_args->rx_rf_port_is_present)); 
        if (status != 0) {
            return;
        }
    }

    if (p_cmd_line_selector->arg_select & ARG_RX_FREQ){
        new_arg.p_long_flag     = "rx-freq";
        new_arg.short_flag      = 'f';
        new_arg.p_info          = "Center frequency (in Hertz) to receive samples";
        new_arg.p_label         = "Hz";
        new_arg.p_var           = &(g_cmd_line_args->rx_freq);
        new_arg.type            = UINT64_VAR_TYPE; 

        status = add_command_line_param(ARG_RX_FREQ, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         &(g_cmd_line_args->rx_freq_is_present)); 
        if (status != 0) {
            return;
        }
    }
    if (p_cmd_line_selector->arg_select & ARG_RX_GAIN){
        new_arg.p_long_flag     = "rx-gain";
        new_arg.short_flag      = 'g';
        new_arg.p_info          = "Manually configure the gain by index rather than\n"
                                   "\t\t\t\t\t\t\tusing automatic" ;
        new_arg.p_label         = "index";
        new_arg.p_var           = &(g_cmd_line_args->rx_gain);
        new_arg.type            = UINT8_VAR_TYPE; 

        status = add_command_line_param(ARG_RX_GAIN, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         &(g_cmd_line_args->rx_gain_manual)); 
        if (status != 0) {
            return;
        }
    }
    if (p_cmd_line_selector->arg_select & ARG_RX_PATH){
        new_arg.p_long_flag     = "rx-path";
        new_arg.short_flag      = 'd';
        new_arg.p_info          = "Output file to store Rx data";
        new_arg.p_label         = "PATH";
        new_arg.p_var           = &(g_cmd_line_args->p_rx_file_path);
        new_arg.type            = STRING_VAR_TYPE ;

        status = add_command_line_param(ARG_RX_PATH, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         &(g_cmd_line_args->rx_file_path_is_present)); 
        if (status != 0) {
            return;
        }
    }
    if (p_cmd_line_selector->arg_select & ARG_RX_PAYLOAD_SAMPLES){
        new_arg.p_long_flag     = "rx-samples";
        new_arg.short_flag      = 's';
        new_arg.p_info          = "Number of samples to receive";
        new_arg.p_label         = "N";
        new_arg.p_var           = &(g_cmd_line_args->rx_samples_to_acquire);
        new_arg.type            = UINT32_VAR_TYPE ;

        status = add_command_line_param(ARG_RX_PAYLOAD_SAMPLES, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         &(g_cmd_line_args->rx_samples_is_present)); 
        if (status != 0) {
            return;
        }
    }
    if (p_cmd_line_selector->arg_select & ARG_RX_BLOCKING){
        new_arg.p_long_flag     = "rx-blocking";
        new_arg.short_flag      = 0;
        new_arg.p_info          = "Perform blocking during skiq_receive call"; 
        new_arg.p_label         = NULL;
        new_arg.p_var           = &(g_cmd_line_args->rx_blocking);
        new_arg.type            = BOOL_VAR_TYPE ;

        status = add_command_line_param(ARG_RX_BLOCKING, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         NULL);
        if (status != 0) {
            return;
        }
    }
    if (p_cmd_line_selector->arg_select & ARG_TRIGGER_SRC){
        new_arg.p_long_flag     = "trigger-src";
        new_arg.short_flag      = 0;
        new_arg.p_info          = "Source of start streaming trigger";
        new_arg.p_label         = "['1pps','immediate','synced']";
        new_arg.p_var           = &(g_cmd_line_args->p_trigger_src);
        new_arg.type            = STRING_VAR_TYPE ;

        status = add_command_line_param(ARG_TRIGGER_SRC, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         &(g_cmd_line_args->trigger_src_is_present)); 
        if (status != 0) {
            return;
        }
    }
    if (p_cmd_line_selector->arg_select & ARG_PPS_SOURCE){
        new_arg.p_long_flag     = "pps-source";
        new_arg.short_flag      = 0;
        new_arg.p_info          = "The PPS input source"; 
        new_arg.p_label         = "['external','host']";
        new_arg.p_var           = &(g_cmd_line_args->p_pps_source);
        new_arg.type            = STRING_VAR_TYPE ;

        status = add_command_line_param(ARG_PPS_SOURCE, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         &(g_cmd_line_args->pps_source_is_present)); 
        if (status != 0) {
            return;
        }
    }
    if (p_cmd_line_selector->arg_select & ARG_SETTLE_TIME){
        new_arg.p_long_flag     = "settle-time";
        new_arg.short_flag      = 0;
        new_arg.p_info          = "Minimum time to delay after configuring radio\n"
                                  "\t\t\t\t\t\t\tprior to transferring samples";
        new_arg.p_label         = "msec";
        new_arg.p_var           = &(g_cmd_line_args->settle_time);
        new_arg.type            = UINT32_VAR_TYPE ;

        status = add_command_line_param(ARG_SETTLE_TIME, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         &(g_cmd_line_args->settle_time_is_present)); 
        if (status != 0) {
            return;
        }
    }
    if (p_cmd_line_selector->arg_select & ARG_PACKED){
        new_arg.p_long_flag     = "packed";
        new_arg.short_flag      = 0;
        new_arg.p_info          = "Use packed mode for I/Q samples";
        new_arg.p_label         = NULL;
        new_arg.p_var           = &(g_cmd_line_args->packed);
        new_arg.type            = BOOL_VAR_TYPE ;

        status = add_command_line_param(ARG_PACKED, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         NULL);
        if (status != 0) {
            return;
        }
    }
    if (p_cmd_line_selector->arg_select & ARG_INCLUDE_META){
        new_arg.p_long_flag     = "meta";
        new_arg.short_flag      = 0;
        new_arg.p_info          = "Save metadata with samples (increases output file size)"; 
        new_arg.p_label         = NULL;
        new_arg.p_var           = &(g_cmd_line_args->include_meta);
        new_arg.type            = BOOL_VAR_TYPE ;

        status = add_command_line_param(ARG_INCLUDE_META, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         NULL);
        if (status != 0) {
            return;
        }
    }

    if (p_cmd_line_selector->arg_select & ARG_USE_COUNTER){
        new_arg.p_long_flag     = "counter";
        new_arg.short_flag      = 0;
        new_arg.p_info          = "Receive sequential counter data";
        new_arg.p_label         = NULL;
        new_arg.p_var           = &(g_cmd_line_args->use_counter);
        new_arg.type            = BOOL_VAR_TYPE ;

        status = add_command_line_param(ARG_USE_COUNTER, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         NULL);
        if (status != 0) {
            return;
        }
    }
    if (p_cmd_line_selector->arg_select & ARG_DISABLE_DC_CORR){
        new_arg.p_long_flag     = "disable-dc";
        new_arg.short_flag      = 0;
        new_arg.p_info          = "Disable DC offset correction";
        new_arg.p_label         = NULL;
        new_arg.p_var           = &(g_cmd_line_args->disable_dc_corr);
        new_arg.type            = BOOL_VAR_TYPE ;

        status = add_command_line_param(ARG_DISABLE_DC_CORR, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         NULL);
        if (status != 0) {
            return;
        }
    }


    /* TX command line args */

    if (p_cmd_line_selector->arg_select & ARG_TX_HDL){
        new_arg.p_long_flag     = "tx-handle";
        new_arg.short_flag      = 0;
        new_arg.p_info          = "Tx handle to use";
        new_arg.p_label         = "['A1','A2','B1','B2','ALL']";
        new_arg.p_var           = &(g_cmd_line_args->p_tx_hdl);
        new_arg.type            = STRING_VAR_TYPE; 

        status = add_command_line_param(ARG_TX_HDL, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         &(g_cmd_line_args->tx_hdl_is_present)); 
        if (status != 0) {
            return;
        }
    }

    if (p_cmd_line_selector->arg_select & ARG_TX_RF_PORT){
        new_arg.p_long_flag     = "tx-rf-port";
        new_arg.short_flag      = 0;
        new_arg.p_info          = "TX RF port 'JN' where N is the number of the RF port \n" 
                                  "\t\t\t\t\t\t\t(configure ability dependent on product)";
        new_arg.p_label         = "'JN'";
        new_arg.p_var           = &(g_cmd_line_args->tx_rf_port);
        new_arg.type            = STRING_VAR_TYPE; 

        status = add_command_line_param(ARG_TX_RF_PORT, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         &(g_cmd_line_args->tx_rf_port_is_present)); 
        if (status != 0) {
            return;
        }
    }

    if (p_cmd_line_selector->arg_select & ARG_TX_FREQ){
        new_arg.p_long_flag     = "tx-freq";
        new_arg.short_flag      = 0;
        new_arg.p_info          = "Center frequency (in Hertz) to transmit samples";
        new_arg.p_label         = "Hz";
        new_arg.p_var           = &(g_cmd_line_args->tx_freq);
        new_arg.type            = UINT64_VAR_TYPE; 

        status = add_command_line_param(ARG_TX_FREQ, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         &(g_cmd_line_args->tx_freq_is_present)); 
        if (status != 0) {
            return;
        }
    }
    if (p_cmd_line_selector->arg_select & ARG_ATTENUATION){
        new_arg.p_long_flag     = "attenuation";
        new_arg.short_flag      = 'a';
        new_arg.p_info          = "Manually configure the attenuation by index rather than\n"
                                   "\t\t\t\t\t\t\tusing automatic" ;
        new_arg.p_label         = "index";
        new_arg.p_var           = &(g_cmd_line_args->attenuation);
        new_arg.type            = UINT8_VAR_TYPE; 

        status = add_command_line_param(ARG_ATTENUATION, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         &(g_cmd_line_args->attenuation_is_present)); 
        if (status != 0) {
            return;
        }
    }
    if (p_cmd_line_selector->arg_select & ARG_BLOCK_SIZE){
        new_arg.p_long_flag     = "block-size";
        new_arg.short_flag      = 0;
        new_arg.p_info          = "Size of TX block in samples (default 1020)" ;
        new_arg.p_label         = "N";
        new_arg.p_var           = &(g_cmd_line_args->block_size);
        new_arg.type            = UINT16_VAR_TYPE; 

        status = add_command_line_param(ARG_BLOCK_SIZE, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         &(g_cmd_line_args->block_size_is_present)); 
        if (status != 0) {
            return;
        }
    }
    
    if (p_cmd_line_selector->arg_select & ARG_TX_FILE_PATH){
        new_arg.p_long_flag     = "tx-path";
        new_arg.short_flag      = 0;
        new_arg.p_info          = "File to transmit from";
        new_arg.p_label         = "PATH";
        new_arg.p_var           = &(g_cmd_line_args->p_tx_file_path);
        new_arg.type            = STRING_VAR_TYPE ;

        status = add_command_line_param(ARG_TX_FILE_PATH, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         &(g_cmd_line_args->tx_file_path_is_present)); 
        if (status != 0) {
            return;
        }
    }

    if (p_cmd_line_selector->arg_select & ARG_TIMESTAMP_BASE){
        new_arg.p_long_flag     = "timestamp-base";
        new_arg.short_flag      = 0;
        new_arg.p_info          = "RF or System (default RF)";
        new_arg.p_label         = "['rf','system']";
        new_arg.p_var           = &(g_cmd_line_args->timestamp_base);
        new_arg.type            = STRING_VAR_TYPE ;

        status = add_command_line_param(ARG_TIMESTAMP_BASE, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         &(g_cmd_line_args->timestamp_base_is_present)); 
        if (status != 0) {
            return;
        }
    }

    if (p_cmd_line_selector->arg_select & ARG_TIMESTAMP_VALUE){
        new_arg.p_long_flag     = "timestamp-value";
        new_arg.short_flag      = 0;
        new_arg.p_info          = "Initial timestamp value, if 0 then transmitting immediately ";
        new_arg.p_label         = "N";
        new_arg.p_var           = &(g_cmd_line_args->timestamp_value);
        new_arg.type            = UINT64_VAR_TYPE ;

        status = add_command_line_param(ARG_TIMESTAMP_VALUE, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         &(g_cmd_line_args->timestamp_value_is_present)); 
        if (status != 0) {
            return;
        }
    }

    if (p_cmd_line_selector->arg_select & ARG_USE_LATE_TIMESTAMP){
        new_arg.p_long_flag     = "use-late-ts";
        new_arg.short_flag      = 0;
        new_arg.p_info          = "Whether to use late timestamps or not ";
        new_arg.p_label         = NULL;
        new_arg.p_var           = &(g_cmd_line_args->use_late_timestamp);
        new_arg.type            = BOOL_VAR_TYPE ;

        status = add_command_line_param(ARG_USE_LATE_TIMESTAMP, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         NULL);
        if (status != 0) {
            return;
        }
    }

    if (p_cmd_line_selector->arg_select & ARG_THREADS){
        new_arg.p_long_flag     = "threads";
        new_arg.short_flag      = 0;
        new_arg.p_info          = "Transmit asynchronously using 'N' threads";
        new_arg.p_label         = "N";
        new_arg.p_var           = &(g_cmd_line_args->num_threads);
        new_arg.type            = UINT8_VAR_TYPE ;

        status = add_command_line_param(ARG_THREADS, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         &(g_cmd_line_args->threads_is_present)); 
        if (status != 0) {
            return;
        }
    }
    if (p_cmd_line_selector->arg_select & ARG_THREAD_PRIORITY){
        new_arg.p_long_flag     = "priority";
        new_arg.short_flag      = 0;
        new_arg.p_info          = "Thread priority of asynchronous TX threads";
        new_arg.p_label         = "N";
        new_arg.p_var           = &(g_cmd_line_args->thread_priority);
        new_arg.type            = INT32_VAR_TYPE ;

        status = add_command_line_param(ARG_THREAD_PRIORITY, 
                                         p_cmd_line_selector, 
                                         args, 
                                         &new_arg,
                                         &ctr,
                                         &(g_cmd_line_args->priority_is_present)); 
        if (status != 0) {
            return;
        }
    }

    /* add terminator */
    if (ctr + 1 >= MAX_ARGS)
    {
        log_error("ERROR: Too many command line arguments requested %" PRIu32 , ctr + 1);
        return;
    }
    args[ctr] = (struct application_argument) COMMAND_LINE_INITIALIZER;
    ctr++;

    *p_num_args = ctr;

}

/*****************************************************************************/
/** @brief Add an application specific command line parameter to the existing args array 
*/

void add_app_specific_args(struct application_argument args[], 
                                  struct application_argument *new_arg,
                                  uint32_t *p_num_args)
{
    uint32_t num_args = *p_num_args;

    log_trace("add_app_specific_args");

    if (num_args + 1 >= MAX_ARGS)
    {
        log_error("ERROR: Too many command line arguments requested %" PRIu32 , num_args + 1);
        return;
    }

    /* num_args is 1 based, array is 0 based overwrite the terminator */
    if (num_args >= 1) {
        args[num_args - 1] = *new_arg;
    }


    /* add terminator at the end */
    args[num_args] = (struct application_argument) COMMAND_LINE_INITIALIZER;
    num_args++;

    *p_num_args = num_args;
}

/*****************************************************************************/
/** @brief   Add the defaults for the selected command line arguments to the long help string

    @note       This application assumes that initialize_application_args() has been 
                called and the memory for p_help_inc_defaults is MAX_LONG_STRING 
                characters long

*/
void initialize_help_string(struct application_argument args[],
                            int num_args,
                            const char *p_help_long, 
                            char *p_help_inc_defaults) {
   
#define MAX_LOCAL_STR 100

    char tmp_str[MAX_LOCAL_STR];
    int len = 0;
    int i;

    log_trace("initialize_help_string");

    snprintf(p_help_inc_defaults, MAX_LONG_STRING, "%s", p_help_long); 

    for (i = 0; i < num_args; i++) {

        /* Ignore any parameters that don't have a description */
        if (args[i].p_long_flag != NULL ) {
            switch (args[i].type) {
                case BOOL_VAR_TYPE:
                    len = snprintf(tmp_str, 
                                   MAX_LOCAL_STR, 
                                   "\t--%s=%s\n", 
                                   args[i].p_long_flag, 
                                   bool_cstr(*(bool *)args[i].p_var));
                    break;

                case STRING_VAR_TYPE:
                    /* snprintf will write AT MOST MAX_LOCAL_STR, but it will actually
                     * only write as much as the length of the created string 
                     */
                    len = snprintf(tmp_str, 
                                   MAX_LOCAL_STR, 
                                   "\t--%s=%s\n", 
                                   args[i].p_long_flag, 
                                   *(char **)(args[i].p_var));
                    break;

                case INT8_VAR_TYPE:
                    len = snprintf(tmp_str, 
                                   MAX_LOCAL_STR, 
                                   "\t--%s=%" PRIi8 "\n", 
                                   args[i].p_long_flag, 
                                   *(int8_t *)(args[i].p_var));
                    break;

                case UINT8_VAR_TYPE:
                    len = snprintf(tmp_str, 
                                   MAX_LOCAL_STR, 
                                   "\t--%s=%" PRIu8 "\n", 
                                   args[i].p_long_flag, 
                                   *(uint8_t *)(args[i].p_var));
                    break;

                case INT16_VAR_TYPE:
                    len = snprintf(tmp_str, 
                                   MAX_LOCAL_STR, 
                                   "\t--%s=%" PRIi16 "\n", 
                                   args[i].p_long_flag, 
                                   *(int16_t *)(args[i].p_var));
                    break;

                case UINT16_VAR_TYPE:
                    len = snprintf(tmp_str, 
                                   MAX_LOCAL_STR, 
                                   "\t--%s=%" PRIu16 "\n", 
                                   args[i].p_long_flag, 
                                   *(uint16_t *)(args[i].p_var));
                    break;

                case INT32_VAR_TYPE:
                    len = snprintf(tmp_str, 
                                   MAX_LOCAL_STR, 
                                   "\t--%s=%" PRIi32 "\n", 
                                   args[i].p_long_flag, 
                                   *(int32_t *)(args[i].p_var));
                    break;

                case UINT32_VAR_TYPE:
                    len = snprintf(tmp_str, 
                                   MAX_LOCAL_STR, 
                                   "\t--%s=%" PRIu32 "\n", 
                                   args[i].p_long_flag, 
                                   *(uint32_t *)(args[i].p_var));
                    break;

                case INT64_VAR_TYPE:
                    len = snprintf(tmp_str, 
                                   MAX_LOCAL_STR, 
                                   "\t--%s=%" PRIi64 "\n", 
                                   args[i].p_long_flag, 
                                   *(int64_t *)(args[i].p_var));
                    break;

                case UINT64_VAR_TYPE:
                    len = snprintf(tmp_str, 
                                   MAX_LOCAL_STR, 
                                   "\t--%s=%" PRIu64 "\n", 
                                   args[i].p_long_flag, 
                                   *(uint64_t *)(args[i].p_var));
                    break;

                case FLOAT_VAR_TYPE:
                    len = snprintf(tmp_str, 
                                   MAX_LOCAL_STR, 
                                   "\t--%s=%f\n", 
                                   args[i].p_long_flag, 
                                   *(float *)(args[i].p_var));
                    break;

                case DOUBLE_VAR_TYPE:
                    len = snprintf(tmp_str, 
                                   MAX_LOCAL_STR, 
                                   "\t--%s=%f\n", 
                                   args[i].p_long_flag, 
                                   *(double *)(args[i].p_var));
                    break;

                default:
                    log_error("Invalid args[].type field %" PRIu8 "", args[i].type);
                    len = 0;
                    break;
            }

            if ((strlen(p_help_inc_defaults) + len) < MAX_LONG_STRING) 
            {
                strcat(p_help_inc_defaults, tmp_str);
            } else {
                log_error("Help string is too long, not concatenating %s", tmp_str);
            }
        }

    }
#undef MAX_LOCAL_STR 
}

/*****************************************************************************/
/** @brief Print out the command line arguments received  

*/
void print_args(uint32_t arg_count, struct application_argument args[])
{
    int i;
    printf("\nCOMMAND LINE PARAMETERS:\n");

    log_trace("print_args");
    for (i=0; i < arg_count-1; i++) {
        switch (args[i].type) {
            case BOOL_VAR_TYPE:
                printf("%-10s\t%s\n",args[i].p_long_flag, bool_cstr(*(bool *)args[i].p_var));
                break;

            case STRING_VAR_TYPE:
                printf("%-10s\t%s\n", args[i].p_long_flag, *(char **)(args[i].p_var));
                break;

            case INT8_VAR_TYPE:
                printf("%-10s\t%" PRIi8 "\n", args[i].p_long_flag, *(int8_t *)(args[i].p_var));
                break;

            case UINT8_VAR_TYPE:
                printf("%-10s\t%" PRIu8 "\n", args[i].p_long_flag, *(uint8_t *)(args[i].p_var));
                break;

            case INT16_VAR_TYPE:
                printf("%-10s\t%" PRIi16 "\n", args[i].p_long_flag, *(int16_t *)(args[i].p_var));
                break;

            case UINT16_VAR_TYPE:
                printf("%-10s\t%" PRIu16 "\n", args[i].p_long_flag, *(uint16_t *)(args[i].p_var));
                break;

            case INT32_VAR_TYPE:
                printf("%-10s\t%" PRIi32 "\n", args[i].p_long_flag, *(int32_t *)(args[i].p_var));
                break;

            case UINT32_VAR_TYPE:
                printf("%-10s\t%" PRIu32 "\n", args[i].p_long_flag, *(uint32_t *)(args[i].p_var));
                break;

            case INT64_VAR_TYPE:
                printf("%-10s\t%" PRIi64 "\n", args[i].p_long_flag, *(int64_t *)(args[i].p_var));
                break;

            case UINT64_VAR_TYPE:
                printf("%-10s\t%" PRIu64 "\n", args[i].p_long_flag, *(uint64_t *)(args[i].p_var));
                break;

            case FLOAT_VAR_TYPE:
                printf("%-10s\t%f\n", args[i].p_long_flag, *(float *)(args[i].p_var));
                break;

            case DOUBLE_VAR_TYPE:
                printf("%-10s\t%f\n", args[i].p_long_flag, *(double *)(args[i].p_var));
                break;

            default:
                log_error("Invalid args[].type field %" PRIu8 "\n", args[i].type);
                break;

        }

#ifdef VERBOSE
        if (args[i].p_is_set != NULL)
        {
            printf("is set\t\t%s\n", bool_cstr((bool)*args[i].p_is_set));
        } 

        printf("is required\t%s\n\n", bool_cstr(args[i].required));
#endif

           
    }

    printf("\n\n");
}


/*************************** ARG PARSER FUNCTIONS *****************************/
/******************************************************************************/


/*****************************************************************************/
/** @brief Convert string containing list delimited by TOKEN_LIST into skiq_rx_hdl_t 
    constant.

*/
int32_t parse_rx_hdl_list( const char *handle_str,
                skiq_rx_hdl_t rx_handles[], 
                uint8_t *p_nr_handles,
                skiq_chan_mode_t *p_chan_mode )
{
    bool handle_requested[skiq_rx_hdl_end];
    skiq_rx_hdl_t rx_hdl;
    char *handle_str_dup;
    int32_t status = 0;

    log_trace("parse_rx_hdl_list");

    *p_nr_handles = 0;

    for ( rx_hdl = skiq_rx_hdl_A1; rx_hdl < skiq_rx_hdl_end; rx_hdl++ )
    {
        handle_requested[rx_hdl] = false;
    }

    handle_str_dup = strdup( handle_str );
    if ( handle_str_dup != NULL )
    {
        char *token = strtok( handle_str_dup, TOKEN_LIST );

        /**
           There are 3 exit criteria for this 'while' loop:

           1. status is non-zero     (invalid handle or duplicate handle)
           2. token is NULL          (strtok has exhausted the string)
           3. handle_str_dup is NULL ('ALL' token was found in the string)
        */
        while ( ( status == 0 ) && ( token != NULL ) && ( handle_str_dup != NULL ) )
        {
            rx_hdl = str2rxhdl( token );
            if ( rx_hdl == skiq_rx_hdl_end )
            {
                if( 0 == strcasecmp(handle_str, "ALL") )
                {
                    free( handle_str_dup );
                    handle_str_dup = NULL;
                }
                else
                {
                    log_error("Invalid handle specified: %s",token);
                    status = ERROR_COMMAND_LINE;
                }
            }
            else
            {
                if (handle_requested[rx_hdl] == true)
                {
                    log_error("handle specified multiple times: %s",token);
                    status = ERROR_COMMAND_LINE;
                }
                else
                {
                    handle_requested[rx_hdl] = true;
                    if ((*p_nr_handles) < skiq_rx_hdl_end)
                    {
                        rx_handles[(*p_nr_handles)++] = rx_hdl;
                    }
                }
            }

            if ( ( status == 0 ) && ( handle_str_dup != NULL ) )
            {
                token = strtok( NULL, TOKEN_LIST );
            }
        }

        if ( handle_str_dup == NULL )
        {
            /* User specified 'ALL' in the list of handles, set number of handles to 0 and return
             * success */
            *p_nr_handles = 0;
            return 0;
        }
        else
        {
            free( handle_str_dup );
            handle_str_dup = NULL;
        }

        /* set chan_mode based on whether one of the second handles in each pair is requested */
        if ( status == 0 && (handle_requested[skiq_rx_hdl_A2] || handle_requested[skiq_rx_hdl_B2]))
        {
            *p_chan_mode = skiq_chan_mode_dual;
        }
        else
        {
            *p_chan_mode = skiq_chan_mode_single;
        }
    }
    else
    {
        log_error("Unable to allocate memory for parsing.");
        status = ERROR_NO_MEMORY;
    }

    if( status == 0 && *p_nr_handles == 0 )
    {
        log_error("No handles specified.");
        /* handle_str_dup has already been freed so no need to do it here */

        status = ERROR_COMMAND_LINE;
    }

    return status;
}

/*****************************************************************************/
/** @brief
    Convert string containing list delimited by TOKEN_LIST into skiq_tx_hdl_t 
    constant.

*/
int32_t parse_tx_hdl_list( const char *handle_str,
                skiq_tx_hdl_t tx_handles[], 
                uint8_t *p_nr_handles,
                skiq_chan_mode_t *p_chan_mode )
{
    bool handle_requested[skiq_tx_hdl_end];
    skiq_tx_hdl_t tx_hdl;
    char *handle_str_dup;
    int32_t status = 0;

    log_trace("parse_tx_hdl_list");

    *p_nr_handles = 0;

    for ( tx_hdl = skiq_tx_hdl_A1; tx_hdl < skiq_tx_hdl_end; tx_hdl++ )
    {
        handle_requested[tx_hdl] = false;
    }

    handle_str_dup = strdup( handle_str );
    if ( handle_str_dup != NULL )
    {
        char *token = strtok( handle_str_dup, TOKEN_LIST );

        /**
           There are 3 exit criteria for this 'while' loop:

           1. status is non-zero     (invalid handle or duplicate handle)
           2. token is NULL          (strtok has exhausted the string)
           3. handle_str_dup is NULL ('ALL' token was found in the string)
        */
        while ( ( status == 0 ) && ( token != NULL ) && ( handle_str_dup != NULL ) )
        {
            tx_hdl = str2txhdl( token );
            if ( tx_hdl == skiq_tx_hdl_end )
            {
                if( 0 == strcasecmp(handle_str, "ALL") )
                {
                    free( handle_str_dup );
                    handle_str_dup = NULL;
                }
                else
                {
                    log_error("Invalid handle specified: %s",token);
                    status = ERROR_COMMAND_LINE;
                }
            }
            else
            {
                if (handle_requested[tx_hdl] == true)
                {
                    log_error("Handle specified multiple times: %s",token);
                    status = ERROR_COMMAND_LINE;
                }
                else
                {
                    handle_requested[tx_hdl] = true;
                    if ((*p_nr_handles) < skiq_tx_hdl_end)
                    {
                        tx_handles[(*p_nr_handles)++] = tx_hdl;
                    }
                }
            }

            if ( ( status == 0 ) && ( handle_str_dup != NULL ) )
            {
                token = strtok( NULL, TOKEN_LIST );
            }
        }

        if ( handle_str_dup == NULL )
        {
            /* User specified 'ALL' in the list of handles, set number of handles to 0 and return
             * success */
            *p_nr_handles = 0;
            return 0;
        }
        else
        {
            free( handle_str_dup );
            handle_str_dup = NULL;
        }

        /* For TX dual mode is set if A1/A2 or A1/B1 */
        if ( status == 0 && 
                ((handle_requested[skiq_rx_hdl_A1] && handle_requested[skiq_rx_hdl_A2]) || 
                 (handle_requested[skiq_rx_hdl_A1] && handle_requested[skiq_rx_hdl_B1]) ))
        {
            *p_chan_mode = skiq_chan_mode_dual;
        }
        else
        {
            *p_chan_mode = skiq_chan_mode_single;
        }
    }
    else
    {
        log_error("Unable to allocate memory for parsing.");
        status = ERROR_NO_MEMORY;
    }

    if( status == 0 && *p_nr_handles == 0 )
    {
        log_error("No handles specified.");
        /* handle_str_dup has already been freed so no need to do it here */

        status = ERROR_COMMAND_LINE;
    }

    return status;
}

/***************************** HELPER FUNCTIONS *******************************/

/******************************************************************************/
/** @brief   Converts a boolean to a printable string
 *
*/
const char * bool_cstr( bool flag )
{
    const char* p_bool_str = (flag) ? "true" : "false";

    return p_bool_str;
}

/*****************************************************************************/
/** @brief Convert string representation to handle constant

*/
skiq_rx_hdl_t str2rxhdl( const char *str )
{
    return \
        ( 0 == strcasecmp( str, "A1" ) ) ? skiq_rx_hdl_A1 :
        ( 0 == strcasecmp( str, "A2" ) ) ? skiq_rx_hdl_A2 :
        ( 0 == strcasecmp( str, "B1" ) ) ? skiq_rx_hdl_B1 :
        ( 0 == strcasecmp( str, "B2" ) ) ? skiq_rx_hdl_B2 :
        ( 0 == strcasecmp( str, "C1" ) ) ? skiq_rx_hdl_C1 :
        ( 0 == strcasecmp( str, "D1" ) ) ? skiq_rx_hdl_D1 :
        skiq_rx_hdl_end;
}


/*****************************************************************************/
/** @brief 
    Convert string representation to handle constant

*/
skiq_tx_hdl_t str2txhdl( const char *str )
{
    return \
        ( 0 == strcasecmp( str, "A1" ) ) ? skiq_tx_hdl_A1 :
        ( 0 == strcasecmp( str, "A2" ) ) ? skiq_tx_hdl_A2 :
        ( 0 == strcasecmp( str, "B1" ) ) ? skiq_tx_hdl_B1 :
        ( 0 == strcasecmp( str, "B2" ) ) ? skiq_tx_hdl_B2 :
        skiq_tx_hdl_end;
}

/******************************************************************************/
/** @brief Convert skiq_rx_hdl_t constant to string representation

*/
const char * rxhdl_cstr( skiq_rx_hdl_t hdl )
{
    return \
        (hdl == skiq_rx_hdl_A1) ? "A1" :
        (hdl == skiq_rx_hdl_A2) ? "A2" :
        (hdl == skiq_rx_hdl_B1) ? "B1" :
        (hdl == skiq_rx_hdl_B2) ? "B2" :
        (hdl == skiq_rx_hdl_C1) ? "C1" :
        (hdl == skiq_rx_hdl_D1) ? "D1" :
        "unknown";
}

/******************************************************************************/
/** @brief 
    Convert skiq_tx_hdl_t constant to string representation

*/
const char * txhdl_cstr( skiq_tx_hdl_t hdl )
{
    return \
        (hdl == skiq_tx_hdl_A1) ? "A1" :
        (hdl == skiq_tx_hdl_A2) ? "A2" :
        (hdl == skiq_tx_hdl_B1) ? "B1" :
        (hdl == skiq_tx_hdl_B2) ? "B2" :
        "unknown";
}

/*****************************************************************************/
/** @brief Convert skiq_1pps_source_t to string representation

*/
const char * pps_source_cstr( skiq_1pps_source_t source )
{
    return \
        (skiq_1pps_source_unavailable == source) ? "unavailable" :
        (skiq_1pps_source_external == source)    ? "external" :
        (skiq_1pps_source_host == source)        ? "host" :
        "unknown";
}


/*****************************************************************************/
/** @brief Convert skiq_trigger_src_t constant to string representation

*/
const char * trigger_src_desc_cstr( skiq_trigger_src_t src )
{
    return \
        (src == skiq_trigger_src_immediate) ? "immediately" :
        (src == skiq_trigger_src_1pps)      ? "on next 1PPS pulse" :
        (src == skiq_trigger_src_synced)    ? "with aligned timestamps" :
        "unknown";
}


/*****************************************************************************/
/** @brief Convert skiq_chan_mode_t constant to string representation

*/
const char * chan_mode_desc_cstr( skiq_chan_mode_t mode )
{
    return \
        (mode == skiq_chan_mode_dual)   ? "dual" :
        (mode == skiq_chan_mode_single) ? "single" :
        "unknown";
}

/*****************************************************************************/
/** @brief Convert the string version of the RF Port into a skiq_rf_port_t

*/
skiq_rf_port_t map_str_to_rf_port( char *str )
{
    return ( 0 == strcasecmp( str, "J1"   ) ) ? skiq_rf_port_J1 :
           ( 0 == strcasecmp( str, "J2"   ) ) ? skiq_rf_port_J2 :
           ( 0 == strcasecmp( str, "J3"   ) ) ? skiq_rf_port_J3 :
           ( 0 == strcasecmp( str, "J4"   ) ) ? skiq_rf_port_J4 :
           ( 0 == strcasecmp( str, "J5"   ) ) ? skiq_rf_port_J5 :
           ( 0 == strcasecmp( str, "J6"   ) ) ? skiq_rf_port_J6 :
           ( 0 == strcasecmp( str, "J7"   ) ) ? skiq_rf_port_J7 :
           ( 0 == strcasecmp( str, "J8"   ) ) ? skiq_rf_port_J8 :
           ( 0 == strcasecmp( str, "J300" ) ) ? skiq_rf_port_J300 :
           ( 0 == strcasecmp( str, "RX1"  ) ) ? skiq_rf_port_Jxxx_RX1 :
           ( 0 == strcasecmp( str, "RX2"  ) ) ? skiq_rf_port_Jxxx_TX1RX2 :
           skiq_rf_port_max;
}


/*****************************************************************************/
/** @brief Dump rconfig to console for debugging

*/
void dump_rconfig(const struct radio_config *p_rconfig, 
                  const struct rx_radio_config *p_rx_rconfig,
                  const struct tx_radio_config *p_tx_rconfig)
{

    log_trace("in dump_rconfig:");

    /* only print out if the logging level is DEBUG or higher */
    if ( L.level <= LLOG_DEBUG)
    {
        printf("\nPrinting rconfig parameters:\n");
        if( p_rconfig != NULL )
        {

            printf("\n Common Radio parameters:");
            printf("\nskiq_initialized: %s",       bool_cstr(p_rconfig->skiq_initialized));
            printf("\nnumber of cards   %" PRIu8,  p_rconfig->num_cards );
            printf("\nsample_rate:      %" PRIu32, p_rconfig->sample_rate );
            printf("\nbandwidth:        %" PRIu32, p_rconfig->bandwidth );
            printf("\niq_order_mode:    %" PRIi32, (int)p_rconfig->sample_order_iq); 
            printf("\npacked:           %s",       bool_cstr(p_rconfig->packed));  
            printf("\npps_source:       %s",       pps_source_cstr(p_rconfig->pps_source) );
        }

        if( p_rx_rconfig != NULL )
        {
            printf("\n\n RX parameters:");
            printf("\nrf_port:          %" PRIi32, (int32_t)p_rx_rconfig->rf_port );
            printf("\nrf_port_usr:      %s",       p_rx_rconfig->rf_port_usr );
            printf("\nfreq:             %" PRIu64, p_rx_rconfig->freq );
            printf("\ngain:             %" PRIu8,  p_rx_rconfig->gain );
            printf("\ngain_manual:      %s",       bool_cstr(p_rx_rconfig->gain_manual)); 
            printf("\nrx_blocking:      %s",       bool_cstr(p_rx_rconfig->rx_blocking));
            printf("\nuse_counter:      %s",       bool_cstr(p_rx_rconfig->use_counter)); 
            printf("\ndisable_dc_corr:  %s",       bool_cstr(p_rx_rconfig->disable_dc_corr));
            printf("\ntrigger_src:      %s",       trigger_src_desc_cstr(p_rx_rconfig->trigger_src) );
            printf("\nall_chans:        %s",       bool_cstr(p_rx_rconfig->all_chans));
        }

        if( p_tx_rconfig != NULL )
        {
            printf("\n\n TX parameters:");
            printf("\nrf_port:          %" PRIi32, (int32_t)p_tx_rconfig->rf_port );
            printf("\nrf_port_usr:      %s",       p_tx_rconfig->rf_port_usr );
            printf("\nfreq:             %" PRIu64, p_tx_rconfig->freq );
            printf("\nattenuation:      %" PRIu8,  p_tx_rconfig->attenuation );
            printf("\nblock_size_in_words:  %" PRIu16, p_tx_rconfig->block_size_in_words );
            printf("\ntimestamp_base:   %" PRIu32, p_tx_rconfig->timestamp_base );
            printf("\ntimestamp_value:  %" PRIu64, p_tx_rconfig->timestamp_value );
            printf("\nuse_late_timestamp: %s",     bool_cstr(p_tx_rconfig->use_late_timestamp ));
            printf("\nthreads:          %" PRIu8,  p_tx_rconfig->num_threads );
            printf("\nthread_priority:  %" PRIi32, p_tx_rconfig->thread_priority );
            printf("\ntransfer_mode:    %" PRIi32 "\n", p_tx_rconfig->transfer_mode );

        }
        uint8_t card_index;
        uint8_t num_cards = p_rconfig->num_cards;
        for( card_index = 0; card_index < num_cards; card_index++ )
        {
            if( p_rconfig != NULL && p_rx_rconfig != NULL )
            {
                uint8_t card_id     = p_rconfig->cards[card_index];
                uint8_t num_handles = p_rx_rconfig->nr_handles[card_id];

                printf("\ncard   %" PRIu8, card_id);
                printf("\n       rx chan mode   %s", chan_mode_desc_cstr(p_rx_rconfig->chan_mode[card_id]));
                printf("\n       rx_num handles    %" PRIu8, num_handles);
                printf("\n       handles         - ");

                uint8_t handle_index;
                for( handle_index = 0; handle_index < num_handles; handle_index++ )
                {
                    printf("%s,", rxhdl_cstr(p_rx_rconfig->handles[card_id][handle_index]));
                }
                printf("\n");
            }

            if( p_rconfig != NULL && p_tx_rconfig != NULL )
            {
                uint8_t card_id     = p_rconfig->cards[card_index];
                uint8_t num_handles = p_tx_rconfig->nr_handles[card_id];

                printf("\ncard   %" PRIu8, card_id);
                printf("\n       tx chan mode   %s", chan_mode_desc_cstr(p_tx_rconfig->chan_mode[card_id]));
                printf("\n       tx_num handles    %" PRIu8, num_handles);
                printf("\n       handles         - ");

                uint8_t handle_index;
                for( handle_index = 0; handle_index < num_handles; handle_index++ )
                {
                    printf("%s,", txhdl_cstr(p_tx_rconfig->handles[card_id][handle_index]));
                }
                printf("\n");
            }
        }
        printf("\n\n");
    }

}

/*****************************************************************************/
/** @brief Dump args array for debugging

*/
void print_arg_array( struct application_argument args[], int num_args) {
    int i;
    printf("COMMAND LINE ARG ARRAY\n");

    for (i = 0; i < num_args; i++){
        printf("i           %d\n", i);
        printf("p_long_flag %s\n", args[i].p_long_flag);
        printf("short_flag  %c\n", args[i].short_flag);
        printf("p_info      %s\n", args[i].p_info);
        printf("p_label     %s\n", args[i].p_label);
        printf("p_var       %p\n", args[i].p_var);
        printf("type        %d\n", args[i].type);
        printf("required    %d\n", args[i].required);
        printf("p_is_set    %p\n\n", args[i].p_is_set);
    }

} 


/********************* RADIO CONFIGURATION FUNCTIONS *************************/
/*****************************************************************************/

/*****************************************************************************/
/** @brief Get ALL handles for a specific card

*/
int32_t get_all_rx_handles( uint8_t card, 
                            skiq_rx_hdl_t rx_handles[], 
                            uint8_t *p_nr_handles,
                            skiq_chan_mode_t *p_chan_mode )
{
    uint8_t         hdl_idx;
    skiq_rx_hdl_t   hdl;
    int32_t         status;
    skiq_param_t    params;

    log_trace("get_all_rx_handles");

    log_trace("skiq_read_parameters");
    status = skiq_read_parameters( card, &params );
    if (0 != status)
    {
        log_error("Failed to read parameters on card %" PRIu8 " with status %"
                PRIi32 "\n", card, status);
    }
    else
    {
        for ( hdl_idx = 0; hdl_idx < params.rf_param.num_rx_channels; hdl_idx++ )
        {
            skiq_rx_hdl_t hdl_conflicts[skiq_rx_hdl_end] = INIT_ARRAY(skiq_rx_hdl_end, skiq_rx_hdl_end);
            uint8_t       num_conflicts = 0;

            /* Assign the receive handle associated with the hdl_idx index */
            hdl = params.rf_param.rx_handles[hdl_idx];

            /* Determine which handles conflict with the current handle */
            log_trace("skiq_read_rx_stream_handle_conflict");
            status = skiq_read_rx_stream_handle_conflict(card, hdl, hdl_conflicts,&num_conflicts);
            if (status != 0)
            {
                log_error("Failed to read rx_stream_handle_conflict on"
                       " card %" PRIu8 "  with status %"
                       PRIi32 "", card, status);
            }
            else
            {
                uint8_t hdl_conflict_idx = 0;
                bool    safe_to_add      = true;

                /* Add the current handle if none of the conflicting handles have already been added */
                for( hdl_conflict_idx=0; (hdl_conflict_idx<num_conflicts) && (status==0); hdl_conflict_idx++ )
                {
                    uint8_t configured_hdls_idx = 0;
                    for ( configured_hdls_idx=0;configured_hdls_idx < *p_nr_handles; configured_hdls_idx++)
                    {
                        if (rx_handles[configured_hdls_idx] == hdl_conflicts[hdl_conflict_idx])
                        {
                            safe_to_add = false;
                            break;
                        }
                    }
                }
                if (safe_to_add)
                {
                    rx_handles[(*p_nr_handles)++] = hdl;
                }
            }

            if( params.rf_param.num_rx_channels > 1 )
            {
                *p_chan_mode = skiq_chan_mode_dual;
            }
            else
            {
                *p_chan_mode = skiq_chan_mode_single;
            }
        }

        if( status == 0 )
        {
            log_info("card %" PRIu8 " using all RX handles (total number of channels is %" PRIu8
                   ") mode: %s", card, *p_nr_handles, chan_mode_desc_cstr(*p_chan_mode));
        }
    }

    return status;
}

/*****************************************************************************/
/** @brief 
    Get ALL handles for a specific card

*/
int32_t get_all_tx_handles( uint8_t card, 
                            skiq_tx_hdl_t tx_handles[], 
                            uint8_t *p_nr_handles,
                            skiq_chan_mode_t *p_chan_mode )
{
    uint8_t         hdl_idx;
    skiq_tx_hdl_t   hdl;
    int32_t         status;
    skiq_param_t    params;

    log_trace("get_all_tx_handles");

    log_trace("skiq_read_parameters");
    status = skiq_read_parameters( card, &params );
    if (0 != status)
    {
        log_error("Failed to read parameters on card %" PRIu8 " with status %"
                PRIi32 "", card, status);
    }
    else
    {
        for ( hdl_idx = 0; hdl_idx < params.rf_param.num_rx_channels; hdl_idx++ )
        {
            /* Assign the receive handle associated with the hdl_idx index */
            hdl = params.rf_param.tx_handles[hdl_idx];


            tx_handles[(*p_nr_handles)++] = hdl;

            if( params.rf_param.num_tx_channels > 1 )
            {
                *p_chan_mode = skiq_chan_mode_dual;
            }
            else
            {
                *p_chan_mode = skiq_chan_mode_single;
            }
        }

        if( status == 0 )
        {
            log_info("card %" PRIu8 " using all TX handles (total number of channels is %" PRIu8
                   ") mode: %s", card, *p_nr_handles, chan_mode_desc_cstr(*p_chan_mode));
        }
    }

    return status;
}

/*****************************************************************************/
/** @brief Map command line arguments to rconfig structure
    Set the following fields in radio_config:
*/
int32_t map_arguments_to_radio_config(  struct cmd_line_args *p_cmd_line_args, 
                                        struct radio_config *p_rconfig )
{
    int32_t status = 0;

    log_trace("map_arguments_to_radio_config");

    /* Initialize rconfig just to be safe */
    *p_rconfig = (struct radio_config)RADIO_CONFIG_INITIALIZER;

    p_rconfig->packed           = p_cmd_line_args->packed;
    p_rconfig->sample_rate      = p_cmd_line_args->sample_rate;

    /* If specified, attempt to find the card with a matching serial number. */
    uint8_t card = 0;
    if (p_cmd_line_args->p_serial_is_present == true) 
    {
        log_trace("skiq_get_card_from_serial_string");
        status = skiq_get_card_from_serial_string(p_cmd_line_args->p_serial, &card);
        if (0 != status)
        {
            log_error("Cannot find card with serial number %s (result"
                    " code %" PRIi32 ")", p_cmd_line_args->p_serial, status);
            return status;
        }

        if (status == 0)
        {
            log_info("found serial number %s as card ID %" PRIu8 ,
                    p_cmd_line_args->p_serial, card);

            /* set up the command line args to the card specified.  */
            p_cmd_line_args->card_id = card;
            p_cmd_line_args->card_is_present = true;
        }

    }

    /* if the user did not specify a card default to all cards */
    if(p_cmd_line_args->card_is_present == false) 
    {
        log_trace("skiq_get_cards");
        status = skiq_get_cards( skiq_xport_type_auto, &p_rconfig->num_cards, p_rconfig->cards );
        if( status != 0 )
        {
            log_error("Unable to acquire number of cards (result code %" PRIi32 ")", 
                    status);
            return status;
        }
        if( p_rconfig->num_cards == 0 )
        {
            log_error("No cards detected");
            status = ERROR_CARD_CONFIGURATION;
            return status;
        }
    }
    else
    {
        /* the user specified a card */
        if ( (SKIQ_MAX_NUM_CARDS - 1) < p_cmd_line_args->card_id )
        {
            log_error("Card ID %" PRIu8 " exceeds the maximum card ID"
                    " (%" PRIu8 ")", p_cmd_line_args->card_id, (SKIQ_MAX_NUM_CARDS - 1));
            status = ERROR_COMMAND_LINE;
            return status;
        }
        p_rconfig->cards[0] = p_cmd_line_args->card_id;
        p_rconfig->num_cards = 1;
    }

    /* if no bandwidth given, make it 80% of the sample rate */
    if (status == 0)
    {
        if (p_cmd_line_args->bw_is_present == true)
        {    
            p_rconfig->bandwidth        = p_cmd_line_args->bandwidth;

        } 
        else 
        {
            p_rconfig->bandwidth        = p_cmd_line_args->sample_rate * (0.8);
            p_cmd_line_args->bandwidth  = p_rconfig->bandwidth;
            log_info("RF bandwidth not specified, so configuring to 80 percent of the sample rate %" 
                    PRIu32 , p_rconfig->bandwidth);
        }
    }
    if (status == 0 && p_cmd_line_args->pps_source_is_present)
    {
        if( 0 == strcasecmp(p_cmd_line_args->p_pps_source, "host") )
        {
            p_rconfig->pps_source = skiq_1pps_source_host;
        }
        else if( 0 == strcasecmp(p_cmd_line_args->p_pps_source, "external") )
        {
            p_rconfig->pps_source = skiq_1pps_source_external;
        }
        else
        {
            log_error("Invalid 1PPS source '%s' specified", p_cmd_line_args->p_pps_source);
            status = ERROR_COMMAND_LINE;
            return status;
        }
    }

    if (status == 0)
    {
        if (p_cmd_line_args->sample_order_qi == true)
        {
            p_rconfig->sample_order_iq  = skiq_iq_order_qi;
        }
        else
        {
            p_rconfig->sample_order_iq  = skiq_iq_order_iq;
        }
    }


    return status;
}

/*****************************************************************************/
/** @brief 
    Map command line arguments to rconfig structure
    Set the following fields in radio_config:
*/
int32_t map_arguments_to_rx_radio_config(  struct cmd_line_args *p_cmd_line_args, 
                                           struct radio_config *p_rconfig,
                                           struct rx_radio_config *p_rx_rconfig )
{
    int32_t status = 0;

    log_trace("map_arguments_to_rx_radio_config");

    /* Initialize rconfig just to be safe */
    *p_rx_rconfig = (struct rx_radio_config)RX_RADIO_CONFIG_INITIALIZER;

    /* Copy data that (at this time) does not require conversion from command line args */
    p_rx_rconfig->freq             = p_cmd_line_args->rx_freq;
    p_rx_rconfig->gain             = p_cmd_line_args->rx_gain;
    p_rx_rconfig->rx_blocking      = p_cmd_line_args->rx_blocking;
    p_rx_rconfig->use_counter      = p_cmd_line_args->use_counter;
    p_rx_rconfig->disable_dc_corr  = p_cmd_line_args->disable_dc_corr;
    p_rx_rconfig->gain_manual      = p_cmd_line_args->rx_gain_manual;


    /* map the rx handles */
    if( status == 0 )
    {
        skiq_rx_hdl_t handles[skiq_rx_hdl_end]  = INIT_ARRAY(skiq_rx_hdl_end, skiq_rx_hdl_end);
        skiq_chan_mode_t chan_mode              = skiq_chan_mode_single;
        uint8_t nr_handles                      = 0;

        /* map argument values to Sidekiq specific variable values */
        status = parse_rx_hdl_list( p_cmd_line_args->p_rx_hdl, handles, &nr_handles, &chan_mode );
        if ( status != 0 )
        {
            log_error("Parsing handles");
            status = ERROR_COMMAND_LINE;
            return status;
        }

        /* If nr_handles == 0 then use ALL handles */
        if( nr_handles == 0 )
        {
            p_rx_rconfig->all_chans = true;
        }
        else
        {
            /* TODO What happens if the mode and handle list are different per card? */
            
            /* copy the handles to all the card's parameters */
            uint8_t i, j;
            for( i=0; i<p_rconfig->num_cards; i++ )
            {
                uint8_t card = p_rconfig->cards[i]; 
                p_rx_rconfig->nr_handles[card] = nr_handles;
                p_rx_rconfig->chan_mode[card] = chan_mode;
                for( j=0; j<skiq_rx_hdl_end; j++ )
                {
                    /* p_rconfig->handles must be of length skiq_rx_hdl_end+1
                    */
                    p_rx_rconfig->handles[card][j] = handles[j];
                }
            }

        }
    }

    /* map the RF port if it was specified */
    if (status == 0)
    {
        if( p_cmd_line_args->rx_rf_port_is_present == true)
        {
            p_rx_rconfig->rf_port = map_str_to_rf_port( p_cmd_line_args->rx_rf_port );

            if( p_rx_rconfig->rf_port == skiq_rf_port_unknown )
            {
                log_error("Unknown RF port specified %s", p_cmd_line_args->rx_rf_port);
                status = ERROR_COMMAND_LINE;
                return status;
            }

            p_rx_rconfig->rf_port_usr = p_cmd_line_args->rx_rf_port;
        }
        else
        {
            p_rx_rconfig->rf_port = skiq_rf_port_unknown;
        }
    }
   

    
    /* map the trigger_src */
    if( status == 0 )
    {
        if ( 0 == strcasecmp( p_cmd_line_args->p_trigger_src, "immediate" ) )
        {
            p_rx_rconfig->trigger_src = skiq_trigger_src_immediate;
        }
        else if ( 0 == strcasecmp( p_cmd_line_args->p_trigger_src, "1pps" ) )
        {
            p_rx_rconfig->trigger_src = skiq_trigger_src_1pps;
        }
        else if ( 0 == strcasecmp( p_cmd_line_args->p_trigger_src, "synced" ) )
        {
            p_rx_rconfig->trigger_src = skiq_trigger_src_synced;
        }
        else
        {
            log_error("Invalid trigger source '%s' specified", p_cmd_line_args->p_trigger_src);
            status = ERROR_COMMAND_LINE;
            return status;
        }
    }



    if( status == 0 )
    {
        if ( p_cmd_line_args->include_meta == true )
        {
            log_info("including metadata in capture output");
        }
    }

    return status;
}

/*****************************************************************************/
/** @brief 
    Map command line arguments to rconfig structure
    Set the following fields in radio_config:
*/
int32_t map_arguments_to_tx_radio_config(  struct cmd_line_args *p_cmd_line_args, 
                                           struct radio_config *p_rconfig,
                                           struct tx_radio_config *p_tx_rconfig )
{
    int32_t status = 0;

    log_trace("map_arguments_to_tx_radio_config");

    /* Initialize rconfig just to be safe */
    *p_tx_rconfig = (struct tx_radio_config)TX_RADIO_CONFIG_INITIALIZER;

    /* Copy data that (at this time) does not require conversion from command line args */
    p_tx_rconfig->freq                = p_cmd_line_args->tx_freq;
    p_tx_rconfig->attenuation         = p_cmd_line_args->attenuation;
    p_tx_rconfig->block_size_in_words = p_cmd_line_args->block_size;
    p_tx_rconfig->timestamp_value     = p_cmd_line_args->timestamp_value;
    p_tx_rconfig->use_late_timestamp  = p_cmd_line_args->use_late_timestamp;
    p_tx_rconfig->num_threads         = p_cmd_line_args->num_threads;
    p_tx_rconfig->thread_priority     = p_cmd_line_args->thread_priority;


    /* map the tx handles */
    if( status == 0 )
    {
        skiq_tx_hdl_t handles[skiq_rx_hdl_end]  = INIT_ARRAY(skiq_tx_hdl_end, skiq_tx_hdl_end);
        skiq_chan_mode_t chan_mode              = skiq_chan_mode_single;
        uint8_t nr_handles                      = 0;

        /* map argument values to Sidekiq specific variable values */
        status = parse_tx_hdl_list( p_cmd_line_args->p_tx_hdl, handles, &nr_handles, &chan_mode );
        if ( status != 0 )
        {
            log_error("Parsing handles, status is %" PRIi32 " ", status);
            status = ERROR_COMMAND_LINE;
            return status;
        }
        else
        {
            /* If nr_handles == 0 then use ALL handles */
            if( nr_handles == 0 )
            {
                p_tx_rconfig->all_chans = true;
            }
            else
            {
                /* TODO What happens if the mode and handle list are different per card? */
                
                /* copy the handles to all the card's parameters */
                uint8_t i, j;
                for( i=0; i<p_rconfig->num_cards; i++ )
                {
                    uint8_t card = p_rconfig->cards[i]; 
                    p_tx_rconfig->nr_handles[card] = nr_handles;
                    p_tx_rconfig->chan_mode[card] = chan_mode;
                    for( j=0; j<skiq_tx_hdl_end; j++ )
                    {
                        /* p_rconfig->handles must be of length skiq_rx_hdl_end+1
                        */
                        p_tx_rconfig->handles[card][j] = handles[j];
                    }
                }

            }
        }
    }

    /* map the RF port if it was specified */
    if (status == 0)
    {
        if( p_cmd_line_args->tx_rf_port_is_present == true)
        {
            p_tx_rconfig->rf_port = map_str_to_rf_port( p_cmd_line_args->tx_rf_port );

            if( p_tx_rconfig->rf_port == skiq_rf_port_unknown )
            {
                log_error("Unknown RF port specified %s", p_cmd_line_args->tx_rf_port);
                status = ERROR_COMMAND_LINE;
                return status;
            }
            p_tx_rconfig->rf_port_usr = p_cmd_line_args->tx_rf_port;
        }
        else
        {
            p_tx_rconfig->rf_port = skiq_rf_port_unknown;
        }
    }
   
    
    /* map the timestamp_base */
    if( status == 0 )
    {
        if ( 0 == strcasecmp( p_cmd_line_args->timestamp_base, "rf" ) )
        {
            p_tx_rconfig->timestamp_base = skiq_tx_rf_timestamp;
        }
        else if ( 0 == strcasecmp( p_cmd_line_args->timestamp_base, "system" ) )
        {
            p_tx_rconfig->timestamp_base = skiq_tx_system_timestamp;
        }
        else
        {
            log_error("Invalid trigger source '%s' specified", p_cmd_line_args->p_trigger_src);
            status = ERROR_COMMAND_LINE;
            return status;
        }
    }

    /* map the transfer mode */
    if (status == 0)
    {
        if (p_cmd_line_args->threads_is_present == true && p_cmd_line_args->num_threads > 1)
        {
            p_tx_rconfig->transfer_mode = skiq_tx_transfer_mode_async;
        }
        else
        {
            p_tx_rconfig->transfer_mode = skiq_tx_transfer_mode_sync;
        }
    }


    return status;
}

/*****************************************************************************/
/** @brief
    Initializes the libsidekiq library.  Then enables the specific cards.
    Then it gets all the handles for the cards.

*/
int32_t init_libsidekiq( skiq_xport_init_level_t init_level, struct radio_config *p_rconfig ) {
    int32_t status = 0;
    int i;

    log_trace("init_libsidekiq");

    if( p_rconfig->skiq_initialized == false )
    {
        /*************************** init libsidekiq **************************/
        /* initialize libsidekiq at a full level 
        */
        log_info("initializing libsidekiq");
        /* Register our own logging function before initializing the library */

        log_trace("skiq_init_without_cards");
        status = skiq_init_without_cards();
        if (status != 0) 
        {
            log_error("Unable to initialize libsidekiq; status %" PRIi32 "", status);
            return status;
        } 
        p_rconfig->skiq_initialized = true;

        /* Loop through each card and attempt to enable it.  */
        for (i = 0; i < p_rconfig->num_cards && (status == 0); i++) {
            log_trace("skiq_enable_cards");
            status = skiq_enable_cards(&p_rconfig->cards[i], 1, init_level);

            if( status != 0 )
            {
                if ( EBUSY == status )
                {
                    log_error("Unable to initialize libsidekiq; card = %" PRIi32  
                            " seem to be in use (result code %" PRIi32 ")", i, status); 
                    break;
                }
                else if ( -EINVAL == status )
                {
                    log_error("Unable to initialize libsidekiq; was a valid card"
                           " specified? card = %" PRIi32 "(result code %" PRIi32 ")", i, status);
                    break;
                }
                else
                {
                    log_error("Unable to initialize libsidekiq card = %" PRIi32 
                            "with status %" PRIi32
                           "", i, status);
                    break;
                }
            }
        }

        /* if we had a failure skiq_exit */
        if (status != 0) 
        {
            if (p_rconfig->skiq_initialized == true) 
            {
                skiq_exit();
                p_rconfig->skiq_initialized = false;
            }
        } 
       

    }

    return status;
}

/*****************************************************************************/
/** @brief 
    Configures the specific card variables that are not RX or TX based.

*/
int32_t configure_radio( uint8_t card, struct radio_config *p_rconfig )
{
    int32_t status = 0;
    log_trace("configure_radio");


    if( p_rconfig->skiq_initialized == true && status == 0 )
    {

        log_info("card %" PRIu8 " starting radio configuration", card);
    }
    else
    {
        status = ERROR_LIBSIDEKIQ_NOT_INITIALIZED;
        return status;
    }

    if (status == 0)
    {
        log_trace("skiq_write_iq_order_mode");
        status = skiq_write_iq_order_mode(card, p_rconfig->sample_order_iq);
        if( 0 != status )
        {
            log_error("Card %" PRIu8 " failed to set iq_order_mode"
                   " (status %" PRIi32 ")", card, status);
            return status;
        }
        if (p_rconfig->sample_order_iq == skiq_iq_order_iq)
        {
            log_info("card %" PRIu8 " configured for I/Q order mode", card);
        }
        else
        {
            log_info("card %" PRIu8 " configured for Q/I order mode", card);
        }

    }
    
    if ( status == 0 )
    {
        /* set the mode (packed) 
           An interface defaults to using un-packed mode if the skiq write iq pack mode()
           is not called.*/
        if( p_rconfig->packed )
        {
            log_trace("skiq_write_iq_pack_mode");
            status = skiq_write_iq_pack_mode(card, p_rconfig->packed);
            if ( status == -ENOTSUP )
            {
                log_error("Card %" PRIu8 " packed mode is not supported on this Sidekiq product "
                        "", card);
                return status;
            }
            else if ( status != 0 )
            {
                log_error("Card %" PRIu8 " unable to set the packed mode (status %" PRIi32 ")",
                        card, status);
                return status;
            }
            else
            {
                log_info("card %" PRIu8 " configured for packed data mode", card);
            }
        }
    }

    /* configure the 1PPS source */ 
    if( ( status == 0 ) && ( p_rconfig->pps_source != skiq_1pps_source_unavailable ))
    {
        log_trace("skiq_write_1pps_source");
        status = skiq_write_1pps_source( card, p_rconfig->pps_source );
        if( status != 0 )
        {
            log_error("Card %" PRIu8 " unable to configure PPS source to %s (status=%" PRIi32 ")",
                  card, pps_source_cstr( p_rconfig->pps_source ), status );
            return status;
        }
        else
        {
            log_info("card %" PRIu8 " configured 1PPS source to %s", card, pps_source_cstr( p_rconfig->pps_source) );
        }
    }


    return status;
}

/*****************************************************************************/
/** @brief 
    Configures the rx sample rate and bandwidth
    
    @param[in]      card:           card to initialize
    @param[in/out]  *p_rconfig:     pointer to radio_config structure
    @param[in/out]  *p_rx_rconfig:  pointer to radio_config structure

    @return         status:         indicating status


    @note  This function is internal only and called from rx_radio_config

*/
static int32_t configure_rx_sample_rate_and_bandwidth(uint8_t card, 
                                                struct radio_config *p_rconfig, 
                                                struct rx_radio_config *p_rx_rconfig)
{
    int status = 0;
    int hdl_idx;
    
    log_trace("configure_rx_sample_rate_and_bandwidth");

    for (hdl_idx = 0; (hdl_idx < p_rx_rconfig->nr_handles[card]) && (status == 0); hdl_idx++)
    {
        skiq_rx_hdl_t hdl = p_rx_rconfig->handles[card][hdl_idx];

        log_trace("skiq_write_rx_sample_rate_and_bandwidth %s", rxhdl_cstr(hdl));
        status = skiq_write_rx_sample_rate_and_bandwidth(card, hdl,
                                                         p_rconfig->sample_rate,
                                                         p_rconfig->bandwidth);
        if (status != 0)
        {
            log_error("Card %" PRIu8 " handle %s failed to set Rx sample rate or bandwidth "
                    "status is %" PRIi32 "", 
                    card, rxhdl_cstr(hdl), status);
            return status;
                   
        }
        uint32_t read_sample_rate;
        uint32_t read_bandwidth;
        uint32_t actual_bandwidth;
        double   actual_sample_rate;

        /* read back the sample rate and bandwidth to determine the achieved bandwidth */
        log_trace("skiq_read_rx_sample_rate_and_bandwidth");
        status = skiq_read_rx_sample_rate_and_bandwidth(card,
                                                        hdl,
                                                        &read_sample_rate,
                                                        &actual_sample_rate,
                                                        &read_bandwidth,
                                                        &actual_bandwidth);
        if ( status != 0 )
        {
            log_error("Card %" PRIu8 " handle %s failed to read sample rate and bandwidth" 
                    " status is %" PRIi32 "", 
                    card, rxhdl_cstr(hdl), status);
            return status;
        }

        /* some cards have limited sample rates that are allowable by the card 
         * So it is possible they requested a sample rate in range, but the card actually set 
         * a different value.  So we display it here and modify to what we sent in the radio config
         * structure */ 
        log_info("card %" PRIu8 " handle %s RX actual sample rate set is %" 
                PRIu32 ", actual bandwidth set is %" PRIu32 ,
                card, rxhdl_cstr(hdl), (uint32_t)actual_sample_rate, actual_bandwidth);

        p_rconfig->sample_rate = actual_sample_rate;
        p_rconfig->bandwidth = actual_bandwidth;
    }


    return status;
}

/*****************************************************************************/
/** @brief 
    Configures the rx rf port
    
    @param[in]      card:           card to initialize
    @param[in/out]  *p_rconfig:     pointer to radio_config structure
    @param[in/out]  *p_rx_rconfig:  pointer to radio_config structure

    @return         status:         indicating status


    @note  This function is internal only and called from rx_radio_config

*/
static int32_t configure_rx_rf_port(uint8_t card, struct radio_config *p_rconfig, struct rx_radio_config *p_rx_rconfig)
{
    int status = 0;
    uint8_t         num_fixed_ports;
    skiq_rf_port_t  fixed_port_list[skiq_rf_port_max];
    uint8_t         num_trx_ports;
    skiq_rf_port_t  trx_port_list[skiq_rf_port_max];
    bool            port_found = false;
    skiq_rf_port_config_t port_config = skiq_rf_port_config_fixed;
    skiq_rf_port_t  curr_port;
    int hdl_idx;

    log_trace("configure_rx_rf_port");

    for ( hdl_idx = 0; (hdl_idx < p_rx_rconfig->nr_handles[card]) && (status == 0); hdl_idx++)
    {
        skiq_rx_hdl_t curr_rx_hdl = p_rx_rconfig->handles[card][hdl_idx];

        /* configure the RF port */
        if( p_rx_rconfig->rf_port != skiq_rf_port_unknown )
        {
            port_found = false;

            log_trace("skiq_read_rx_rf_port_for_hdl");
            status = skiq_read_rx_rf_port_for_hdl(card, curr_rx_hdl, &curr_port); 
            if (status != 0)
            {
                log_error("Card %" PRIu8 " handle %s failed skiq_read_rx_rf_port_for_hdl "
                    " with status %" PRIi32 "", card, rxhdl_cstr(curr_rx_hdl), status);
                return status;
            }

            if (curr_port == p_rx_rconfig->rf_port)
            {
                log_info("card %" PRIu8 " handle %s the port is already configured properly for port %s",
                       card, rxhdl_cstr(curr_rx_hdl), p_rx_rconfig->rf_port_usr);
                return status;

            }

            /* note that only specific ports are available in certain modes.  In order 
              to figure out if we need to switch the RF port mode, we first must see
              what ports are available vs. what we requested
            */
            log_trace("skiq_read_rx_rf_ports_avail_for_hdl");
            status = skiq_read_rx_rf_ports_avail_for_hdl( card,
                                                     curr_rx_hdl,
                                                     &num_fixed_ports,
                                                     &(fixed_port_list[0]),
                                                     &num_trx_ports,
                                                     &(trx_port_list[0]) ) ;
            if (status != 0)
            {
                log_error("Card %" PRIu8 " handle %s failed skiq_read_rx_rf_ports_avail_for_hdl "
                    "with status %" PRIi32 "", card, rxhdl_cstr(curr_rx_hdl), status);
                return status;    
            }

            uint8_t j;

            /* look for the port in the fixed list */
            for( j = 0; (j < num_fixed_ports) && (port_found == false); j++ )
            {
                if( fixed_port_list[j] == p_rx_rconfig->rf_port )
                {
                    port_config = skiq_rf_port_config_fixed;
                    port_found = true;
                }
            }

            /* now look for the port in the TRX list */
            for( j = 0; (j < num_trx_ports) && (port_found == false); j++ )
            {
                if( trx_port_list[j] == p_rx_rconfig->rf_port )
                {
                    port_config = skiq_rf_port_config_trx;
                    port_found = true;
                }
            }

            /* if we found the user requested port, we need to make sure our
               RF port config is set to the correct mode
            */
            if( port_found == true )
            {
                log_trace("skiq_write_rf_port_config");
                status = skiq_write_rf_port_config( card, port_config );
                if (status != 0)
                {
                    log_error("Card %" PRIu8 " handle %s unable to find port requested %s ", 
                            card, rxhdl_cstr(curr_rx_hdl), p_rx_rconfig->rf_port_usr);
                    status = ERROR_COMMAND_LINE;
                    return status;
                }

                // if we're in TRx, this port could be set for either RX or TX
                // so make sure that we force it to RX since we're only receiving
                if( port_config == skiq_rf_port_config_trx )
                {

                    log_trace("skiq_write_rf_port_operation");
                    status = skiq_write_rf_port_operation( card, false ) ;
                    if (status != 0)
                    {
                        log_error("Card %" PRIu8 " handle %s unable to write_port_operation (receive) " 
                                " with status %" PRIi32 "", card, rxhdl_cstr(curr_rx_hdl), status);
                        return status;
                    }
                }

                if (status == 0)
                {
                    log_info("card %" PRIu8 " handle %s "
                           " successfully configured RF port operation (receive) ", 
                           card, rxhdl_cstr(curr_rx_hdl));
                }
            }

            // now configure the RF port for the handle
            if (status == 0)
            {
                status=skiq_write_rx_rf_port_for_hdl(card, curr_rx_hdl, p_rx_rconfig->rf_port );
                if (status != 0)
                {
                    log_error("Card %" PRIu8 " handle %s unable to configure the RX RF port to %s " 
                            " with status %" PRIi32, 
                            card, rxhdl_cstr(curr_rx_hdl), p_rx_rconfig->rf_port_usr, status);
                    return status;
                } else
                {
                    log_info("card %" PRIu8 " handle %s successfully configured RF port to %s", 
                            card, rxhdl_cstr(curr_rx_hdl), p_rx_rconfig->rf_port_usr);
                }
            }
        }

    }


    return status;
}

/*****************************************************************************/
/** @brief 
    Configures the rx rf gain
    
    @param[in]      card:           card to initialize
    @param[in/out]  *p_rconfig:     pointer to radio_config structure
    @param[in/out]  *p_rx_rconfig:  pointer to radio_config structure

    @return         status:         indicating status


    @note  This function is internal only and called from rx_radio_config

*/
static int32_t configure_rx_rf_gain(uint8_t card, struct radio_config *p_rconfig, struct rx_radio_config *p_rx_rconfig)
{
    int status = 0;
    int hdl_idx;
    skiq_rx_gain_t gain_mode;

    log_trace("configure_rx_rf_gain");

    if (p_rx_rconfig->gain_manual)
    {
        log_info("card %" PRIu8 " all handles are configured for manual gain", card);
        gain_mode = skiq_rx_gain_manual;
    }
    else
    {
        log_info("card %" PRIu8 " all handles are configured for automatic gain", card);
        gain_mode = skiq_rx_gain_auto;
    }

    for (hdl_idx = 0; (hdl_idx < p_rx_rconfig->nr_handles[card]) && (status == 0); hdl_idx++)
    {
        skiq_rx_hdl_t hdl = p_rx_rconfig->handles[card][hdl_idx];
        
        log_trace("skiq_write_rx_gain_mode");
        status = skiq_write_rx_gain_mode( card, hdl, gain_mode );
        if (status != 0)
        {
            log_error("Card %" PRIu8 " handle %s failed to set Rx gain mode "
                    "status is %" PRIi32 , card, rxhdl_cstr(hdl), status);
            goto exit;
        }
        else if ( gain_mode == skiq_rx_gain_manual )
        {
            /* see if the manual gain set is within the card/port range */
            /* some cards have different ranges per port */
            uint8_t min = 0;
            uint8_t max = 0;

            log_trace("skiq_read_rx_gain_index_range");
            status = skiq_read_rx_gain_index_range(card, hdl, &min, &max);
            if (status != 0)
            {
                log_error("Card %" PRIu8 " handle %s failed to read gain index range " 
                        " (status %" PRIi32 ")", 
                        card, rxhdl_cstr(hdl), status);
                goto exit;
            }

            if (p_rx_rconfig->gain < min || p_rx_rconfig->gain > max)
            {

                log_error("card %" PRIu8 " handle %s the gain index is out of range "
                        "the range is %" PRIu8 " to %" PRIu8 " ", 
                    card, rxhdl_cstr(hdl), min, max);
                status = ERROR_COMMAND_LINE;
                goto exit;
            }
            else 
            {
                log_info("Info: card %" PRIu8 " handle %s set gain index to %" PRIu8 " ", 
                    card, rxhdl_cstr(hdl), p_rx_rconfig->gain);
            }
            status=skiq_write_rx_gain(card, hdl, p_rx_rconfig->gain);
            if (status != 0)
            {
                log_error("card %" PRIu8 " handle %s failed to set gain index to %" PRIu8 
                        " (status %" PRIi32 ")", 
                        card, rxhdl_cstr(hdl), p_rx_rconfig->gain, status);

                /* see if the manual gain set is within the card/port range */
                /* some cards have different ranges per port */
                uint8_t min = 0;
                uint8_t max = 0;

                log_trace("skiq_read_rx_gain_index_range");
                status = skiq_read_rx_gain_index_range(card, hdl, &min, &max);
                if (status != 0)
                {
                    log_error("card %" PRIu8 " handle %s failed to read gain index range " 
                            " (status %" PRIi32 ")", 
                            card, rxhdl_cstr(hdl), status);
                }

                log_info("card %" PRIu8 " handle %s the valid gain index range is %" PRIu8 " to %" PRIu8 " ", 
                    card, rxhdl_cstr(hdl), min, max);
                goto exit;
            }
        }
    }

exit:

    return status;
}

/*****************************************************************************/
/** @brief
    Configures the RX specific parameters

*/
int32_t configure_rx_radio( uint8_t card, struct radio_config *p_rconfig, struct rx_radio_config *p_rx_rconfig )
{
    int32_t status = 0;

    log_trace("configure_rx_radio");

    if( p_rconfig->skiq_initialized == true && status == 0 )
    {
        log_info("card %" PRIu8 " starting RX configuration", card);

        /* get handles for each card */ 
        /* if the user requested ALL handles, iterate through the cards to get the handles for each card
        */
        if( (status == 0) && (p_rx_rconfig->all_chans) )
        {

            status = get_all_rx_handles( card, p_rx_rconfig->handles[card], 
                    &p_rx_rconfig->nr_handles[card], &p_rx_rconfig->chan_mode[card] );
            if (status != 0) 
            {
                log_error("could not get handles for card %" 
                        PRIu8 "status %" PRIi32"", card, status);
                goto exit;
            }
        }

        /* if the user specified an rf port try to set it */
        if (status == 0)
        {
            if (p_rx_rconfig->rf_port != skiq_rf_port_unknown)
            {
                status = configure_rx_rf_port(card, p_rconfig, p_rx_rconfig);
            }
        }

        /* set the sample rate and bandwidth */
        if (status == 0)
        {
            status = configure_rx_sample_rate_and_bandwidth(card, p_rconfig, p_rx_rconfig);
        }
        
        /* set the LO frequency */
        if (status == 0)
        {
            uint64_t min = 0;
            uint64_t max = 0;
            int hdl_idx;

            log_trace("skiq_read_rx_LO_freq_range");
            status = skiq_read_rx_LO_freq_range(card, &max, &min);
            if ( status != 0 )
            {
                log_error("card %" PRIu8 " failed to read LO freq range status is %" PRIi32 ,
                       card, status);
                goto exit;
            }

            /* tune the Rx chain to the requested freq for each specified handle */
            for (hdl_idx = 0; (hdl_idx < p_rx_rconfig->nr_handles[card]) && (status == 0); hdl_idx++)
            {
                skiq_rx_hdl_t hdl = p_rx_rconfig->handles[card][hdl_idx];

                /* see if they are trying to set the freq range within the card's capabilities */
                if (p_rx_rconfig->freq < min || p_rx_rconfig->freq > max)
                {
                    log_error("card %" PRIu8 " handle %s requested frequency %" PRIu64 " is outside "
                            "allowable range %" PRIu64 " and %" PRIu64,
                           card, rxhdl_cstr(hdl), p_rx_rconfig->freq, min, max);
                    status = ERROR_COMMAND_LINE;
                    goto exit;
                }
                log_trace("skiq_write_rx_LO_freq");
                status = skiq_write_rx_LO_freq(card, hdl, p_rx_rconfig->freq);
                if ( status != 0 )
                {
                    log_error("card %" PRIu8 " handle %s failed to set LO freq status is %" PRIi32 ,
                           card, rxhdl_cstr(hdl), status);
                    goto exit;
                }
            }
        }

        /* initialize the channel mode */
        if( status == 0 )
        {
            skiq_chan_mode_t curr_chan_mode;

            log_trace("skiq_read_chan_mode");
            status = skiq_read_chan_mode(card, &curr_chan_mode);
            if ( status != 0 )
            {
                log_error("card %" PRIu8 " failed to read channel mode status is %" 
                        PRIi32 , card, status);
                goto exit;
            }

            /* Don't change the channel mode if it is already set to dual */
            if (curr_chan_mode != skiq_chan_mode_dual)
            {
                log_info("card %" PRIu8 " setting channel mode to %s", 
                        card, chan_mode_desc_cstr(p_rx_rconfig->chan_mode[card]));

                log_trace("skiq_write_chan_mode");
                status = skiq_write_chan_mode(card, p_rx_rconfig->chan_mode[card]);
                if ( status != 0 )
                {
                    log_error("card %" PRIu8 " failed to set channel mode status is %" 
                            PRIi32 , card, status);
                    goto exit;
                }
            }

        }

        /* initialize the receive parameters for a given card */
        if( status == 0 )
        {
            if ( ( status == 0 ) && p_rx_rconfig->rx_blocking )
            {
                /* set a modest rx timeout */
                log_trace("skiq_set_rx_transfer_timeout");
                status = skiq_set_rx_transfer_timeout( card, TRANSFER_TIMEOUT );
                if( status != 0 )
                {
                    log_error("card %" PRIu8 " unable to set RX transfer timeout status is %" 
                            PRIi32 , card, status);
                    goto exit;
                }
            }
        }


        /* set the gain */
        if (status == 0)
        {
            status = configure_rx_rf_gain(card, p_rconfig, p_rx_rconfig);
        }

        /* set the data source (IQ or counter) */
        if ( status == 0 )
        {
            skiq_data_src_t data_src;
            int hdl_idx;

            if ( p_rx_rconfig->use_counter )
            {
                log_info("card %" PRIu8 " all handles are configured for counter data mode", card);
                data_src = skiq_data_src_counter;
            }
            else
            {
                log_info("card %" PRIu8 " all handles are configured for I/Q (not counter) data mode", card);
                data_src = skiq_data_src_iq;
            }

            for (hdl_idx = 0; (hdl_idx < p_rx_rconfig->nr_handles[card]) && (status == 0); hdl_idx++)
            {

                log_trace("skiq_write_rx_data_src");
                status = skiq_write_rx_data_src(card, p_rx_rconfig->handles[card][hdl_idx], data_src);
                if ( status != 0 )
                {
                    log_error("card %" PRIu8 " handle %s failed to set data source to counter mode "
                           "status is %" PRIi32 "", card, rxhdl_cstr(p_rx_rconfig->handles[card][hdl_idx]), status);
                    goto exit;
                }
            }
        }

        /* set the dc offset correction */
        if ( status == 0 )
        {
            if ( p_rx_rconfig->disable_dc_corr )
            {
                int hdl_idx;

                log_info("card %" PRIu8 " disabling DC offset correction",
                        card);
                for (hdl_idx = 0; (hdl_idx < p_rx_rconfig->nr_handles[card]) && (status == 0); hdl_idx++)
                {
                    log_trace("skiq_write_rx_dc_offset_corr");
                    status = skiq_write_rx_dc_offset_corr(card,
                                p_rx_rconfig->handles[card][hdl_idx], false);
                    if (status != 0)
                    {
                        log_error("card %" PRIu8 " handle %s failed to disable DC offset correction"
                               " with status %"
                               PRIi32 , card, rxhdl_cstr(p_rx_rconfig->handles[card][hdl_idx]), status);
                        goto exit;
                    }
                }
            }
        }
    } 
    else
    {
        log_error("libsidekiq not initialized");
         status = ERROR_LIBSIDEKIQ_NOT_INITIALIZED;
    }

exit:

    if (status != 0)
    {
        /* need to make sure all the cards we initialized are uninitialized */
        skiq_exit();
        p_rconfig->skiq_initialized = false;
    }

    return status;
}

/*****************************************************************************/
/** @brief 
    This is called when in Async TX mode after each buffer has completed processing.
    The user can override this by calling skiq_register_tx_complete_callback() 
    with a different function address.

*/
void tx_default_callback_handler( int32_t status, skiq_tx_block_t *p_data, void *p_user )
{
    if( status != 0  && status != -2)
    {
        log_error("In default callback handler failed with status %" PRIi32 "", status);
    }
}

/*****************************************************************************/
/** @brief 
    Configures the tx sample rate and bandwidth

    @param[in]      card:           card to initialize
    @param[in/out]  *p_rconfig:     pointer to radio_config structure
    @param[in/out]  *p_tx_rconfig:  pointer to radio_config structure

    @return         status:         indicating status


    @note   This function is local only and is called by configure_tx_radio 

*/
static int32_t configure_tx_sample_rate_and_bandwidth( uint8_t card, struct radio_config *p_rconfig, struct tx_radio_config *p_tx_rconfig )
{
   int status = 0;
   int hdl_idx;

    log_trace("configure_tx_sample_rate_and_bandwidth");


    for (hdl_idx = 0; (hdl_idx < p_tx_rconfig->nr_handles[card]) && (status == 0); hdl_idx++)
    {
        skiq_tx_hdl_t hdl = p_tx_rconfig->handles[card][hdl_idx];



        /* There is an issue with X4, if the user wants to configure a TX sample rate > 250MHz then 
         * they also must configure a C or D RX port! AND it must be done before the tx sample rate 
         * is configured.
         */

        bool is_A = false;
        bool is_B = false;

        if (hdl == skiq_tx_hdl_A1 || hdl == skiq_tx_hdl_A2)
        {
            is_A = true;
        }

        if (hdl == skiq_tx_hdl_B1 || hdl == skiq_tx_hdl_B2)
        {
            is_B = true;
        }

        if ( p_rconfig->sample_rate > 250000000)
        {
            if (is_A == true)
            {
                log_trace("skiq_write_rx_sample_rate_and_bandwidth %s", "skiq_rx_hdl_C1");
                status = skiq_write_rx_sample_rate_and_bandwidth(card, skiq_rx_hdl_C1,
                                                                  p_rconfig->sample_rate,
                                                                  p_rconfig->bandwidth);
            }

            if (is_B == true)
            {
                log_trace("skiq_write_rx_sample_rate_and_bandwidth %s", "skiq_rx_hdl_D1");
                status = skiq_write_rx_sample_rate_and_bandwidth(card, skiq_rx_hdl_D1,
                                                                  p_rconfig->sample_rate,
                                                                  p_rconfig->bandwidth);

            }
            if (status != 0)
            {
                log_error("card %" PRIu8 " handle %s failed to set TX sample rate or bandwidth "
                        "status is %" PRIi32 , card, txhdl_cstr(hdl), status);
                return status;
            }


        }

        log_trace("skiq_write_tx_sample_rate_and_bandwidth %s", txhdl_cstr(hdl));
        status = skiq_write_tx_sample_rate_and_bandwidth(card, hdl,
                                                               p_rconfig->sample_rate,
                                                               p_rconfig->bandwidth);
        if (status != 0)
        {
            log_error("card %" PRIu8 " handle %s failed to set TX sample rate or bandwidth "
                    "status is %" PRIi32 , card, txhdl_cstr(hdl), status);
            return status;
        }

        uint32_t read_sample_rate;
        uint32_t read_bandwidth;
        uint32_t actual_bandwidth;
        double   actual_sample_rate;

        /* read  back the sample rate and bandwidth to determine the achieved bandwidth */
        log_trace("skiq_read_tx_sample_rate_and_bandwidth");
        status = skiq_read_tx_sample_rate_and_bandwidth(card, hdl,
                                                        &read_sample_rate,
                                                        &actual_sample_rate,
                                                        &read_bandwidth,
                                                        &actual_bandwidth);
        if ( status != 0 )
        {
            log_error("card %" PRIu8 " failed to read sample rate and bandwidth" 
                    " ... status is %" PRIi32 , 
                    card, status);
        }

        log_info("card %" PRIu8 " handle %s TX actual sample rate is %" PRIu32 ", actual bandwidth is %" PRIu32 ,
                card, txhdl_cstr(hdl), (uint32_t) actual_sample_rate, actual_bandwidth);

        p_rconfig->sample_rate = actual_sample_rate;
        p_rconfig->bandwidth = actual_bandwidth;
    }


    return status;
}

/*****************************************************************************/
/** @brief 
    Configures the tx rf_port

    @param[in]      card:           card to initialize
    @param[in/out]  *p_rconfig:     pointer to radio_config structure
    @param[in/out]  *p_tx_rconfig:  pointer to radio_config structure

    @return         status:         indicating status


    @note   This function is local only and is called by configure_tx_radio 

*/
static int32_t configure_tx_rf_port( uint8_t card, struct radio_config *p_rconfig, struct tx_radio_config *p_tx_rconfig )
{
    int status = 0;
    uint8_t num_fixed_ports;
    skiq_rf_port_t fixed_port_list[skiq_rf_port_max];
    uint8_t num_trx_ports;
    skiq_rf_port_t trx_port_list[skiq_rf_port_max];
    bool port_found=false;
    skiq_rf_port_config_t port_config = skiq_rf_port_config_fixed;
    skiq_rf_port_t curr_port;
    int hdl_idx;

    log_trace("configure_tx_rf_port");

    for (hdl_idx = 0; (hdl_idx < p_tx_rconfig->nr_handles[card]) && (status == 0); hdl_idx++)
    {
        skiq_tx_hdl_t curr_tx_hdl = p_tx_rconfig->handles[card][hdl_idx];

        /* configure the RF port */
        if( p_tx_rconfig->rf_port != skiq_rf_port_unknown )
        {
            /* see if the rx port configured now is already the one they want */
            log_trace("skiq_read_tx_rf_port_for_hdl");
            status = skiq_read_tx_rf_port_for_hdl(card, curr_tx_hdl, &curr_port); 
            if (status != 0)
            {
                log_error("card %" PRIu8 " handle %s failed skiq_read_tx_rf_port_for_hdl "
                    " with status %" PRIi32 , card, txhdl_cstr(curr_tx_hdl), status);
                return status;
            }

            if (curr_port == p_tx_rconfig->rf_port)
            {
                log_info("card %" PRIu8 " handle %s the port is already configured properly for port %s",
                       card, txhdl_cstr(curr_tx_hdl), p_tx_rconfig->rf_port_usr);
                return status;

            }


            /* note that only specific ports are available in certain modes.  In order 
              to figure out if we need to switch the RF port mode, we first must see
              what ports are available vs. what we requested
            */
            log_trace("skiq_read_tx_rf_ports_avail_for_hdl");
            status = skiq_read_tx_rf_ports_avail_for_hdl( card,
                                                     curr_tx_hdl,
                                                     &num_fixed_ports,
                                                     &(fixed_port_list[0]),
                                                     &num_trx_ports,
                                                     &(trx_port_list[0]) ) ;
            if (status != 0)
            {
                log_error("card %" PRIu8 " handle %s failed skiq_read_tx_rf_ports_avail_for_hdl "
                    " with status %" PRIi32 , card, txhdl_cstr(curr_tx_hdl), status);
                return status;
            }

            uint8_t j;

            /* look for the port in the fixed list */
            for( j = 0; (j < num_fixed_ports) && (port_found == false); j++ )
            {
                if( fixed_port_list[j] == p_tx_rconfig->rf_port )
                {
                    port_config = skiq_rf_port_config_fixed;
                    port_found = true;
                }
            }

            /* now look for the port in the TRX list */
            for( j = 0; (j < num_trx_ports) && (port_found == false); j++ )
            {
                if( trx_port_list[j] == p_tx_rconfig->rf_port )
                {
                    port_config = skiq_rf_port_config_trx;
                    port_found = true;
                }
            }

            bool fixed = false;
            bool trx = false;
            log_trace("skiq_read_rf_port_config_avail");
            status = skiq_read_rf_port_config_avail(card, &fixed, &trx);

            /* if we found the user requested port, we need to make sure our
               RF port config is set to the correct mode
            */
            if( port_found == true )
            {
                log_trace("skiq_write_rf_port_config");
                status = skiq_write_rf_port_config( card, port_config );
                if (status != 0)
                {
                    log_error("card %" PRIu8 " handle %s unable to configure RF port to %" PRIi32 " " 
                            " with status %" PRIi32 , 
                            card, txhdl_cstr(curr_tx_hdl), (int32_t)port_config, status);
                    return status;
                }
                
                // if we're in TRx, this port could be set for either RX or TX
                // so make sure that we force it to TX since we're transmitting
                if( port_config == skiq_rf_port_config_trx )
                {
                    log_trace("skiq_write_rf_port_operation");
                    status = skiq_write_rf_port_operation( card, true ) ;
                    if (status != 0)
                    {
                        log_error("card %" PRIu8 "handle %s unable to write_port_operation (transmit)" 
                                " with status %" PRIi32 , card, txhdl_cstr(curr_tx_hdl), status);
                        return status;
                    }
                }
                if (status == 0)
                {
                    log_info("card %" PRIu8 " handle %s successfully configured RF port operation (transmit)", 
                           card, txhdl_cstr(curr_tx_hdl));
                }
            }
            else
            {
                log_error("card %" PRIu8 " unable to find port requested %s for handle %s", 
                        card, p_tx_rconfig->rf_port_usr, txhdl_cstr(curr_tx_hdl));
                status = ERROR_COMMAND_LINE;
                return status;
            }

            // now configure the RF port for the handle
            if (status == 0)
            {
                log_trace("skiq_write_rf_port_for_hdl");
                status=skiq_write_tx_rf_port_for_hdl(card, curr_tx_hdl, p_tx_rconfig->rf_port );
                if (status != 0)
                {
                    log_error("card %" PRIu8 " handle %s unable to configure the TX RF port to %s " 
                            " with status %" PRIi32 "", 
                             card, txhdl_cstr(curr_tx_hdl), p_tx_rconfig->rf_port_usr, status);
                    return status;
                }

                log_info("successfully configured RF port to %s", p_tx_rconfig->rf_port_usr);
            }

        }
    }
    return status;
}

/*****************************************************************************/
/** @brief 
    Configures the tx timestmap

    @param[in]      card:           card to initialize
    @param[in/out]  *p_rconfig:     pointer to radio_config structure
    @param[in/out]  *p_tx_rconfig:  pointer to radio_config structure

    @return         status:         indicating status


    @note   This function is local only and is called by configure_tx_radio 

*/
static int32_t configure_tx_timestamp( uint8_t card, struct radio_config *p_rconfig, struct tx_radio_config *p_tx_rconfig )
{
    int status = 0;

    skiq_tx_flow_mode_t tx_mode;
    uint64_t timestamp = p_tx_rconfig->timestamp_value;
    skiq_tx_hdl_t hdl;
    int hdl_idx;

    log_trace("configure_tx_timestamp");

    for (hdl_idx = 0; (hdl_idx < p_tx_rconfig->nr_handles[card]) && (status == 0); hdl_idx++)
    {
        hdl = p_tx_rconfig->handles[card][hdl_idx];

        if( p_tx_rconfig->use_late_timestamp )
        {
            log_trace("skiq_tx_with_timestamps_allow_late_data_flow_mode");
            tx_mode = skiq_tx_with_timestamps_allow_late_data_flow_mode;
            if( timestamp == 0 )
            {
                p_tx_rconfig->timestamp_value = DEFAULT_TX_TIMESTAMP_VALUE;
                log_info("card %" PRIu8 " Handle %s "  
                        "Info: no timestamp value specified with late mode; using"
                        " default value of %" PRIu64 , 
                        card, txhdl_cstr(hdl),
                        timestamp);
            }
        }
        else if( timestamp != 0 )
        {
            tx_mode = skiq_tx_with_timestamps_data_flow_mode;
        }
        else
        {
            tx_mode = skiq_tx_immediate_data_flow_mode;
        }

        log_trace("skiq_write_tx_data_flow_mode");
        status = skiq_write_tx_data_flow_mode(card, hdl, (skiq_tx_flow_mode_t)(tx_mode));
        if( status != 0 )
        {
            if( (-ENOTSUP == status) &&
                (skiq_tx_with_timestamps_allow_late_data_flow_mode == tx_mode) )
            {
                log_error("card %" PRIu8 " Handle %s "  
                        "The currently loaded bitfile doesn't"
                        " support late timestamp mode (result code %" PRIi32
                        ")", 
                        card, txhdl_cstr(hdl),
                        status);
                return status;
            }
            else
            {
                log_error("card %" PRIu8 " Handle %s "  
                        "Unable to configure TX data flow mode"
                        " (result code %" PRIi32 ")",
                        card, txhdl_cstr(hdl), status);
                return status;
            }
        }
        if( tx_mode == skiq_tx_immediate_data_flow_mode )
        {
            log_info("card %" PRIu8 " Handle %s "  
                    "Using immediate TX data flow mode",
                     card, txhdl_cstr(hdl));
        }
        else if( tx_mode == skiq_tx_with_timestamps_allow_late_data_flow_mode )
        {
            log_info("card %" PRIu8 " Handle %s "  
                    "Using timestamps TX data flow mode (allowing late"
                    " timestamps)",
                     card, txhdl_cstr(hdl));
        }
        else if( tx_mode == skiq_tx_with_timestamps_data_flow_mode )
        {
            log_info("card %" PRIu8 " Handle %s "  
                    "Using timestamp TX data flow mode",
                     card, txhdl_cstr(hdl));

        }

        p_tx_rconfig->timestamp_increment = 
            SKIQ_NUM_PACKED_SAMPLES_IN_BLOCK(p_tx_rconfig->block_size_in_words);

        if(( tx_mode == skiq_tx_with_timestamps_data_flow_mode ) ||
           ( tx_mode == skiq_tx_with_timestamps_allow_late_data_flow_mode ))
        {
            log_info("initial timestamp is %" PRIu64 , timestamp);
            log_info("timestamp increment is %u", p_tx_rconfig->timestamp_increment);
        }

        //Set the timestamp base if flow mode allows for transmit on timestamp
        if(tx_mode != skiq_tx_immediate_data_flow_mode)
        {
            log_trace("skiq_write_tx_timestamps_base");
            status = skiq_write_tx_timestamp_base(card, p_tx_rconfig->timestamp_base);
            if(status != 0)
            {
                log_error("card %" PRIu8 " Handle %s "  
                        "Unable to set timestamp base for TX on timestamp" 
                                " (result code %" PRIi32 ")",
                        card, txhdl_cstr(hdl), status);
                return status;
            }
        }

        // reset the timestamp
        log_trace("skiq_reset_timestamps");
        status = skiq_reset_timestamps(card);
        if ( status != 0 )
        {
            log_error("card %" PRIu8 " Handle %s "  
                    "Unable to reset timestamps (result code %"
                    PRIi32 ")",
                    card, txhdl_cstr(hdl), status);
            return status;
        }

    }

    return status;
}

/*****************************************************************************/
/** @brief 
    Configures the sync or async mode

    @param[in]      card:           card to initialize
    @param[in/out]  *p_rconfig:     pointer to radio_config structure
    @param[in/out]  *p_tx_rconfig:  pointer to radio_config structure

    @return         status:         indicating status


    @note   This function is local only and is called by configure_tx_radio 
           

*/
static int32_t configure_tx_sync_async( uint8_t card, struct radio_config *p_rconfig, 
                                        struct tx_radio_config *p_tx_rconfig )
{
    int status = 0;
    skiq_tx_hdl_t hdl;
    uint32_t priority = p_tx_rconfig->thread_priority;
    int hdl_idx;

    log_trace("configure_tx_sync_async");

    for (hdl_idx = 0; (hdl_idx < p_tx_rconfig->nr_handles[card]) && (status == 0); hdl_idx++)
    {
        hdl = p_tx_rconfig->handles[card][hdl_idx];

        log_trace("skiq_write_tx_transfer_mode");
        status = skiq_write_tx_transfer_mode(card, hdl, p_tx_rconfig->transfer_mode);
        if ( status != 0 )
        {
            log_error("card %" PRIu8 " Handle %s "  
                    "Unable to write TX transfer mode (result code %"
                    PRIi32 ")", card, txhdl_cstr(hdl), status);
            goto exit;
        }

        /* if in async mode need to set number of threads, thread priority and callback handler */
        if (p_tx_rconfig->transfer_mode == skiq_tx_transfer_mode_async)
        {
            log_info("card %" PRIu8 " Handle %s "
                   "In TX Async transfer mode, setting number of threads to %" PRIi32 "", 
                   card, txhdl_cstr(hdl), p_tx_rconfig->num_threads);

            log_trace("skiq_write_num_tx_threads");
            status = skiq_write_num_tx_threads(card, p_tx_rconfig->num_threads);
            if ( status != 0 )
            {
                log_error("card %" PRIu8 " Handle %s "  
                        "Unable to write num_tx_threads (result code %"
                        PRIi32 ")",
                        card, txhdl_cstr(hdl), status);
                goto exit;
            }

            log_trace("skiq_register_tx_complete_callback");
            status = skiq_register_tx_complete_callback(card, &tx_default_callback_handler); 
            if ( status != 0 )
            {
                log_error("card %" PRIu8 " Handle %s "  
                        "Unable to write TX complete callback function (result code %"
                        PRIi32 ")", card, txhdl_cstr(hdl), status);
                goto exit;
            }


            if (priority != DEFAULT_THREAD_PRIORITY)
            {
                log_info("card %" PRIu8 " Handle %s "
                        "setting thread priority to %" PRIi32 , 
                        card, txhdl_cstr(hdl), priority);

                log_trace("skiq_write_tx_thread_priority");
                status  = skiq_write_tx_thread_priority(card, priority); 
                if ( status != 0 )
                {
                    log_error("card %" PRIu8 " Handle %s "  
                            "Unable to write thread priority (result code %"
                            PRIi32 ")",
                            card, txhdl_cstr(hdl), status);
                    goto exit;
                }
            }

        }
        else
        {
            log_info("card %" PRIu8 " Handle %s "
                   "In TX Sync transfer mode", 
                   card, txhdl_cstr(hdl));

        }

    }

exit:

    return status;
}

/*****************************************************************************/
/** @brief 
    Configures the TX specific parameters

*/
int32_t configure_tx_radio( uint8_t card, struct radio_config *p_rconfig, struct tx_radio_config *p_tx_rconfig )
{
    int32_t status = 0;

    log_trace("configure_tx_radio");

    if( p_rconfig->skiq_initialized == true && status == 0 )
    {
        log_info("card %" PRIu8 " starting TX configuration", card);

        if( (status == 0) && (p_tx_rconfig->all_chans) )
        {

            status = get_all_tx_handles( card, p_tx_rconfig->handles[card], 
                    &p_tx_rconfig->nr_handles[card], &p_tx_rconfig->chan_mode[card] );
            if (status != 0) 
            {
                log_error("could not get handles for card %" 
                        PRIu8 "status %" PRIi32, card, status);

                goto exit;
                 
            }
        }

        /* if the user specified an rf port try to set it */
        if (status == 0)
        {
            if (p_tx_rconfig->rf_port != skiq_rf_port_unknown)
            {
                status = configure_tx_rf_port(card, p_rconfig, p_tx_rconfig);
            }
        }

        /* set the sample rate and bandwidth */
        if (status == 0)
        {
            status = configure_tx_sample_rate_and_bandwidth(card, p_rconfig, p_tx_rconfig);
        }
        
        /* set the LO frequency */
        if (status == 0)
        {
            uint64_t min = 0;
            uint64_t max = 0;
            int hdl_idx;

            log_trace("skiq_read_tx_LO_freq_range");
            status = skiq_read_tx_LO_freq_range(card, &max, &min);
            if ( status != 0 )
            {
                log_error("card %" PRIu8 " failed to read LO freq range status is %" PRIi32 ,
                       card, status);
                goto exit;
            }

            /* tune the Tx chain to the requested freq for each specified handle */
            for (hdl_idx = 0; (hdl_idx < p_tx_rconfig->nr_handles[card]) && (status == 0); hdl_idx++)
            {
                skiq_tx_hdl_t hdl = p_tx_rconfig->handles[card][hdl_idx];

                /* see if they are trying to set the freq range within the card's capabilities */
                if ((p_tx_rconfig->freq < min) || (p_tx_rconfig->freq > max))
                {
                    log_error("card %" PRIu8 " handle %s requested frequency %" PRIu64 " is outside "
                            "allowable range %" PRIu64 " and %" PRIu64,
                           card, txhdl_cstr(hdl), p_tx_rconfig->freq, min, max);
                    status = ERROR_COMMAND_LINE;
                    goto exit;
                }

                log_trace("skiq_write_tx_LO_freq");
                status = skiq_write_tx_LO_freq(card, hdl, p_tx_rconfig->freq);
                if ( status != 0 )
                {
                    log_error("card %" PRIu8 " handle %s failed to set LO freq status is %" PRIi32 ,
                           card, txhdl_cstr(hdl), status);
                    goto exit;
                }

            }
        }
        /* initialize the channel mode */
        if( status == 0 )
        {
            skiq_chan_mode_t curr_chan_mode;

            log_trace("skiq_read_chan_mode");
            status = skiq_read_chan_mode(card, &curr_chan_mode);
            if ( status != 0 )
            {
                log_error("card %" PRIu8 " failed to read channel mode status is %" 
                        PRIi32 , card, status);
                goto exit;
            }

            /* Don't change the channel mode if it is already set to dual */
            if (curr_chan_mode != skiq_chan_mode_dual)
            {
                log_info("card %" PRIu8 " setting channel mode to %s", 
                        card, chan_mode_desc_cstr(p_tx_rconfig->chan_mode[card]));

                log_trace("skiq_write_chan_mode");
                status = skiq_write_chan_mode(card, p_tx_rconfig->chan_mode[card]);
                if ( status != 0 )
                {
                    log_error("card %" PRIu8 " failed to set channel mode status is %" 
                            PRIi32 , card, status);
                    goto exit;
                }
            }

        }

        /* set attenuation */
        if ( status == 0 )
        {
            uint16_t attenuation = p_tx_rconfig->attenuation;
            int hdl_idx;

            for (hdl_idx = 0; (hdl_idx < p_tx_rconfig->nr_handles[card]) && (status == 0); hdl_idx++)
            {
                skiq_tx_hdl_t hdl = p_tx_rconfig->handles[card][hdl_idx];

                log_trace("skiq_write_tx_attenuation");
                status = skiq_write_tx_attenuation(card, hdl, attenuation);
                if ( status != 0 )
                {
                    log_error("card %" PRIu8 " Handle %s "  
                            "Unable to configure Tx attenuation (result code %" PRIi32 ")", 
                            card, txhdl_cstr(hdl), status);
                    goto exit;
                }

                log_info("card %" PRIu8 " handle %s "  "attenuation is %.2f dB ",
                       card, txhdl_cstr(hdl), ((float)attenuation / 4.0));

            }
        }

        /* set block_size */
        if ( status == 0 )
        {
            int hdl_idx;

            for (hdl_idx = 0; (hdl_idx < p_tx_rconfig->nr_handles[card]) && (status == 0); hdl_idx++)
            {
                skiq_tx_hdl_t hdl = p_tx_rconfig->handles[card][hdl_idx];
                log_trace("skiq_write_tx_block_size");
                status = skiq_write_tx_block_size(card, hdl, 
                                                  p_tx_rconfig->block_size_in_words);
                if (status != 0)
                {
                    log_error("card %" PRIu8 " Handle %s "  
                            "Unable to configure Tx block size (result code %" PRIi32 ")", 
                            card, txhdl_cstr(hdl), status);
                    goto exit;
                }

                log_info("card %" PRIu8 " Handle %s Block size set to %u words",
                       card, txhdl_cstr(hdl),
                       p_tx_rconfig->block_size_in_words);
            }

        }

        /* set timestamp data flow mode */
        if (status == 0)
        {
            status = configure_tx_timestamp(card, p_rconfig, p_tx_rconfig);
        }

        /* set sync/async parameters */
        if (status == 0)
        {
            status = configure_tx_sync_async(card, p_rconfig, p_tx_rconfig);
        }

    } 
    else
    {
        log_error("libsidekiq not initialized");
         status = ERROR_LIBSIDEKIQ_NOT_INITIALIZED;
    }

exit:

    if (status != 0)
    {
        /* need to make sure all the cards we initialized are uninitialized */
        skiq_exit();
        p_rconfig->skiq_initialized = false;
    }
 

    return status;

}



