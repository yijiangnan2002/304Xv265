/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       adk_test_va.h
\brief      Interface for specifc application testing functions
*/

#ifndef ADK_TEST_VA_H
#define ADK_TEST_VA_H

#include <va_audio_types.h>

void appTestVaTap(void);
void appTestVaDoubleTap(void);
void appTestVaPressAndHold(void);
void appTestVaRelease(void);
void appTestVaHeldRelease(void);
void appTestSetActiveVa2GAA(void);
void appTestSetActiveVa2AMA(void);
bool appTestStartVaCapture(va_audio_codec_t encoder, unsigned num_of_mics);
bool appTestStopVaCapture(void);
bool appTestIsVaAudioActive(void);
unsigned appTest_VaGetSelectedAssistant(void);
#ifdef INCLUDE_AMA
void appTestPrintAmaLocale(void);
void appTestSetAmaLocale(const char *locale);
#endif
#ifdef INCLUDE_WUW
bool appTestStartVaWakeUpWordDetection(va_wuw_engine_t wuw_engine, unsigned num_of_mics, bool, va_audio_codec_t encoder);
bool appTestStopVaWakeUpWordDetection(void);
#endif

#endif /* ADK_TEST_VA_H */

