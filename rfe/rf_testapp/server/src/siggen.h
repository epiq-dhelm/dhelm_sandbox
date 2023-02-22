/**
 * @file siggen.h
 *
 * @brief
 *
 *
 * <pre>
 * Copyright 2014-2021 Epiq Solutions, All Rights Reserved
 * </pre>
 */

#ifndef __SIGGEN_H__
#define __SIGGEN_H__

#include "sidekiq_api.h"
#include "arg_parser.h"
#include "utils_common.h"



/*****************************************************************************/
/** @brief
    Configures 

    @param[in]      
    @param[in/out]  

    @return         


    @note   
*/
extern int32_t startCW(                         uint8_t card,
                                                struct radio_config *p_rconfig,
                                                struct tx_radio_config *p_tx_rconfig ,
                                                uint64_t freq,
                                                uint32_t span,
                                                uint32_t power_level);


/*****************************************************************************/
/** @brief
    Configures 

    @param[in]      
    @param[in/out]  

    @return         


    @note   
*/
extern int32_t stopGen(                         uint8_t card,
                                                struct radio_config *p_rconfig,
                                                struct tx_radio_config *p_tx_rconfig); 

/*****************************************************************************/
/** @brief
    Configures 

    @param[in]      
    @param[in/out]  

    @return         


    @note   
*/
extern int32_t startSweep(                      uint8_t card,
                                                struct radio_config *p_rconfig,
                                                struct tx_radio_config *p_tx_rconfig ,
                                                uint32_t start_freq_MHz,
                                                uint32_t power_level,
                                                uint32_t steps,
                                                uint32_t freq_step_MHz,
                                                uint32_t step_time_ms,
                                                uint32_t span_MHz);


extern        void tx_complete( int32_t status, skiq_tx_block_t *p_data, void *p_user );

#endif

