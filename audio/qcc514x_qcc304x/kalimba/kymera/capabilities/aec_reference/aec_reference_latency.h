/**
* Copyright (c) 2014 - 2020 Qualcomm Technologies International, Ltd.
 * \defgroup aec_reference_cap
 *
 * \file  aec_reference_latency.h
 *
 * AEC Reference Capability
 * \ingroup capabilities
 */
#ifndef AEC_REFERENCE_LATENCY_H
#define AEC_REFERENCE_LATENCY_H
#include "capabilities.h"
#include "cbops/cbops_c.h"

/****************************************************************************
Public Type Declarations
*/

typedef struct aec_latency_common
{
	unsigned jitter;	  	  /**< latency threshold in samples */
	unsigned block_size;      /**< frame period in samples */
	unsigned block_sync;      /**< signal that mic path has a frame */
	unsigned mic_data;        /**< amount of data in sent to mic (will wrap by one
                               * blocks size every time goes above block size) */
	unsigned speaker_data;    /**< amount of data in sent to ref (will wrap by one
                               * every time mic_data wraps */
	unsigned ref_delay;       /**< amount by which reference precedes mic */
    unsigned min_space;       /**< minimum space expected in each of REF/MIC buffers */

    unsigned rm_adjustment;   /** applied rate adjustment */

    unsigned speaker_drops;   /* counter showing number of samples dropped from ref output */
    unsigned speaker_inserts; /* counter showing number of samples inserted into ref output */
    unsigned speaker_delay;   /* current latency between ref and mic outputs, in samples */

}aec_latency_common;

typedef struct _latency_op{
    unsigned    index;          /**< index of mic master channel */
    unsigned    available;      /**< current available space in buffer */
    aec_latency_common *common; /**< pointer to aec latency structure */
    unsigned transfer_drops;    /* counter showing number of samples dropped because of lack of minimum space */
}latency_op;

/****************************************************************************
Public Function Declarations
*/

/**
 * create_mic_latency_op
 * \brief creates cbops latency operator for mic graph
 *
 * \param idx index of mic master channel
 * \param common Pointer to common aec_latency_common latency structure
 * \return pointer to created cbops latency operator, NULL if fails to create.
 */
cbops_op* create_mic_latency_op(unsigned idx, aec_latency_common *common);

/**
 * create_speaker_latency_op
 * \brief creates cbops latency operator for speaker graph
 *
 * \param idx index of speaker master channel
 * \param common Pointer to common aec_latency_common latency structure
 * \return pointer to created cbops latency operator, NULL if fails to create.
 */
cbops_op* create_speaker_latency_op(unsigned idx, aec_latency_common *common);

void aec_ref_purge_mics(cbops_graph *mic_graph,unsigned num_mics);
int aecref_calc_ref_rate(unsigned mic_rt,int mic_ra,unsigned spkr_rt,int spkr_ra);
int aecref_calc_sync_mic_rate(int spkr_ra, unsigned spkr_rt, unsigned mic_rt);
/****************************************************************************
Public Variable Declarations
*/

extern unsigned cbops_mic_latency_table[];
extern unsigned cbops_speaker_latency_table[];

/** The address of the function vector table. This is aliased in ASH */

#endif	// AEC_REFERENCE_LATENCY_H
