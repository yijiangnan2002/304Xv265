/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   %%version */
/**
 * \file
 *
 * \defgroup sched sched
 * \ingroup core
 *
 * This is a shim header hiding client code from the details of what scheduler
 * we're actually compiling against.
 */

#ifndef SCHED_H_
#define SCHED_H_

#ifdef OS_OXYGOS
#include "sched_oxygen/sched_oxygen.h"
#elif defined(OS_FREERTOS)
#include "sched_freertos/sched_freertos.h"
#else
#include "sched_carlos/sched_carlos.h"
#endif



#endif /* SCHED_H_ */
