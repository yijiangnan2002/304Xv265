/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the gaia framework earbud plugin
*/

#ifdef INCLUDE_GAIA
#include "earbud_gaia_plugin.h"

#include <logging.h>
#include <panic.h>

#include <gaia_framework_feature.h>
#include <earbud_config.h>



void EarbudGaiaPlugin_PrimaryAboutToChange(uint8 delay)
{
    /* Inform the mobile app that the device is going to Primary/Secondary swap */
    gaia_transport_index index = 0;
    gaia_transport *t = Gaia_TransportIterate(&index);
    while (t)
    {
        if (Gaia_TransportIsConnected(t))
        {
            earbud_plugin_handover_types_t handover_type = Gaia_TransportHasFeature(t, GAIA_TRANSPORT_FEATURE_DYNAMIC_HANDOVER) ? dynamic_handover : static_handover;
            DEBUG_LOG_INFO("EarbudGaiaPlugin_PrimaryAboutToChange, handover_type %u, delay %u", handover_type, delay);
            if (handover_type == static_handover)
            {
                uint8 payload[2] = {handover_type, delay};
                GaiaFramework_SendNotificationWithTransport(t, GAIA_EARBUD_FEATURE_ID, primary_earbud_about_to_change, sizeof(payload), payload);
            }
        }
        t = Gaia_TransportIterate(&index);
    }
}


void EarbudGaiaPlugin_RoleChanged(tws_topology_role role)
{
    if (role == tws_topology_role_primary)
    {
        uint8 value = appConfigIsLeft() ? 0 : 1;
        GaiaFramework_SendNotification(GAIA_EARBUD_FEATURE_ID, primary_earbud_changed, sizeof(value), &value);
    }
}


static void earbudGaiaPlugin_IsPrimaryLeftOrRight(GAIA_TRANSPORT *t)
{
    /* Left earbud sets value to 0, right sets value to 1 */
    uint8 value = appConfigIsLeft() ? 0 : 1;
    DEBUG_LOG("earbudGaiaPlugin_WhichEarbudIsPrimary, %d", value);

    GaiaFramework_SendResponse(t, GAIA_EARBUD_FEATURE_ID, is_primary_left_or_right, sizeof(value), &value);
}


static void earbudGaiaPlugin_SendAllNotifications(GAIA_TRANSPORT *t)
{
    DEBUG_LOG("earbudGaiaPlugin_SendAllNotifications");
    UNUSED(t);
}


static void earbudGaiaPlugin_HandoverComplete(GAIA_TRANSPORT *t, bool is_primary)
{
    DEBUG_LOG("earbudGaiaPlugin_HandoverComplete, is_primary %u", is_primary);
    UNUSED(t);
}


static gaia_framework_command_status_t earbudGaiaPlugin_MainHandler(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("earbudGaiaPlugin_MainHandler, called for %d", pdu_id);

    UNUSED(payload_length);
    UNUSED(payload);

    switch (pdu_id)
    {
        case is_primary_left_or_right:
            earbudGaiaPlugin_IsPrimaryLeftOrRight(t);
            break;

        default:
            DEBUG_LOG_ERROR("earbudGaiaPlugin_MainHandler, unhandled call for %d", pdu_id);
            return command_not_handled;
    }

    return command_handled;
}

void EarbudGaiaPlugin_Init(void)
{
    static const gaia_framework_plugin_functions_t functions =
    {
        .command_handler = earbudGaiaPlugin_MainHandler,
        .send_all_notifications = earbudGaiaPlugin_SendAllNotifications,
        .transport_connect = NULL,
        .transport_disconnect = NULL,
        .handover_complete = earbudGaiaPlugin_HandoverComplete,
    };

    DEBUG_LOG("EarbydGaiaPlugin_Init");
    GaiaFramework_RegisterFeature(GAIA_EARBUD_FEATURE_ID, EARBUD_GAIA_PLUGIN_VERSION, &functions);
}

#endif /* INCLUDE_GAIA */
