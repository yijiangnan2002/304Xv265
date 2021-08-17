/*
 * This file is autogenerated from api.xml by api_codegen.py
 *
 * Copyright (c) 2020 Qualcomm Technologies International, Ltd.
 */
#include "hydra/hydra_types.h"

const uint32 trap_version[3] = {10, 22, 0};

const uint32 trapset_bitmap[3] =
{
    (1 <<  2) | /* CHARGER */
    (1 <<  3) | /* FILE */
    (1 <<  4) | /* VOICE */
    (1 <<  5) | /* UART */
    (1 << 10) | /* AUDIO */
    (1 << 12) | /* KALIMBA */
    (1 << 13) | /* RFCOMM */
    (1 << 15) | /* BLUESTACK */
    (1 << 17) | /* OPERATOR */
    (1 << 19) | /* CHARGERMESSAGE */
    (1 << 20) | /* PSU */
    (1 << 21) | /* MICBIAS */
    (1 << 22) | /* STREAM */
    (1 << 24) | /* ATT */
    (1 << 28) | /* CORE */
    0U,
    (1 <<  0) | /* HOST */
    (1 <<  3) | /* LED */
    (1 <<  5) | /* USB */
    (1 <<  6) | /* USB_HUB */
    (1 << 10) | /* IICSTREAM */
    (1 << 11) | /* IIR */
    (1 << 13) | /* IIC */
    (1 << 14) | /* STATUS */
    (1 << 18) | /* IIR16BIT */
    (1 << 21) | /* IMAGEUPGRADE */
    (1 << 22) | /* CSB */
    (1 << 23) | /* AUDIO_MCLK */
    (1 << 24) | /* CRYPTO */
    (1 << 27) | /* BITSERIAL */
    (1 << 28) | /* AUDIO_ANC */
    (1 << 29) | /* WAKE_ON_AUDIO */
    (1 << 30) | /* PROFILE */
    (1 << 31) | /* BDADDR */
    0U,
    (1 <<  0) | /* MARSHAL */
    (1 <<  1) | /* RA_PARTITION */
    (1 <<  2) | /* ACL_CONTROL */
    (1 <<  3) | /* MIRRORING */
    (1 <<  4) | /* TEST2 */
    (1 <<  5) | /* FLASH_OPS */
    (1 <<  6) | /* DEBUG_PARTITION */
    (1 <<  7) | /* VSDM */
    (1 <<  8) | /* CHARGERCOMMS */
    (1 << 10) | /* LE_AUDIO */
    (1 << 11) | /* LE_AE */
    0U
};

const uint32 trapset_bitmap_length = 3;

