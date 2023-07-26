/**
 * @file sigann.h
 *
 * @brief
 *
 *
 * <pre>
 * Copyright 2014-2021 Epiq Solutions, All Rights Reserved
 * </pre>
 */

#ifndef __SIGANN_H
#define __SIGANN_H

#include "sidekiq_api.h"
#include "arg_parser.h"
#include "utils_common.h"


#define SWEEPPOINTS     512


/*****************************************************************************/
/** @brief
    Configures 

    @param[in]      
    @param[in/out]  

    @return         


    @note   
*/
extern int32_t peakSearch(                      uint8_t card,
                                                struct radio_config *p_rconfig,
                                                struct rx_radio_config *p_rx_rconfig ,
                                                uint64_t center_freq,
                                                uint32_t span,
                                                uint64_t *peak_freq,
                                                int32_t *peak_power);


/*****************************************************************************/
/** @brief
    Configures 

    @param[in]      
    @param[in/out]  

    @return         


    @note   
*/
extern int32_t getData(                      uint8_t card,
                                                struct radio_config *p_rconfig,
                                                struct rx_radio_config *p_rx_rconfig ,
                                                uint64_t center_freq,
                                                uint32_t span,
                                                double* freq_array,
                                                double* power_array);


#endif

