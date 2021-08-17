/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   select_focus_domains Focus Select
\ingroup    focus_domains
\brief      API for iterating through the active audio sources in the registry.
*/

#include "sources_iterator.h"

#include <source_param_types.h>
#include "audio_sources_list.h"
#include "audio_sources.h"
#include "audio_sources_interface_registry.h"
#include <voice_sources.h>

#include <panic.h>
#include <stdlib.h>
#include <macros.h>

typedef struct generic_source_iterator_tag
{
    generic_source_t active_sources[max_audio_sources+max_voice_sources];
    uint8 num_active_sources;
    uint8 next_index;

} iterator_internal_t;

static void sourcesIterator_AddVoiceSources(iterator_internal_t * iter) {
    voice_source_t source = voice_source_none;
    while(++source < max_voice_sources)
    {
        if (VoiceSources_IsSourceRegisteredForTelephonyControl(source))
        {
            iter->active_sources[iter->num_active_sources].type = source_type_voice;
            iter->active_sources[iter->num_active_sources].u.voice = source;
            iter->num_active_sources++;
        }
    }
}

static void sourcesIterator_AddAudioSources(iterator_internal_t * iter) {
    interface_list_t list = {0};
    audio_source_t source = audio_source_none;
    while(++source < max_audio_sources)
    {
        list = AudioInterface_Get(source, audio_interface_type_media_control);
        if (list.number_of_interfaces >= 1)
        {
            iter->active_sources[iter->num_active_sources].type = source_type_audio;
            iter->active_sources[iter->num_active_sources].u.audio = source;
            iter->num_active_sources++;
        }
    }
}

sources_iterator_t SourcesIterator_Create(source_type_t type)
{
    iterator_internal_t * iter = NULL;

    iter = (iterator_internal_t *)PanicUnlessMalloc(sizeof(iterator_internal_t));
    memset(iter, 0, sizeof(iterator_internal_t));

    if (type == source_type_max)
    {
        sourcesIterator_AddVoiceSources(iter);
        sourcesIterator_AddAudioSources(iter);
    }
    else if (type == source_type_audio)
    {
        sourcesIterator_AddAudioSources(iter);
    }
    else if (type == source_type_voice)
    {
        sourcesIterator_AddVoiceSources(iter);
    }
    return iter;
}

generic_source_t SourcesIterator_NextGenericSource(sources_iterator_t iterator)
{
    generic_source_t next_source = { .type=source_type_invalid, .u=voice_source_none};
    PanicNull(iterator);

    if (iterator->next_index < iterator->num_active_sources)
    {
        next_source = iterator->active_sources[iterator->next_index++];
    }

    return next_source;
}

void SourcesIterator_Destroy(sources_iterator_t iterator)
{
    PanicNull(iterator);

    memset(iterator, 0, sizeof(iterator_internal_t));
    free(iterator);
}
