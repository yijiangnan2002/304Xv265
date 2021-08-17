/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   select_focus_domains Focus Select
\ingroup    focus_domains
\brief      This module is an implementation of the focus interface which supports
            selecting the active, focussed device during multipoint use cases.
*/

#include "focus_select.h"

#include "focus_audio_source.h"
#include "focus_device.h"
#include "focus_generic_source.h"
#include "focus_voice_source.h"

#include <audio_sources.h>
#include <audio_router.h>
#include <bt_device.h>
#include <connection_manager.h>
#include <device.h>
#include <device_list.h>
#include <device_properties.h>
#include <handset_service_config.h>
#include <logging.h>
#include <panic.h>
#include <ui.h>
#include <ui_inputs.h>
#include <sources_iterator.h>
#include <stdlib.h>

#define CONVERT_AUDIO_SOURCE_TO_ARRAY_INDEX(x) ((x)-1)
#define CONVERT_VOICE_SOURCE_TO_ARRAY_INDEX(x) ((x)-1)

/* Note: the lower bits in these masks are to accomodate the integer priority of the source */
#define SOURCE_TYPE_HAS_PRIORITY    0x01
#define SOURCE_HAS_AUDIO            0x40

#ifndef ENABLE_A2DP_BARGE_IN
#define ENABLE_A2DP_BARGE_IN FALSE
#endif

typedef struct
{
    unsigned context;
    bool has_audio;
    uint8 priority;
} source_cache_data_t;

/* Used to collect context information from the voice and audio sources available
   in the framework, in a standard form. This data can then be processed to decide
   which source should be assigned foreground focus. */
typedef struct
{
    source_cache_data_t cache_data_by_voice_source_array[CONVERT_VOICE_SOURCE_TO_ARRAY_INDEX(max_voice_sources)];
    source_cache_data_t cache_data_by_audio_source_array[CONVERT_AUDIO_SOURCE_TO_ARRAY_INDEX(max_audio_sources)];
    generic_source_t highest_priority_source;
    bool highest_priority_source_has_audio;
    unsigned highest_priority_context;
    uint8 highest_priority;
    uint8 num_highest_priority_sources;

} focus_status_t;

/* Functions of this type shall compute the priority of the current source and assign the focus status
   cache with the result. They shall return a pointer to the assigned cache struct. */
typedef source_cache_data_t * (*priority_calculator_fn_t)(focus_status_t * focus_status, generic_source_t source);

static focus_select_audio_tie_break_t * audio_source_tie_break_ordering = NULL;
static focus_select_voice_tie_break_t * voice_source_tie_break_ordering = NULL;
static bool a2dp_barge_in_enabled;
static bool voice_sources_have_priority;

/* Look-up table mapping the audio_context into a priority suitable for comparison
   with voice sources priorities for determining which source should have focus
   for audio routing. 0 is the lowest priority. */
static int8 audio_context_to_audio_prio_mapping[] = {
    0, //context_audio_disconnected
    1, //context_audio_connected
    3, //context_audio_is_streaming
    4, //context_audio_is_playing
    5, //context_audio_is_va_response
    2, //context_audio_is_paused
};
COMPILE_TIME_ASSERT(ARRAY_DIM(audio_context_to_audio_prio_mapping) == max_audio_contexts,
                    FOCUS_SELECT_invalid_size_audio_context_to_audio_prio_mapping_table);

/* Look-up table mapping the voice_context symbol to the relative priority of
   that context in determining focus. This table considers priorities for audio
   routing purposes. 0 is the lowest priority. */
static int8 voice_context_to_audio_prio_mapping[] = {
    0, // context_voice_disconnected
    1, // context_voice_connected,
    7, // context_voice_ringing_outgoing,
    5, // context_voice_ringing_incoming,
    6, // context_voice_in_call,
};
COMPILE_TIME_ASSERT(ARRAY_DIM(voice_context_to_audio_prio_mapping) == max_voice_contexts,
                    FOCUS_SELECT_invalid_size_audio_prio_mapping_table);

/* Look-up table mapping the voice_context symbol to the relative priority of
   that context in determining focus. This table considers priorities for UI
   interactions. 0 is the lowest priority. */
static int8 voice_context_to_ui_prio_mapping[] = {
    0, // context_voice_disconnected
    1, // context_voice_connected,
    3, // context_voice_ringing_outgoing,
    4, // context_voice_ringing_incoming,
    2, // context_voice_in_call,
};
COMPILE_TIME_ASSERT(ARRAY_DIM(voice_context_to_ui_prio_mapping) == max_voice_contexts,
                    FOCUS_SELECT_invalid_size_ui_prio_mapping_table);

static source_cache_data_t * focusSelect_SetCacheDataForSource(focus_status_t * focus_status, generic_source_t source, unsigned context, bool has_audio, uint8 priority)
{
    source_cache_data_t * cache = NULL;
    PanicFalse(source.type != source_type_invalid);
    if (GenericSource_IsVoice(source))
    {
        PanicFalse(source.u.voice != voice_source_none);
        cache = &focus_status->cache_data_by_voice_source_array[CONVERT_VOICE_SOURCE_TO_ARRAY_INDEX(source.u.voice)];
        cache->has_audio = has_audio;
        cache->context = context;
        cache->priority = priority;
    }
    else if (GenericSource_IsAudio(source))
    {
        PanicFalse(source.u.audio != audio_source_none);
        cache = &focus_status->cache_data_by_audio_source_array[CONVERT_AUDIO_SOURCE_TO_ARRAY_INDEX(source.u.audio)];
        cache->has_audio = has_audio;
        cache->context = context;
        cache->priority = priority;
    }
    return cache;
}

static source_cache_data_t * focusSelect_AudioSourceCalculatePriorityForUi(focus_status_t * focus_status, generic_source_t curr_source)
{
    uint8 source_priority = 0;
    unsigned source_context = BAD_CONTEXT;

    if (GenericSource_IsAudio(curr_source))
    {
        source_context = AudioSources_GetSourceContext(curr_source.u.audio);
        source_priority = source_context;
    }
    else
    {
        Panic();
    }

    PanicFalse(source_context != BAD_CONTEXT);

    return focusSelect_SetCacheDataForSource(focus_status, curr_source, source_context, FALSE, source_priority);
}

static source_cache_data_t * focusSelect_VoiceSourceCalculatePriorityForUi(focus_status_t * focus_status, generic_source_t curr_source)
{
    uint8 source_priority = 0;
    unsigned source_context = BAD_CONTEXT;

    if (GenericSource_IsVoice(curr_source))
    {
        source_context = VoiceSources_GetSourceContext(curr_source.u.voice);
        source_priority = voice_context_to_ui_prio_mapping[source_context];
    }
    else
    {
        Panic();
    }

    PanicFalse(source_context != BAD_CONTEXT);

    return focusSelect_SetCacheDataForSource(focus_status, curr_source, source_context, FALSE, source_priority);
}

static source_cache_data_t * focusSelect_CalculatePriorityForAudio(focus_status_t * focus_status, generic_source_t curr_source)
{
    uint8 source_priority = 0;
    bool source_has_audio = FALSE;
    unsigned source_context = BAD_CONTEXT;

    if (GenericSource_IsVoice(curr_source))
    {
        source_has_audio = VoiceSources_IsVoiceChannelAvailable(curr_source.u.voice);
        source_context = VoiceSources_GetSourceContext(curr_source.u.voice);

        source_priority = voice_context_to_audio_prio_mapping[source_context];
        source_priority |= (source_has_audio ? SOURCE_HAS_AUDIO : 0x0);
        source_priority <<= 1;
        source_priority |= (voice_sources_have_priority ? SOURCE_TYPE_HAS_PRIORITY : 0x0);
    }
    else if (GenericSource_IsAudio(curr_source))
    {
        source_context = AudioSources_GetSourceContext(curr_source.u.audio);

        source_priority = audio_context_to_audio_prio_mapping[source_context];
        source_priority <<= 1;
        source_priority |= (!voice_sources_have_priority ? SOURCE_TYPE_HAS_PRIORITY : 0x0);
    }

    PanicFalse(source_context != BAD_CONTEXT);

    return focusSelect_SetCacheDataForSource(focus_status, curr_source, source_context, source_has_audio, source_priority);
}

static unsigned focusSelect_GetContext(focus_status_t * focus_status, generic_source_t source)
{
    unsigned context = BAD_CONTEXT;
    unsigned index = 0;

    if (GenericSource_IsAudio(source))
    {
        PanicFalse(source.u.audio != audio_source_none);
        index = CONVERT_AUDIO_SOURCE_TO_ARRAY_INDEX(source.u.audio);
        context = focus_status->cache_data_by_audio_source_array[index].context;
    }
    else if (GenericSource_IsVoice(source))
    {
        PanicFalse(source.u.voice != voice_source_none);
        index = CONVERT_VOICE_SOURCE_TO_ARRAY_INDEX(source.u.voice);
        context = focus_status->cache_data_by_voice_source_array[index].context;
    }
    return context;
}

static bool focusSelect_IsAudioSourceContextConnected(focus_status_t * focus_status, audio_source_t source)
{
    generic_source_t gen_source = {.type = source_type_audio, .u.audio = source};
    return focusSelect_GetContext(focus_status, gen_source) == context_audio_connected;
}

static bool focusSelect_IsSourceContextHighestPriority(focus_status_t * focus_status, generic_source_t source)
{
    return focusSelect_GetContext(focus_status, source) == focus_status->highest_priority_context;
}

static bool focusSelect_CompileFocusStatus(sources_iterator_t iter, focus_status_t * focus_status, priority_calculator_fn_t calculate_priority)
{
    bool source_found = FALSE;
    generic_source_t curr_source = SourcesIterator_NextGenericSource(iter);

    while (GenericSource_IsValid(curr_source))
    {
        /* Compute the priority for the source and assign it to the cache. */
        source_cache_data_t * cache = calculate_priority(focus_status, curr_source);

        /* Compare the source priority with the previous highest priority generic source. */
        uint8 previous_highest_priority = focus_status->highest_priority;

        DEBUG_LOG_V_VERBOSE("focusSelect_CompileFocusStatus src_type=%d src=%d prios this=%x prev_highest=%x",
                            curr_source.type, curr_source.u.voice, cache->priority, previous_highest_priority);

        if (!source_found || cache->priority > previous_highest_priority)
        {
            /* New highest priority source found */
            focus_status->highest_priority_source = curr_source;
            focus_status->highest_priority_context = cache->context;
            focus_status->highest_priority_source_has_audio = cache->has_audio;
            focus_status->highest_priority = cache->priority;
            focus_status->num_highest_priority_sources = 1;
            source_found = TRUE;
        }
        else if (cache->priority == previous_highest_priority)
        {
            /* The sources have equal priority, this may cause a tie break to occur later */
            focus_status->num_highest_priority_sources += 1;
        }
        else
        {
            /* Indicates the source is lower priority than existing, do nothing */
        }

        curr_source = SourcesIterator_NextGenericSource(iter);
    }

    return source_found;
}

static audio_source_t focusSelect_GetMruAudioSource(void)
{
    uint8 is_mru_handset = TRUE;
    device_t device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_mru, &is_mru_handset, sizeof(uint8));
    
    if (device == NULL)
    {
        deviceType type = DEVICE_TYPE_HANDSET;
        device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_type, &type, sizeof(deviceType));
    }
    
    if (device != NULL)
    {
        return DeviceProperties_GetAudioSource(device);
    }
    
    return audio_source_none;
}

static audio_source_t focusSelect_ConvertAudioTieBreakToSource(focus_select_audio_tie_break_t prio)
{
    audio_source_t source = audio_source_none;
    switch(prio)
    {
    case FOCUS_SELECT_AUDIO_LINE_IN:
        source = audio_source_line_in;
        break;
    case FOCUS_SELECT_AUDIO_USB:
        source = audio_source_usb;
        break;
    case FOCUS_SELECT_AUDIO_A2DP:
        {
            source = AudioRouter_GetLastRoutedAudio();
            
            if(a2dp_barge_in_enabled || source == audio_source_none)
            {
                source = focusSelect_GetMruAudioSource();
            }
        }
        break;
    case FOCUS_SELECT_AUDIO_LEA_UNICAST:
        source = audio_source_le_audio_unicast;
        break;
    case FOCUS_SELECT_AUDIO_LEA_BROADCAST:
        source = audio_source_le_audio_broadcast;
        break;
    default:
        break;
    }
    return source;
}

static void focusSelect_HandleTieBreak(focus_status_t * focus_status)
{
    audio_source_t last_routed_audio;

    /* Nothing to be done if all audio sources are disconnected or there is no need to tie break */
    if (focus_status->highest_priority_context == context_audio_disconnected ||
        focus_status->num_highest_priority_sources == 1)
    {
        return;
    }

    last_routed_audio = AudioRouter_GetLastRoutedAudio();

    /* A tie break is needed. Firstly, use the last routed audio source, if one exists */
    if (last_routed_audio != audio_source_none &&
        last_routed_audio < max_audio_sources &&
        focusSelect_IsAudioSourceContextConnected(focus_status, last_routed_audio) )
    {
        focus_status->highest_priority_source.type = source_type_audio;
        focus_status->highest_priority_source.u.audio = last_routed_audio;
    }
    /* Otherwise, run through the prioritisation of audio sources and select the highest */
    else
    {
        generic_source_t curr_source = {.type=source_type_audio, .u.audio=audio_source_none};
        if (audio_source_tie_break_ordering != NULL)
        {
            /* Tie break using the Application specified priority. */
            for (int i=0; i<FOCUS_SELECT_AUDIO_MAX_SOURCES; i++)
            {
                curr_source.u.audio = focusSelect_ConvertAudioTieBreakToSource(audio_source_tie_break_ordering[i]);
                if (curr_source.u.audio != audio_source_none && focusSelect_IsSourceContextHighestPriority(focus_status, curr_source))
                {
                    focus_status->highest_priority_source = curr_source;
                    break;
                }
            }
        }
        else
        {
            /* Tie break using implicit ordering of audio source. */
            for (curr_source.u.audio++ ; curr_source.u.audio < max_audio_sources; curr_source.u.audio++)
            {
                if (focusSelect_IsSourceContextHighestPriority(focus_status, curr_source))
                {
                    focus_status->highest_priority_source = curr_source;
                    break;
                }
            }
        }
    }

    DEBUG_LOG("focusSelect_HandleTieBreak enum:audio_source_t:%d enum:audio_source_provider_context_t:%d",
               focus_status->highest_priority_source.u.audio,
               focus_status->highest_priority_context);
}

static voice_source_t focusSelect_ConvertVoiceTieBreakToSource(focus_status_t * focus_status, focus_select_voice_tie_break_t prio)
{
    voice_source_t source = voice_source_none;
    switch(prio)
    {
    case FOCUS_SELECT_VOICE_USB:
        source = voice_source_usb;
        break;
    case FOCUS_SELECT_VOICE_LEA_UNICAST:
        source = voice_source_le_audio_unicast;
        break;
    case FOCUS_SELECT_VOICE_HFP:
        {
            uint8 hfp_1_prio = focus_status->cache_data_by_voice_source_array[CONVERT_VOICE_SOURCE_TO_ARRAY_INDEX(voice_source_hfp_1)].priority;
            uint8 hfp_2_prio = focus_status->cache_data_by_voice_source_array[CONVERT_VOICE_SOURCE_TO_ARRAY_INDEX(voice_source_hfp_2)].priority;

            DEBUG_LOG_V_VERBOSE("focusSelect_ConvertVoiceTieBreakToSource hfp_1_prio=%d hfp_2_prio=%d", hfp_1_prio, hfp_2_prio);

            bool tie_breaking_between_hfp_sources = (hfp_1_prio == hfp_2_prio) && (focus_status->highest_priority == hfp_1_prio);
            bool is_only_hfp_1_in_tie_break = (hfp_1_prio == focus_status->highest_priority) && (hfp_1_prio != hfp_2_prio);
            bool is_only_hfp_2_in_tie_break = (hfp_2_prio == focus_status->highest_priority) && (hfp_1_prio != hfp_2_prio);

            if (is_only_hfp_1_in_tie_break)
            {
                source = voice_source_hfp_1;
            }
            else if (is_only_hfp_2_in_tie_break)
            {
                source = voice_source_hfp_2;
            }
            else if (tie_breaking_between_hfp_sources)
            {
                /* Only use MRU to decide voice_source to return if we are tie breaking between two HFP sources,
                   i.e. not between HFP and USB, for example. */
                uint8 is_mru_handset = TRUE;
                device_t device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_mru, &is_mru_handset, sizeof(uint8));
                if (device != NULL)
                {
                    /* Check voice source associated with the MRU device is not none and a tied voice source that we are tie-breaking. */
                    voice_source_t mru_voice_source = DeviceProperties_GetVoiceSource(device);
                    if (mru_voice_source != voice_source_none &&
                        focus_status->cache_data_by_voice_source_array[CONVERT_VOICE_SOURCE_TO_ARRAY_INDEX(mru_voice_source)].priority == hfp_1_prio)
                    {
                        DEBUG_LOG_DEBUG("focusSelect_HandleVoiceTieBreak using MRU device voice source");
                        source = mru_voice_source;
                    }
                    else
                    {
                        DEBUG_LOG_DEBUG("focusSelect_HandleVoiceTieBreak MRU voice source is not in tie, using hfp_1");
                        source = voice_source_hfp_1;
                    }
                }
                else
                {
                    DEBUG_LOG_DEBUG("focusSelect_HandleVoiceTieBreak No MRU device, using hfp_1");
                    source = voice_source_hfp_1;
                }
            }
            else
            {
                /* HFP is not available or not a tie break source, skip. */
            }
        }
        break;
    default:
        break;
    }
    return source;
}

static void focusSelect_HandleVoiceTieBreak(focus_status_t * focus_status)
{
    /* Nothing to be done if all voice sources are disconnected or there is no need to tie break */
    if (focus_status->highest_priority_context == context_voice_disconnected ||
        focus_status->num_highest_priority_sources == 1)
    {
        return;
    }

    /* Run through the prioritisation of voice sources and select the highest */
    generic_source_t curr_source = {.type = source_type_voice, .u.voice = voice_source_none};
    if (voice_source_tie_break_ordering != NULL)
    {
        /* Tie break using the Application specified priority. */
        for (int i=0; i<FOCUS_SELECT_VOICE_MAX_SOURCES; i++)
        {
            curr_source.u.voice = focusSelect_ConvertVoiceTieBreakToSource(focus_status, voice_source_tie_break_ordering[i]);
            if (curr_source.u.voice != voice_source_none && focusSelect_IsSourceContextHighestPriority(focus_status, curr_source))
            {
                focus_status->highest_priority_source = curr_source;
                break;
            }
        }
    }
    else
    {
        /* Tie break using implicit ordering of voice source. */
        for (curr_source.u.voice++ ; curr_source.u.voice < max_voice_sources; curr_source.u.voice++)
        {
            if (focusSelect_IsSourceContextHighestPriority(focus_status, curr_source))
            {
                focus_status->highest_priority_source = curr_source;
                break;
            }
        }
    }

    DEBUG_LOG_VERBOSE("focusSelect_HandleVoiceTieBreak selected enum:voice_source_t:%d  enum:voice_source_provider_context_t:%d",
                      focus_status->highest_priority_source.u.voice,
                      focus_status->highest_priority_context);
}


static generic_source_t focusSelect_GetFocusedSourceForAudioRouting(void)
{
    focus_status_t focus_status_struct = {0};
    focus_status_t * focus_status = &focus_status_struct;

    sources_iterator_t iter = SourcesIterator_Create(source_type_max);
    focusSelect_CompileFocusStatus(iter, focus_status, focusSelect_CalculatePriorityForAudio);
    SourcesIterator_Destroy(iter);

    if (focus_status->num_highest_priority_sources != 1)
    {
        if (GenericSource_IsVoice(focus_status->highest_priority_source))
        {
            focusSelect_HandleVoiceTieBreak(focus_status);
        }
        else if (GenericSource_IsAudio(focus_status->highest_priority_source))
        {
            focusSelect_HandleTieBreak(focus_status);
        }
    }

    bool is_focused_source_disconnected =
         (GenericSource_IsVoice(focus_status->highest_priority_source) &&
          focus_status->highest_priority_context == context_voice_disconnected) ||
         (GenericSource_IsAudio(focus_status->highest_priority_source) &&
          focus_status->highest_priority_context == context_audio_disconnected);

    if (is_focused_source_disconnected && !focus_status->highest_priority_source_has_audio)
    {
        focus_status->highest_priority_source.type = source_type_invalid;
        focus_status->highest_priority_source.u.voice = 0;
    }

    DEBUG_LOG_DEBUG("focusSelect_GetFocusedSourceForAudioRouting src=(enum:source_type_t:%d,%d)",
                    focus_status->highest_priority_source.type,
                    focus_status->highest_priority_source.u.audio);

    return focus_status->highest_priority_source;
}

static bool focusSelect_GetAudioSourceForContext(audio_source_t * audio_source)
{
    bool source_found = FALSE;
    focus_status_t focus_status = {0};

    *audio_source = audio_source_none;

    sources_iterator_t iter = SourcesIterator_Create(source_type_audio);
    source_found = focusSelect_CompileFocusStatus(iter, &focus_status, focusSelect_AudioSourceCalculatePriorityForUi);
    SourcesIterator_Destroy(iter);

    if (source_found)
    {
        focusSelect_HandleTieBreak(&focus_status);

        /* Assign selected audio source */
        *audio_source = focus_status.highest_priority_source.u.audio;
    }

    DEBUG_LOG_DEBUG("focusSelect_GetAudioSourceForContext enum:audio_source_t:%d found=%d",
                    *audio_source, source_found);

    return source_found;
}

static bool focusSelect_GetAudioSourceForUiInput(ui_input_t ui_input, audio_source_t * audio_source)
{
    bool source_found = FALSE;
    focus_status_t focus_status = {0};

    /* For audio sources, we don't need to consider the UI Input type. This is because it
       is effectively prescreened by the UI component, which responds to the context returned
       by this module in the API focusSelect_GetAudioSourceForContext().

       A concrete example being we should only receive ui_input_stop if
       focusSelect_GetAudioSourceForContext() previously provided context_audio_is_streaming.
       In that case there can only be a single streaming source and it shall consume the
       UI Input. All other contentions are handled by focusSelect_HandleTieBreak. */

    *audio_source = audio_source_none;

    sources_iterator_t iter = SourcesIterator_Create(source_type_audio);
    source_found = focusSelect_CompileFocusStatus(iter, &focus_status, focusSelect_AudioSourceCalculatePriorityForUi);
    SourcesIterator_Destroy(iter);

    if (source_found)
    {
        focusSelect_HandleTieBreak(&focus_status);

        /* Assign selected audio source */
        *audio_source = focus_status.highest_priority_source.u.audio;
    }

    DEBUG_LOG_DEBUG("focusSelect_GetAudioSourceForUiInput enum:ui_input_t:%d enum:audio_source_t:%d found=%d",
                    ui_input, *audio_source, source_found);

    return source_found;
}

static focus_t focusSelect_GetFocusForAudioSource(const audio_source_t audio_source)
{
    generic_source_t source_to_check = {.type=source_type_audio, .u.audio= audio_source};

    generic_source_t focused_source = focusSelect_GetFocusedSourceForAudioRouting();

    if (GenericSource_IsSame(focused_source, source_to_check))
    {
        return focus_foreground;
    }
    else
    {
        return focus_none;
    }
}

static bool focusSelect_GetVoiceSourceForUiInteraction(voice_source_t * voice_source)
{
    bool source_found = FALSE;
    focus_status_t focus_status = {0};
    *voice_source = voice_source_none;

    sources_iterator_t iter = SourcesIterator_Create(source_type_voice);
    source_found = focusSelect_CompileFocusStatus(iter, &focus_status, focusSelect_VoiceSourceCalculatePriorityForUi);
    SourcesIterator_Destroy(iter);

    if (source_found)
    {
        focusSelect_HandleVoiceTieBreak(&focus_status);

        /* Assign selected voice source */
        *voice_source = focus_status.highest_priority_source.u.voice;
    }

    return source_found;
}

static bool focusSelect_GetVoiceSourceForContext(ui_providers_t provider, voice_source_t * voice_source)
{
    bool source_found = focusSelect_GetVoiceSourceForUiInteraction(voice_source);

    DEBUG_LOG_DEBUG("focusSelect_GetVoiceSourceForContext enum:ui_providers_t:%d enum:voice_source_t:%d found=%d",
                    provider, *voice_source, source_found);

    return source_found;
}

static bool focusSelect_GetVoiceSourceForUiInput(ui_input_t ui_input, voice_source_t * voice_source)
{
    bool source_found = focusSelect_GetVoiceSourceForUiInteraction(voice_source);

    DEBUG_LOG_DEBUG("focusSelect_GetVoiceSourceForUiInput enum:ui_input_t:%d enum:voice_source_t:%d found=%d",
                    ui_input, *voice_source, source_found);

    return source_found;
}

static focus_t focusSelect_GetFocusForVoiceSource(const voice_source_t voice_source)
{
    generic_source_t source_to_check = {.type = source_type_voice, .u.voice = voice_source};

    generic_source_t focused_source = focusSelect_GetFocusedSourceForAudioRouting();

    if (GenericSource_IsSame(focused_source, source_to_check))
    {
        return focus_foreground;
    }
    else
    {
        return focus_none;
    }
}

/* Used to collect the device information to identify the lowest priority device.*/
typedef struct
{
    device_t highest_priority_device;
    device_t lowest_priority_device;
} device_focus_status_t;

static bool focusSelect_GetHandsetDevice(device_t *device)
{
    DEBUG_LOG_FN_ENTRY("focusSelect_GetHandsetDevice");
    bool device_found = FALSE;

    PanicNull(device);

    for (uint8 pdl_index = 0; pdl_index < DeviceList_GetMaxTrustedDevices(); pdl_index++)
    {
        if (BtDevice_GetIndexedDevice(pdl_index, device))
        {
            if (BtDevice_GetDeviceType(*device) == DEVICE_TYPE_HANDSET)
            {

                uint8 is_excluded = FALSE;
                Device_GetPropertyU8(*device, device_property_excludelist, &is_excluded);

                if (!is_excluded)
                {
                    device_found = TRUE;
                    break;
                }
            }
        }
    }

    return device_found;
}

static void focusSelect_GetMruHandsetDevice(device_focus_status_t *focus_status)
{
    uint8 is_mru_handset = TRUE;
    device_t device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_mru, &is_mru_handset, sizeof(uint8));
    if(device != NULL)
    {
        focus_status->highest_priority_device = device;
    }
    else
    {
        deviceType type = DEVICE_TYPE_HANDSET;
        device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_type, &type, sizeof(deviceType));
        focus_status->highest_priority_device = device;
    }
}

static bool focusSelect_DeviceHasVoiceAudioFocus(device_t device)
{
    voice_source_t voice_source = DeviceProperties_GetVoiceSource(device);
    audio_source_t audio_source = DeviceProperties_GetAudioSource(device);
    
    if(focusSelect_GetFocusForVoiceSource(voice_source) == focus_foreground)
        return TRUE;

    if(focusSelect_GetFocusForAudioSource(audio_source) == focus_foreground)
        return TRUE;

    return FALSE;
}

static bool focusSelect_CompileConnectedDevicesFocusStatus(device_focus_status_t *focus_status)
{
    device_t* devices = NULL;
    bool device_found = FALSE;

    unsigned num_connected_handsets = BtDevice_GetConnectedHandsets(&devices);

    if(num_connected_handsets == 1)
    {
        focus_status->highest_priority_device = devices[0];
        focus_status->lowest_priority_device = devices[0];
        device_found = TRUE;
    }
    else
    {
        if(focusSelect_DeviceHasVoiceAudioFocus(devices[0]))
        {
            focus_status->highest_priority_device = devices[0];
        }
        else if(focusSelect_DeviceHasVoiceAudioFocus(devices[1]))
        {
            focus_status->highest_priority_device = devices[1];
        }
        else
        {
            focusSelect_GetMruHandsetDevice(focus_status);
        }
        
        if(focus_status->highest_priority_device == devices[0])
        {
            focus_status->lowest_priority_device = devices[1];
        }
        else
        {
            focus_status->lowest_priority_device = devices[0];
        }

        if(focus_status->lowest_priority_device != NULL)
        {
            device_found = TRUE;
        }
    }
    free(devices);
    return device_found;
}

static bool focusSelect_GetDeviceForUiInput(ui_input_t ui_input, device_t * device)
{
    bool device_found = FALSE;

    switch (ui_input)
    {
        case ui_input_connect_handset:
            device_found = focusSelect_GetHandsetDevice(device);
            break;
        case ui_input_disconnect_lru_handset:
            DEBUG_LOG_DEBUG("focusSelect_GetDeviceForUiInput enum:ui_input_t:%d", ui_input);
            device_focus_status_t focus_status = {0};
            device_found = focusSelect_CompileConnectedDevicesFocusStatus(&focus_status);
            if(device_found)
            {
                *device = focus_status.lowest_priority_device;
            }
            break;
        default:
            DEBUG_LOG_WARN("focusSelect_GetDeviceForUiInput enum:ui_input_t:%d not supported", ui_input);
            break;
    }

    return device_found;
}

static bool focusSelect_GetDeviceForContext(ui_providers_t provider, device_t* device)
{
    UNUSED(provider);
    UNUSED(device);

    return FALSE;
}

static focus_t focusSelect_GetFocusForDevice(const device_t device)
{
    focus_t focus = focus_none;
    generic_source_t routed_source = Focus_GetFocusedGenericSourceForAudioRouting();

    if (GenericSource_IsVoice(routed_source) && VoiceSource_IsHfp(routed_source.u.voice))
    {
        if (DeviceProperties_GetVoiceSource(device) == routed_source.u.voice)
        {
            focus = focus_foreground;
        }
    }
    else if (GenericSource_IsAudio(routed_source))
    {
        if (DeviceProperties_GetAudioSource(device) == routed_source.u.audio)
        {
            focus = focus_foreground;
        }
    }

    DEBUG_LOG("focusSelect_GetFocusForDevice device=0x%p src=(enum:source_type_t:%d,%d) enum:focus_t:%d",
              device, routed_source.type, routed_source.u.audio, focus);

    return focus;
}

static bool focusSelect_ExcludeDevice(device_t device)
{
    DEBUG_LOG_FN_ENTRY("focusSelect_ExcludeDevice device 0x%p", device);
    if (device)
    {
        return Device_SetPropertyU8(device, device_property_excludelist, TRUE);
    }
    return FALSE;
}

static bool focusSelect_IncludeDevice(device_t device)
{
    DEBUG_LOG_FN_ENTRY("focusSelect_IncludeDevice device 0x%p", device);

    if (device)
    {
        return Device_SetPropertyU8(device, device_property_excludelist, FALSE);
    }
    return FALSE;
}

static void focusSelect_ResetExcludedDevices(void)
{
    DEBUG_LOG_FN_ENTRY("focusSelect_ResetExcludedDevices");

    device_t* devices = NULL;
    unsigned num_devices = 0;
    uint8 excluded = TRUE;

    DeviceList_GetAllDevicesWithPropertyValue(device_property_excludelist, 
                                        (void*)&excluded, sizeof(excluded), 
                                        &devices, &num_devices);

    if (devices && num_devices)
    {
        for (unsigned i = 0; i < num_devices; i++)
        {
            bdaddr handset_addr = DeviceProperties_GetBdAddr(devices[i]);
            bool is_acl_connected = ConManagerIsConnected(&handset_addr);

            /* Only remove the device from exclude list if ACL is not connected. */
            if(!is_acl_connected)
            {
                focusSelect_IncludeDevice(devices[i]);
            }
        }
    }
    free(devices);
    devices = NULL;
}

static const focus_device_t interface_fns_for_device =
{
    .for_context = focusSelect_GetDeviceForContext,
    .for_ui_input = focusSelect_GetDeviceForUiInput,
    .focus = focusSelect_GetFocusForDevice,
    .add_to_excludelist = focusSelect_ExcludeDevice,
    .remove_from_excludelist = focusSelect_IncludeDevice,
    .reset_excludelist = focusSelect_ResetExcludedDevices
};

static const focus_get_audio_source_t interface_fns =
{
    .for_context = focusSelect_GetAudioSourceForContext,
    .for_ui_input = focusSelect_GetAudioSourceForUiInput,
    .focus = focusSelect_GetFocusForAudioSource
};

static const focus_get_voice_source_t voice_interface_fns =
{
    .for_context = focusSelect_GetVoiceSourceForContext,
    .for_ui_input = focusSelect_GetVoiceSourceForUiInput,
    .focus = focusSelect_GetFocusForVoiceSource
};

static const focus_get_generic_source_t generic_source_interface_fns =
{
    .for_audio_routing = focusSelect_GetFocusedSourceForAudioRouting
};

bool FocusSelect_Init(Task init_task)
{
    UNUSED(init_task);

    DEBUG_LOG_FN_ENTRY("FocusSelect_Init");

    Focus_ConfigureDevice(&interface_fns_for_device);
    Focus_ConfigureAudioSource(&interface_fns);
    Focus_ConfigureVoiceSource(&voice_interface_fns);
    Focus_ConfigureGenericSource(&generic_source_interface_fns);

    audio_source_tie_break_ordering = NULL;
    voice_source_tie_break_ordering = NULL;
    voice_sources_have_priority = TRUE;
    a2dp_barge_in_enabled = ENABLE_A2DP_BARGE_IN;

    return TRUE;
}

void FocusSelect_ConfigureAudioSourceTieBreakOrder(const focus_select_audio_tie_break_t *prioritisation)
{
    audio_source_tie_break_ordering = (focus_select_audio_tie_break_t *)prioritisation;
}

void FocusSelect_ConfigureVoiceSourceTieBreakOrder(const focus_select_voice_tie_break_t *prioritisation)
{
    voice_source_tie_break_ordering = (focus_select_voice_tie_break_t *)prioritisation;
}

void FocusSelect_EnableA2dpBargeIn(bool barge_in_enable)
{
    a2dp_barge_in_enabled = barge_in_enable;
}
