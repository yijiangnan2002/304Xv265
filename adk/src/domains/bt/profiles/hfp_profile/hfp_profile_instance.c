/*!
\copyright  Copyright (c) 2008 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Application domain HFP dynamic instance management.
*/

#include "hfp_profile_instance.h"

#include "system_state.h"
#include "bt_device.h"
#include "device_properties.h"
#include "hfp_profile_config.h"
#include "link_policy.h"
#include "voice_sources.h"
#include "hfp_profile_audio.h"
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
#include "ui.h"

#include <profile_manager.h>
#include <av.h>
#include <connection_manager.h>
#include <device.h>
#include <device_list.h>
#include <logging.h>
#include <mirror_profile.h>
#include <message.h>
#include <panic.h>
#include <ps.h>

#include <stdio.h>

/* Local Function Prototypes */
static void hfpProfile_InstanceHandleMessage(Task task, MessageId id, Message message);

#define HFP_MAX_NUM_INSTANCES 2

#define for_all_hfp_instances(hfp_instance, iterator) for(hfp_instance = HfpInstance_GetFirst(iterator); hfp_instance != NULL; hfp_instance = HfpInstance_GetNext(iterator))

static void HfpInstance_AddDeviceHfpInstanceToIterator(device_t device, void * iterator_data)
{
    hfpInstanceTaskData* hfp_instance = HfpProfileInstance_GetInstanceForDevice(device);

    if(hfp_instance)
    {
        hfp_instance_iterator_t * iterator = (hfp_instance_iterator_t *)iterator_data;
        iterator->instances[iterator->index] = hfp_instance;
        iterator->index++;
    }
}

hfpInstanceTaskData * HfpInstance_GetFirst(hfp_instance_iterator_t * iterator)
{
    memset(iterator, 0, sizeof(hfp_instance_iterator_t));

    DeviceList_Iterate(HfpInstance_AddDeviceHfpInstanceToIterator, iterator);

    iterator->index = 0;

    return iterator->instances[iterator->index];
}

hfpInstanceTaskData * HfpInstance_GetNext(hfp_instance_iterator_t * iterator)
{
    iterator->index++;

    if(iterator->index >= HFP_MAX_NUM_INSTANCES)
        return NULL;

    return iterator->instances[iterator->index];
}

static hfp_link_priority hfpProfileInstance_GetLinkForInstance(hfpInstanceTaskData * instance)
{
    hfp_link_priority link = hfp_invalid_link;

    PanicNull(instance);

    device_t device = HfpProfileInstance_FindDeviceFromInstance(instance);
    if (device != NULL)
    {
        bdaddr addr = DeviceProperties_GetBdAddr(device);
        link = HfpLinkPriorityFromBdaddr(&addr);
    }

    DEBUG_LOG_VERBOSE(
        "hfpProfileInstance_GetLinkForInstance instance:%p enum:hfp_link_priority:%d",
        instance, link);

    return link;
}

/*! \brief Handle remote support features confirmation
*/
static void appHfpHandleClDmRemoteFeaturesConfirm(const CL_DM_REMOTE_FEATURES_CFM_T *cfm)
{
    tp_bdaddr bd_addr;
    if (SinkGetBdAddr(cfm->sink, &bd_addr))
    {
        hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForBdaddr(&bd_addr.taddr.addr);

        PanicNull(instance);

        DEBUG_LOG("appHfpHandleClDmRemoteFeaturesConfirm(%p)", instance);

        switch (appHfpGetState(instance))
        {
            case HFP_STATE_CONNECTED_IDLE:
            case HFP_STATE_CONNECTED_OUTGOING:
            case HFP_STATE_CONNECTED_INCOMING:
            case HFP_STATE_CONNECTED_ACTIVE:
            case HFP_STATE_DISCONNECTING:
            case HFP_STATE_DISCONNECTED:
            {
                if (cfm->status == hci_success)
                {
                    uint16 features[PSKEY_LOCAL_SUPPORTED_FEATURES_SIZE] = PSKEY_LOCAL_SUPPORTED_FEATURES_DEFAULTS;
                    uint16 packets;
                    uint16 index;

                    /* Read local supported features to determine SCO packet types */
                    PsFullRetrieve(PSKEY_LOCAL_SUPPORTED_FEATURES, &features, PSKEY_LOCAL_SUPPORTED_FEATURES_SIZE);

                    /* Get supported features that both HS & AG support */
                    for (index = 0; index < PSKEY_LOCAL_SUPPORTED_FEATURES_SIZE; index++)
                    {
                        printf("%04x ", features[index]);
                        features[index] &= cfm->features[index];
                    }
                    printf("");

                    /* Calculate SCO packets we should use */
                    packets = sync_hv1;
                    if (features[0] & 0x2000)
                        packets |= sync_hv3;
                    if (features[0] & 0x1000)
                        packets |= sync_hv2;

                    /* Only use eSCO for HFP 1.5+ */
                    if (instance->profile == hfp_handsfree_profile)
                    {
                        if (features[1] & 0x8000)
                            packets |= sync_ev3;
                        if (features[2] & 0x0001)
                            packets |= sync_ev4;
                        if (features[2] & 0x0002)
                            packets |= sync_ev5;
                        if (features[2] & 0x2000)
                        {
                            packets |= sync_2ev3;
                            if (features[2] & 0x8000)
                                packets |= sync_2ev5;
                        }
                        if (features[2] & 0x4000)
                        {
                            packets |= sync_3ev3;
                            if (features[2] & 0x8000)
                                packets |= sync_3ev5;
                        }
                    }

                    /* Update supported SCO packet types */
                    instance->sco_supported_packets = packets;

                    DEBUG_LOG("appHfpHandleClDmRemoteFeaturesConfirm(%p), SCO packets %x", instance, packets);
                }
            }
            break;

            default:
                HfpProfile_HandleError(instance, CL_DM_REMOTE_FEATURES_CFM, cfm);
                break;
        }
    }
}

/*! \brief Handle encrypt confirmation
*/
static void appHfpHandleClDmEncryptConfirmation(const CL_SM_ENCRYPT_CFM_T *cfm)
{
    tp_bdaddr bd_addr;
    if (SinkGetBdAddr(cfm->sink, &bd_addr))
    {
        hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForBdaddr(&bd_addr.taddr.addr);

        PanicNull(instance);

        DEBUG_LOG("appHfpHandleClDmEncryptConfirmation(%p) encypted=%d", instance, cfm->encrypted);

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
                /* Store encrypted status */
                instance->bitfields.encrypted = cfm->encrypted;

                /* Check if SCO is now encrypted (or not) */
                HfpProfile_CheckEncryptedSco(instance);
            }
            break;

            default:
                HfpProfile_HandleError(instance, CL_SM_ENCRYPT_CFM, cfm);
                break;
        }
    }
}

/*! \brief Handle connect HFP SLC request
*/
static void appHfpHandleInternalHfpConnectRequest(const HFP_INTERNAL_HFP_CONNECT_REQ_T *req)
{
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForBdaddr(&req->addr);

    PanicNull(instance);

    hfpState state = appHfpGetState(instance);

    DEBUG_LOG("appHfpHandleInternalHfpConnectRequest(%p), enum:hfpState:%d %04x,%02x,%06lx",
              instance, state, req->addr.nap, req->addr.uap, req->addr.lap);

    switch (state)
    {
        case HFP_STATE_DISCONNECTED:
        {
            /* Check ACL is connected */
            if (ConManagerIsConnected(&req->addr))
            {
                /* Store connection flags */
                instance->bitfields.flags = req->flags;

                /* Store AG Bluetooth Address and profile type */
                instance->ag_bd_addr = req->addr;
                instance->profile = req->profile;

                /* Move to connecting local state */
                appHfpSetState(instance, HFP_STATE_CONNECTING_LOCAL);
            }
            else
            {
                DEBUG_LOG("appHfpHandleInternalHfpConnectRequest, no ACL %x,%x,%lx",
                           req->addr.nap, req->addr.uap, req->addr.lap);

                /* Set disconnect reason */
                instance->bitfields.disconnect_reason = APP_HFP_CONNECT_FAILED;

                /* Move to 'disconnected' state */
                appHfpSetState(instance, HFP_STATE_DISCONNECTED);

                HfpProfileInstance_Destroy(instance);
            }
        }
        return;

        case HFP_STATE_CONNECTING_REMOTE:
        case HFP_STATE_CONNECTING_LOCAL:
            return;

        case HFP_STATE_DISCONNECTING:
        {
            MAKE_HFP_MESSAGE(HFP_INTERNAL_HFP_CONNECT_REQ);

            /* repost the connect message pending final disconnection of the profile
             * via the lock */
            message->addr = req->addr;
            message->profile = req->profile;
            message->flags = req->flags;
            MessageSendConditionally(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_HFP_CONNECT_REQ, message,
                                     HfpProfileInstance_GetLock(instance));
        }
        return;

        default:
            HfpProfile_HandleError(instance, HFP_INTERNAL_HFP_CONNECT_REQ, req);
            return;
    }
}

/*! \brief Handle disconnect HFP SLC request
*/
static void appHfpHandleInternalHfpDisconnectRequest(const HFP_INTERNAL_HFP_DISCONNECT_REQ_T *req)
{
    DEBUG_LOG("appHfpHandleInternalHfpDisconnectRequest enum:hfpState:%d", appHfpGetState(req->instance));

    switch (appHfpGetState(req->instance))
    {
        case HFP_STATE_CONNECTED_IDLE:
        case HFP_STATE_CONNECTED_INCOMING:
        case HFP_STATE_CONNECTED_OUTGOING:
        case HFP_STATE_CONNECTED_ACTIVE:
        {
            /* Move to disconnecting state */
            appHfpSetState(req->instance, HFP_STATE_DISCONNECTING);
        }
        return;

        case HFP_STATE_DISCONNECTED:
            return;

        default:
            HfpProfile_HandleError(req->instance, HFP_INTERNAL_HFP_DISCONNECT_REQ, req);
            return;
    }
}

/*! \brief Handle last number redial request
*/
static void appHfpHandleInternalHfpLastNumberRedialRequest(hfpInstanceTaskData * instance)
{
    DEBUG_LOG("appHfpHandleInternalHfpLastNumberRedialRequest enum:hfpState:%d", appHfpGetState(instance));

    switch (appHfpGetState(instance))
    {
        case HFP_STATE_CONNECTED_IDLE:
        {
            if (instance->profile == hfp_headset_profile)
            {
                /* Send button press */
                HfpHsButtonPressRequest(hfpProfileInstance_GetLinkForInstance(instance));
            }
            else
            {
                HfpDialLastNumberRequest(hfpProfileInstance_GetLinkForInstance(instance));
            }
        }
        return;

        case HFP_STATE_CONNECTED_INCOMING:
        case HFP_STATE_CONNECTED_OUTGOING:
        case HFP_STATE_CONNECTED_ACTIVE:
        case HFP_STATE_DISCONNECTED:
            /* Ignore last number redial request as it doesn't make sense in these states */
            return;

        default:
            HfpProfile_HandleError(instance, HFP_INTERNAL_HFP_LAST_NUMBER_REDIAL_REQ, NULL);
            return;
    }
}

/*! \brief Handle voice dial request
*/
static void appHfpHandleInternalHfpVoiceDialRequest(hfpInstanceTaskData * instance)
{
    DEBUG_LOG("appHfpHandleInternalHfpVoiceDialRequest(%p)", instance);

    switch (appHfpGetState(instance))
    {
        case HFP_STATE_CONNECTED_IDLE:
        {
            if (instance->profile == hfp_headset_profile)
            {
                HfpHsButtonPressRequest(hfpProfileInstance_GetLinkForInstance(instance));
            }
            else
            {
                HfpVoiceRecognitionEnableRequest(hfpProfileInstance_GetLinkForInstance(instance),
                                                 instance->bitfields.voice_recognition_request = TRUE);
            }
        }
        return;

        case HFP_STATE_CONNECTED_INCOMING:
        case HFP_STATE_CONNECTED_OUTGOING:
        case HFP_STATE_CONNECTED_ACTIVE:
        case HFP_STATE_DISCONNECTED:
            /* Ignore voice dial request as it doesn't make sense in these states */
            return;

        default:
            HfpProfile_HandleError(instance, HFP_INTERNAL_HFP_VOICE_DIAL_REQ, NULL);
            return;
    }
}

/*! \brief Handle voice dial disable request
*/
static void appHfpHandleInternalHfpVoiceDialDisableRequest(hfpInstanceTaskData * instance)
{
    DEBUG_LOG("appHfpHandleInternalHfpVoiceDialDisableRequest(%p)", instance);

    switch (appHfpGetState(instance))
    {
        case HFP_STATE_CONNECTED_IDLE:
        {
            if (instance->profile == hfp_headset_profile)
            {
                HfpHsButtonPressRequest(hfpProfileInstance_GetLinkForInstance(instance));
            }
            else
            {
               HfpVoiceRecognitionEnableRequest(hfpProfileInstance_GetLinkForInstance(instance),
                                                instance->bitfields.voice_recognition_request = FALSE);
            }
        }
        return;

        case HFP_STATE_CONNECTED_INCOMING:
        case HFP_STATE_CONNECTED_OUTGOING:
        case HFP_STATE_CONNECTED_ACTIVE:
        case HFP_STATE_DISCONNECTED:
            /* Ignore voice dial request as it doesn't make sense in these states */
            return;

        default:
            HfpProfile_HandleError(instance, HFP_INTERNAL_HFP_VOICE_DIAL_DISABLE_REQ, NULL);
            return;
    }
}

static void appHfpHandleInternalNumberDialRequest(HFP_INTERNAL_NUMBER_DIAL_REQ_T * message)
{
    DEBUG_LOG("appHfpHandleInternalNumberDialRequest(%p)", message->instance);

    switch (appHfpGetState(message->instance))
    {
        case HFP_STATE_CONNECTED_IDLE:
        {
            if (message->instance->profile == hfp_headset_profile)
            {
                HfpHsButtonPressRequest(hfpProfileInstance_GetLinkForInstance(message->instance));
            }
            else
            {
                HfpDialNumberRequest(hfpProfileInstance_GetLinkForInstance(message->instance),
                                     message->length, message->number);
            }
        }
        return;

        case HFP_STATE_CONNECTED_INCOMING:
        case HFP_STATE_CONNECTED_OUTGOING:
        case HFP_STATE_CONNECTED_ACTIVE:
        case HFP_STATE_DISCONNECTED:
            /* Ignore number dial request as it doesn't make sense in these states */
            return;

        default:
            HfpProfile_HandleError(message->instance, HFP_INTERNAL_NUMBER_DIAL_REQ, NULL);
            return;
    }
}

/*! \brief Handle accept call request
*/
static void appHfpHandleInternalHfpCallAcceptRequest(hfpInstanceTaskData * instance)
{
    hfpState state = appHfpGetState(instance);

    DEBUG_LOG("appHfpHandleInternalHfpCallAcceptRequest(%p) enum:hfpState:%d", instance, state);

    switch (state)
    {
        case HFP_STATE_CONNECTED_INCOMING:
        {
            if (instance->profile == hfp_headset_profile)
            {
                HfpHsButtonPressRequest(hfpProfileInstance_GetLinkForInstance(instance));
            }
            else
            {
                HfpCallAnswerRequest(hfpProfileInstance_GetLinkForInstance(instance), TRUE);
            }
        }
        return;

        case HFP_STATE_CONNECTED_IDLE:
        case HFP_STATE_CONNECTED_OUTGOING:
        case HFP_STATE_CONNECTED_ACTIVE:
        case HFP_STATE_DISCONNECTED:
            /* Ignore call accept request as it doesn't make sense in these states */
            return;

        default:
            HfpProfile_HandleError(instance, HFP_INTERNAL_HFP_CALL_ACCEPT_REQ, NULL);
            return;
    }
}

/*! \brief Handle reject call request
*/
static void appHfpHandleInternalHfpCallRejectRequest(hfpInstanceTaskData * instance)
{
    DEBUG_LOG("appHfpHandleInternalHfpCallRejectRequest(%p)", instance);

    switch (appHfpGetState(instance))
    {
        case HFP_STATE_CONNECTED_INCOMING:
        {
            if (instance->profile == hfp_headset_profile)
            {
                Telephony_NotifyError(HfpProfileInstance_GetVoiceSourceForInstance(instance));
            }
            else
            {
                HfpCallAnswerRequest(hfpProfileInstance_GetLinkForInstance(instance), FALSE);
            }
        }
        return;

        case HFP_STATE_CONNECTED_IDLE:
        case HFP_STATE_CONNECTED_OUTGOING:
        case HFP_STATE_CONNECTED_ACTIVE:
        case HFP_STATE_DISCONNECTED:
            /* Ignore call accept request as it doesn't make sense in these states */
            return;

        default:
            HfpProfile_HandleError(instance, HFP_INTERNAL_HFP_CALL_REJECT_REQ, NULL);
            return;
    }
}

/*! \brief Handle hangup call request
*/
static void appHfpHandleInternalHfpCallHangupRequest(hfpInstanceTaskData * instance)
{
    DEBUG_LOG("appHfpHandleInternalHfpCallHangupRequest(%p)", instance);

    switch (appHfpGetState(instance))
    {
        case HFP_STATE_CONNECTED_ACTIVE:
        case HFP_STATE_CONNECTED_OUTGOING:
        {
            if (instance->profile == hfp_headset_profile)
            {
                HfpHsButtonPressRequest(hfpProfileInstance_GetLinkForInstance(instance));
            }
            else
            {
                HfpCallTerminateRequest(hfpProfileInstance_GetLinkForInstance(instance));
            }
        }
        return;

        case HFP_STATE_CONNECTED_IDLE:
        case HFP_STATE_CONNECTED_INCOMING:
        case HFP_STATE_DISCONNECTED:
            /* Ignore call accept request as it doesn't make sense in these states */
            return;

        default:
            HfpProfile_HandleError(instance, HFP_INTERNAL_HFP_CALL_HANGUP_REQ, NULL);
            return;
    }
}

/*! \brief Handle mute/unmute request
*/
static void appHfpHandleInternalHfpMuteRequest(const HFP_INTERNAL_HFP_MUTE_REQ_T *req)
{
    DEBUG_LOG("appHfpHandleInternalHfpMuteRequest(%p)", req->instance);

    switch (appHfpGetState(req->instance))
    {
        case HFP_STATE_CONNECTED_ACTIVE:
        {
            voice_source_t source = HfpProfileInstance_GetVoiceSourceForInstance(req->instance);
            if (req->mute)
            {
                Telephony_NotifyMicrophoneMuted(source);
            }
            else
            {
                Telephony_NotifyMicrophoneUnmuted(source);
            }

            /* Set mute flag */
            req->instance->bitfields.mute_active = req->mute;

            /* Re-configure audio chain */
            appKymeraScoMicMute(req->mute);
        }
        return;

        case HFP_STATE_CONNECTED_IDLE:
        case HFP_STATE_CONNECTED_INCOMING:
        case HFP_STATE_CONNECTED_OUTGOING:
        case HFP_STATE_DISCONNECTED:
            /* Ignore call accept request as it doesn't make sense in these states */
            return;

        default:
            HfpProfile_HandleError(req->instance, HFP_INTERNAL_HFP_CALL_HANGUP_REQ, NULL);
            return;
    }
}

static hfp_audio_transfer_direction hfpProfile_GetVoiceSourceHfpDirection(voice_source_audio_transfer_direction_t direction)
{
    if ((direction < voice_source_audio_transfer_to_hfp) || (direction > voice_source_audio_transfer_toggle))
    {
        DEBUG_LOG_ERROR("hfpProfile_GetVoiceSourceHfpDirection Invalid direction");
        Panic();
    }

    if(direction == voice_source_audio_transfer_to_hfp)
    {
        return hfp_audio_to_hfp;
    }
    else if(direction == voice_source_audio_transfer_to_ag)
    {
        return hfp_audio_to_ag;
    }
    else
    {
        return hfp_audio_transfer;
    }
}

/*! \brief Handle audio transfer request
*/
static void appHfpHandleInternalHfpTransferRequest(const HFP_INTERNAL_HFP_TRANSFER_REQ_T *req)
{
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(req->source);

    PanicNull(instance);

    DEBUG_LOG("appHfpHandleInternalHfpTransferRequest state enum:hfpState:%u direction enum:voice_source_audio_transfer_direction_t:%d",
               appHfpGetState(instance),
               req->direction);

    switch (appHfpGetState(instance))
    {
        case HFP_STATE_CONNECTED_ACTIVE:
        case HFP_STATE_CONNECTED_INCOMING:
        case HFP_STATE_CONNECTED_OUTGOING:
        {
            HfpAudioTransferRequest(hfpProfileInstance_GetLinkForInstance(instance),
                                    hfpProfile_GetVoiceSourceHfpDirection(req->direction),
                                    instance->sco_supported_packets  ^ sync_all_edr_esco,
                                    NULL);
        }
        return;

        case HFP_STATE_CONNECTED_IDLE:
        case HFP_STATE_DISCONNECTED:
            /* Ignore call accept request as it doesn't make sense in these states */
            return;

        default:
            HfpProfile_HandleError(instance, HFP_INTERNAL_HFP_TRANSFER_REQ, NULL);
            return;
    }
}

/*! \brief Handle HSP incoming call timeout

    We have had a ring indication for a while, so move back to 'connected
    idle' state.
*/
static void appHfpHandleHfpHspIncomingTimeout(hfpInstanceTaskData* instance)
{
    DEBUG_LOG("appHfpHandleHfpHspIncomingTimeout(%p)", instance);

    switch (appHfpGetState(instance))
    {
        case HFP_STATE_CONNECTED_INCOMING:
        {
            /* Move back to connected idle state */
            appHfpSetState(instance, HFP_STATE_CONNECTED_IDLE);
        }
        return;

        default:
            HfpProfile_HandleError(instance, HFP_INTERNAL_HSP_INCOMING_TIMEOUT, NULL);
            return;
    }
}

static void appHfpHandleInternalOutOfBandRingtoneRequest(hfpInstanceTaskData * instance)
{
    if(HFP_STATE_CONNECTED_INCOMING == appHfpGetState(instance))
    {
        Telephony_NotifyCallIncomingOutOfBandRingtone(HfpProfileInstance_GetVoiceSourceForInstance(instance));

        MESSAGE_MAKE(msg, HFP_INTERNAL_OUT_OF_BAND_RINGTONE_REQ_T);
        msg->instance = instance;
        MessageCancelFirst(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_OUT_OF_BAND_RINGTONE_REQ);
        MessageSendLater(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_OUT_OF_BAND_RINGTONE_REQ, msg, D_SEC(5));
    }
}

static void hfpProfileInstance_InitTaskData(hfpInstanceTaskData* instance)
{
    /* Set up instance task handler */
    instance->task.handler = hfpProfile_InstanceHandleMessage;

    /* By default, assume remote device supports all HFP standard packet types.
       This is modified when the remote features are read from the device after
       connection. */
    instance->sco_supported_packets = sync_all_sco | sync_ev3 | sync_2ev3;

    /* Initialise state */
    instance->sco_sink = 0;
    instance->hfp_lock = 0;
    instance->bitfields.disconnect_reason = APP_HFP_CONNECT_FAILED;
    instance->bitfields.hf_indicator_assigned_num = hf_indicators_invalid;
    instance->bitfields.call_accepted = FALSE;
    instance->codec = hfp_wbs_codec_mask_none;
    instance->wesco = 0;
    instance->tesco = 0;
    instance->qce_codec_mode_id = CODEC_MODE_ID_UNSUPPORTED;

    /* Move to disconnected state */
    instance->state = HFP_STATE_DISCONNECTED;
}

/*! \brief Message Handler for a specific HFP Instance

    This function is the main message handler for the HFP instance, every
    message is handled in it's own seperate handler function.  The switch
    statement is broken into seperate blocks to reduce code size, if execution
    reaches the end of the function then it is assumed that the message is
    unhandled.
*/
static void hfpProfile_InstanceHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    DEBUG_LOG("hfpProfile_InstanceHandleMessage id 0x%x", id);

    /* Handle internal messages */
    switch (id)
    {
        case HFP_INTERNAL_HSP_INCOMING_TIMEOUT:
            appHfpHandleHfpHspIncomingTimeout(((HFP_INTERNAL_HSP_INCOMING_TIMEOUT_T *)message)->instance);
            return;

        case HFP_INTERNAL_HFP_CONNECT_REQ:
            appHfpHandleInternalHfpConnectRequest((HFP_INTERNAL_HFP_CONNECT_REQ_T *)message);
            return;

        case HFP_INTERNAL_HFP_DISCONNECT_REQ:
            appHfpHandleInternalHfpDisconnectRequest((HFP_INTERNAL_HFP_DISCONNECT_REQ_T *)message);
            return;

        case HFP_INTERNAL_HFP_LAST_NUMBER_REDIAL_REQ:
            appHfpHandleInternalHfpLastNumberRedialRequest(((HFP_INTERNAL_HFP_LAST_NUMBER_REDIAL_REQ_T *)message)->instance);
            return;

        case HFP_INTERNAL_HFP_VOICE_DIAL_REQ:
            appHfpHandleInternalHfpVoiceDialRequest(((HFP_INTERNAL_HFP_VOICE_DIAL_REQ_T *)message)->instance);
            return;

        case HFP_INTERNAL_HFP_VOICE_DIAL_DISABLE_REQ:
            appHfpHandleInternalHfpVoiceDialDisableRequest(((HFP_INTERNAL_HFP_VOICE_DIAL_DISABLE_REQ_T *)message)->instance);
            return;

        case HFP_INTERNAL_HFP_CALL_ACCEPT_REQ:
            appHfpHandleInternalHfpCallAcceptRequest(((HFP_INTERNAL_HFP_CALL_ACCEPT_REQ_T *)message)->instance);
            return;

        case HFP_INTERNAL_HFP_CALL_REJECT_REQ:
            appHfpHandleInternalHfpCallRejectRequest(((HFP_INTERNAL_HFP_CALL_REJECT_REQ_T *)message)->instance);
            return;

        case HFP_INTERNAL_HFP_CALL_HANGUP_REQ:
            appHfpHandleInternalHfpCallHangupRequest(((HFP_INTERNAL_HFP_CALL_HANGUP_REQ_T *)message)->instance);
            return;

        case HFP_INTERNAL_HFP_MUTE_REQ:
            appHfpHandleInternalHfpMuteRequest((HFP_INTERNAL_HFP_MUTE_REQ_T *)message);
            return;

        case HFP_INTERNAL_HFP_TRANSFER_REQ:
            appHfpHandleInternalHfpTransferRequest((HFP_INTERNAL_HFP_TRANSFER_REQ_T *)message);
            return;

        case HFP_INTERNAL_NUMBER_DIAL_REQ:
            appHfpHandleInternalNumberDialRequest((HFP_INTERNAL_NUMBER_DIAL_REQ_T *)message);
            return;

        case HFP_INTERNAL_OUT_OF_BAND_RINGTONE_REQ:
            appHfpHandleInternalOutOfBandRingtoneRequest(((HFP_INTERNAL_OUT_OF_BAND_RINGTONE_REQ_T *)message)->instance);

    }

    /* Handle connection library messages */
    switch (id)
    {
        case CL_DM_REMOTE_FEATURES_CFM:
            appHfpHandleClDmRemoteFeaturesConfirm((CL_DM_REMOTE_FEATURES_CFM_T *)message);
            return;

        case CL_SM_ENCRYPT_CFM:
            appHfpHandleClDmEncryptConfirmation((CL_SM_ENCRYPT_CFM_T *)message);
            return;
   }
}

device_t HfpProfileInstance_FindDeviceFromInstance(hfpInstanceTaskData* instance)
{
    return DeviceList_GetFirstDeviceWithPropertyValue(device_property_hfp_instance, &instance, sizeof(hfpInstanceTaskData *));
}

static void hfpProfileInstance_SetInstanceForDevice(device_t device, hfpInstanceTaskData* instance)
{
    PanicFalse(Device_SetProperty(device, device_property_hfp_instance, &instance, sizeof(hfpInstanceTaskData *)));
}

typedef struct voice_source_search_data
{
    /*! The voice source associated with the device to find */
    voice_source_t source_to_find;
    /*! Set to TRUE if a device with the source is found */
    bool source_found;
} voice_source_search_data_t;

static void hfpProfileInstance_SearchForHandsetWithVoiceSource(device_t device, void * data)
{
    voice_source_search_data_t *search_data = data;
    if ((DeviceProperties_GetVoiceSource(device) == search_data->source_to_find) &&
        (BtDevice_GetDeviceType(device) == DEVICE_TYPE_HANDSET))
    {
        search_data->source_found = TRUE;
    }
}

static voice_source_t hfpProfileInstance_AllocateVoiceSourceToDevice(hfpInstanceTaskData *instance)
{
    voice_source_search_data_t search_data = {voice_source_hfp_1, FALSE};
    device_t device = HfpProfileInstance_FindDeviceFromInstance(instance);
    PanicFalse(device != NULL);

    /* Find a free voice source */
    DeviceList_Iterate(hfpProfileInstance_SearchForHandsetWithVoiceSource, &search_data);
    if (search_data.source_found)
    {
        /* If hfp_1 has been allocated, try to allocate hfp_2 */
        search_data.source_found = FALSE;
        search_data.source_to_find = voice_source_hfp_2;
        DeviceList_Iterate(hfpProfileInstance_SearchForHandsetWithVoiceSource, &search_data);
    }
    if (!search_data.source_found)
    {
        /* A free audio_source exists, allocate it to the device with the instance. */
        DeviceProperties_SetVoiceSource(device, search_data.source_to_find);
        DEBUG_LOG_VERBOSE("hfpProfileInstance_AllocateVoiceSourceToDevice inst(%p) device=%p enum:voice_source_t:%d",
                          instance, device, search_data.source_to_find);
    }
    else
    {
        /* It should be impossible to have connected the HFP profile if we have already
           two connected voice sources for HFP, this may indicate a handle was leaked. */
        Panic();
    }

    return search_data.source_to_find;
}

hfpInstanceTaskData * HfpProfileInstance_GetInstanceForDevice(device_t device)
{
    hfpInstanceTaskData** pointer_to_instance;
    size_t size_pointer_to_instance;

    if(device && Device_GetProperty(device, device_property_hfp_instance, (void**)&pointer_to_instance, &size_pointer_to_instance))
    {
        PanicFalse(size_pointer_to_instance == sizeof(hfpInstanceTaskData*));
        return *pointer_to_instance;
    }
    DEBUG_LOG_VERBOSE("HfpProfileInstance_GetInstanceForDevice device=%p has no device_property_hfp_instance", device);
    return NULL;
}

/*! \brief Get HFP lock */
uint16 * HfpProfileInstance_GetLock(hfpInstanceTaskData* instance)
{
    return &instance->hfp_lock;
}

/*! \brief Set HFP lock */
void HfpProfileInstance_SetLock(hfpInstanceTaskData* instance, uint16 lock)
{
    instance->hfp_lock = lock;
}

/*! \brief Is HFP SCO/ACL encrypted */
bool HfpProfileInstance_IsEncrypted(hfpInstanceTaskData* instance)
{
    return instance->bitfields.encrypted;
}

hfpInstanceTaskData * HfpProfileInstance_GetInstanceForBdaddr(const bdaddr *bd_addr)
{
    hfpInstanceTaskData* instance = NULL;
    device_t device = NULL;

    PanicNull((void *)bd_addr);

    device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device != NULL)
    {
        instance = HfpProfileInstance_GetInstanceForDevice(device);
    }

    return instance;
}

device_t HfpProfileInstance_FindDeviceFromVoiceSource(voice_source_t source)
{
    return DeviceList_GetFirstDeviceWithPropertyValue(device_property_voice_source, &source, sizeof(voice_source_t));
}

hfpInstanceTaskData * HfpProfileInstance_GetInstanceForSource(voice_source_t source)
{
    hfpInstanceTaskData* instance = NULL;

    if (source != voice_source_none)
    {
        device_t device = HfpProfileInstance_FindDeviceFromVoiceSource(source);

        if (device != NULL)
        {
            instance = HfpProfileInstance_GetInstanceForDevice(device);
        }
    }

    DEBUG_LOG_V_VERBOSE("HfpProfileInstance_GetInstanceForSource(%p) enum:voice_source_t:%d",
                         instance, source);

    return instance;
}

voice_source_t HfpProfileInstance_GetVoiceSourceForInstance(hfpInstanceTaskData * instance)
{
    voice_source_t source = voice_source_none;

    PanicNull(instance);

    device_t device = BtDevice_GetDeviceForBdAddr(&instance->ag_bd_addr);
    if (device)
    {
        source = DeviceProperties_GetVoiceSource(device);
    }

    return source;
}

void HfpProfileInstance_RegisterVoiceSourceInterfaces(voice_source_t voice_source)
{
    /* Register voice source interfaces implementated by HFP profile */
    VoiceSources_RegisterAudioInterface(voice_source, HfpProfile_GetAudioInterface());
    VoiceSources_RegisterTelephonyControlInterface(voice_source, HfpProfile_GetTelephonyControlInterface());
    VoiceSources_RegisterObserver(voice_source, HfpProfile_GetVoiceSourceObserverInterface());
}

void HfpProfileInstance_DeregisterVoiceSourceInterfaces(voice_source_t voice_source)
{
    //VoiceSources_RegisterAudioInterface(source, NULL);
    VoiceSources_DeregisterTelephonyControlInterface(voice_source);
    //VoiceSources_RegisterVolume(source, NULL);
    VoiceSources_DeregisterObserver(voice_source, HfpProfile_GetVoiceSourceObserverInterface());
}

hfpInstanceTaskData * HfpProfileInstance_Create(const bdaddr *bd_addr, bool allocate_source)
{
    voice_source_t new_source = voice_source_none;

    DEBUG_LOG_FN_ENTRY("HfpProfileInstance_Create");

    device_t device = PanicNull(BtDevice_GetDeviceForBdAddr(bd_addr));

    /* Panic if we have a duplicate instance somehow */
    hfpInstanceTaskData* instance = HfpProfileInstance_GetInstanceForDevice(device);
    PanicNotNull(instance);

    /* Allocate new instance */
    instance = PanicUnlessNew(hfpInstanceTaskData);
    memset(instance, 0 , sizeof(*instance));
    hfpProfileInstance_SetInstanceForDevice(device, instance);

    DEBUG_LOG("HfpProfileInstance_Create(%p) device=%p", instance, device);

    /* Initialise instance */
    hfpProfileInstance_InitTaskData(instance);

    /* Set Bluetooth address of remote device */
    instance->ag_bd_addr = *bd_addr;

    /* initialise the routed state */
    instance->source_state = source_state_disconnected;

    if (appDeviceIsHandset(bd_addr))
    {
        if(allocate_source)
        {
            new_source = hfpProfileInstance_AllocateVoiceSourceToDevice(instance);
            HfpProfileInstance_RegisterVoiceSourceInterfaces(new_source);
        }
    }
    else
    {
        Panic(); /* Unexpected device type */
    }

    /* Return pointer to new instance */
    return instance;
}

/*! \brief Destroy HFP instance

    This function should only be called if the instance no longer has HFP connected.
    If HFP is still connected, the function will silently fail.

    The function will panic if the instance is not valid, or if the instance
    is already destroyed.

    \param  instance The instance to destroy

*/
void HfpProfileInstance_Destroy(hfpInstanceTaskData *instance)
{
    DEBUG_LOG("HfpProfileInstance_Destroy(%p)", instance);
    device_t device = HfpProfileInstance_FindDeviceFromInstance(instance);

    PanicNull(device);

    /* Destroy instance only if state machine is disconnected. */
    if (HfpProfile_IsDisconnected(instance))
    {
        DEBUG_LOG("HfpProfileInstance_Destroy(%p) permitted", instance);

        /* Flush any messages still pending delivery */
        MessageFlushTask(&instance->task);

        /* Clear entry and free instance */
        hfpProfileInstance_SetInstanceForDevice(device, NULL);
        free(instance);

        voice_source_t source = DeviceProperties_GetVoiceSource(device);
        DeviceProperties_RemoveVoiceSource(device);

        /* Deregister voice source interfaces that were implementated by the instance. */
        HfpProfileInstance_DeregisterVoiceSourceInterfaces(source);
    }
    else
    {
        DEBUG_LOG("HfpProfileInstance_Destroy(%p) HFP (%d) not disconnected, or HFP Lock Pending",
                   instance, !HfpProfile_IsDisconnected(instance));
    }
}


