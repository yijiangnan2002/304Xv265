/*!

Copyright (c) 2001 - 2016 Qualcomm Technologies International, Ltd.
  %%version

\file   types.h

\brief This file contains basic type definitions.

*/

#ifndef BLUESTACK__TYPES_H
#define BLUESTACK__TYPES_H

#ifdef OS_OXYGOS
#include "sched_oxygen/sched_oxygen_priority.h"
#endif


#ifdef BLUESTACK_TYPES_INT_TYPE_FILE
#define BLUESTACK_TYPES_DONE
#include BLUESTACK_TYPES_INT_TYPE_FILE
#endif /* BLUESTACK_TYPES_INT_TYPE_FILE */

#ifdef BLUESTACK_TYPES_INT_TYPE_DEFS
#define BLUESTACK_TYPES_DONE
BLUESTACK_TYPES_INT_TYPE_DEFS
#endif /* BLUESTACK_TYPES_INT_TYPE_DEFS */

#ifndef BLUESTACK_TYPES_DONE

#include <stdint.h>

#ifndef DONT_HAVE_LIMITS_H
#include <limits.h>
#endif /* ndef DONT_HAVE_LIMITS_H */

typedef uint8_t bool_t;
typedef uint32_t uint24_t;

#endif /* ndef BLUESTACK_TYPES_DONE */
#undef BLUESTACK_TYPES_DONE

typedef uint16_t phandle_t;

#ifndef HOSTTOOLS_BUILD
/* Opaque context passed in Bluestack primitives, typically used to hold the 
 * pointer passed to the application. Hence, it is defined as "int" - 
 * appropriate size to hold the pointer 
 * in the platforms (Kalimba and XAP) we're interested in.
 */
typedef unsigned int context_t;
#endif /* #ifndef HOSTTOOLS_BUILD */

#define INVALID_PHANDLE ((phandle_t)0xFFFF)
#ifdef OS_OXYGOS
/* The Oxygen scheduler has 24-bit-wide queue IDs, so we need to be able to
 * manually extend them at the scheduler interface */
#define PHANDLE_TO_QUEUEID(phdl) ((uint24_t)((phdl) | DEFAULT_PRIORITY << 16))
#define QUEUEID_TO_PHANDLE(qid) ((uint16_t)(qid & 0xffff))
#else
#define PHANDLE_TO_QUEUEID(phdl) (phdl)
#define QUEUEID_TO_PHANDLE(qid) (qid)
#endif
/*!
    Type used with InquiryGetPriority() and InquirySetPriority().
*/
typedef enum
{
    inquiry_normal_priority, /*!< Set inquiry priority to normal */
    inquiry_low_priority     /*!< Set inquiry priority to lower than that of ACL data */
} InquiryPriority;

#endif /* ndef BLUESTACK__TYPES_H */
