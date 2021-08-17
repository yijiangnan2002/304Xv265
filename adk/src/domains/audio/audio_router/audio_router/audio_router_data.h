/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   audio_router_data Audio Router
\ingroup    audio_domain
\brief      Data handling functions for use in the audio router.
*/

#ifndef AUDIO_ROUTER_DATA_H_
#define AUDIO_ROUTER_DATA_H_

#include "audio_sources_list.h"

void AudioRouterData_StoreLastRoutedAudio(audio_source_t audio_source);

#ifdef HOSTED_TEST_ENVIRONMENT
#include "audio_router_typedef.h"
void AudioRouter_SetDataContainer(audio_router_data_container_t * container);
#endif /* HOSTED_TEST_ENVIRONMENT */

#endif /* AUDIO_ROUTER_DATA_H_*/
