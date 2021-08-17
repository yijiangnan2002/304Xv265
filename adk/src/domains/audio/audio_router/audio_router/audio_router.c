/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of audio router functions.
*/

#include "audio_router.h"
#include "logging.h"
#include "audio_sources.h"
#include "voice_sources.h"
#include "kymera_adaptation.h"
#include "audio_router_typedef.h"
#include "panic.h"
#include "audio_router_data.h"
#include <stdlib.h>

static audio_router_t* router_instance_handlers = NULL;
static const audio_router_voice_routing_ind_t *voice_routing_observer = NULL;

static void audioRouter_PanicIfSourceInvalid(generic_source_t source)
{
    switch(source.type)
    {
        case source_type_voice:
            PanicFalse(source.u.voice < max_voice_sources);
            break;
        case source_type_audio:
            PanicFalse(source.u.audio < max_audio_sources);
            break;
        default:
            Panic();
            break;
    }
}

void AudioRouter_ConfigureHandlers(const audio_router_t* handlers)
{
    DEBUG_LOG_FN_ENTRY("AudioRouter_ConfigureHandlers");

    PanicNull((void*)handlers);

    router_instance_handlers = (audio_router_t*)handlers;
}

void AudioRouter_AddSource(generic_source_t source)
{
    DEBUG_LOG_FN_ENTRY("AudioRouter_AddSource");

    audioRouter_PanicIfSourceInvalid(source);

    PanicNull(router_instance_handlers);

    router_instance_handlers->add_source(source);
}
 
void AudioRouter_RemoveSource(generic_source_t source)
{
    DEBUG_LOG_FN_ENTRY("AudioRouter_RemoveSource");

    audioRouter_PanicIfSourceInvalid(source);

    PanicNull(router_instance_handlers);

    router_instance_handlers->remove_source(source);
}

bool AudioRouter_IsDeviceInUse(device_t device)
{
    DEBUG_LOG_FN_ENTRY("AudioRouter_IsDeviceInUse");

    PanicNull(router_instance_handlers);

    return router_instance_handlers->is_device_in_use(device);

}

void AudioRouter_Update(void)
{
    DEBUG_LOG_FN_ENTRY("AudioRouter_Update");

    PanicNull(router_instance_handlers);

    router_instance_handlers->update();
}

static void audioRouter_SendRoutingIndication(void)
{
    DEBUG_LOG_FN_ENTRY("audioRouter_SendRoutingIndication");

    if (voice_routing_observer && (voice_routing_observer->VoiceRoutingInd))
    {
        voice_routing_observer->VoiceRoutingInd();
    }
}

static void audioRouter_SendUnRoutedIndication(void)
{
    DEBUG_LOG_FN_ENTRY("audioRouter_SendUnRoutedIndication");

    if (voice_routing_observer && (voice_routing_observer->VoiceUnroutedInd))
    {
        voice_routing_observer->VoiceUnroutedInd();
    }
}

static bool audioRouter_CommonConnectAudioSource(generic_source_t source)
{
    bool success = FALSE;

    connect_parameters_t connect_parameters;

    DEBUG_LOG_FN_ENTRY("audioRouter_CommonConnectAudioSource enum:audio_source_t:%d", source.u.audio);

    if(AudioSources_GetConnectParameters(source.u.audio, 
                                            &connect_parameters.source_params))
    {
        connect_parameters.source.type = source_type_audio;
        connect_parameters.source.u.audio = source.u.audio;

        KymeraAdaptation_Connect(&connect_parameters);
        AudioSources_ReleaseConnectParameters(source.u.audio,
                                                &connect_parameters.source_params);

        AudioRouterData_StoreLastRoutedAudio(source.u.audio);
        AudioSources_OnAudioRoutingChange(source.u.audio, source_routed);
        success = TRUE;
    }
    else
    {
        DEBUG_LOG_ERROR("ERROR - unexpected failure getting connection params, routing audio failed");
    }
    return success;
}

static bool audioRouter_CommonConnectVoiceSource(generic_source_t source)
{
    bool success = FALSE;
    connect_parameters_t connect_parameters;

    DEBUG_LOG_FN_ENTRY("audioRouter_CommonConnectVoiceSource enum:voice_source_t:%d", source.u.voice);

    if(VoiceSources_GetConnectParameters(source.u.voice,
                                            &connect_parameters.source_params))
    {
        connect_parameters.source.type = source_type_voice;
        connect_parameters.source.u.voice = source.u.voice;
        audioRouter_SendRoutingIndication();
        KymeraAdaptation_Connect(&connect_parameters);
        VoiceSources_ReleaseConnectParameters(source.u.voice,
                                                &connect_parameters.source_params);
        success = TRUE;
    }
    else
    {
        DEBUG_LOG_ERROR("ERROR - unexpected failure getting connection params, routing voice failed");
    }

    return success;
}

bool AudioRouter_CommonConnectSource(generic_source_t source)
{
    bool success = FALSE;

    DEBUG_LOG_FN_ENTRY("AudioRouter_CommonConnectSource enum:source_type_t:%d", source.type);

    audioRouter_PanicIfSourceInvalid(source);

    switch(source.type)
    {
        case source_type_voice:
            success = audioRouter_CommonConnectVoiceSource(source);
            break;

        case source_type_audio:
            success = audioRouter_CommonConnectAudioSource(source);
            break;

        default:
            break;
    }

    return success;
}

static bool audioRouter_CommonDisconnectAudioSource(generic_source_t source)
{
    bool success = FALSE;
    disconnect_parameters_t disconnect_parameters;

    DEBUG_LOG_FN_ENTRY("audioRouter_CommonDisconnectAudioSource enum:audio_source_t:%d", source.u.audio);

    if(AudioSources_GetDisconnectParameters(source.u.audio, 
                                            &disconnect_parameters.source_params))
    {
        disconnect_parameters.source.type = source_type_audio;
        disconnect_parameters.source.u.audio = source.u.audio;

        KymeraAdaptation_Disconnect(&disconnect_parameters);
        AudioSources_ReleaseDisconnectParameters(source.u.audio,
                                                    &disconnect_parameters.source_params);
        AudioSources_OnAudioRoutingChange(source.u.audio, source_unrouted);
        success = TRUE;
    }
    else
    {
        DEBUG_LOG_ERROR("ERROR - unexpected failure getting disconnection params, disconnecting audio failed");
    }

    return success;
}

static bool audioRouter_CommonDisconnectVoiceSource(generic_source_t source)
{
    bool success = FALSE;
    disconnect_parameters_t disconnect_parameters;

    DEBUG_LOG_FN_ENTRY("audioRouter_CommonDisconnectVoiceSource enum:voice_source_t:%d", source.u.voice);

    if(VoiceSources_GetDisconnectParameters(source.u.voice, 
                                            &disconnect_parameters.source_params))
    {
        disconnect_parameters.source.type = source_type_voice;
        disconnect_parameters.source.u.voice = source.u.voice;

        KymeraAdaptation_Disconnect(&disconnect_parameters);
        VoiceSources_ReleaseDisconnectParameters(source.u.voice,
                                                    &disconnect_parameters.source_params);
        audioRouter_SendUnRoutedIndication();

        success = TRUE;
    }
    else
    {
        DEBUG_LOG_ERROR("ERROR - unexpected failure getting disconnection params, disconnecting voice failed");
    }

    return success;
}

bool AudioRouter_CommonDisconnectSource(generic_source_t source)
{
    bool success = FALSE;

    DEBUG_LOG_FN_ENTRY("AudioRouter_CommonDisconnectSource enum:source_type_t:%d", source.type);

    audioRouter_PanicIfSourceInvalid(source);

    switch(source.type)
    {
        case source_type_voice:
            success = audioRouter_CommonDisconnectVoiceSource(source);
            break;

        case source_type_audio:
            success = audioRouter_CommonDisconnectAudioSource(source);
            break;

        default:
            break;
    }
    return success;
}

source_status_t AudioRouter_CommonSetSourceState(generic_source_t source, source_state_t state)
{
    source_status_t response = source_status_error;

    audioRouter_PanicIfSourceInvalid(source);

    switch(source.type)
    {
        case source_type_voice:
            DEBUG_LOG_INFO("AudioRouter_CommonSetSourceState enum:voice_source_t:%d, state enum:source_state_t:%d", source.u.voice, state);
            response = VoiceSources_SetState(source.u.voice, state);
            break;

        case source_type_audio:
            DEBUG_LOG_INFO("AudioRouter_CommonSetSourceState enum:audio_source_t:%d, state enum:source_state_t:%d", source.u.audio, state);
            response = AudioSources_SetState(source.u.audio, state);
            break;

        default:
            Panic();
            break;
    }

    return response;
}

void AudioRouter_RegisterForVoiceRoutingInd(const audio_router_voice_routing_ind_t *interface)
{
    DEBUG_LOG_FN_ENTRY("AudioRouter_RegisterForVoiceRoutingInd");

    PanicNull((void*)interface);

    voice_routing_observer = interface;
}
