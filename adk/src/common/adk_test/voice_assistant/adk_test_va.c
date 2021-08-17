/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       adk_test_va.c
\brief      Implementation of specifc application testing functions
*/

#ifndef DISABLE_TEST_API

#include <logging.h>
#include <ui.h>
#include <logical_input_switch.h>
#include <voice_ui.h>
#include <voice_ui_audio.h>
#include <voice_ui_container.h>
#include <voice_ui_va_client_if.h>
#include <voice_audio_manager.h>
#include "ama_config.h"

#include <ama.h>
#include <ama_audio.h>

#include "adk_test_va.h"

#ifdef GC_SECTIONS
/* Move all functions in KEEP_PM section to ensure they are not removed during
 * garbage collection */
#pragma unitcodesection KEEP_PM
static const void *tableOfAdkSymbolsToKeep[] = {
    (void*)appTestVaTap,
    (void*)appTestVaDoubleTap,
    (void*)appTestVaPressAndHold,
    (void*)appTestVaRelease,
    (void*)appTestVaHeldRelease,
    (void*)appTest_VaGetSelectedAssistant,
    (void*)appTestSetActiveVa2GAA,
    (void*)appTestSetActiveVa2AMA,
    (void*)appTestStartVaCapture,
    (void*)appTestStopVaCapture,
    (void*)appTestIsVaAudioActive,
#ifdef INCLUDE_AMA
    (void*)appTestPrintAmaLocale,
#endif
#ifdef INCLUDE_WUW
    (void*)appTestStartVaWakeUpWordDetection,
    (void*)appTestStopVaWakeUpWordDetection,
#endif
};
void appTestShowKeptAdkSymbols(void);
void appTestShowKeptAdkSymbols(void)
{
    for( size_t i = 0 ; i < sizeof(tableOfAdkSymbolsToKeep)/sizeof(tableOfAdkSymbolsToKeep[0]) ; i++ )
    {
        if( tableOfAdkSymbolsToKeep[i] != NULL )
        {
            DEBUG_LOG_ALWAYS("Have %p",tableOfAdkSymbolsToKeep[i]);
        }
    }
}

#endif

static const va_audio_encode_config_t va_encode_config_table[] =
{
    {.encoder = va_audio_codec_sbc, .encoder_params.sbc =
        {
            .bitpool_size = 24,
            .block_length = 16,
            .number_of_subbands = 8,
            .allocation_method = sbc_encoder_allocation_method_loudness,
        }
    },
    {.encoder = va_audio_codec_msbc, .encoder_params.msbc = {.bitpool_size = 24}},
    {.encoder = va_audio_codec_opus, .encoder_params.opus = {.frame_size = 40}},
};

void appTestVaTap(void)
{
    DEBUG_LOG_ALWAYS("appTestVaTap");
    /* Simulates a "Button Down -> Button Up -> Single Press Detected" sequence
    for the default configuration of a dedicated VA button */
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_1);
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_6);
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_3);
}

void appTestVaDoubleTap(void)
{
    DEBUG_LOG_ALWAYS("appTestVaDoubleTap");
    /* Simulates a "Button Down -> Button Up -> Button Down -> Button Up -> Double Press Detected"
    sequence for the default configuration of a dedicated VA button */
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_1);
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_6);
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_1);
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_6);
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_4);
}

void appTestVaPressAndHold(void)
{
    DEBUG_LOG_ALWAYS("appTestVaPressAndHold");
    /* Simulates a "Button Down -> Hold" sequence for the default configuration
    of a dedicated VA button */
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_1);
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_5);
}

void appTestVaRelease(void)
{
    DEBUG_LOG_ALWAYS("appTestVaRelease");
    /* Simulates a "Button Up" event for the default configuration
    of a dedicated VA button */
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_6);
}

void appTestVaHeldRelease(void)
{
    DEBUG_LOG_ALWAYS("appTestVaHeldRelease");
    /* Simulates a "Long Press" event for the default configuration
    of a dedicated VA button */
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_1);
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_2);
}

static unsigned vaTestDropDataInSource(Source source)
{
    DEBUG_LOG_ALWAYS("vaTestDropDataInSource");
    SourceDrop(source, SourceSize(source));
    return 0;
}

static bool vaTestPopulateVaMicConfig(unsigned num_of_mics, va_audio_mic_config_t *config)
{
    config->sample_rate = 16000;
    config->min_number_of_mics = 1;
    /* Use it as max in order to attempt to use this many mics */
    config->max_number_of_mics = num_of_mics;

    return (config->max_number_of_mics >= 1);
}

static bool vaTestPopulateVaEncodeConfig(va_audio_codec_t encoder, va_audio_encode_config_t *config)
{
    bool status = FALSE;
    unsigned i;

    for(i = 0; i < ARRAY_DIM(va_encode_config_table); i++)
    {
        if (va_encode_config_table[i].encoder == encoder)
        {
            status = TRUE;
            if (config)
            {
                *config = va_encode_config_table[i];
            }
            break;
        }
    }

    return status;
}

static bool vaTestPopulateVoiceCaptureParams(va_audio_codec_t encoder, unsigned num_of_mics, va_audio_voice_capture_params_t *params)
{
    return vaTestPopulateVaMicConfig(num_of_mics, &params->mic_config) && vaTestPopulateVaEncodeConfig(encoder, &params->encode_config);
}

unsigned appTest_VaGetSelectedAssistant(void)
{
    voice_ui_provider_t va = VoiceUi_GetSelectedAssistant();

    DEBUG_LOG_DEBUG("appTestGetActiveVa: enum:voice_ui_provider_t:%u", va);
    return va;
}

void appTestSetActiveVa2GAA(void)
{
#ifdef INCLUDE_GAA
    VoiceUi_SelectVoiceAssistant(voice_ui_provider_gaa, voice_ui_reboot_allowed);
#else
    DEBUG_LOG_ALWAYS("Gaa not included in the build");
#endif
}


void appTestSetActiveVa2AMA(void)
{
#ifdef INCLUDE_AMA
    VoiceUi_SelectVoiceAssistant(voice_ui_provider_ama, voice_ui_reboot_allowed);
#else
    DEBUG_LOG_ALWAYS("AMA not included in the build");
#endif
}

bool appTestStartVaCapture(va_audio_codec_t encoder, unsigned num_of_mics)
{
    bool status = FALSE;
    va_audio_voice_capture_params_t params = {0};

    if (vaTestPopulateVoiceCaptureParams(encoder, num_of_mics, &params))
    {
        status = VoiceAudioManager_StartCapture(vaTestDropDataInSource, &params);
    }

    return status;
}

bool appTestStopVaCapture(void)
{
    return VoiceAudioManager_StopCapture();
}

bool appTestIsVaAudioActive(void)
{
    return VoiceUi_IsVaActive();
}
#ifdef INCLUDE_AMA
void appTestPrintAmaLocale(void)
{
    char locale[AMA_LOCALE_STR_SIZE];

    if (AmaAudio_GetDeviceLocale(locale, sizeof(locale)))
    {
        DEBUG_LOG_ALWAYS("appTestPrintAmaLocale: \"%c%c%c%c%c\"",
                            locale[0], locale[1], locale[2], locale[3], locale[4]);
    }
    else
    {
        DEBUG_LOG_ALWAYS("appTestPrintAmaLocale: Failed to get locale");
    }
}
void appTestSetAmaLocale(const char *locale)
{
    DEBUG_LOG_ALWAYS("appTestSetAmaLocale: \"%c%c%c%c\"",
                        locale[0], locale[1], locale[2], locale[3]);
    VoiceUi_SetPackedLocale((uint8*)locale);
}
#endif
#ifdef INCLUDE_WUW
static const struct
{
    va_wuw_engine_t engine;
    unsigned        capture_ts_based_on_wuw_start_ts:1;
    uint16          max_pre_roll_in_ms;
    uint16          pre_roll_on_capture_in_ms;
    const char     *model;
} wuw_detection_start_table[] =
{
    {va_wuw_engine_qva, TRUE, 2000, 500, "tfd_0.bin"},
    {va_wuw_engine_gva, TRUE, 2000, 500, "gaa_model.bin"},
    {va_wuw_engine_apva, FALSE, 2000, 500, AMA_DEFAULT_LOCALE}
};


static struct
{
    unsigned         start_capture_on_detection:1;
    va_audio_codec_t encoder_for_capture_on_detection;
    va_wuw_engine_t  wuw_engine;
} va_config = {0};

static DataFileID vaTestLoadWuwModel(wuw_model_id_t model)
{
    return OperatorDataLoadEx(model, DATAFILE_BIN, STORAGE_INTERNAL, FALSE);
}

static bool vaTestPopulateWuwDetectionParams(va_wuw_engine_t engine, unsigned num_of_mics, va_audio_wuw_detection_params_t *params)
{
    bool status = FALSE;
    unsigned i;

    if (vaTestPopulateVaMicConfig(num_of_mics, &params->mic_config) == FALSE)
    {
        return FALSE;
    }

    for(i = 0; i < ARRAY_DIM(wuw_detection_start_table); i++)
    {
        if (wuw_detection_start_table[i].engine == engine)
        {
            FILE_INDEX model = FileFind(FILE_ROOT, wuw_detection_start_table[i].model, strlen(wuw_detection_start_table[i].model));
            if (model != FILE_NONE)
            {
                status = TRUE;
                if (params)
                {
                    params->wuw_config.engine = wuw_detection_start_table[i].engine;
                    params->wuw_config.model = model;
                    params->wuw_config.LoadWakeUpWordModel = vaTestLoadWuwModel;
                    params->max_pre_roll_in_ms = wuw_detection_start_table[i].max_pre_roll_in_ms;
                }
                break;
            }
        }
    }

    return status;
}

static bool vaTestPopulateStartCaptureTimeStamp(const va_audio_wuw_detection_info_t *wuw_info, uint32 *timestamp)
{
    bool status = FALSE;
    unsigned i;

    for(i = 0; i < ARRAY_DIM(wuw_detection_start_table); i++)
    {
        if (wuw_detection_start_table[i].engine == va_config.wuw_engine)
        {
            uint32 pre_roll = wuw_detection_start_table[i].pre_roll_on_capture_in_ms * 1000;
            status = TRUE;
            if (timestamp)
            {
                if (wuw_detection_start_table[i].capture_ts_based_on_wuw_start_ts)
                {
                    *timestamp = wuw_info->start_timestamp - pre_roll;
                }
                else
                {
                    *timestamp = wuw_info->end_timestamp - pre_roll;
                }
            }
            break;
        }
    }

    return status;
}

static va_audio_wuw_detected_response_t vaTestWuwDetected(const va_audio_wuw_detection_info_t *wuw_info)
{
    va_audio_wuw_detected_response_t response = {0};

    if (va_config.start_capture_on_detection &&
        vaTestPopulateVaEncodeConfig(va_config.encoder_for_capture_on_detection, &response.capture_params.encode_config) &&
        vaTestPopulateStartCaptureTimeStamp(wuw_info, &response.capture_params.start_timestamp))
    {
        response.start_capture = TRUE;
        response.capture_callback = vaTestDropDataInSource;
    }

    return response;
}

bool appTestStartVaWakeUpWordDetection(va_wuw_engine_t wuw_engine, unsigned num_of_mics, bool start_capture_on_detection, va_audio_codec_t encoder)
{
    va_audio_wuw_detection_params_t params = {0};

    if (vaTestPopulateWuwDetectionParams(wuw_engine, num_of_mics, &params) == FALSE)
    {
        return FALSE;
    }

    if (start_capture_on_detection && (vaTestPopulateVaEncodeConfig(encoder, NULL) == FALSE))
    {
        return FALSE;
    }

    va_config.start_capture_on_detection = start_capture_on_detection;
    va_config.encoder_for_capture_on_detection = encoder;
    va_config.wuw_engine = wuw_engine;

    return VoiceAudioManager_StartDetection(vaTestWuwDetected, &params);
}

bool appTestStopVaWakeUpWordDetection(void)
{
    return VoiceAudioManager_StopDetection();
}
#endif /* INCLUDE_WUW */
#endif /* DISABLE_TEST_API */
