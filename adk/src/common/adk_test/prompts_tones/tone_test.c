/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of common tones specific testing functions.
*/

#include "tone_test.h"
#include "ui_tones.h"

#ifdef GC_SECTIONS
/* Move all functions in KEEP_PM section to ensure they are not removed during
 * garbage collection */
#pragma unitcodesection KEEP_PM
#endif

void ToneTest_Play(MessageId event)
{
    MessageSend(UiTones_GetUiTonesTask(), event, NULL);
}
