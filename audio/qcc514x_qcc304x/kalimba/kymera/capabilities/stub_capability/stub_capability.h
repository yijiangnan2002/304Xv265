/****************************************************************************
 * Copyright (c) 2014 - 2017 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \file  stub_capability.h
 * \ingroup capabilities
 *
 * Stub Capability public header file. <br>
 *
 */

#ifndef STUB_CAPABILITY_H
#define STUB_CAPABILITY_H


/** The capability data structure for stub capability */
extern const CAPABILITY_DATA stub_capability_cap_data;

/* Message handlers */
extern bool stub_capability_start(OPERATOR_DATA *op_data, void *message_data,
                                unsigned *response_id, void **response_data);

/** The data processing function for stub capability */
extern void stub_capability_process_data(OPERATOR_DATA*, TOUCHED_TERMINALS*);

#endif /* STUB_CAPABILITY_H */
