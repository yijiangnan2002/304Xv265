/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       earbud_soc_config.c
\brief     voltage-> percentage configuration table

    This file contains voltage->percentage configuration table which maps different voltages to
    percentage values.
*/

#include "earbud_soc_config.h"

/*! \brief voltage->percentage config table*/
const soc_lookup_t earbud_soc_config_table[] =
{
    {3000, 0},
    {3012, 1},
    {3024, 2},
    {3036, 3},
    {3048, 4},
    {3060, 5},
    {3072, 6},
    {3084, 7},
    {3096, 8},
    {3108, 9},
    {3120, 10},
    {3132, 11},
    {3144, 12},
    {3156, 13},
    {3168, 14},
    {3180, 15},
    {3192, 16},
    {3204, 17},
    {3216, 18},
    {3228, 19},
    {3240, 20},
    {3252, 21},
    {3264, 22},
    {3276, 23},
    {3288, 24},
    {3300, 25},
    {3312, 26},
    {3324, 27},
    {3336, 28},
    {3348, 29},
    {3360, 30},
    {3372, 31},
    {3384, 32},
    {3396, 33},
    {3408, 34},
    {3420, 35},
    {3432, 36},
    {3444, 37},
    {3456, 38},
    {3468, 39},
    {3480, 40},
    {3492, 41},
    {3504, 42},
    {3516, 43},
    {3528, 44},
    {3540, 45},
    {3552, 46},
    {3564, 47},
    {3576, 48},
    {3588, 49},
    {3600, 50},
    {3612, 51},
    {3624, 52},
    {3636, 53},
    {3648, 54},
    {3660, 55},
    {3672, 56},
    {3684, 57},
    {3696, 58},
    {3708, 59},
    {3720, 60},
    {3732, 61},
    {3744, 62},
    {3756, 63},
    {3768, 64},
    {3780, 65},
    {3792, 66},
    {3804, 67},
    {3816, 68},
    {3823, 69},
    {3840, 70},
    {3852, 71},
    {3864, 72},
    {3876, 73},
    {3888, 74},
    {3900, 75},
    {3912, 76},
    {3924, 77},
    {3936, 78},
    {3948, 79},
    {3960, 80},
    {3972, 81},
    {3984, 82},
    {3996, 83},
    {4008, 84},
    {4020, 85},
    {4032, 86},
    {4044, 87},
    {4056, 88},
    {4068, 89},
    {4080, 90},
    {4092, 91},
    {4104, 92},
    {4116, 93},
    {4128, 94},
    {4140, 95},
    {4152, 96},
    {4164, 97},
    {4176, 98},
    {4188, 99},
    {4200, 100}
};

const soc_lookup_t* EarbudSoC_GetConfigTable(unsigned* table_length)
{
    *table_length = ARRAY_DIM(earbud_soc_config_table);
    return earbud_soc_config_table;
}

