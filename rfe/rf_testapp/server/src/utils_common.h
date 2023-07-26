/**
 * @file utils_common.h
 *
 * @brief
 *
 * The goal of encapsulation is to make it easy to use portions of the code if needed.
 * Steps have been taken to encapsulate data and functionality:
 * struct cmd_line_args is used by ((write) arg_parser(), (read) map_arguments_to_radio_config())
 * struct thread_params is used by ((write) main(), (read) receive_thread())
 * struct radio_config is used by ( (write) map_arguments_to_radio_config(),
 *                                  (read/write) configure_radio(),
 *                                  (read) receive_thread(),
 *                                  (read) dump_rconfig())
 * struct thread_variables is used by ((write/read) receive_thread())
 * struct rx_stats is used by ((write/read) receive_thread())
 *
 * <pre>
 * Copyright 2014-2021 Epiq Solutions, All Rights Reserved
 * </pre>
 */

#ifndef __COMMON_H__
#define __COMMON_H__


/***** INCLUDES *****/
#if (defined __MINGW32__)
#define OUTPUT_PATH_MAX                     260
#else
#include <linux/limits.h>
#define OUTPUT_PATH_MAX                     PATH_MAX
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <time.h>

#include "sidekiq_api.h"
#include "arg_parser.h"


/***** DEFINES *****/
#define LOG_VERSION "0.1.0"

/* RX and TX transfer timeout (in ms) */
#define TRANSFER_TIMEOUT                    (10000)

#define ERROR_COMMAND_LINE                  -EINVAL
#define ERROR_LIBSIDEKIQ_NOT_INITIALIZED    -ECANCELED
#define ERROR_NO_MEMORY                     -ENOMEM
#define ERROR_CARD_CONFIGURATION            -EPROTO
#define ERROR_NOT_PERMITTED                 -EPERM
#define ERROR_VALUE_TOO_LARGE               -EOVERFLOW
#define ERROR_VALUE_OUT_OF_RANGE            -ERANGE

/* a simple pair of macros to round up integer division */
#define _ROUND_UP(_numerator, _denominator)    (_numerator + (_denominator - 1)) / _denominator
#define ROUND_UP(_numerator, _denominator)     (_ROUND_UP((_numerator),(_denominator)))

/* MACRO used to swap elements, used depending on the IQ order mode before verify_data() */
#define SWAP(a, b) do { typeof(a) t; t = a; a = b; b = t; } while(0)

// [0 ... size] = value is a GCC specific method of initializing an array
// https://gcc.gnu.org/onlinedocs/gcc/Designated-Inits.html 
#define INIT_ARRAY(size, value)             { [0 ... (size-1)] = value } 

/* The maximum number of command line args allowed */
#define MAX_ARGS                            100

/* The maximum size of the long help string including defaults */
#define MAX_LONG_STRING                     20000

#ifndef DEFAULT_CARD_NUMBER
#   define DEFAULT_CARD_NUMBER              0
#endif

#ifndef DEFAULT_REPEAT
#   define DEFAULT_REPEAT                   0
#endif

#ifndef DEFAULT_SAMPLE_ORDER_QI
#   define DEFAULT_SAMPLE_ORDER_QI          false
#endif

#ifndef DEFAULT_RATE
#   define DEFAULT_RATE                     10000000
#endif

#ifndef DEFAULT_BANDWIDTH
#   define DEFAULT_BANDWIDTH                8000000
#endif

#ifndef DEFAULT_RX_HDL
#   define DEFAULT_RX_HDL                   "A1"
#endif

#ifndef DEFAULT_RX_RF_PORT
#   define DEFAULT_RX_RF_PORT               "unknown"
#endif

#ifndef DEFAULT_RX_FREQUENCY
#   define DEFAULT_RX_FREQUENCY             1000000000
#endif

#ifndef DEFAULT_RX_GAIN
#   define DEFAULT_RX_GAIN                  255
#endif

#ifndef DEFAULT_FILE_PATH
#   define DEFAULT_FILE_PATH                "samples.bin" 
#endif

#ifndef DEFAULT_NUM_SAMPLES
#   define DEFAULT_NUM_SAMPLES              2000
#endif

#ifndef DEFAULT_BLOCKING_RX                     
#   define DEFAULT_BLOCKING_RX              false
#endif

#ifndef DEFAULT_TRIGGER_SRC
#   define DEFAULT_TRIGGER_SRC              "immediate"
#endif

#ifndef DEFAULT_PPS_SRC
#   define DEFAULT_PPS_SRC                  "unavailable" 
#endif

#ifndef DEFAULT_SETTLE_TIME_MS
#   define DEFAULT_SETTLE_TIME_MS           500
#endif

#ifndef DEFAULT_PACKED                          
#   define DEFAULT_PACKED                   false
#endif

#ifndef DEFAULT_INCLUDE_META                    
#   define DEFAULT_INCLUDE_META             false
#endif

#ifndef DEFAULT_USE_COUNTER
#   define DEFAULT_USE_COUNTER              false
#endif

#ifndef DEFAULT_DISABLE_DC_CORR                 
#   define DEFAULT_DISABLE_DC_CORR          false
#endif

#ifndef DEFAULT_TX_HDL
#   define DEFAULT_TX_HDL                   "A1" 
#endif

#ifndef DEFAULT_TX_PORT
#   define DEFAULT_TX_PORT                  "unknown"
#endif

#ifndef DEFAULT_TX_FREQUENCY
#   define DEFAULT_TX_FREQUENCY              1000000000
#endif

#ifndef DEFAULT_TX_ATTENUATION
#   define DEFAULT_TX_ATTENUATION            100
#endif

#ifndef DEFAULT_TX_BLOCK_SIZE
#   define DEFAULT_TX_BLOCK_SIZE             1020 
#endif

#ifndef DEFAULT_TWO_CHANNEL_TX_BLOCK_SIZE
#   define DEFAULT_TWO_CHANNEL_TX_BLOCK_SIZE 1022 
#endif

#ifndef DEFAULT_TX_FILE_PATH
#   define DEFAULT_TX_FILE_PATH              "tx_samples.bin" 
#endif

#ifndef DEFAULT_TX_TIMESTAMP_BASE
#   define DEFAULT_TX_TIMESTAMP_BASE         "rf" 
#endif

#ifndef DEFAULT_TX_TIMESTAMP_VALUE
#   define DEFAULT_TX_TIMESTAMP_VALUE        100000   
#endif

#ifndef DEFAULT_THREADS
#   define DEFAULT_THREADS                   0   
#endif

#ifndef DEFAULT_THREAD_PRIORITY
#   define DEFAULT_THREAD_PRIORITY           -1   
#endif

/* https://gcc.gnu.org/onlinedocs/gcc-4.8.5/cpp/Stringification.html */
#define xstr(s)                             str(s)
#define str(s)                              #s

/* Delimiter used when parsing lists provided as input */
#define TOKEN_LIST                          ","


/* logging macros */
#define log_trace(...) log_log(LLOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define log_debug(...) log_log(LLOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...)  log_log(LLOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...)  log_log(LLOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) log_log(LLOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define log_fatal(...) log_log(LLOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)


/***** LOGGING TYPEDEFS *******************/

typedef struct {
  va_list ap;
  const char *fmt;
  const char *file;
  struct tm *time;
  void *udata;
  int line;
  int level;
} log_Event;

typedef void (*log_LogFn)(log_Event *ev);
typedef void (*log_LockFn)(bool lock, void *udata);

/* LOG_DEBUG and LOG_INFO are in syslog.h so need to change defines */
typedef enum { LLOG_TRACE=0 , LLOG_DEBUG , LLOG_INFO , LLOG_WARN , LLOG_ERROR , LLOG_FATAL  } log_levels_t;


/***** DEFAULT COMMAND LINE ARGUMENTS *****/

/* These are the default command line arguments the application can choose
 * Other command line arguments can be added within the application
 */
struct cmd_line_args
{
/* parameters common to rx_tx */
    uint8_t             card_id;                // The Card ID(s) used
    bool                card_is_present;        // Whether the user entered a card ID
    char*               p_serial;               // Specify card number by serial number
    bool                p_serial_is_present;    // Whether the serial number is entered
    uint32_t            sample_rate;            // Sample Rate of the cards
    bool                rate_is_present;        // Whether the user entered a sample rate
    uint32_t            bandwidth;              // Bandwidth of the cards
    bool                bw_is_present;          // Whether the user entered a bandwidth
    uint32_t            repeat;                 // How many times to repeat test
    bool                repeat_is_present;      // Whether the user entered a repeat value
    bool                sample_order_qi;        // The default sample order is I/Q.  This says use Q/I 
    char*               p_pps_source;           // Where to get 1 pps clock from.
    bool                pps_source_is_present;  // Whether the user entered the trigger_src 

/* rx parameters */
    char*               p_rx_hdl;               // The handles(s) used
    bool                rx_hdl_is_present;      // Whether the user entered a handle list
    char*               rx_rf_port;             // Mapped handle to "J" port number
    bool                rx_rf_port_is_present;  // Whether the user entered a port number
    uint64_t            rx_freq;                // Center Frequency of the RX ports
    bool                rx_freq_is_present;     // Whether the usre entered an rx_frequency
    uint8_t             rx_gain;                // Rx gain index
    bool                rx_gain_manual;         // If a gain index entered then manual gain
    char*               p_rx_file_path;         // Path to RX file to place samples.
    bool                rx_file_path_is_present;// Whether the user entered a file path 
    uint32_t            rx_samples_to_acquire;  // Number of rx samples to acquire
    bool                rx_samples_is_present;  // Whether the user entered the rx samples.
    bool                rx_blocking;            // Set the receive mode to block on every rx_samples call
    char*               p_trigger_src;          // What to use to start the streaming
    bool                trigger_src_is_present; // Whether the user entered the trigger_src 
    uint32_t            settle_time;            // How many ms to wait before streaming
    bool                settle_time_is_present; // Whether the user entered the trigger_src 
    bool                packed;                 // Whether to pack samples tight or use int16/int16
    bool                include_meta;           // Whether to include meta-data in the file
    bool                use_counter;            // Request that the FPGA generates sequential samples
    bool                disable_dc_corr;        // Disable the FPGA DC correction 

/* tx parameters */
    char*               p_tx_hdl;               // The handles(s) used
    bool                tx_hdl_is_present;      // Whether the user entered a handle list
    char*               tx_rf_port;             // Mapped handle to "J" port number
    bool                tx_rf_port_is_present;  // Whether the user entered a port number
    uint64_t            tx_freq;                // Center Frequency of the RX ports
    bool                tx_freq_is_present;     // Whether the user entered an tx_frequency
    uint8_t             attenuation;            // Tx attenuation
    bool                attenuation_is_present; 
    uint16_t            block_size;             // Size of tx block
    bool                block_size_is_present;  
    char*               p_tx_file_path;         // Path to file to transmit samples from
    bool                tx_file_path_is_present;
    char*               timestamp_base;         // RF or System timestamp
    bool                timestamp_base_is_present; 
    uint64_t            timestamp_value;        // Value to start timestamps
    bool                timestamp_value_is_present;
    bool                use_late_timestamp;     // Whether to use late timestamp
    uint8_t             num_threads;            // Number of threads for async mode
    bool                threads_is_present;     
    int32_t             thread_priority;        // The priority of the threads
    bool                priority_is_present; 
};

#define COMMAND_LINE_ARGS_INITIALIZER                                           \
{                                                                               \
    .card_id                         = DEFAULT_CARD_NUMBER,                     \
    .card_is_present                 = false,                                   \
    .p_serial                        = NULL,                                    \
    .p_serial_is_present             = false,                                   \
    .sample_rate                     = DEFAULT_RATE,                            \
    .rate_is_present                 = false,                                   \
    .bandwidth                       = DEFAULT_BANDWIDTH,                       \
    .bw_is_present                   = false,                                   \
    .repeat                          = DEFAULT_REPEAT,                          \
    .repeat_is_present               = false,                                   \
    .sample_order_qi                 = DEFAULT_SAMPLE_ORDER_QI,                 \
    .p_pps_source                    = DEFAULT_PPS_SRC,                         \
    .pps_source_is_present           = false,                                   \
    .p_rx_hdl                        = DEFAULT_RX_HDL,                          \
    .rx_hdl_is_present               = false,                                   \
    .rx_rf_port                      = DEFAULT_RX_RF_PORT,                      \
    .rx_rf_port_is_present           = false,                                   \
    .rx_freq                         = DEFAULT_RX_FREQUENCY,                    \
    .rx_freq_is_present              = false,                                   \
    .rx_gain                         = DEFAULT_RX_GAIN,                         \
    .rx_gain_manual                  = false,                                   \
    .p_rx_file_path                  = DEFAULT_FILE_PATH,                       \
    .rx_file_path_is_present         = false,                                   \
    .rx_samples_to_acquire           = DEFAULT_NUM_SAMPLES,                     \
    .rx_samples_is_present           = false,                                   \
    .rx_blocking                     = DEFAULT_BLOCKING_RX,                     \
    .p_trigger_src                   = DEFAULT_TRIGGER_SRC,                     \
    .trigger_src_is_present          = false,                                   \
    .settle_time                     = DEFAULT_SETTLE_TIME_MS,                  \
    .settle_time_is_present          = false,                                   \
    .packed                          = DEFAULT_PACKED,                          \
    .include_meta                    = DEFAULT_INCLUDE_META,                    \
    .use_counter                     = DEFAULT_USE_COUNTER,                     \
    .disable_dc_corr                 = DEFAULT_DISABLE_DC_CORR,                 \
    .p_tx_hdl                        = DEFAULT_TX_HDL,                          \
    .tx_hdl_is_present               = false,                                   \
    .tx_rf_port                      = DEFAULT_TX_PORT,                         \
    .tx_rf_port_is_present           = false,                                   \
    .tx_freq                         = DEFAULT_TX_FREQUENCY,                    \
    .tx_freq_is_present              = false,                                   \
    .attenuation                     = DEFAULT_TX_ATTENUATION,                  \
    .attenuation_is_present          = false,                                   \
    .block_size                      = DEFAULT_TX_BLOCK_SIZE,                   \
    .block_size_is_present           = false,                                   \
    .p_tx_file_path                  = DEFAULT_TX_FILE_PATH,                    \
    .tx_file_path_is_present         = false,                                   \
    .timestamp_base                  = DEFAULT_TX_TIMESTAMP_BASE,               \
    .timestamp_base_is_present       = false,                                   \
    .timestamp_value                 = 0,                                       \
    .timestamp_value_is_present      = false,                                   \
    .use_late_timestamp              = false,                                   \
    .num_threads                     = DEFAULT_THREADS,                         \
    .threads_is_present              = false,                                   \
    .thread_priority                 = DEFAULT_THREAD_PRIORITY,                 \
    .priority_is_present             = false,                                   \
}                                                                               \

/* Argument Selectors
 * these are used to define which of the default command line arguments
 * the app wants to have implemented
 */
#define ARG_NO_ARG                      0
#define ARG_CARD_ID                     (1<<0)
#define ARG_SAMPLE_RATE                 (1<<1)
#define ARG_BANDWIDTH                   (1<<2)
#define ARG_SERIAL_NUM                  (1<<3)
#define ARG_REPEAT                      (1<<4)
#define ARG_SAMPLE_ORDER_QI             (1<<5)

#define ARG_RX_HDL                      (1<<6)
#define ARG_RX_RF_PORT                  (1<<7)
#define ARG_RX_FREQ                     (1<<8)
#define ARG_RX_GAIN                     (1<<9)
#define ARG_RX_PATH                     (1<<10)
#define ARG_RX_PAYLOAD_SAMPLES          (1<<11)
#define ARG_RX_BLOCKING                 (1<<12)
#define ARG_TRIGGER_SRC                 (1<<13)
#define ARG_PPS_SOURCE                  (1<<14)
#define ARG_SETTLE_TIME                 (1<<15)
#define ARG_PACKED                      (1<<16)
#define ARG_INCLUDE_META                (1<<17)
#define ARG_USE_COUNTER                 (1<<18)
#define ARG_DISABLE_DC_CORR             (1<<19)

#define ARG_TX_HDL                      (1<<20)
#define ARG_TX_RF_PORT                  (1<<21)
#define ARG_TX_FREQ                     (1<<22)
#define ARG_ATTENUATION                 (1<<23)
#define ARG_BLOCK_SIZE                  (1<<24)
#define ARG_TX_FILE_PATH                (1<<25)
#define ARG_TIMESTAMP_BASE              (1<<26)
#define ARG_TIMESTAMP_VALUE             (1<<27)
#define ARG_USE_LATE_TIMESTAMP          (1<<28)
#define ARG_THREADS                     (1<<29)
#define ARG_THREAD_PRIORITY             ((uint64_t) ( (uint64_t)1 ) << 30)

/* The application sets these fields to define which of the common arguments to use */
struct cmd_line_selector {
    uint64_t arg_select;    // Bitmask which fields are shown to the user
    uint64_t arg_required;  // Bitmask which fields the user MUST enter or error out
} ;

#define COMMAND_SELECTOR_INITIALIZER                                        \
{                                                                           \
    .arg_select                         = 0,                                \
    .arg_required                       = 0,                                \
}                                                                           \


#define COMMAND_LINE_INITIALIZER                                            \
{                                                                           \
        .p_long_flag                    = NULL,                             \
        .short_flag                     = 0,                                \
        .p_info                         = NULL,                             \
        .p_label                        = NULL,                             \
        .p_var                          = NULL,                             \
        .type                           = BOOL_VAR_TYPE,                    \
        .required                       = false,                            \
        .p_is_set                       = NULL,                             \
}                                                                           \

/***** TYPE DEFINITIONS *****/

/* Command line parameters are mapped to this structure.
   The radios are initialized from this structure.
   This is done because not all command line parameters map
   directly to radio parameters. Parameters that do map directly
   are here for consistency and future adaptions.
   The radio_config functions only access the radio_config structure.
 */
struct radio_config
{
    uint8_t             num_cards;                      // skiq_get_cards
    uint32_t            sample_rate;                    // common across cards but configured for each port 
    uint32_t            bandwidth;                      // 
    uint8_t             cards[SKIQ_MAX_NUM_CARDS];      // skiq_get_cards
    skiq_iq_order_t     sample_order_iq;                // skiq_write_qi_order_mode 
    bool                packed;                         // skiq_write_iq_pack_mode
    skiq_1pps_source_t  pps_source;                     // skiq_write_1pps_source
    bool                skiq_initialized;               // skiq_init
};
/* Unless otherwise noted, the radio config structure is initialized
   based on the command line data in the function 
   map_arguments_to_radio_config()
*/   
#define RADIO_CONFIG_INITIALIZER                          \
{                                                         \
    .num_cards          = 0,                              \
    .sample_rate        = DEFAULT_RATE,                \
    .bandwidth          = DEFAULT_BANDWIDTH,           \
    .cards              = INIT_ARRAY(SKIQ_MAX_NUM_CARDS, SKIQ_MAX_NUM_CARDS), \
    .sample_order_iq    = skiq_iq_order_iq,                          \
    .packed             = DEFAULT_PACKED,                 \
    .pps_source         = skiq_1pps_source_unavailable,   \
    .skiq_initialized   = false,                          \
}                                                         \

struct rx_radio_config
{
    /* handles should be referenced as follow -> rx_handles[cards[card_index]][handle] */
    skiq_rx_hdl_t       handles[SKIQ_MAX_NUM_CARDS][skiq_rx_hdl_end]; 
    uint8_t             nr_handles[SKIQ_MAX_NUM_CARDS]; // nr_handles[cards[card_index]][handle] 
    bool                all_chans;                      // set to indicate RX all handles
    skiq_chan_mode_t    chan_mode[SKIQ_MAX_NUM_CARDS];  // skiq_write_chan_mode 
    skiq_rf_port_t      rf_port;                        // rf_port mapped to handle
    char *              rf_port_usr;                    // user representation of rf port
    uint64_t            freq;                           // skiq_write_rx_LO_freq
    uint8_t             gain;                           // skiq_write_rx_gain
    bool                gain_manual;                    // skiq_write_rx_gain_mode 
    bool                rx_blocking;                    // skiq_set_rx_transfer_timeout
    skiq_trigger_src_t  trigger_src;                    // skiq_write_timestamp_reset_on_1pps
    bool                use_counter;                    // skiq_write_rx_data_src
    bool                disable_dc_corr;                // skiq_write_rx_dc_offset_corr
};

/* Unless otherwise noted, the radio config structure is initialized
   based on the command line data in the function 
   map_arguments_to_radio_config()
*/   
#define RX_RADIO_CONFIG_INITIALIZER                       \
{                                                         \
    .handles            = INIT_ARRAY(SKIQ_MAX_NUM_CARDS, INIT_ARRAY(skiq_rx_hdl_end,skiq_rx_hdl_end)), \
    .nr_handles         = INIT_ARRAY(SKIQ_MAX_NUM_CARDS, 0), \
    .all_chans          = false,                          \
    .rf_port            = skiq_rf_port_unknown,           \
    .rf_port_usr        = DEFAULT_RX_RF_PORT,             \
    .chan_mode          = INIT_ARRAY(SKIQ_MAX_NUM_CARDS, skiq_chan_mode_single),  \
    .freq               = DEFAULT_RX_FREQUENCY,           \
    .gain               = DEFAULT_RX_GAIN,                \
    .gain_manual        = false,                          \
    .rx_blocking        = DEFAULT_BLOCKING_RX,            \
    .trigger_src        = skiq_trigger_src_1pps,          \
    .use_counter        = DEFAULT_USE_COUNTER,            \
    .disable_dc_corr    = DEFAULT_DISABLE_DC_CORR,        \
}                                                         \

struct tx_radio_config
{
    /* handles should be referenced as follow -> rx_handles[cards[card_index]][handle] */
    skiq_tx_hdl_t       handles[SKIQ_MAX_NUM_CARDS][skiq_rx_hdl_end]; 
    uint8_t             nr_handles[SKIQ_MAX_NUM_CARDS]; // nr_handles[cards[card_index]][handle] 
    bool                all_chans;                      // set to indicate RX all handles
    skiq_chan_mode_t    chan_mode[SKIQ_MAX_NUM_CARDS];  // skiq_write_chan_mode 
    skiq_rf_port_t      rf_port;                        // rf_port mapped to handle
    char *              rf_port_usr;                    // user representation of rf port
    uint64_t            freq;                           // skiq_write_tx_LO_freq
    uint16_t            attenuation;                    // skiq_write_tx_attenuation
    uint16_t            block_size_in_words;            // Size of tx block                
    skiq_tx_timestamp_base_t timestamp_base;                 
    uint64_t            timestamp_value;
    uint32_t            timestamp_increment;    
    bool                use_late_timestamp;            
    uint8_t             num_threads;
    int32_t             thread_priority;
    skiq_tx_transfer_mode_t transfer_mode;
};

/* Unless otherwise noted, the radio config structure is initialized
   based on the command line data in the function 
   map_arguments_to_radio_config()
*/   
#define TX_RADIO_CONFIG_INITIALIZER                         \
{                                                           \
    .handles                = INIT_ARRAY(SKIQ_MAX_NUM_CARDS, INIT_ARRAY(skiq_tx_hdl_end,skiq_tx_hdl_end)), \
    .nr_handles             = INIT_ARRAY(SKIQ_MAX_NUM_CARDS, 0), \
    .all_chans              = false,                        \
    .rf_port                = skiq_rf_port_unknown,         \
    .rf_port_usr            = DEFAULT_RX_RF_PORT,           \
    .chan_mode              = INIT_ARRAY(SKIQ_MAX_NUM_CARDS, skiq_chan_mode_single),  \
    .freq                   = DEFAULT_TX_FREQUENCY,         \
    .attenuation            = DEFAULT_TX_ATTENUATION,       \
    .block_size_in_words    = DEFAULT_TX_BLOCK_SIZE,        \
    .timestamp_base         = skiq_tx_rf_timestamp,         \
    .timestamp_value        = 0,                            \
    .timestamp_increment    = 0,                            \
    .use_late_timestamp     = false,                        \
    .num_threads            = DEFAULT_THREADS,              \
    .thread_priority        = DEFAULT_THREAD_PRIORITY,      \
}                                                           \

/* this is used in param_cstr which looks up a specifc card paramater and returns a string. 
 * This can be expanded as more card parameters are needed
 */
typedef enum
{
    cardname=0,
    lo_freq_min = 1,
    lo_freq_max = 2,
    sample_rate_min=3,
    sample_rate_max=4,
    param_unknown
} param_request_t;

    
/***** LOCAL FUNCTIONS *****/

/* Logging Functions */
/*****************************************************************************/
/** @brief  Set the logging parameters for the parameters that control stderr logging

    @param[in]      level:              All logs below the given level will not be written to stderr. 
                                        By default the level is LOG_TRACE, such that nothing is ignored.
    @param[in]      use_color:          Whether the stderr should use color or not
    @param[in]      use_time:           Whether to add TIME and FILE:LINE to the output
                                        time FILE:LINE will always be added to LLOG_FATAL or LLOG_ERROR logs

    @return         void       
                             
    $notes:         Available levels are LLOG_TRACE, LLOG_DEBUG, LLOG_INFO, LLOG_WARN, LLOG_ERROR, LLOG_FATAL
                    Defaults are:
                    level = LLOG_TRACE
                    use_color = false
                    use_time = false

*/
extern void log_set_level(                      int level, 
                                                bool use_color, 
                                                bool use_time);

/*****************************************************************************/
/** @brief Quiet-mode can be enabled by passing true to the log_set_quiet() function. 
 *  While this mode is enabled the library will not output anything to stderr, 
 *  but will continue to write to files and callbacks if any are set. 

    @param[in]      enable:              true to turn on and false to turn off
*/
extern void log_set_quiet(                      bool enable);



/*****************************************************************************/
/** @brief One or more file pointers where the log will be written can be provided to the library 
 *   by using the log_add_fp() function.  Any messages below the give level are ignored. 

    @param[in]      fp:                 FILE pointer to desired logging file, by default no file

    @return         0:                  Success
                    -1:                 No room in fp array

*/
extern int log_add_fp(                          FILE *fp, 
                                                int level);

/*****************************************************************************/
/** @brief If the log will be written to from multiple threads a lock function can be set. 
 *  When called the function is passed the boolean true if the lock should be acquired or 
 *  false if the lock should be released and the given udata value.

    @param[in]      fn:                 The function to call
    @param[in]      udata:              The udata structure that the logger will use ;
*/
extern void log_set_lock(                       log_LockFn fn, 
                                                void *udata);


/*****************************************************************************/
/** @brief One or more callback functions which are called with the log data can be provided to 
 * the library by using the log_add_callback() function. 
 * A callback function is passed a log_Event structure containing the line number, 
 * filename, fmt string, va printf va_list, level and the given udata.

    @param[in]      fn:                 The function to call
    @param[in]      udata:              The udata structure that the logger will use ;
    @param[in]      level:              The udata structure that the logger will use ;

    @return         0:                  Success
                    -1:                 No room in fp array

*/
extern int log_add_callback(                    log_LogFn fn, 
                                                void *udata, 
                                                int level);

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
extern void log_log(                            int level, 
                                                const char *file, 
                                                int line, 
                                                const char *fmt, ...) ;

extern void stderr_callback(log_Event *ev);

/* Command Line Arg Functions */
/*****************************************************************************/
/** @brief Add a command line parameter

    @param[in]      selector:            value of this parameter bitmask
    @param[in]      p_cmd_line_selector: structure of bitmasks for param selectors   
    @param[in/out]  args[]:              existing array of command line args
    @param[out]     new_arg:             new structure to add to args 
    @param[in/out]  p_num_args:          pointer to the place to return the number of args initalized
    @param[in]      p_parameter:         address of the place to point to for present indication

    @return                             - 0 success
                                        - 1 errors
*/
extern uint32_t add_command_line_param(         uint64_t selector,
                                                struct cmd_line_selector *p_cmd_line_selector,
                                                struct application_argument args[],
                                                struct application_argument *new_arg,
                                                uint32_t *p_num_args,
                                                void * p_parameter);

/*****************************************************************************/
/** @brief Initialize the application arg array info that is passed into arg_parser

    @param[in]   p_cmd_line_selector:   bitmask of the parameters desired
    @param[out]  args[]:                array passed to arg_parser
    @param[out]  g_cmd_line_args:       structure containing all the variables
                                        that need to be populated
    @param[out]  p_num_args:            pointer to the place to return the number
                                        of args initialized

    @return      void
*/
extern void initialize_application_args(        struct cmd_line_selector *p_cmd_line_selector,
                                                struct application_argument args[],
                                                struct cmd_line_args *g_cmd_line_args,
                                                uint32_t *p_num_args);

/*****************************************************************************/
/** @brief Add an application specific command line parameter to the existing args array

    @param[in]      args[]:                 existing array of arguments
    @param[out]     new_arg:                structure containing all the variables
                                            that need to be copied into args
    @param[in/out]  p_num_args:             pointer to the place to return the number
                                            of args initalized

    @return         void

*/
extern void add_app_specific_args(              struct application_argument args[], 
                                                struct application_argument *new_arg,
                                                uint32_t *p_num_args);

/*****************************************************************************/
/** @brief   Add the defaults for the selected command line arguments to the long help string
 *

    @param[in]      args[]:                 the args array after initialization 
    @param[in]      num_args:               the number of arguments in the args array
    @param[in]      p_help_long:            the string input that has application 
                                            specific text 
    @param[in/out]  p_help_inc_defaults:    the string output including the defaults 

    @return         void

    @note       This application assumes that initialize_application_args() has been
                called and the memory is allocated for p_help_inc_defaults and is 
                MAX_LONG_STRING characters long

*/
extern void initialize_help_string(             struct application_argument args[],
                                                int num_args,
                                                const char *p_help_long,
                                                char *p_help_inc_defaults);

/*****************************************************************************/
/** @brief Print out the command line arguments received

    @param[in]  arg_count:      The number of arguments in the array
    @param[in]  args[]:         Pointer to the array of arguments

    @return     void

*/
extern void print_args(                         uint32_t arg_count,
                                                struct application_argument args[]);

/* Command line parsing */
/*****************************************************************************/
/** @brief Convert string containing list delimited by TOKEN_LIST into skiq_rx_hdl_t
    constant.

    @param[in]  *handle_str:   input string containing list of handles
    @param[out] *rx_handles:   array of skiq_rx_hdl_t and size skiq_rx_hdl_end
    @param[out] *p_nr_handles: pointer to number of handles ( 0 for ALL )
    @param[out] *p_chan_mode:  pointer to skiq_chan_mode_t

    @return int32_t:           - ERROR_NO_MEMORY
                               - ERROR_COMMAND_LINE
                               - 0 success
*/
extern int32_t parse_hdl_list(                  const char *handle_str,
                                                skiq_rx_hdl_t rx_handles[], 
                                                uint8_t *p_nr_handles,
                                                skiq_chan_mode_t *p_chan_mode );

/* Helper Functions */

/******************************************************************************/
/** @brief
    Converts a boolean to a printable string
 
    @param[in]  flag:         boolean flag 

    @return     char *:       printable string (char*)
 
*/
const char * bool_cstr(                         bool flag );

/*****************************************************************************/
/** @brief Convert string representation to handle constant

    @param[in] *str: string representation of handle

    @return hdl:     skiq_rx_hdl_t (enum for handle) skiq_rx_hdl_end is returned
                     if no valid input.
*/
extern skiq_rx_hdl_t str2rxhdl(                 const char *str );

/*****************************************************************************/
/** @brief
    Convert string representation to handle constant

    @param[in] *str: string representation of handle

    @return hdl:     skiq_tx_hdl_t (enum for handle) skiq_tx_hdl_end is returned
                     if no valid input.
*/
extern skiq_tx_hdl_t str2txhdl(                 const char *str );

/******************************************************************************/
/** @brief Convert skiq_rx_hdl_t constant to string representation

    @param[in] hdl:     skiq_rx_hdl_t (enum for handle)

    @return    char*:   string representation of handle "unknown" is returned if
                        invalid input
*/
extern const char *rxhdl_cstr(                  skiq_rx_hdl_t hdl );

/******************************************************************************/
/** @brief
    Convert skiq_tx_hdl_t constant to string representation

    @param[in] hdl:     skiq_tx_hdl_t (enum for handle)

    @return    char*:   string representation of handle "unknown" is returned if
                        invalid input
*/
extern const char *txhdl_cstr(                  skiq_tx_hdl_t hdl );

/*****************************************************************************/
/** @brief Convert skiq_1pps_source_t to string representation

    @param[in] source:  skiq_1pps_source_t (enum for pps source)

    @return    char*:   string representation of pps source "unknown" is returned if 
                        invalid input

*/
extern const char *pps_source_cstr(             skiq_1pps_source_t pps_source );

/*****************************************************************************/
/** @brief Convert skiq_trigger_src_t constant to string representation

    @param[in]  src:    skiq_trigger_src_t (enum for trigger)

    @return     char*:  string representation of handle "unknown" is returned if
                        invalid input

*/
extern const char *trigger_src_desc_cstr(       skiq_trigger_src_t src );

/*****************************************************************************/
/** @brief Convert skiq_chan_mode_t constant to string representation

    @param[in]  src:    skiq_chan_mode_t (enum for trigger)

    @return     char*:  string representation of handle "unknown" is returned if
                        invalid input

*/
extern const char *chan_mode_desc_cstr(         skiq_chan_mode_t mode );


/*****************************************************************************/
/** @brief Convert the integer version of the RF Port into a skiq_rf_port_t

    @param[in]      port            integer representation of the port number

    @return         skiq_rf_port_t  representative of the port number
*/
extern skiq_rf_port_t map_int_to_rf_port(       uint32_t port );

/*****************************************************************************/
/** @brief Dump rconfig to console for debugging

    @param[in]      *p_rconfig:     radio config pointer

    @return         char*:          string representation of handle
*/
extern void dump_rconfig(                       const struct radio_config *p_rconfig,
                                                const struct rx_radio_config *p_rx_rconfig,
                                                const struct tx_radio_config *p_tx_rconfig);

/*****************************************************************************/
/** @brief Dump args array for debugging

    @param[in]  *args:          pointer to args array
    @param[in]  num_args:       number of arguments in the array

    @return     void
*/
extern void print_arg_array(                    struct application_argument args[], 
                                                int num_args);
/* Radio configuration */

/*****************************************************************************/
/** @brief Get ALL handles for a specific card

    @param[in]  card:          card_id
    @param[out] *rx_handles:   array of skiq_rx_hdl_t and size skiq_rx_hdl_end
    @param[out] *p_nr_handles: pointer to number of handles
    @param[out] *p_chan_mode:  pointer to skiq_chan_mode_t

    @return int32_t:           - skiq_read_num_rx_chans
                               - skiq_read_rx_stream_handle_conflict
                               - 0 success
*/
extern int32_t get_all_handles(                 uint8_t card, 
                                                skiq_rx_hdl_t rx_handles[], 
                                                uint8_t *p_nr_handles,
                                                skiq_chan_mode_t *p_chan_mode );

/*****************************************************************************/
/** @brief Map command line arguments to rconfig structure
    Set the following fields in radio_config:
        trigger_src
        pps_source
        all_chan
        chan_mode
        all_chans
        rx_freq          (set to p_cmd_line_args->rx_freq)
        sample_rate      (set to p_cmd_line_args->sample_rate)
        bandwidth        (set to p_cmd_line_args->bandwidth)
        packed           (set to p_cmd_line_args->packed)
        use_counter      (set to p_cmd_line_args->use_counter)
        disable_dc_corr  (set to p_cmd_line_args->disable_dc_corr)
        blocking_rx      (set to p_cmd_line_args->blocking_rx)

    @param[in]   *cmd_line_args: (read) pointer to command line args
    @param[ouit] *rconfig:       (write) pointer to radio_config structure

    @return int32_t:             - ERROR_COMMAND_LINE
                                 - ERROR_NO_MEMORY
                                 - 0 success
*/
extern int32_t map_arguments_to_radio_config(   struct cmd_line_args *p_cmd_line_args, 
                                                struct radio_config *p_rconfig );

/*****************************************************************************/
/** @brief
    Map command line arguments to rconfig structure
    Set the following fields in radio_config:
        trigger_src
        pps_source
        all_chan
        chan_mode
        all_chans
        rx_freq          (set to p_cmd_line_args->rx_freq)
        sample_rate      (set to p_cmd_line_args->sample_rate)
        bandwidth        (set to p_cmd_line_args->bandwidth)
        packed           (set to p_cmd_line_args->packed)
        use_counter      (set to p_cmd_line_args->use_counter)
        disable_dc_corr  (set to p_cmd_line_args->disable_dc_corr)
        blocking_rx      (set to p_cmd_line_args->blocking_rx)

    @param[in]   *cmd_line_args: (read) pointer to command line args
    @param[ouit] *rconfig:       (write) pointer to radio_config structure

    @return int32_t:             - ERROR_COMMAND_LINE
                                 - ERROR_NO_MEMORY
                                 - 0 success
*/
extern int32_t map_arguments_to_rx_radio_config(    struct cmd_line_args *p_cmd_line_args, 
                                                    struct radio_config *p_rconfig,
                                                    struct rx_radio_config *p_rx_rconfig );


/*****************************************************************************/
/** @brief
    Map command line arguments to rconfig structure
    Set the following fields in radio_config:
        trigger_src
        pps_source
        all_chan
        chan_mode
        all_chans
        rx_freq          (set to p_cmd_line_args->rx_freq)
        sample_rate      (set to p_cmd_line_args->sample_rate)
        bandwidth        (set to p_cmd_line_args->bandwidth)
        packed           (set to p_cmd_line_args->packed)
        use_counter      (set to p_cmd_line_args->use_counter)
        disable_dc_corr  (set to p_cmd_line_args->disable_dc_corr)
        blocking_rx      (set to p_cmd_line_args->blocking_rx)

    @param[in]   *cmd_line_args: (read) pointer to command line args
    @param[ouit] *rconfig:       (write) pointer to radio_config structure

    @return int32_t:             - ERROR_COMMAND_LINE
                                 - ERROR_NO_MEMORY
                                 - 0 success
*/
extern int32_t map_arguments_to_tx_radio_config(   struct cmd_line_args *p_cmd_line_args, 
                                                   struct radio_config *p_rconfig, 
                                                   struct tx_radio_config *p_tx_rconfig );

/*****************************************************************************/
/** @brief 
    Initializes the libsidekiq library.  Then enables the specific cards.
    Then it gets all the handles for the cards.

    @param[in]      init_level      full or basic
    @param[in/out]  *rconfig:       pointer to radio_config structure

    @return         status:         indicating status


*/
extern int32_t init_libsidekiq(                 skiq_xport_init_level_t init_level, 
                                                struct radio_config *p_rconfig ); 

/*****************************************************************************/
/** @brief Configures the radio card at index 'card' given a radio_config structure

    @param[in]      card:           card to initialize
    @param[in/out]  *rconfig:       pointer to radio_config structure

    @return         status:         indicating status


    @note   This function does not error out if the requested sample rate 
            or bandwidth is not attained exactly.  It prints out the actual
            rates attained.  It is up to the application to check and error
            out if that is the desired behavior
*/
extern int32_t configure_radio(                 uint8_t card, 
                                                struct radio_config *p_rconfig );

/*****************************************************************************/
/** @brief 
    Configures the radio card at index 'card' given a radio_config structure

    @param[in]      card:           card to initialize
    @param[in/out]  *rconfig:       pointer to radio_config structure

    @return         status:         indicating status


    @note   This function does not error out if the requested sample rate 
            or bandwidth is not attained exactly.  It prints out the actual
            rates attained.  It is up to the application to check and error
            out if that is the desired behavior
*/
extern int32_t configure_rx_radio(              uint8_t card, 
                                                struct radio_config *p_rconfig,
                                                struct rx_radio_config *p_rx_rconfig );


/*****************************************************************************/
/** @brief 
    Configures the radio card at index 'card' given a radio_config structure

    @param[in]      card:           card to initialize
    @param[in/out]  *rconfig:       pointer to radio_config structure

    @return         status:         indicating status


    @note   This function does not error out if the requested sample rate 
            or bandwidth is not attained exactly.  It prints out the actual
            rates attained.  It is up to the application to check and error
            out if that is the desired behavior
*/
extern int32_t configure_tx_radio(              uint8_t card, 
                                                struct radio_config *p_rconfig, 
                                                struct tx_radio_config *p_tx_rconfig );
#endif
