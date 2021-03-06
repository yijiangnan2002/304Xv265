/*!
\copyright  Copyright (c) 2014-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Utilities for working with byte arrays.

@{
*/

#ifndef BYTE_UTILS_H_
#define BYTE_UTILS_H_

#include <csrtypes.h>

#define MAKEWORD_HI_LO(msb, lsb) ((uint16)(((uint16)((uint8)((msb) & 0xff))) << 8 | ((uint8)((lsb) & 0xff))))
#define MAKEWORD(lsb, msb)       ((uint16)(((uint8)((lsb) & 0xff)) | ((uint16)((uint8)((msb) & 0xff))) << 8))
#define MAKELONG(lsw, msw)       ((uint32)(((uint16)((lsw) & 0xffff)) | ((uint32)((uint16)((msw) & 0xffff))) << 16))
#define LOWORD(l)                ((uint16)((l) & 0xffff))
#define HIWORD(l)                ((uint16)((l) >> 16))
#define LOBYTE(w)                ((uint8)((w) & 0xff))
#define HIBYTE(w)                ((uint8)((w) >> 8))

/* Return true is bit set in mask are set in word */
#define ByteUtilsAreBitsSet(word, mask) (((word) & (mask)) == (mask))
#define ByteUtilsAreBitsClear(word, mask) (((word) & (mask)) == 0)

uint16 ByteUtilsMemCpyToStream(uint8 *dst, const uint8 *src, uint16 size);
uint16 ByteUtilsMemCpyFromStream(uint8 *dst, const uint8 *src, uint16 size);
uint16 ByteUtilsMemCpy(uint8 *dst, uint16 dstIndex, const uint8 *src, uint16 srcIndex, uint16 size);
uint16 ByteUtilsMemCpy16(uint8 *dst, uint16 dstIndex, const uint16 *src, uint16 srcIndex, uint16 size);
uint16 ByteUtilsMemCpyPackString(uint16 *dst, const uint8 *src, uint16 size);
uint16 ByteUtilsMemCpyUnpackString(uint8 *dst, const uint16 *src, uint16 size);
uint16 ByteUtilsGetPackedStringLen(const uint16 *src, const uint16 max_len);

uint16 ByteUtilsSet1Byte(uint8 *dst, uint16 byteIndex, uint8 val);
uint16 ByteUtilsSet2Bytes(uint8 *dst, uint16 byteIndex, uint16 val);
uint16 ByteUtilsSet4Bytes(uint8 *dst, uint16 byteIndex, uint32 val);

uint8 ByteUtilsGet1ByteFromStream(const uint8 *src);
uint16 ByteUtilsGet2BytesFromStream(const uint8 *src);
uint32 ByteUtilsGet4BytesFromStream(const uint8 *src);

uint16 ByteUtilsGet1Byte(const uint8 *src, uint16 byteIndex, uint8 *val);
uint16 ByteUtilsGet2Bytes(const uint8 *src, uint16 byteIndex, uint16 *val);
uint16 ByteUtilsGet4Bytes(const uint8 *src, uint16 byteIndex, uint32 *val);

uint16 ByteUtilsStrLCpyUnpack(uint8 *dst, const uint16 *src, uint16 dstsize);

/*! Implementation of strnlen_s from C11, that is not yet available.

    The function returns the length of the string. If the string 
    exceeds the supplied max_length then max_length is returned.
    If src is NULL then zero is returned

    \param[in] string Pointer to string to find length of
    \param max_length Maximum allowed length of the supplied string

    \return the string length
 */
uint16 ByteUtilsStrnlen_S(const uint8 *string, uint16 max_length);

#endif /* BYTE_UTILS_H_ */

/* @} */
