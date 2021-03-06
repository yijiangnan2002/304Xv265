/**
* Copyright (c) 2014 - 2017 Qualcomm Technologies International, Ltd.
* \file  cvc_receive_cap_asm.h
* \ingroup  capabilities
*
*  CVC receive
*
*/

#ifndef CVC_RCV_CAP_ASM_H
#define CVC_RCV_CAP_ASM_H

#include "cvc_recv_gen_asm.h" 
#include "cvc_recv_config.h"

/*Temporary macro for debug purposes. */
#ifndef DEBUG_MODE
  #define CALL_PANIC_OR_JUMP(x) jump x
#else
  #define CALL_PANIC_OR_JUMP(x) call $error 
#endif

/* Description for Operator Data Structure */

/* Set From Dynamic Loader */
.CONST   $CVC_RCV_CAP.ROOT.INST_ALLOC_PTR_FIELD          0*ADDR_PER_WORD;
.CONST   $CVC_RCV_CAP.ROOT.SCRATCH_ALLOC_PTR_FIELD       1*ADDR_PER_WORD;
.CONST   $CVC_RCV_CAP.ROOT.INPUT_STREAM_MAP_PTR_FIELD    2*ADDR_PER_WORD;
.CONST   $CVC_RCV_CAP.ROOT.OUTPUT_STREAM_MAP_PTR_FIELD   3*ADDR_PER_WORD;
.CONST   $CVC_RCV_CAP.ROOT.MODE_TABLE_PTR_FIELD          4*ADDR_PER_WORD;
.CONST   $CVC_RCV_CAP.ROOT.INIT_TABLE_PTR_FIELD          5*ADDR_PER_WORD;
.CONST   $CVC_RCV_CAP.ROOT.STATUS_TABLE_PTR_FIELD        6*ADDR_PER_WORD;
.CONST   $CVC_RCV_CAP.ROOT.MODULES_PTR_FIELD             7*ADDR_PER_WORD;
.CONST   $CVC_RCV_CAP.ROOT.PARAMS_PTR_FIELD              8*ADDR_PER_WORD;
.CONST   $CVC_RCV_CAP.ROOT.UPSAMPLE_PTR_FIELD            9*ADDR_PER_WORD;

/* Shared Variables (Set Before dynamic load) */
.CONST   $CVC_RCV_CAP.ROOT.CVCLIB_TABLE_FIELD           10*ADDR_PER_WORD;
.CONST   $CVC_RCV_CAP.ROOT.VAD_DCB_COEFFS_PTR_FIELD     11*ADDR_PER_WORD;
.CONST   $CVC_RCV_CAP.ROOT.OMS_CONFIG_PTR_FIELD         12*ADDR_PER_WORD;
.CONST   $CVC_RCV_CAP.ROOT.ANAL_FB_CONFIG_PTR_FIELD     13*ADDR_PER_WORD;
.CONST   $CVC_RCV_CAP.ROOT.SYNTH_FB_CONFIG_PTR_FIELD    14*ADDR_PER_WORD;
.CONST   $CVC_RCV_CAP.ROOT.FB_SPLIT_PTR_FIELD           15*ADDR_PER_WORD;
.CONST   $CVC_RCV_CAP.ROOT.CUR_MODE_PTR_FIELD           16*ADDR_PER_WORD;
.CONST   $CVC_RCV_CAP.ROOT.NDVC_SHARE_PTR_FIELD         17*ADDR_PER_WORD;

// Misc Control Data
.CONST  $CVC_RCV_CAP.ROOT.OP_ALL_CONNECTED              18*ADDR_PER_WORD;
.CONST  $CVC_RCV_CAP.ROOT.CAPABILITY_ID                 19*ADDR_PER_WORD;
.CONST  $CVC_RCV_CAP.ROOT.CUR_MODE                      20*ADDR_PER_WORD;
.CONST  $CVC_RCV_CAP.ROOT.HOST_MODE                     21*ADDR_PER_WORD;
.CONST  $CVC_RCV_CAP.ROOT.OBPM_MODE                     22*ADDR_PER_WORD;
.CONST  $CVC_RCV_CAP.ROOT.OVR_CONTROL                   23*ADDR_PER_WORD;
.CONST  $CVC_RCV_CAP.ROOT.ALGREINIT                     24*ADDR_PER_WORD;
.CONST  $CVC_RCV_CAP.ROOT.AGC_STATE                     25*ADDR_PER_WORD;
.CONST  $CVC_RCV_CAP.ROOT.DATA_VARIANT                  26*ADDR_PER_WORD;

.CONST   $CVC_RCV_CAP.ROOT.FRAME_SIZE_IN_FIELD          27*ADDR_PER_WORD;
.CONST   $CVC_RCV_CAP.ROOT.FRAME_SIZE_OUT_FIELD         28*ADDR_PER_WORD;
.CONST   $CVC_RCV_CAP.ROOT.SAMPLE_RATE_FIELD            29*ADDR_PER_WORD;

.CONST   $CVC_RCV_CAP.ROOT.PARMS_DEF                    30*ADDR_PER_WORD;
        
// Module Access Points
.CONST   $CVC_RCV_CAP.MODULE.AGC_PTR_FIELD              0*ADDR_PER_WORD;
.CONST   $CVC_RCV_CAP.MODULE.OMS_PTR_FIELD              1*ADDR_PER_WORD;
.CONST   $CVC_RCV_CAP.MODULE.HARM_PTR_FIELD             2*ADDR_PER_WORD;
.CONST   $CVC_RCV_CAP.MODULE.VAD_PTR_FIELD              3*ADDR_PER_WORD;
.CONST   $CVC_RCV_CAP.MODULE.AEQ_PTR_FIELD              4*ADDR_PER_WORD;
.CONST   $CVC_RCV_CAP.MODULE.RCVIN_PTR_FIELD            5*ADDR_PER_WORD;
.CONST   $CVC_RCV_CAP.MODULE.RCVOUT_PTR_FIELD           6*ADDR_PER_WORD;

// Capability Status
.CONST $CVC_RCV_CAP.STATUS.PEAK_DAC_OFFSET      		0*ADDR_PER_WORD; // auto clear
.CONST $CVC_RCV_CAP.STATUS.PEAK_SCO_IN_OFFSET   		1*ADDR_PER_WORD; // auto clear   
.CONST $CVC_RCV_CAP.STATUS.AEQ_GAIN_LOW         		2*ADDR_PER_WORD; 
.CONST $CVC_RCV_CAP.STATUS.AEQ_GAIN_HIGH        		3*ADDR_PER_WORD; 
.CONST $CVC_RCV_CAP.STATUS.AEQ_STATE            		4*ADDR_PER_WORD; 
.CONST $CVC_RCV_CAP.STATUS.AEQ_POWER_TEST       		5*ADDR_PER_WORD; 
.CONST $CVC_RCV_CAP.STATUS.AEQ_TONE_POWER       		6*ADDR_PER_WORD; 
.CONST $CVC_RCV_CAP.STATUS.VAD_RCV_DET_OFFSET           7*ADDR_PER_WORD; 
.CONST $CVC_RCV_CAP.STATUS.RCV_AGC_SPEECH_LVL   		8*ADDR_PER_WORD; 
.CONST $CVC_RCV_CAP.STATUS.RCV_AGC_GAIN         		9*ADDR_PER_WORD; 
.CONST $CVC_RCV_CAP.STATUS.BLOCK_SIZE                 10;
     
#define RCV_VARIANT_NB   0
#define RCV_VARIANT_WB   1
#define RCV_VARIANT_UWB  2
#define RCV_VARIANT_SWB  3
#define RCV_VARIANT_FB   4
#define RCV_VARIANT_BEX  5

#endif
