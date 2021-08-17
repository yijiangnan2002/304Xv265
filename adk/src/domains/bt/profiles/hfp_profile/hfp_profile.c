/*!
\copyright  Copyright (c) 2008 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Application domain HFP component.
*/

#include <panic.h>
#include <ps.h>
#include <ps_key_map.h>

#ifdef INCLUDE_HFP

#include "hfp_profile.h"

#include "system_state.h"
#include "bt_device.h"
#include "device_properties.h"
#include "link_policy.h"
#include "voice_sources.h"
#include "hfp_profile_audio.h"
#include "hfp_profile_battery_level.h"
#include "hfp_profile_config.h"
#include "hfp_profile_instance.h"
#include "hfp_profile_private.h"
#include "hfp_profile_sm.h"
#include "hfp_profile_telephony_control.h"
#include "hfp_profile_voice_source_link_prio_mapping.h"
#include "hfp_profile_volume.h"
#include "hfp_profile_volume_observer.h"
#include "telephony_messages.h"
#include "volume_messages.h"
#include "volume_utils.h"
#include "kymera.h"

#include <profile_manager.h>
#include <av.h>
#include <connection_manager.h>
#include <device.h>
#include <device_list.h>
#include <device_db_serialiser.h>
#include <focus_voice_source.h>
#include <focus_generic_source.h>
#include <logging.h>
#include <mirror_profile.h>
#include <stdio.h>
#include <stdlib.h>
#include <message.h>
#include "ui.h"

/*! There is checking that the messages assigned by this module do
not overrun into the next module's message ID allocation */
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(APP_HFP, APP_HFP_MESSAGE_END)
ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(HFP_INTERNAL_MESSAGE_END)

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_ENUM(hfp_profile_messages)
LOGGING_PRESERVE_MESSAGE_ENUM(hfp_profile_internal_messages)

#ifdef TEST_HFP_CODEC_PSKEY
/* PS Key used for the setting the supported HFP codecs during testing */
#define HFP_CODEC_PSKEY (181)

#define HFP_CODEC_PS_BIT_NB             (1<<0)
#define HFP_CODEC_PS_BIT_WB             (1<<1)
#define HFP_CODEC_PS_BIT_SWB            (1<<2)
#endif


/*! \brief Application HFP component main data structure. */
hfpTaskData hfp_profile_task_data;

/*! \brief Call status state lookup table

    This table is used to convert the call setup and call indicators into
    the appropriate HFP state machine state
*/
const hfpState hfp_call_state_table[10] =
{
    /* hfp_call_state_idle              */ HFP_STATE_CONNECTED_IDLE,
    /* hfp_call_state_incoming          */ HFP_STATE_CONNECTED_INCOMING,
    /* hfp_call_state_incoming_held     */ HFP_STATE_CONNECTED_INCOMING,
    /* hfp_call_state_outgoing          */ HFP_STATE_CONNECTED_OUTGOING,
    /* hfp_call_state_active            */ HFP_STATE_CONNECTED_ACTIVE,
    /* hfp_call_state_twc_incoming      */ HFP_STATE_CONNECTED_IDLE, /* Not supported */
    /* hfp_call_state_twc_outgoing      */ HFP_STATE_CONNECTED_IDLE, /* Not supported */
    /* hfp_call_state_held_active       */ HFP_STATE_CONNECTED_IDLE, /* Not supported */
    /* hfp_call_state_held_remaining    */ HFP_STATE_CONNECTED_IDLE, /* Not supported */
    /* hfp_call_state_multiparty        */ HFP_STATE_CONNECTED_IDLE, /* Not supported */
};

/* Local Function Prototypes */
static void hfpProfile_TaskMessageHandler(Task task, MessageId id, Message message);

/*! \brief Is HFP voice recognition active for the specified instance*/
static bool hfpProfile_IsVoiceRecognitionActive(hfpInstanceTaskData * instance)
{
    return (instance != NULL) ? instance->bitfields.voice_recognition_active : FALSE;
}

/*! \brief Check SCO encryption

    This functions is called to check if SCO is encrypted or not.  If there is
    a SCO link active, a call is in progress and the link becomes unencrypted,
    send a Telephony message that could be used to provide an indication tone
    to the user, depenedent on UI configuration.
*/
void HfpProfile_CheckEncryptedSco(hfpInstanceTaskData * instance)
{
    DEBUG_LOG("HfpProfile_CheckEncryptedSco(%p) encrypted=%d sink=%x)",
              instance, instance->bitfields.encrypted, instance->sco_sink);

    /* Check SCO is active */
    if (HfpProfile_IsScoActiveForInstance(instance) && appHfpIsCallForInstance(instance))
    {
        /* Check if link is encrypted */
        if (!HfpProfileInstance_IsEncrypted(instance))
        {
            voice_source_t source = HfpProfileInstance_GetVoiceSourceForInstance(instance);
            hfp_link_priority link = HfpProfile_GetHfpLinkPrioForVoiceSource(source);
            if (link != hfp_invalid_link)
            {
                Telephony_NotifyCallBecameUnencrypted(HfpProfile_GetVoiceSourceForHfpLinkPrio(link));
            }
            /* \todo Mute the MIC to prevent eavesdropping */
        }
    }
}

/*! \brief Handle HFP Library initialisation confirmation
*/
static void hfpProfile_HandleHfpInitCfm(const HFP_INIT_CFM_T *cfm)
{
    DEBUG_LOG("hfpProfile_HandleHfpInitCfm status enum:hfp_init_status:%d", cfm->status);

    /* Check HFP initialisation was successful */
    if (cfm->status == hfp_init_success)
    {
        /* Tell main application task we have initialised */
        MessageSend(SystemState_GetTransitionTask(), APP_HFP_INIT_CFM, 0);
    }
    else
    {
        Panic();
    }
}

/*! \brief Handle SLC connect indication
*/
static void hfpProfile_HandleHfpSlcConnectInd(const HFP_SLC_CONNECT_IND_T *ind)
{
    DEBUG_LOG_FN_ENTRY("hfpProfile_HandleHfpSlcConnectInd lap=%d accepted=%d", ind->addr.lap, ind->accepted);

    if (!ind->accepted)
    {
        return;
    }

    hfpInstanceTaskData* instance = HfpProfileInstance_GetInstanceForBdaddr(&ind->addr);
    if (instance == NULL)
    {
        instance = HfpProfileInstance_Create(&ind->addr, TRUE);
    }
    if (instance != NULL)
    {
        hfpState state = appHfpGetState(instance);

        DEBUG_LOG("hfpProfile_HandleHfpSlcConnectInd(%p) enum:hfpState:%d", instance, state);

        if (state == HFP_STATE_DISCONNECTED)
        {
            /* Store address of AG */
            instance->ag_bd_addr = ind->addr;

            /* Set hfp link QHS status */            
            HfpSetSupportedQhsMode(&ind->addr, BtDevice_WasQhsConnected(&ind->addr));

            appHfpSetState(instance, HFP_STATE_CONNECTING_REMOTE);
        }
    }
}

/*! \brief Send SLC status indication to all clients on the list.
 */
static void hfpProfile_SendSlcStatus(bool connected, const bdaddr* bd_addr)
{
    Task next_client = 0;

    while (TaskList_Iterate(TaskList_GetFlexibleBaseTaskList(appHfpGetSlcStatusNotifyList()), &next_client))
    {
        MAKE_HFP_MESSAGE(APP_HFP_SLC_STATUS_IND);
        message->slc_connected = connected;
        message->bd_addr = *bd_addr;
        MessageSend(next_client, APP_HFP_SLC_STATUS_IND, message);
    }
}

static deviceLinkMode hfpProfile_GetLinkMode(device_t device)
{
    deviceLinkMode link_mode = DEVICE_LINK_MODE_UNKNOWN;
    void *value = NULL;
    size_t size = sizeof(deviceLinkMode);
    if (Device_GetProperty(device, device_property_link_mode, &value, &size))
    {
        link_mode = *(deviceType *)value;
    }
    return link_mode;
}

/*! \brief Determine if a device supports secure connections

    \param bd_addr Pointer to read-only device BT address.
    \return bool TRUE if secure connects supported, FALSE otherwise.
*/
static bool hfpProfile_IsSecureConnection(const bdaddr *bd_addr)
{
    bool is_secure_connection = FALSE;
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        is_secure_connection = (hfpProfile_GetLinkMode(device) == DEVICE_LINK_MODE_SECURE_CONNECTION);
    }
    return is_secure_connection;
}

static void hfpProfile_SendCallStatusNotification(hfpInstanceTaskData * instance)
{
    hfpState state = appHfpGetState(instance);
    switch(state)
    {
    case HFP_STATE_CONNECTED_INCOMING:
    case HFP_STATE_CONNECTED_OUTGOING:
    case HFP_STATE_CONNECTED_ACTIVE:
    {
        voice_source_t source = HfpProfileInstance_GetVoiceSourceForInstance(instance);
        if (VoiceSource_IsHfp(source))
        {
            if (state == HFP_STATE_CONNECTED_INCOMING)
            {
                Telephony_NotifyCallIncoming(source);
            }
            else
            {
                Telephony_NotifyCallActive(source);
            }
        }
        break;
    }

    default:
        break;
    }
}

/*! \brief Handle SLC connect confirmation
*/
static void hfpProfile_HandleHfpSlcConnectCfm(const HFP_SLC_CONNECT_CFM_T *cfm)
{
    hfpInstanceTaskData* instance = HfpProfileInstance_GetInstanceForBdaddr(&cfm->bd_addr);

    PanicNull(instance);

    hfpState state = appHfpGetState(instance);

    DEBUG_LOG("hfpProfile_HandleHfpSlcConnectCfm(%p) enum:hfpState:%d enum:hfp_connect_status:%d",
              instance, state, cfm->status);

    switch (state)
    {
        case HFP_STATE_CONNECTING_LOCAL:
        case HFP_STATE_CONNECTING_REMOTE:
        {
            if (cfm->status == hfp_connect_success)
            {
                hfp_call_state call_state = hfp_call_state_idle;

                /* Inform the hfp library if the link is secure */
                if (hfpProfile_IsSecureConnection(&cfm->bd_addr))
                {
                    HfpLinkSetLinkMode(cfm->priority, TRUE);
                }

                /* Update the HFP instance members at time of SLC connection. */
                PanicFalse(HfpLinkGetCallState(cfm->priority, &call_state));
                instance->bitfields.call_state = call_state;
                instance->slc_sink = cfm->sink;
                instance->profile = cfm->profile;
                appHfpSetState(instance, hfp_call_state_table[call_state]);

                /* Turn off link-loss management. */
                HfpManageLinkLoss(cfm->priority, FALSE);

                /* Ensure the underlying ACL is encrypted. */
                ConnectionSmEncrypt(HfpProfile_GetInstanceTask(instance), instance->slc_sink, TRUE);

                /* Notify clients of SLC connection and also if there is an active call */
                hfpProfile_SendSlcStatus(TRUE, &cfm->bd_addr);
                hfpProfile_SendCallStatusNotification(instance);
            }
            else
            {
                /* The SLC connection was not successful, notify clients */
                Telephony_NotifyCallConnectFailure(HfpProfile_GetVoiceSourceForHfpLinkPrio(cfm->priority));

                /* Tear down the HFP instance. */
                instance->bitfields.disconnect_reason = APP_HFP_CONNECT_FAILED;
                appHfpSetState(instance, HFP_STATE_DISCONNECTED);
                HfpProfileInstance_Destroy(instance);
            }
        }
        break;

        default:
            HfpProfile_HandleError(instance, HFP_SLC_CONNECT_CFM, cfm);
            break;
    }
}

/*! \brief Handle SLC disconnect indication
*/
static void hfpProfile_HandleHfpSlcDisconnectInd(const HFP_SLC_DISCONNECT_IND_T *ind)
{
    hfpInstanceTaskData* instance = HfpProfileInstance_GetInstanceForBdaddr(&ind->bd_addr);

    PanicNull(instance);

    voice_source_t source = HfpProfile_GetVoiceSourceForHfpLinkPrio(ind->priority);
    hfpState state = appHfpGetState(instance);

    DEBUG_LOG("hfpProfile_HandleHfpSlcDisconnectInd(%p) enum:hfpState:%d enum:hfp_disconnect_status:%d",
              instance, state, ind->status);

    switch (state)
    {
        case HFP_STATE_CONNECTING_LOCAL:
        case HFP_STATE_CONNECTING_REMOTE:
        case HFP_STATE_CONNECTED_IDLE:
        case HFP_STATE_CONNECTED_OUTGOING:
        case HFP_STATE_CONNECTED_INCOMING:
        case HFP_STATE_CONNECTED_ACTIVE:
        {
            /* Check if SCO is still up */
            if (instance->sco_sink)
            {
                /* Disconnect SCO */
                HfpAudioDisconnectRequest(ind->priority);
            }

            /* Reconnect on link loss */
            if (ind->status == hfp_disconnect_link_loss && !instance->bitfields.detach_pending)
            {
                Telephony_NotifyDisconnectedDueToLinkloss(source);

                 /* Set disconnect reason */
                 instance->bitfields.disconnect_reason = APP_HFP_DISCONNECT_LINKLOSS;
            }
            else
            {
                Telephony_NotifyDisconnected(source);

                /* Set disconnect reason */
                instance->bitfields.disconnect_reason = APP_HFP_DISCONNECT_NORMAL;
            }

            /* Inform clients */
            hfpProfile_SendSlcStatus(FALSE, HfpProfile_GetHandsetBdAddr(instance));

            /* Move to disconnected state */
            appHfpSetState(instance, HFP_STATE_DISCONNECTED);

            HfpProfileInstance_Destroy(instance);
        }
        break;

        case HFP_STATE_DISCONNECTING:
        case HFP_STATE_DISCONNECTED:
        {
            /* If the status is "transferred" do not notify clients and change
               state in the usual manner. Notifying clients could cause UI
               changes (e.g. playing the "disconnected" prompt) which isn't
               required during handover, as the link is "transferred", not
               disconnected. The new secondary sets its state to
               HFP_STATE_DISCONNECTED on commit, allowing the HFP instance
               to be cleanly destroyed. */
            if(ind->status != hfp_disconnect_transferred)
            {
                Telephony_NotifyDisconnected(source);

                /* Set disconnect reason */
                instance->bitfields.disconnect_reason = APP_HFP_DISCONNECT_NORMAL;

                /* Move to disconnected state */
                appHfpSetState(instance, HFP_STATE_DISCONNECTED);
            }
            HfpProfileInstance_Destroy(instance);
        }
        break;

        default:
            HfpProfile_HandleError(instance, HFP_SLC_DISCONNECT_IND, ind);
            return;
    }
}

/*! \brief Handle SCO Audio connect indication
*/
static void hfpProfile_HandleHfpAudioConnectInd(const HFP_AUDIO_CONNECT_IND_T *ind)
{
    DEBUG_LOG_FN_ENTRY("hfpProfile_HandleHfpAudioConnectInd enum:hfp_link_priority:%d", ind->priority);

    voice_source_t source = HfpProfile_GetVoiceSourceForHfpLinkPrio(ind->priority);
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    if (instance == NULL)
    {
        /* Reject SCO connection */
        HfpAudioConnectResponse(ind->priority, FALSE, 0, NULL, FALSE);
    }
    else
    {
        hfpState state = appHfpGetState(instance);

        DEBUG_LOG("hfpProfile_HandleHfpAudioConnectInd(%p) enum:hfpState:%d", instance, state);

        switch (state)
        {
            case HFP_STATE_CONNECTED_IDLE:
            case HFP_STATE_CONNECTED_OUTGOING:
            case HFP_STATE_CONNECTED_INCOMING:
            case HFP_STATE_CONNECTED_ACTIVE:
            case HFP_STATE_CONNECTING_LOCAL:
            case HFP_STATE_CONNECTING_REMOTE:
            {
                /* Set flag so context presented to focus module reflects that this link will have audio */
                instance->bitfields.esco_connecting = TRUE;

                if(HfpProfile_IsScoActive() && (Focus_GetFocusForVoiceSource(source) != focus_foreground))
                {
                    /* If we already have an active SCO and this link does not have priority, reject it */
                    HfpAudioConnectResponse(ind->priority, FALSE, 0, NULL, FALSE);
                    instance->bitfields.esco_connecting = FALSE;

                    if(state == HFP_STATE_CONNECTED_INCOMING)
                    {
                        /* Fake an out-of-band ring once to notify user */
                        MESSAGE_MAKE(msg, HFP_INTERNAL_OUT_OF_BAND_RINGTONE_REQ_T);
                        msg->instance = instance;
                        MessageCancelFirst(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_OUT_OF_BAND_RINGTONE_REQ);
                        MessageSend(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_OUT_OF_BAND_RINGTONE_REQ, msg);
                    }
                }
                else
                {
                    if (hfp_profile_task_data.sco_sync_task)
                    {
                        MESSAGE_MAKE(sync_ind, APP_HFP_SCO_CONNECTING_SYNC_IND_T);
                        sync_ind->device = BtDevice_GetDeviceForBdAddr(&instance->ag_bd_addr);
                        MessageSend(hfp_profile_task_data.sco_sync_task, APP_HFP_SCO_CONNECTING_SYNC_IND, sync_ind);
                    }
                    else
                    {
                        /* If no sync task, just accept */
                        HfpAudioConnectResponse(ind->priority, TRUE, instance->sco_supported_packets ^ sync_all_edr_esco, NULL, FALSE);
                    }
                }
            }
            break;

            default:
            {
                /* Reject SCO connection */
                HfpAudioConnectResponse(ind->priority, FALSE, 0, NULL, FALSE);
            }
            break;
        }
    }
}

/*! \brief Handle SCO Audio connect confirmation
*/
static void hfpProfile_HandleHfpAudioConnectCfm(const HFP_AUDIO_CONNECT_CFM_T *cfm)
{
    voice_source_t source = HfpProfile_GetVoiceSourceForHfpLinkPrio(cfm->priority);
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    PanicNull(instance);

    hfpState state = appHfpGetState(instance);

    DEBUG_LOG("hfpProfile_HandleHfpAudioConnectCfm(%p) enum:hfpState:%d enum:hfp_audio_connect_status:%d",
              instance, state, cfm->status);

    instance->bitfields.esco_connecting = FALSE;

    switch (state)
    {
        case HFP_STATE_CONNECTED_IDLE:
        case HFP_STATE_CONNECTED_OUTGOING:
        case HFP_STATE_CONNECTED_INCOMING:
        case HFP_STATE_CONNECTED_ACTIVE:
        case HFP_STATE_DISCONNECTING:
        case HFP_STATE_CONNECTING_LOCAL:
        case HFP_STATE_CONNECTING_REMOTE:
        {
            /* Check if audio connection was successful. */
            if (cfm->status == hfp_audio_connect_success)
            {
                /* Inform client tasks SCO is active */
                TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(appHfpGetStatusNotifyList()), APP_HFP_SCO_CONNECTED_IND);

                /* Store sink associated with SCO */
                instance->sco_sink = cfm->audio_sink;

                /* Check if SCO is now encrypted (or not) */
                HfpProfile_CheckEncryptedSco(instance);

                /* Update link policy now SCO is active */
                appLinkPolicyUpdatePowerTable(HfpProfile_GetHandsetBdAddr(instance));

#ifdef INCLUDE_AV
                /* Check if AG only supports HV1 SCO packets, if so then disconnect A2DP & AVRCP */
                if (appAvHasAConnection() && (instance->sco_supported_packets == sync_hv1))
                {
                    /* Disconnect AV */
                    appAvDisconnectHandset();

                    /* Set flag for AV re-connect */
                    instance->bitfields.sco_av_reconnect = TRUE;
                }
                else
                {
                    /* Clear flag for AV re-connect */
                    instance->bitfields.sco_av_reconnect = FALSE;
                }
#endif

                HfpProfile_StoreConnectParams(instance, cfm->codec, cfm->wesco, cfm->tesco, cfm->qce_codec_mode_id);

#if defined(INCLUDE_MIRRORING)
                MirrorProfile_HandleHfpAudioConnectConfirmation(source);
#else
                Telephony_NotifyCallAudioConnected(source);
#endif

                /* Check if in HSP mode, use audio connection as indication of active call */
                if (instance->profile == hfp_headset_profile)
                {
                    /* Move to active call state */
                    appHfpSetState(instance, HFP_STATE_CONNECTED_ACTIVE);
                }

                /* Play SCO connected tone, only play if state is ConnectedIncoming,
                   ConnectedOutgoing or ConnectedActive and not voice recognition */
                if (appHfpIsCallForInstance(instance) && !hfpProfile_IsVoiceRecognitionActive(instance))
                {
                    /* Set flag indicating we need UI tone when SCO disconnects */
                    instance->bitfields.sco_ui_indication = TRUE;

                    Telephony_NotifyCallAudioRenderedLocal(source);
                }
                else
                {
                    /* Clear flag indicating we don't need UI tone when SCO disconnects */
                    instance->bitfields.sco_ui_indication = FALSE;
                }
            }
            else if (cfm->status == hfp_audio_connect_in_progress)
            {
                /* This can happen if we have asked to transfer the audio to this device
                   multiple times before the first HFP_AUDIO_CONNECT_CFM was received.

                   Do nothing here because eventually we should get the CFM for the
                   first request with another success or failure status. */
                instance->bitfields.esco_connecting = TRUE;
            }
        }
        return;

        default:
            HfpProfile_HandleError(instance, HFP_AUDIO_CONNECT_CFM, cfm);
            return;
    }
}

/*! \brief Handle SCO Audio disconnect indication
*/
static void hfpProfile_HandleHfpAudioDisconnectInd(const HFP_AUDIO_DISCONNECT_IND_T *ind)
{
    /* The SCO has been transferred to the secondary earbud. Ignore this message.
       The SLC disconnection will clean up the hfp state */
    if (ind->status == hfp_audio_disconnect_transferred)
    {
        return;
    }

    voice_source_t source = HfpProfile_GetVoiceSourceForHfpLinkPrio(ind->priority);
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    /* The instance may have been destroyed already as part of SLC disconnect request earlier, leave the handler in this case */
    if(NULL == instance)
    {
        return;
    }

    hfpState state = appHfpGetState(instance);

    DEBUG_LOG("hfpProfile_HandleHfpAudioDisconnectInd(%p) enum:hfp_audio_disconnect_status:%d enum:hfpState:%d",
              instance, ind->status, state);

    instance->bitfields.esco_connecting = FALSE;

    switch (state)
    {
        case HFP_STATE_CONNECTED_IDLE:
        case HFP_STATE_CONNECTED_OUTGOING:
        case HFP_STATE_CONNECTED_INCOMING:
        case HFP_STATE_CONNECTED_ACTIVE:
        case HFP_STATE_DISCONNECTING:
        case HFP_STATE_DISCONNECTED:
        case HFP_STATE_CONNECTING_LOCAL:
        case HFP_STATE_CONNECTING_REMOTE:
        {
            /* Inform client tasks SCO is inactive */
            TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(appHfpGetStatusNotifyList()), APP_HFP_SCO_DISCONNECTED_IND);

            /* Check if have SCO link */
            if (instance->sco_sink)
            {
#ifdef INCLUDE_AV
                /* Check if we need to reconnect AV */
                if (instance->bitfields.sco_av_reconnect)
                    appAvConnectHandset(FALSE);
#endif
                Telephony_NotifyCallAudioRenderedLocal(source);

                Telephony_NotifyCallAudioDisconnected(source);

                /* Check if in HSP mode, if so then end the call */
                if (instance->profile == hfp_headset_profile && appHfpIsConnectedForInstance(instance))
                {
                    /* Move to connected state */
                    appHfpSetState(instance, HFP_STATE_CONNECTED_IDLE);
                }

                /* Clear SCO sink */
                instance->sco_sink = 0;

                /* Clear any SCO unencrypted reminders */
                HfpProfile_CheckEncryptedSco(instance);

                /* Update link policy now SCO is inactive */
                appLinkPolicyUpdatePowerTable(HfpProfile_GetHandsetBdAddr(instance));
            }
        }
        return;

        default:
            HfpProfile_HandleError(instance, HFP_AUDIO_DISCONNECT_IND, ind);
            return;
    }
}

/* TODO: Support for HFP encryption change ? */

/*! \brief Handle Ring indication
*/
static void hfpProfile_HandleHfpRingInd(const HFP_RING_IND_T *ind)
{
    voice_source_t source = HfpProfile_GetVoiceSourceForHfpLinkPrio(ind->priority);
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG("hfpProfile_HandleHfpRingInd(%p) in_band=%d", instance, ind->in_band);

    PanicNull(instance);

    switch (appHfpGetState(instance))
    {
        case HFP_STATE_CONNECTED_IDLE:
        {
            /* Check if in HSP mode, use rings as indication of incoming call */
            if (instance->profile == hfp_headset_profile)
            {
                /* Move to incoming call establishment */
                appHfpSetState(instance, HFP_STATE_CONNECTED_INCOMING);

                /* Start HSP incoming call timeout */
                MAKE_HFP_MESSAGE(HFP_INTERNAL_HSP_INCOMING_TIMEOUT);
                message->instance = instance;
                MessageSendLater(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_HSP_INCOMING_TIMEOUT, message, D_SEC(5));
            }

            /* Play ring tone if AG doesn't support in band ringing */
            if (!ind->in_band && !instance->bitfields.call_accepted)
            {
                Telephony_NotifyCallIncomingOutOfBandRingtone(source);
            }
        }
        return;

        case HFP_STATE_CONNECTED_INCOMING:
        {
            /* Check if in HSP mode, use rings as indication of incoming call */
            if (instance->profile == hfp_headset_profile)
            {
                /* Reset incoming call timeout */
                MessageCancelFirst(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_HSP_INCOMING_TIMEOUT);

                MAKE_HFP_MESSAGE(HFP_INTERNAL_HSP_INCOMING_TIMEOUT);
                message->instance = instance;
                MessageSendLater(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_HSP_INCOMING_TIMEOUT, message, D_SEC(5));
            }
        }
        /* Fallthrough */

        case HFP_STATE_CONNECTED_ACTIVE:
        {
            /* Play ring tone if AG doesn't support in band ringing or this source is not routed */
            if ((!ind->in_band || instance->source_state != source_state_connected) && !instance->bitfields.call_accepted)
            {
                Telephony_NotifyCallIncomingOutOfBandRingtone(source);
            }
        }
        return;

        case HFP_STATE_DISCONNECTING:
            return;

        default:
            HfpProfile_HandleError(instance, HFP_RING_IND, ind);
            return;
    }
}

/*! \brief Handle service indication
*/
static void hfpProfile_HandleHfpServiceInd(const HFP_SERVICE_IND_T *ind)
{
    voice_source_t source = HfpProfile_GetVoiceSourceForHfpLinkPrio(ind->priority);
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG("hfpProfile_HandleHfpServiceInd(%p) enum:hfp_link_priority:%d service=%d",
              instance, ind->priority, ind->service);

    PanicNull(instance);

    switch (appHfpGetState(instance))
    {
        case HFP_STATE_CONNECTING_LOCAL:
        case HFP_STATE_CONNECTING_REMOTE:
        case HFP_STATE_CONNECTED_IDLE:
        case HFP_STATE_CONNECTED_OUTGOING:
        case HFP_STATE_CONNECTED_INCOMING:
        case HFP_STATE_CONNECTED_ACTIVE:
        case HFP_STATE_DISCONNECTING:
        {
            /* TODO: Handle service/no service */
        }
        return;

        default:
            HfpProfile_HandleError(instance, HFP_SERVICE_IND, ind);
            return;
    }
}

/*! \brief Handle call state indication
*/
static void hfpProfile_HandleHfpCallStateInd(const HFP_CALL_STATE_IND_T *ind)
{
    voice_source_t source = HfpProfile_GetVoiceSourceForHfpLinkPrio(ind->priority);
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    PanicNull(instance);

    hfpState hfp_instance_state = appHfpGetState(instance);

    DEBUG_LOG("hfpProfile_HandleHfpCallStateInd(%p) enum:hfpState:%d enum:hfp_call_state:%d",
              instance, hfp_instance_state, ind->call_state );

    switch (hfp_instance_state)
    {
        case HFP_STATE_CONNECTING_LOCAL:
        case HFP_STATE_CONNECTING_REMOTE:
        case HFP_STATE_DISCONNECTING:
        {
            /* Store call setup indication */
            instance->bitfields.call_state = ind->call_state;
        }
        return;

        case HFP_STATE_CONNECTED_IDLE:
        case HFP_STATE_CONNECTED_OUTGOING:
        case HFP_STATE_CONNECTED_INCOMING:
        case HFP_STATE_CONNECTED_ACTIVE:
        {
            hfpState state;

            /* Store call setup indication */
            instance->bitfields.call_state = ind->call_state;

            /* Move to new state, depending on call state */
            state = hfp_call_state_table[instance->bitfields.call_state];
            if (hfp_instance_state != state)
                appHfpSetState(instance, state);
        }
        return;

        default:
            HfpProfile_HandleError(instance, HFP_CALL_STATE_IND, ind);
            return;
    }
}

/*! \brief Handle voice recognition indication
*/
static void hfpProfile_HandleHfpVoiceRecognitionInd(const HFP_VOICE_RECOGNITION_IND_T *ind)
{
    voice_source_t source = HfpProfile_GetVoiceSourceForHfpLinkPrio(ind->priority);
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG("hfpProfile_HandleHfpVoiceRecognitionInd(%p) enabled=%d", instance, ind->enable);

    PanicNull(instance);

    switch (appHfpGetState(instance))
    {
        case HFP_STATE_CONNECTING_LOCAL:
        case HFP_STATE_CONNECTING_REMOTE:
        case HFP_STATE_CONNECTED_IDLE:
        case HFP_STATE_CONNECTED_OUTGOING:
        case HFP_STATE_CONNECTED_INCOMING:
        case HFP_STATE_CONNECTED_ACTIVE:
        case HFP_STATE_DISCONNECTING:
        {
            instance->bitfields.voice_recognition_active = ind->enable;
        }
        return;

        default:
            HfpProfile_HandleError(instance, HFP_VOICE_RECOGNITION_IND, ind);
            return;
    }
}

/*! \brief Handle voice recognition enable confirmation
*/
static void hfpProfile_HandleHfpVoiceRecognitionEnableCfm(const HFP_VOICE_RECOGNITION_ENABLE_CFM_T *cfm)
{
    voice_source_t source = HfpProfile_GetVoiceSourceForHfpLinkPrio(cfm->priority);
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    PanicNull(instance);

    hfpState state = appHfpGetState(instance);

    DEBUG_LOG("hfpProfile_HandleHfpVoiceRecognitionEnableCfm(%p) enum:hfpState:%d enum:hfp_lib_status:%d ",
              instance, state, cfm->status);

    switch (state)
    {
        case HFP_STATE_CONNECTED_IDLE:
        case HFP_STATE_CONNECTED_OUTGOING:
        case HFP_STATE_CONNECTED_INCOMING:
        case HFP_STATE_CONNECTED_ACTIVE:
        case HFP_STATE_DISCONNECTING:
        {
            if (cfm->status == hfp_success)
                instance->bitfields.voice_recognition_active = instance->bitfields.voice_recognition_request;
            else
                instance->bitfields.voice_recognition_request = instance->bitfields.voice_recognition_active;
        }
        return;

        default:
            HfpProfile_HandleError(instance, HFP_VOICE_RECOGNITION_ENABLE_CFM, cfm);
            return;
    }
}

/*! \brief Handle caller ID indication
*/
static void hfpProfile_HandleHfpCallerIdInd(const HFP_CALLER_ID_IND_T *ind)
{
    voice_source_t source = HfpProfile_GetVoiceSourceForHfpLinkPrio(ind->priority);
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG("hfpProfile_HandleHfpCallerIdInd(%p)", instance);

    PanicNull(instance);

    switch (appHfpGetState(instance))
    {
        case HFP_STATE_CONNECTED_INCOMING:
        {
            /* Check we haven't already accepted the call */
            if (!instance->bitfields.call_accepted)
            {
                /* Queue prompt & number
                 * This was a todo on playing voice prompts to announce the caller id from text to speech */
            }
        }
        return;

        case HFP_STATE_DISCONNECTING:
            return;

        default:
            HfpProfile_HandleError(instance, HFP_CALLER_ID_IND, ind);
            return;
    }
}

/*! \brief Handle caller ID enable confirmation
*/
static void hfpProfile_HandleHfpCallerIdEnableCfm(const HFP_CALLER_ID_ENABLE_CFM_T *cfm)
{
    voice_source_t source = HfpProfile_GetVoiceSourceForHfpLinkPrio(cfm->priority);
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    PanicNull(instance);

    hfpState state = appHfpGetState(instance);

    DEBUG_LOG("hfpProfile_HandleHfpCallerIdEnableCfm(%p) enum:hfpState:%d enum:hfp_lib_status:%d ",
              instance, state, cfm->status);

    switch (state)
    {
        case HFP_STATE_CONNECTED_IDLE:
        case HFP_STATE_CONNECTED_OUTGOING:
        case HFP_STATE_CONNECTED_INCOMING:
        case HFP_STATE_CONNECTED_ACTIVE:
        case HFP_STATE_DISCONNECTING:
        {
            if (cfm->status == hfp_success)
                instance->bitfields.caller_id_active = TRUE;
        }
        return;

        default:
            HfpProfile_HandleError(instance, HFP_CALLER_ID_ENABLE_CFM, cfm);
            return;
    }
}

/*! \brief Handle volume indication
*/
static void hfpProfile_HandleHfpVolumeSyncSpeakerGainInd(const HFP_VOLUME_SYNC_SPEAKER_GAIN_IND_T *ind)
{
    voice_source_t source = HfpProfile_GetVoiceSourceForHfpLinkPrio(ind->priority);
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG("hfpProfile_HandleHfpVolumeSyncSpeakerGainInd(%p) vol=%d", instance, ind->volume_gain);

    PanicNull(instance);

    switch (appHfpGetState(instance))
    {
        case HFP_STATE_CONNECTING_LOCAL:
        case HFP_STATE_CONNECTING_REMOTE:
        case HFP_STATE_CONNECTED_IDLE:
        case HFP_STATE_CONNECTED_OUTGOING:
        case HFP_STATE_CONNECTED_INCOMING:
        case HFP_STATE_CONNECTED_ACTIVE:
        case HFP_STATE_DISCONNECTING:
        {
            Volume_SendVoiceSourceVolumeUpdateRequest(source, event_origin_external, ind->volume_gain);
        }
        return;

        default:
            HfpProfile_HandleError(instance, HFP_VOLUME_SYNC_SPEAKER_GAIN_IND, ind);
            return;
    }
}

/*! \brief Handle microphone volume indication
*/
static void hfpProfile_HandleHfpVolumeSyncMicGainInd(const HFP_VOLUME_SYNC_MICROPHONE_GAIN_IND_T *ind)
{
    voice_source_t source = HfpProfile_GetVoiceSourceForHfpLinkPrio(ind->priority);
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG("hfpProfile_HandleHfpVolumeSyncMicGainInd(%p) mic_gain=%d", instance, ind->mic_gain);

    PanicNull(instance);

    switch (appHfpGetState(instance))
    {
        case HFP_STATE_CONNECTING_LOCAL:
        case HFP_STATE_CONNECTING_REMOTE:
        case HFP_STATE_CONNECTED_IDLE:
        case HFP_STATE_CONNECTED_OUTGOING:
        case HFP_STATE_CONNECTED_INCOMING:
        case HFP_STATE_CONNECTED_ACTIVE:
        case HFP_STATE_DISCONNECTING:
        {
            /* Set input gain */
            device_t device = HfpProfileInstance_FindDeviceFromInstance(instance);
            Device_SetPropertyU8(device, device_property_hfp_mic_gain, ind->mic_gain);

            /* Store new configuration */
            HfpProfile_StoreConfig(device);
        }
        return;

        default:
            HfpProfile_HandleError(instance, HFP_VOLUME_SYNC_MICROPHONE_GAIN_IND, ind);
            return;
    }
}

/*! \brief Handle answer call confirmation
*/
static void hfpProfile_HandleHfpCallAnswerCfm(const HFP_CALL_ANSWER_CFM_T *cfm)
{
    voice_source_t source = HfpProfile_GetVoiceSourceForHfpLinkPrio(cfm->priority);
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    PanicNull(instance);

    hfpState state = appHfpGetState(instance);

    DEBUG_LOG("hfpProfile_HandleHfpCallAnswerCfm(%p) enum:hfpState:%d enum:hfp_lib_status:%d",
              instance, state, cfm->status);

    switch (state)
    {
        case HFP_STATE_CONNECTED_INCOMING:
        {
            if (cfm->status == hfp_success)
            {
                /* Flag call as accepted, so we ignore any ring indications or caller ID */
                instance->bitfields.call_accepted = TRUE;
            }
        }
        return;

        case HFP_STATE_CONNECTED_IDLE:
        case HFP_STATE_CONNECTED_ACTIVE:
        case HFP_STATE_DISCONNECTING:
            return;

        default:
            HfpProfile_HandleError(instance, HFP_CALL_ANSWER_CFM, cfm);
            return;
    }
}

/*! \brief Handle terminate call confirmation
*/
static void hfpProfile_HandleHfpCallTerminateCfm(const HFP_CALL_TERMINATE_CFM_T *cfm)
{
    voice_source_t source = HfpProfile_GetVoiceSourceForHfpLinkPrio(cfm->priority);
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG("hfpProfile_HandleHfpCallTerminateCfm(%p)", instance);

    PanicNull(instance);

    switch (appHfpGetState(instance))
    {
        case HFP_STATE_CONNECTED_IDLE:
        case HFP_STATE_CONNECTED_OUTGOING:
        case HFP_STATE_CONNECTED_INCOMING:
        case HFP_STATE_CONNECTED_ACTIVE:
        case HFP_STATE_DISCONNECTING:
            return;

        default:
            HfpProfile_HandleError(instance, HFP_CALL_TERMINATE_CFM, cfm);
            return;
    }
}

/*! \brief Handle unrecognised AT commands as TWS+ custom commands.
 */
static void hfpProfile_HandleHfpUnrecognisedAtCmdInd(HFP_UNRECOGNISED_AT_CMD_IND_T* ind)
{
    voice_source_t source = HfpProfile_GetVoiceSourceForHfpLinkPrio(ind->priority);
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG("hfpProfile_HandleHfpUnrecognisedAtCmdInd(%p)", instance);

    PanicNull(instance);

    switch (appHfpGetState(instance))
    {
        case HFP_STATE_CONNECTED_IDLE:
        case HFP_STATE_CONNECTED_OUTGOING:
        case HFP_STATE_CONNECTED_INCOMING:
        case HFP_STATE_CONNECTED_ACTIVE:
        case HFP_STATE_DISCONNECTING:
        {
            /* copy the message and send to register AT client */
            MAKE_HFP_MESSAGE_WITH_LEN(APP_HFP_AT_CMD_IND, ind->size_data);
            message->addr = instance->ag_bd_addr;
            message->size_data = ind->size_data;
            memcpy(message->data, ind->data, ind->size_data);
            MessageSend(hfp_profile_task_data.at_cmd_task, APP_HFP_AT_CMD_IND, message);
        }
        return;

        default:
        {
            uint16 i;
            for(i = 0; i < ind->size_data; ++i)
            {
                DEBUG_LOG("0x%x %c", ind->data[i], ind->data[i]);
            }
            HfpProfile_HandleError(instance, HFP_UNRECOGNISED_AT_CMD_IND, ind);
            return;
        }
    }

}

static void hfpProfile_HandleHfpHfIndicatorsReportInd(const HFP_HF_INDICATORS_REPORT_IND_T *ind)
{
    DEBUG_LOG("hfpProfile_HandleHfpHfIndicatorsReportInd, num=%u, mask=%04x", ind->num_hf_indicators, ind->hf_indicators_mask);
}

static void hfpProfile_HandleHfpHfIndicatorsInd(const HFP_HF_INDICATORS_IND_T *ind)
{
    voice_source_t source = HfpProfile_GetVoiceSourceForHfpLinkPrio(ind->priority);
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG("hfpProfile_HandleHfpHfIndicatorsInd, num %u, status %u", ind->hf_indicator_assigned_num, ind->hf_indicator_status);

    PanicNull(instance);

    if (ind->hf_indicator_assigned_num == hf_battery_level)
    {
        HfpProfile_EnableBatteryHfInd(instance, ind->hf_indicator_status);
    }
}

static void hfpProfile_HandleHfpAtCmdCfm(HFP_AT_CMD_CFM_T *cfm)
{
    DEBUG_LOG("hfpProfile_HandleHfpAtCmdCfm status enum:hfp_lib_status:%d", cfm->status);
    MAKE_HFP_MESSAGE(APP_HFP_AT_CMD_CFM);
    message->status = cfm->status == hfp_success ? TRUE : FALSE;
    MessageSend(hfp_profile_task_data.at_cmd_task, APP_HFP_AT_CMD_CFM, message);
}

/*! \brief Handle indication of change in a connection status.

    Some phones will disconnect the ACL without closing any L2CAP/RFCOMM
    connections, so we check the ACL close reason code to determine if this
    has happened.

    If the close reason code was not link-loss and we have an HFP profile
    on that link, mark it as detach pending, so that we can gracefully handle
    the L2CAP or RFCOMM disconnection that will follow shortly.
 */
static void hfpProfile_HandleConManagerConnectionInd(CON_MANAGER_CONNECTION_IND_T *ind)
{
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForBdaddr(&ind->bd_addr);

    /* if disconnection and not an connection timeout, see if we need to mark
     * the HFP profile at having a pending detach */
    if (!ind->connected && ind->reason != hci_error_conn_timeout)
    {
        if (instance && !HfpProfile_IsDisconnected(instance) && BdaddrIsSame(&ind->bd_addr, &instance->ag_bd_addr) && !ind->ble)
        {
            DEBUG_LOG("hfpProfile_HandleConManagerConnectionInd, detach pending");
            instance->bitfields.detach_pending = TRUE;
        }
    }
}

static bool hfpProfile_DisconnectInternal(bdaddr *bd_addr)
{
    bool disconnect_request_sent = FALSE;
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForBdaddr(bd_addr);

    DEBUG_LOG("hfpProfile_DisconnectInternal(%p)", instance);

    if (instance && !HfpProfile_IsDisconnected(instance))
    {
        MAKE_HFP_MESSAGE(HFP_INTERNAL_HFP_DISCONNECT_REQ);

        /* Send message to HFP task */
        message->silent = FALSE;
        message->instance = instance;
        MessageSendConditionally(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_HFP_DISCONNECT_REQ,
                                 message, HfpProfileInstance_GetLock(instance));
        disconnect_request_sent = TRUE;
    }
    return disconnect_request_sent;
}

static void hfpProfile_InitTaskData(void)
{
    /* set up common hfp profile task handler. */
    hfp_profile_task_data.task.handler = hfpProfile_TaskMessageHandler;

    /* create list for SLC notification clients */
    TaskList_InitialiseWithCapacity(appHfpGetSlcStatusNotifyList(), HFP_SLC_STATUS_NOTIFY_LIST_INIT_CAPACITY);

    /* create list for general status notification clients */
    TaskList_InitialiseWithCapacity(appHfpGetStatusNotifyList(), HFP_STATUS_NOTIFY_LIST_INIT_CAPACITY);

    /* create lists for connection/disconnection requests */
    TaskList_WithDataInitialise(&hfp_profile_task_data.connect_request_clients);
    TaskList_WithDataInitialise(&hfp_profile_task_data.disconnect_request_clients);
	
	PanicFalse(BandwidthManager_RegisterFeature(BANDWIDTH_MGR_FEATURE_ESCO, high_bandwidth_manager_priority, NULL));
}

/*! \brief Entering `Initialising HFP` state

    This function is called when the HFP state machine enters
    the 'Initialising HFP' state, it calls the HfpInit() function
    to initialise the profile library for HFP.
*/
static void hfpProfile_InitHfpLibrary(void)
{
    hfp_init_params hfp_params = {0};
    uint16 supp_features = (HFP_VOICE_RECOGNITION |
                            HFP_NREC_FUNCTION |
                            HFP_REMOTE_VOL_CONTROL |
                            HFP_CODEC_NEGOTIATION |
                            HFP_HF_INDICATORS |
                            HFP_ESCO_S4_SUPPORTED);

    /* Initialise an HFP profile instance */
    hfp_params.supported_profile = hfp_handsfree_profile;
    hfp_params.supported_features = supp_features;
    hfp_params.disable_nrec = TRUE;
    hfp_params.extended_errors = FALSE;
    hfp_params.optional_indicators.service = hfp_indicator_off;
    hfp_params.optional_indicators.signal_strength = hfp_indicator_off;
    hfp_params.optional_indicators.roaming_status = hfp_indicator_off;
    hfp_params.optional_indicators.battery_charge = hfp_indicator_off;
    hfp_params.multipoint = TRUE;
    hfp_params.supported_wbs_codecs = hfp_wbs_codec_mask_cvsd | hfp_wbs_codec_mask_msbc;
    hfp_params.link_loss_time = 1;
    hfp_params.link_loss_interval = 5;
    if (appConfigHfpBatteryIndicatorEnabled())
    {
        hfp_params.hf_indicators = hfp_battery_level_mask;
    }
    else
    {
        hfp_params.hf_indicators = hfp_indicator_mask_none;
    }

#ifdef INCLUDE_SWB
    if (appConfigScoSwbEnabled())
    {
        hfp_params.hf_codec_modes = CODEC_64_2_EV3;
    }
    else
    {
        hfp_params.hf_codec_modes = 0;
    }
#endif

#ifdef TEST_HFP_CODEC_PSKEY

    uint16 hfp_codec_pskey = 0;
    PsRetrieve(PS_KEY_TEST_HFP_CODEC, &hfp_codec_pskey, sizeof(hfp_codec_pskey));

    DEBUG_LOG_ALWAYS("hfpProfile_InitHfpLibrary 0x%x", hfp_codec_pskey);

    hfp_params.supported_wbs_codecs =  (hfp_codec_pskey & HFP_CODEC_PS_BIT_NB) ? hfp_wbs_codec_mask_cvsd: 0;
    hfp_params.supported_wbs_codecs |= (hfp_codec_pskey & HFP_CODEC_PS_BIT_WB) ? hfp_wbs_codec_mask_msbc  : 0;

    if (appConfigScoSwbEnabled())
    {
        hfp_params.hf_codec_modes = (hfp_codec_pskey & HFP_CODEC_PS_BIT_SWB) ? CODEC_64_2_EV3 : 0;
    }

    /* Disable codec negotiation if we ONLY support narrow band */
    if (hfp_codec_pskey == HFP_CODEC_PS_BIT_NB)
        hfp_params.supported_features &= ~(HFP_CODEC_NEGOTIATION);

#endif

    HfpInit(&hfp_profile_task_data.task, &hfp_params, NULL);
}

static voice_source_t hfpProfile_GetForegroundVoiceSource(void)
{
    voice_source_t foreground_voice_source = voice_source_none;
    generic_source_t routed_source = Focus_GetFocusedGenericSourceForAudioRouting();

    if (GenericSource_IsVoice(routed_source) && VoiceSource_IsHfp(routed_source.u.voice))
    {
        foreground_voice_source = routed_source.u.voice;
    }
    return foreground_voice_source;
}

bool HfpProfile_Init(Task init_task)
{
    UNUSED(init_task);

    hfpProfile_InitTaskData();

    hfpProfile_InitHfpLibrary();

    VoiceSources_RegisterVolume(voice_source_hfp_1, HfpProfile_GetVoiceSourceVolumeInterface());
    VoiceSources_RegisterVolume(voice_source_hfp_2, HfpProfile_GetVoiceSourceVolumeInterface());

    /* Register to receive notifications of (dis)connections */
    ConManagerRegisterConnectionsClient(&hfp_profile_task_data.task);

    ProfileManager_RegisterProfile(profile_manager_hfp_profile, hfpProfile_Connect, hfpProfile_Disconnect);

    return TRUE;
}

bool HfpProfile_ConnectHandset(void)
{
    bdaddr bd_addr;

    /* Get handset device address */
    if (appDeviceGetHandsetBdAddr(&bd_addr) && BtDevice_IsProfileSupported(&bd_addr, DEVICE_PROFILE_HFP))
    {
        device_t device = BtDevice_GetDeviceForBdAddr(&bd_addr);
        if (device)
        {
            uint8 our_hfp_profile = 0;
            Device_GetPropertyU8(device, device_property_hfp_profile, &our_hfp_profile);
            return HfpProfile_ConnectWithBdAddr(&bd_addr, our_hfp_profile);
        }
    }

    return FALSE;
}

void hfpProfile_Connect(const Task client_task, bdaddr *bd_addr)
{
    PanicNull((bdaddr *)bd_addr);
    if (BtDevice_IsProfileSupported(bd_addr, DEVICE_PROFILE_HFP))
    {
        device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
        if (device)
        {
            uint8 our_hfp_profile = 0;
            Device_GetPropertyU8(device, device_property_hfp_profile, &our_hfp_profile);

            ProfileManager_AddRequestToTaskList(TaskList_GetBaseTaskList(&hfp_profile_task_data.connect_request_clients), device, client_task);
            if (!HfpProfile_ConnectWithBdAddr(bd_addr, our_hfp_profile))
            {
                /* If already connected, send an immediate confirmation */
                ProfileManager_SendConnectCfmToTaskList(TaskList_GetBaseTaskList(&hfp_profile_task_data.connect_request_clients),
                                                        bd_addr, profile_manager_success, HfpProfile_FindClientSendConnectCfm);
            }
        }
    }
}

void hfpProfile_Disconnect(const Task client_task, bdaddr *bd_addr)
{
    PanicNull((bdaddr *)bd_addr);
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        ProfileManager_AddRequestToTaskList(TaskList_GetBaseTaskList(&hfp_profile_task_data.disconnect_request_clients), device, client_task);
        if (!hfpProfile_DisconnectInternal(bd_addr))
        {
            /* If already disconnected, send an immediate confirmation */
            ProfileManager_SendConnectCfmToTaskList(TaskList_GetBaseTaskList(&hfp_profile_task_data.disconnect_request_clients),
                                                    bd_addr, profile_manager_success, HfpProfile_FindClientSendDisconnectCfm);
        }
    }
}

bool HfpProfile_ConnectWithBdAddr(const bdaddr *bd_addr, hfp_connection_type_t connection_type)
{
    DEBUG_LOG("HfpProfile_ConnectWithBdAddr");

    hfpInstanceTaskData* instance = HfpProfileInstance_GetInstanceForBdaddr(bd_addr);
    if (!instance)
    {
        instance = HfpProfileInstance_Create(bd_addr, TRUE);
    }

    /* Check if not already connected */
    if (!appHfpIsConnectedForInstance(instance))
    {
        /* Store address of AG */
        instance->ag_bd_addr = *bd_addr;
        
        MAKE_HFP_MESSAGE(HFP_INTERNAL_HFP_CONNECT_REQ);

        /* Send message to HFP task */
        message->addr = *bd_addr;
        message->profile = connection_type;
        message->flags = 0;
        MessageSendConditionally(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_HFP_CONNECT_REQ, message,
                                 ConManagerCreateAcl(bd_addr));

        /* Connect will now be handled by HFP task */
        return TRUE;
    }

    /* Already connected */
    return FALSE;
}

void HfpProfile_StoreConfig(device_t device)
{
    /* Cancel any pending messages */
    MessageCancelFirst(&hfp_profile_task_data.task, HFP_INTERNAL_CONFIG_WRITE_REQ);

    /* Store configuration after a 5 seconds */
    MAKE_HFP_MESSAGE(HFP_INTERNAL_CONFIG_WRITE_REQ);
    message->device = device;
    MessageSendLater(&hfp_profile_task_data.task, HFP_INTERNAL_CONFIG_WRITE_REQ, message, D_SEC(5));
}

void appHfpClientRegister(Task task)
{
    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(appHfpGetSlcStatusNotifyList()), task);
}

void HfpProfile_RegisterStatusClient(Task task)
{
    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(appHfpGetStatusNotifyList()), task);
}

uint8 appHfpGetVolume(hfpInstanceTaskData * instance)
{
    volume_t volume = HfpProfile_GetDefaultVolume();
    device_t device = HfpProfileInstance_FindDeviceFromInstance(instance);
    PanicNull(device);
    DeviceProperties_GetVoiceVolume(device, volume.config, &volume);
    return volume.value;
}

static void hfpProfile_TaskMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    DEBUG_LOG("hfpProfile_TaskMessageHandler MESSAGE:HfpMessageId:0x%04X", id);

    /* HFP profile library messages */
    switch (id)
    {
        case HFP_INIT_CFM:
            hfpProfile_HandleHfpInitCfm((HFP_INIT_CFM_T *)message);
            return;

        case HFP_SLC_CONNECT_IND:
            hfpProfile_HandleHfpSlcConnectInd((HFP_SLC_CONNECT_IND_T *)message);
            return;

        case HFP_SLC_CONNECT_CFM:
            hfpProfile_HandleHfpSlcConnectCfm((HFP_SLC_CONNECT_CFM_T *)message);
            return;

        case HFP_SLC_DISCONNECT_IND:
            hfpProfile_HandleHfpSlcDisconnectInd((HFP_SLC_DISCONNECT_IND_T *)message);
            return;

        case HFP_AUDIO_CONNECT_IND:
             hfpProfile_HandleHfpAudioConnectInd((HFP_AUDIO_CONNECT_IND_T *)message);
             return;

        case HFP_AUDIO_CONNECT_CFM:
             hfpProfile_HandleHfpAudioConnectCfm((HFP_AUDIO_CONNECT_CFM_T *)message);
             return;

        case HFP_AUDIO_DISCONNECT_IND:
             hfpProfile_HandleHfpAudioDisconnectInd((HFP_AUDIO_DISCONNECT_IND_T *)message);
             return;

        case HFP_RING_IND:
             hfpProfile_HandleHfpRingInd((HFP_RING_IND_T *)message);
             return;

        case HFP_SERVICE_IND:
             hfpProfile_HandleHfpServiceInd((HFP_SERVICE_IND_T *)message);
             return;

        case HFP_CALL_STATE_IND:
             hfpProfile_HandleHfpCallStateInd((HFP_CALL_STATE_IND_T *)message);
             return;

        case HFP_VOICE_RECOGNITION_IND:
             hfpProfile_HandleHfpVoiceRecognitionInd((HFP_VOICE_RECOGNITION_IND_T *)message);
             return;

        case HFP_VOICE_RECOGNITION_ENABLE_CFM:
             hfpProfile_HandleHfpVoiceRecognitionEnableCfm((HFP_VOICE_RECOGNITION_ENABLE_CFM_T *)message);
             return;

        case HFP_CALLER_ID_IND:
             hfpProfile_HandleHfpCallerIdInd((HFP_CALLER_ID_IND_T *)message);
             return;

        case HFP_CALLER_ID_ENABLE_CFM:
             hfpProfile_HandleHfpCallerIdEnableCfm((HFP_CALLER_ID_ENABLE_CFM_T *)message);
             return;

        case HFP_VOLUME_SYNC_SPEAKER_GAIN_IND:
             hfpProfile_HandleHfpVolumeSyncSpeakerGainInd((HFP_VOLUME_SYNC_SPEAKER_GAIN_IND_T *)message);
             return;

        case HFP_VOLUME_SYNC_MICROPHONE_GAIN_IND:
             hfpProfile_HandleHfpVolumeSyncMicGainInd((HFP_VOLUME_SYNC_MICROPHONE_GAIN_IND_T *)message);
             return;

        case HFP_CALL_ANSWER_CFM:
             hfpProfile_HandleHfpCallAnswerCfm((HFP_CALL_ANSWER_CFM_T *)message);
             return;

        case HFP_CALL_TERMINATE_CFM:
             hfpProfile_HandleHfpCallTerminateCfm((HFP_CALL_TERMINATE_CFM_T *)message);
             return;

        case HFP_AT_CMD_CFM:
             hfpProfile_HandleHfpAtCmdCfm((HFP_AT_CMD_CFM_T*)message);
             return;

        case HFP_UNRECOGNISED_AT_CMD_IND:
             hfpProfile_HandleHfpUnrecognisedAtCmdInd((HFP_UNRECOGNISED_AT_CMD_IND_T*)message);
             return;

        /* Handle additional messages */
        case HFP_HS_BUTTON_PRESS_CFM:
        case HFP_DIAL_LAST_NUMBER_CFM:
        case HFP_SIGNAL_IND:
        case HFP_ROAM_IND:
        case HFP_BATTCHG_IND:
        case HFP_CALL_WAITING_IND:
        case HFP_EXTRA_INDICATOR_INDEX_IND:
        case HFP_EXTRA_INDICATOR_UPDATE_IND:
        case HFP_NETWORK_OPERATOR_IND:
        case HFP_CURRENT_CALLS_CFM:
        case HFP_DIAL_NUMBER_CFM:
            return;

        case HFP_HF_INDICATORS_REPORT_IND:
            hfpProfile_HandleHfpHfIndicatorsReportInd((HFP_HF_INDICATORS_REPORT_IND_T *)message);
            return;

        case HFP_HF_INDICATORS_IND:
            hfpProfile_HandleHfpHfIndicatorsInd((HFP_HF_INDICATORS_IND_T *)message);
            return;
    }

    /* Handle internal messages */
    switch(id)
    {
        case HFP_INTERNAL_CONFIG_WRITE_REQ:
        {
            HFP_INTERNAL_CONFIG_WRITE_REQ_T * req = (HFP_INTERNAL_CONFIG_WRITE_REQ_T *) message;
            PanicNull(req);
            HfpProfile_HandleConfigWriteRequest(req->device);
            return;
        }
    }

    /* Handle other messages */
    switch (id)
    {
        case CON_MANAGER_CONNECTION_IND:
            hfpProfile_HandleConManagerConnectionInd((CON_MANAGER_CONNECTION_IND_T *)message);
            return;

        default:
            HfpProfile_HandleBatteryMessages(id, message);
            break;
    }
}

void hfpProfile_RegisterHfpMessageGroup(Task task, message_group_t group)
{
    PanicFalse(group == APP_HFP_MESSAGE_GROUP);
    HfpProfile_RegisterStatusClient(task);
}

void hfpProfile_RegisterSystemMessageGroup(Task task, message_group_t group)
{
    PanicFalse(group == SYSTEM_MESSAGE_GROUP);
    HfpProfile_RegisterStatusClient(task);
}

void HfpProfile_HandleConfigWriteRequest(device_t device)
{
    DEBUG_LOG("HfpProfile_HandleConfigWriteRequest(%p)", device);
    DeviceDbSerialiser_SerialiseDevice(device);
}

/* \brief Inform hfp profile of current device Primary/Secondary role.
 */
void HfpProfile_SetRole(bool primary)
{
    if (primary)
    {
        /* Register voice source interface for hfp profile */
        VoiceSources_RegisterAudioInterface(voice_source_hfp_1, HfpProfile_GetAudioInterface());
        VoiceSources_RegisterAudioInterface(voice_source_hfp_2, HfpProfile_GetAudioInterface());
    }

}

bool HfpProfile_FindClientSendConnectCfm(Task task, task_list_data_t *data, void *arg)
{
    bool found_client_task  = FALSE;

    profile_manager_send_client_cfm_params * params = (profile_manager_send_client_cfm_params *)arg;

    if (data->ptr == params->device)
    {
        found_client_task = TRUE;
        MESSAGE_MAKE(msg, APP_HFP_CONNECT_CFM_T);
        msg->device = params->device;

        if(params->result == profile_manager_success)
        {
            msg->successful = TRUE;
        }
        else
        {
            msg->successful = FALSE;
        }

        DEBUG_LOG("HfpProfile_FindClientSendConnectCfm toTask=%p enum:profile_manager_request_cfm_result_t:%d",
                  task, params->result);

        MessageSend(task, APP_HFP_CONNECT_CFM, msg);

        TaskList_RemoveTask(TaskList_GetBaseTaskList(&hfp_profile_task_data.connect_request_clients), task);
    }
    return !found_client_task;
}

bool HfpProfile_FindClientSendDisconnectCfm(Task task, task_list_data_t *data, void *arg)
{
    bool found_client_task  = FALSE;
    profile_manager_send_client_cfm_params * params = (profile_manager_send_client_cfm_params *)arg;

    if (data->ptr == params->device)
    {
        found_client_task = TRUE;
        MESSAGE_MAKE(msg, APP_HFP_DISCONNECT_CFM_T);
        msg->device = params->device;

        if(params->result == profile_manager_success)
        {
            msg->successful = TRUE;
        }
        else
        {
            msg->successful = FALSE;
        }

        DEBUG_LOG("HfpProfile_FindClientSendDisconnectCfm toTask=%p enum:profile_manager_request_cfm_result_t:%d",
                  task, params->result);

        MessageSend(task, APP_HFP_DISCONNECT_CFM, msg);

        TaskList_RemoveTask(TaskList_GetBaseTaskList(&hfp_profile_task_data.disconnect_request_clients), task);
    }
    return !found_client_task;
}

void HfpProfile_HandleError(hfpInstanceTaskData * instance, MessageId id, Message message)
{
    UNUSED(message);

    DEBUG_LOG_ERROR("HfpProfile_HandleError enum:hfpState:%d, MESSAGE:hfp_profile_internal_messages:%x",
                    instance->state, id);

    /* Check if we are connected */
    if (appHfpIsConnectedForInstance(instance))
    {
        /* Move to 'disconnecting' state */
        appHfpSetState(instance, HFP_STATE_DISCONNECTING);
    }
}

/*! \brief Is HFP connected */
bool appHfpIsConnectedForInstance(hfpInstanceTaskData * instance)
{
    return ((appHfpGetState(instance) >= HFP_STATE_CONNECTED_IDLE) && (appHfpGetState(instance) <= HFP_STATE_CONNECTED_ACTIVE));
}

/*! \brief Is HFP connected */
bool appHfpIsConnected(void)
{
    bool is_connected = FALSE;
    hfpInstanceTaskData * instance = NULL;
    hfp_instance_iterator_t iterator;

    for_all_hfp_instances(instance, &iterator)
    {
        is_connected = appHfpIsConnectedForInstance(instance);
        if (is_connected)
            break;
    }
    return is_connected;
}

/*! \brief Is HFP in a call */
bool appHfpIsCallForInstance(hfpInstanceTaskData * instance)
{
    return ((appHfpGetState(instance) >= HFP_STATE_CONNECTED_OUTGOING) && (appHfpGetState(instance) <= HFP_STATE_CONNECTED_ACTIVE));
}

bool appHfpIsCall(void)
{
    bool is_call = FALSE;
    hfpInstanceTaskData * instance = NULL;
    hfp_instance_iterator_t iterator;

    for_all_hfp_instances(instance, &iterator)
    {
        is_call = appHfpIsCallForInstance(instance);
        if (is_call)
            break;
    }
    return is_call;
}

/*! \brief Is HFP in an active call */
bool appHfpIsCallActiveForInstance(hfpInstanceTaskData * instance)
{
    return (appHfpGetState(instance) == HFP_STATE_CONNECTED_ACTIVE);
}

bool appHfpIsCallActive(void)
{
    bool is_call_active = FALSE;
    hfpInstanceTaskData * instance = NULL;
    hfp_instance_iterator_t iterator;

    for_all_hfp_instances(instance, &iterator)
    {
        is_call_active = appHfpIsCallActiveForInstance(instance);
        if (is_call_active)
            break;
    }
    return is_call_active;
}

/*! \brief Is HFP in an incoming call */
bool appHfpIsCallIncomingForInstance(hfpInstanceTaskData * instance)
{
    return (appHfpGetState(instance) == HFP_STATE_CONNECTED_INCOMING);
}

bool appHfpIsCallIncoming(void)
{
    bool is_call_incoming = FALSE;
    hfpInstanceTaskData * instance = NULL;
    hfp_instance_iterator_t iterator;

    for_all_hfp_instances(instance, &iterator)
    {
        is_call_incoming = appHfpIsCallIncomingForInstance(instance);
        if (is_call_incoming)
            break;
    }
    return is_call_incoming;
}

/*! \brief Is HFP in an outgoing call */
bool appHfpIsCallOutgoingForInstance(hfpInstanceTaskData * instance)
{
    return (appHfpGetState(instance) == HFP_STATE_CONNECTED_OUTGOING);
}

bool appHfpIsCallOutgoing(void)
{
    bool is_call_outgoing = FALSE;
    hfpInstanceTaskData * instance = NULL;
    hfp_instance_iterator_t iterator;

    for_all_hfp_instances(instance, &iterator)
    {
        is_call_outgoing = appHfpIsCallOutgoingForInstance(instance);
        if (is_call_outgoing)
            break;
    }
    return is_call_outgoing;
}

/*! \brief Is HFP disconnected */
bool HfpProfile_IsDisconnected(hfpInstanceTaskData * instance)
{
    return ((appHfpGetState(instance) < HFP_STATE_CONNECTING_LOCAL) || (appHfpGetState(instance) > HFP_STATE_DISCONNECTING));
}

/*! \brief returns hfp task pointer to requesting component

    \return hfp task pointer.
*/
Task HfpProfile_GetInstanceTask(hfpInstanceTaskData * instance)
{
    PanicNull(instance);
    return &instance->task;
}

/*! \brief Get HFP sink */
Sink HfpProfile_GetSink(hfpInstanceTaskData * instance)
{
    PanicNull(instance);
    return instance->slc_sink;
}

/*! \brief Get current AG address */
bdaddr * HfpProfile_GetHandsetBdAddr(hfpInstanceTaskData * instance)
{
    PanicNull(instance);
    return &(instance->ag_bd_addr);
}

/*! \brief Is HFP SCO active with the specified HFP instance. */
bool HfpProfile_IsScoActiveForInstance(hfpInstanceTaskData * instance)
{
    PanicNull(instance);
    return (instance->sco_sink != 0);
}

/*! \brief Is HFP SCO connecting with the specified HFP instance. */
bool HfpProfile_IsScoConnectingForInstance(hfpInstanceTaskData * instance)
{
    PanicNull(instance);
    return (instance->bitfields.esco_connecting != 0);
}

/*! \brief Is HFP SCO active */
bool HfpProfile_IsScoActive(void)
{
    bool is_sco_active = FALSE;
    hfpInstanceTaskData * instance = NULL;
    hfp_instance_iterator_t iterator;

    for_all_hfp_instances(instance, &iterator)
    {
        is_sco_active = HfpProfile_IsScoActiveForInstance(instance);
        if (is_sco_active)
            break;
    }
    return is_sco_active;
}

/*! \brief Is microphone muted */
bool HfpProfile_IsMicrophoneMuted(hfpInstanceTaskData * instance)
{
    PanicNull(instance);
    return instance->bitfields.mute_active;
}

hfpInstanceTaskData * HfpProfile_GetInstanceForVoiceSourceWithUiFocus(void)
{
    hfpInstanceTaskData* instance = NULL;
    voice_source_t source = voice_source_none;

    if (Focus_GetVoiceSourceForContext(ui_provider_telephony, &source))
    {
        instance = HfpProfileInstance_GetInstanceForSource(source);
    }
    return instance;
}

hfpInstanceTaskData * HfpProfile_GetInstanceForVoiceSourceWithAudioFocus(void)
{
    voice_source_t source = hfpProfile_GetForegroundVoiceSource();
    return HfpProfileInstance_GetInstanceForSource(source);
}

uint8 HfpProfile_GetDefaultMicGain(void)
{
    return HFP_MICROPHONE_GAIN;
}

void HfpProfile_SetScoConnectingSyncTask(Task task)
{
    PanicNotNull(hfp_profile_task_data.sco_sync_task);
    hfp_profile_task_data.sco_sync_task = task;
}

void HfpProfile_ScoConnectingSyncResponse(device_t device, Task task, bool accept)
{
    hfpInstanceTaskData *instance = HfpProfileInstance_GetInstanceForDevice(device);
    if (instance)
    {
        if (instance->bitfields.esco_connecting)
        {
            hfp_link_priority priority = HfpLinkPriorityFromBdaddr(&instance->ag_bd_addr);
            HfpAudioConnectResponse(priority, accept, instance->sco_supported_packets ^ sync_all_edr_esco, NULL, FALSE);
        }
    }

    /* Only one task currently supported so ignore */
    UNUSED(task);
}

MESSAGE_BROKER_GROUP_REGISTRATION_MAKE(APP_HFP, hfpProfile_RegisterHfpMessageGroup, NULL);
MESSAGE_BROKER_GROUP_REGISTRATION_MAKE(SYSTEM, hfpProfile_RegisterSystemMessageGroup, NULL);

#endif
