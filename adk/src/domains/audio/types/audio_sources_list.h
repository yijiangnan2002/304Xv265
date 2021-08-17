/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      List of supported audio sources.
*/

#ifndef AUDIO_SOURCES_LIST_H_
#define AUDIO_SOURCES_LIST_H_

typedef enum
{
    audio_source_none,
    audio_source_a2dp_1,
    audio_source_a2dp_2,
    audio_source_usb,
    audio_source_line_in,
    audio_source_le_audio_unicast,
    audio_source_le_audio_broadcast,
    max_audio_sources
} audio_source_t;

#define for_all_a2dp_audio_sources(a2dp_audio_source) for(a2dp_audio_source = audio_source_a2dp_1; a2dp_audio_source <= audio_source_a2dp_2; a2dp_audio_source++)

#endif /* AUDIO_SOURCES_LIST_H_ */
