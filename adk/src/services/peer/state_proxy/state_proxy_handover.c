/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       state_proxy_handover.c
\brief      State Proxy Handover related interfaces

*/

#include "service_marshal_types.h"
#include "app_handover_if.h"
#include "state_proxy_private.h"
#include "state_proxy_handover.h"
#include "state_proxy_connection.h"
#include "state_proxy_flags.h"

#include <logging.h>
#include <stdlib.h>

/******************************************************************************
 * Local Function Prototypes
 ******************************************************************************/
static bool stateProxy_Veto(void);

static bool stateProxy_Marshal(const bdaddr *bd_addr,
                               marshal_type_t type,
                               void **marshal_obj);

static app_unmarshal_status_t stateProxy_Unmarshal(const bdaddr *bd_addr,
                                 marshal_type_t type,
                                 void *unmarshal_obj);

static void stateProxy_Commit(bool is_primary);

/******************************************************************************
 * Global Declarations
 ******************************************************************************/

/*! #state_proxy_task_data_t marshal type descriptor. */
const marshal_type_descriptor_t marshal_type_descriptor_state_proxy_task_data_t = MAKE_MARSHAL_TYPE_DEFINITION_BASIC(state_proxy_data_t);

/*----------------------------------------------------------------------------*/

const marshal_type_info_t state_proxy_marshal_types[] = {
    MARSHAL_TYPE_INFO(state_proxy_task_data_t, MARSHAL_TYPE_CATEGORY_GENERIC)
};

const marshal_type_list_t state_proxy_marshal_types_list = {state_proxy_marshal_types, ARRAY_DIM(state_proxy_marshal_types)};

REGISTER_HANDOVER_INTERFACE(STATE_PROXY, &state_proxy_marshal_types_list, stateProxy_Veto, stateProxy_Marshal, stateProxy_Unmarshal, stateProxy_Commit);


/******************************************************************************
 * Local Function Definitions
 ******************************************************************************/
/*!
    \brief Handle Veto check during handover
    \return bool
*/
static bool stateProxy_Veto(void)
{
    /* No conditions to veto */
    return FALSE;
}

/*!
    \brief The function shall set marshal_obj to the address of the object to
           be marshalled.

    \param[in] bd_addr      Bluetooth address of the link to be marshalled.
    \param[in] type         Type of the data to be marshalled.
    \param[out] marshal_obj Holds address of data to be marshalled
    \return TRUE:           Required address of the requested object has been copied to marshal_obj.
            FALSE:          No data is required to be marshalled. marshal_obj is set to NULL.

*/
static bool stateProxy_Marshal(const bdaddr *bd_addr,
                               marshal_type_t type,
                               void **marshal_obj)
{
    bool status = FALSE;
    DEBUG_LOG("stateProxy_Marshal");
    *marshal_obj = NULL;
    UNUSED(bd_addr);

    switch (type)
    {

        case MARSHAL_TYPE(state_proxy_task_data_t):
            *marshal_obj = stateProxy_GetLocalData();
            status = TRUE;
            break;

        default:
            /* Should have never come here */
            Panic();
            break;
    }

    return status;
}

/*!
    \brief The function shall copy the unmarshal_obj associated to specific
            marshal type

    \param[in] bd_addr      Bluetooth address of the link to be unmarshalled.
    \param[in] type         Type of the unmarshalled data.
    \param[in] unmarshal_obj Address of the unmarshalled object.
    \return unmarshalling result. Based on this, caller decides whether to free
            the marshalling object or not.
*/
static app_unmarshal_status_t stateProxy_Unmarshal(const bdaddr *bd_addr,
                                 marshal_type_t type,
                                 void *unmarshal_obj)
{
    DEBUG_LOG("stateProxy_Unmarshal");
    app_unmarshal_status_t result = UNMARSHAL_FAILURE;
    UNUSED(bd_addr);

    switch (type)
    {

        case MARSHAL_TYPE(state_proxy_task_data_t):
        {
            state_proxy_data_t *remote = stateProxy_GetRemoteData();
            free(remote);
            /* Realloc so pmalloc debug tracks the owner of this pool correctly */
            remote = PanicNull(realloc(unmarshal_obj, sizeof(*remote)));
            stateProxy_GetRemoteData() = remote;
            result = UNMARSHAL_SUCCESS_DONT_FREE_OBJECT;
        }
        break;

        default:
            break;
    }

    return result;
}

#define COPY_FLAG_LOCAL_TO_REMOTE(flag_name) stateProxy_GetRemoteFlag(flag_name) = stateProxy_GetLocalFlag(flag_name)

/*!
    \brief Component commits to the specified role

    The component should take any actions necessary to commit to the
    new role.

    \param[in] is_primary   TRUE if new role is primary, else secondary

*/
static void stateProxy_Commit(bool is_primary)
{
    state_proxy_data_flags_t *clearflags;

    DEBUG_LOG("stateProxy_Commit");

    if (is_primary)
    {
        stateProxy_GetInitialFlags();

        clearflags = &stateProxy_GetRemoteData()->flags;
    }
    else
    {
        COPY_FLAG_LOCAL_TO_REMOTE(is_pairing);
        clearflags = &stateProxy_GetLocalData()->flags;
    }

    /* Clear flags that cannot be active on the secondary device during handover */
    clearflags->is_pairing = FALSE;

    StateProxy_SetRole(is_primary);
}


