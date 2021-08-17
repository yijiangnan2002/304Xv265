/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Procedure to select Le Advertising parameters based on system conditions
*/

#include "tws_topology_procedure_select_le_advertise_params.h"
#include "le_advertising_manager.h"
#include <logging.h>
#include <message.h>
#include <panic.h>

const LE_ADV_PARAMS_SET_TYPE_T proc_select_le_advertise_params_fast = {.type = LE_ADVERTISING_PARAMS_SET_TYPE_FAST };
const LE_ADV_PARAMS_SET_TYPE_T proc_select_le_advertise_params_fast_fallback = {.type = LE_ADVERTISING_PARAMS_SET_TYPE_FAST_FALLBACK };
const LE_ADV_PARAMS_SET_TYPE_T proc_select_le_advertise_params_slow = {.type = LE_ADVERTISING_PARAMS_SET_TYPE_SLOW };


void TwsTopology_ProcedureSelectLeAdvertiseParamsStart(Task result_task,
                                        procedure_start_cfm_func_t proc_start_cfm_fn,
                                        procedure_complete_func_t proc_complete_fn,
                                        Message goal_data);
void TwsTopology_ProcedureSelectLeAdvertiseParamsCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn);

const procedure_fns_t proc_select_le_adv_params_fns = {
    TwsTopology_ProcedureSelectLeAdvertiseParamsStart,
    TwsTopology_ProcedureSelectLeAdvertiseParamsCancel,
};

void TwsTopology_ProcedureSelectLeAdvertiseParamsStart(Task result_task,
                                           procedure_start_cfm_func_t proc_start_cfm_fn,
                                           procedure_complete_func_t proc_complete_fn,
                                           Message goal_data)
{
    LE_ADV_PARAMS_SET_TYPE_T *params_type = (LE_ADV_PARAMS_SET_TYPE_T*)goal_data;

    UNUSED(result_task);

    DEBUG_LOG("TwsTopology_ProcedureSelectLeAdvertiseParamsStart");

    LeAdvertisingManager_ParametersSelect(params_type->type);
    proc_start_cfm_fn(tws_topology_procedure_select_le_adv_params, procedure_result_success);
    Procedures_DelayedCompleteCfmCallback(proc_complete_fn,
                                            tws_topology_procedure_select_le_adv_params,
                                            procedure_result_success);
}

void TwsTopology_ProcedureSelectLeAdvertiseParamsCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    DEBUG_LOG("TwsTopology_ProcedureSelectLeAdvertiseParamsCancel");

    Procedures_DelayedCancelCfmCallback(proc_cancel_cfm_fn,
                                         tws_topology_procedure_select_le_adv_params,
                                         procedure_result_success);
}
