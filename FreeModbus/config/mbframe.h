/* 
 * FreeModbus Libary: A portable Modbus implementation for Modbus ASCII/RTU.
 * Copyright (c) 2006 Christian Walter <wolti@sil.at>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * File: $Id: mbframe.h,v 1.9 2006/12/07 22:10:34 wolti Exp $
 */

#ifndef _MB_FRAME_H
#define _MB_FRAME_H

#include "stdint.h"
#include "os.h"

#ifdef __cplusplus
PR_BEGIN_EXTERN_C
#endif

/*!
 * Constants which defines the format of a modbus frame. The example is
 * shown for a Modbus RTU/ASCII frame. Note that the Modbus PDU is not
 * dependent on the underlying transport.
 *
 * <code>
 * <------------------------ MODBUS SERIAL LINE PDU (1) ------------------->
 *              <----------- MODBUS PDU (1') ---------------->
 *  +-----------+---------------+----------------------------+-------------+
 *  | Address   | Function Code | Data                       | CRC/LRC     |
 *  +-----------+---------------+----------------------------+-------------+
 *  |           |               |                                   |
 * (2)        (3/2')           (3')                                (4)
 *
 * (1)  ... MB_SER_PDU_SIZE_MAX = 256
 * (2)  ... MB_SER_PDU_ADDR_OFF = 0
 * (3)  ... MB_SER_PDU_PDU_OFF  = 1
 * (4)  ... MB_SER_PDU_SIZE_CRC = 2
 *
 * (1') ... MB_PDU_SIZE_MAX     = 253
 * (2') ... MB_PDU_FUNC_OFF     = 0
 * (3') ... MB_PDU_DATA_OFF     = 1
 * </code>
 */
/* ----------------------- Defines ------------------------------------------*/
#define MB_SER_PDU_SIZE_MIN     4       /*!< Minimum size of a Modbus RTU frame. */
#define MB_SER_PDU_SIZE_MAX     256     /*!< Maximum size of a Modbus RTU frame. */
#define MB_SER_PDU_SIZE_CRC     2       /*!< Size of CRC field in PDU. */
#define MB_SER_PDU_ADDR_OFF     0       /*!< Offset of slave address in Ser-PDU. */
#define MB_SER_PDU_PDU_OFF      1       /*!< Offset of Modbus-PDU in Ser-PDU. */

#define MB_PDU_SIZE_MAX          253 /*!< Maximum size of a PDU. */
#define MB_PDU_SIZE_MIN          1   /*!< Minimum size of a PDU. */
#define MB_PDU_FUNC_OFF          0   /*!< Offset of function code in PDU. */
#define MB_PDU_DATA_OFF          1   /*!< Offset for response data in PDU. */

#define MB_CPN_FRAME_SIZE_MIN         22      /*!< Minimum size of a Modbus CPN frame. */
#define MB_CPN_FRAME_SIZE_MAX         1022    /*!< Maximum size of a Modbus CPN frame. */
#define MB_CPN_PDU_SIZE_MAX           1004    /*!< Maximum size of a cpn PDU. */
#define MB_CPN_PDU_SIZE_MIN           4       /*!< Minimum size of a cpn PDU. */
#define MB_CPN_PDU_FUNC_OFF           0       /*!< Offset of cpn function code in cpn PDU. */
#define MB_CPN_PDU_DATA_OFF           4       /*!< Offset for response data in cpn PDU. */
#define MB_CPN_PDU_VALUE_COUNT_OFF    1       /*!< Offset of the count of values in cpn PDU. */


#define MB_CPN_VALUE_DEFINE_SIZE      8    /*!< The size of the defination of a CPN value. */
#define MB_CPN_VALUE_DATA_NAME_OFF    0    /*!< The offset of the name of CPN value. */
#define MB_CPN_VALUE_DATA_BYTES_OFF   4    /*!< The offset of the data bytes of CPN value. */
#define MB_CPN_VALUE_DATA_TYPE_OFF    6    /*!< The offset of the type of CPN value. */

#define MB_CPN_FUNC_CODE_OFF_TO_REAL    100  /*!< Offset of cpn function code to real code. */
#define MB_CPN_FLOAT_INT_MUX            10   /*!< The mux of the CPN float data to integer data. */

#define single          0x00
#define boolean         0x01
#define int8            0x02
#define int16           0x03
#define int32           0x04
#define uint8           0x05
#define uint16          0x06
#define uint32          0x07
#define real32          0x08
#define visible_string  0x09
#define octet_string    0x0A
#define unicode_string  0x0B
#define time_of_day     0x0C
#define time_difference 0x0D

#define domain          0x0F
#define int24           0x10
#define real64          0x11
#define int40           0x12
#define int48           0x13
#define int56           0x14
#define int64           0x15
#define uint24          0x16

#define uint40          0x18
#define uint48          0x19
#define uint56          0x1A
#define uint64          0x1B

#define CPN_UINT8        0x01
#define CPN_UINT16       0x02
#define CPN_UINT32       0x03
#define CPN_FLOAT        0x04

// Integers
#define INTEGER8 signed char
#define INTEGER16 short
#define INTEGER24
#define INTEGER32 long
#define INTEGER40
#define INTEGER48
#define INTEGER56
#define INTEGER64

// Unsigned integers
#define UNS8   unsigned char
#define UNS16  unsigned short
#define UNS32  unsigned long
	
#define VALUE_CHANGED   1
#define VALUE_SENDED    0

#ifndef RW
    #define RW 0 
#endif

#ifndef WO
    #define WO 1 
#endif

#ifndef RO
    #define RO 2 
#endif

typedef uint8_t BOOL;

typedef unsigned char UCHAR;
typedef char    CHAR;

typedef uint16_t USHORT;
typedef int16_t SHORT;

typedef uint32_t ULONG;
typedef int32_t LONG;

/*! \ingroup modbus
 * \brief Parity used for characters in serial mode.
 *
 * The parity which should be applied to the characters sent over the serial
 * link. Please note that this values are actually passed to the porting
 * layer and therefore not all parity modes might be available.
 */
typedef enum
{
    MB_PAR_NONE,                /*!< No parity. */
    MB_PAR_ODD,                 /*!< Odd parity. */
    MB_PAR_EVEN                 /*!< Even parity. */
} eMBParity;

typedef enum                      /* Modbus数据类型 */
{
  RegHoldData = 0,
  RegInputData ,
  CoilData ,
  DiscInData ,
  ValCPNData ,
}eDataType ;  

#ifdef __cplusplus
PR_END_EXTERN_C
#endif
#endif
