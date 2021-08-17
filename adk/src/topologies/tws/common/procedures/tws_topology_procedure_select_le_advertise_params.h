/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Procedure to select Le Advertising parameters based on system conditions
*/

#ifndef TWS_TOPOLOGY_PROCEDURE_SELECT_LE_ADVERTISE_PARAMS_H
#define TWS_TOPOLOGY_PROCEDURE_SELECT_LE_ADVERTISE_PARAMS_H


#include "tws_topology_procedures.h"
#include "tws_topology_config.h"

typedef struct
{
    tws_topology_le_adv_params_set_type_t type;
}LE_ADV_PARAMS_SET_TYPE_T;

extern const procedure_fns_t proc_select_le_adv_params_fns;

extern const LE_ADV_PARAMS_SET_TYPE_T proc_select_le_advertise_params_fast;

#define PROC_SELECT_LE_ADVERTISE_PARAMS_FAST    ((Message)&proc_select_le_advertise_params_fast)

extern const LE_ADV_PARAMS_SET_TYPE_T proc_select_le_advertise_params_fast_fallback;

#define PROC_SELECT_LE_ADVERTISE_PARAMS_FAST_FALLBACK    ((Message)&proc_select_le_advertise_params_fast_fallback)

extern const LE_ADV_PARAMS_SET_TYPE_T proc_select_le_advertise_params_slow;

#define PROC_SELECT_LE_ADVERTISE_PARAMS_SLOW    ((Message)&proc_select_le_advertise_params_slow)


#endif // TWS_TOPOLOGY_PROCEDURE_SELECT_LE_ADVERTISE_PARAMS_H
