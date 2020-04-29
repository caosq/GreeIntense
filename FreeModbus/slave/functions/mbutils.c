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
 * File: $Id: mbutils.c,v 1.6 2007/02/18 23:49:07 wolti Exp $
 */

/* ----------------------- System includes ----------------------------------*/
#include "stdlib.h"
#include "string.h"

/* ----------------------- Platform includes --------------------------------*/
#include "port.h"

/* ----------------------- Modbus includes ----------------------------------*/

#include "mbproto.h"
#include "mbframe.h"
#include "mbutils.h"
#include "mbmap.h"


/* ----------------------- Defines ------------------------------------------*/
#define BITS_UCHAR      8U

/* ----------------------- Start implementation -----------------------------*/

/***********************************************************************************
 * @brief  线圈和离散量置位函数
 * @param  ucByteBuf     Modbus的PDU缓冲区数据指针
 * @param  usAddress     起始地址
 * @param  ucNBits       个数
 * @param  eDataType     数据类型（线圈、离散量）
 * @return eMBException  异常码
 * @author laoc
 * @date 2019.01.22
 *************************************************************************************/
eMBErrorCode eMBSlaveUtilSetBits(sMBSlaveInfo* psMBSlaveInfo, UCHAR* ucByteBuf, 
                                 USHORT usAddress, UCHAR ucNBits, eDataType eDataType)
{
    eMBErrorCode    eStatus = MB_ENOERR;
    USHORT          usNPreBits, iNReg, iBits, i;
    UCHAR*          pucValue; 
	UCHAR           ucBit;
	
    sMBSlaveBitData *    pucBitData = NULL;
	
	iNReg = (USHORT)(ucNBits / BITS_UCHAR) + 1;
    usNPreBits = (USHORT)(ucNBits % BITS_UCHAR);

	while (iNReg > 0)
	{
        if( (iNReg == 1) && (usNPreBits > 0) ) //未满8个bit
        {
            iBits = usNPreBits;
        }
        else    //超过8bits
        {
            iBits = BITS_UCHAR;
        }             
        for ( i = 0; i < iBits; i++ )
        {
            switch(eDataType)
            {	
#if MB_FUNC_READ_COILS_ENABLED > 0 || MB_FUNC_WRITE_COIL_ENABLED > 0 || MB_FUNC_WRITE_MULTIPLE_COILS_ENABLED > 0					
                case CoilData:		   
                    (void)eMBSlaveCoilsMap(psMBSlaveInfo, usAddress, &pucBitData);         //扫描线圈字典，找到对应的线圈
				break;
#endif
				   
#if MB_FUNC_READ_DISCRETE_INPUTS_ENABLED > 0
                case DiscInData:
                    (void)eMBSlaveDiscreteMap(psMBSlaveInfo, usAddress, &pucBitData);
                break;
#endif
                default:break;			
            }	
            if( (pucBitData != NULL) && (pucBitData->pvValue != NULL) && (pucBitData->ucAccessMode != RO) )
            {
                ucBit = (UCHAR)( ((*ucByteBuf) & (1 << i) ) >> i );   //取对应位的值
                if( *(UCHAR*)pucBitData->pvValue != ucBit )
                {		
                    (*(UCHAR*)(pucBitData->pvValue)) = (UCHAR)ucBit;			
                }	
            }
            else
            {
                eStatus = MB_ENOREG;
                return eStatus;
            }
            usAddress++;
		}
		iNReg--;
		ucByteBuf++;	
	}
    return eStatus; 
}


/***********************************************************************************
 * @brief  线圈和离散量获取函数
 * @param  ucByteBuf     Modbus的PDU缓冲区数据指针
 * @param  usAddress     起始地址
 * @param  ucNBits       个数
 * @param  eDataType     数据类型（线圈、离散量）
 * @return eMBException  异常码
 * @author laoc
 * @date 2019.01.22
 *************************************************************************************/
eMBErrorCode eMBSlaveUtilGetBits(sMBSlaveInfo* psMBSlaveInfo, UCHAR* ucByteBuf, 
                                 USHORT usAddress, UCHAR ucNBits, eDataType eDataType)
{
	eMBErrorCode    eStatus = MB_ENOERR;
    USHORT          usNPreBits, iNReg, iBits, i;
    UCHAR*          pucValue; 
	
	sMBSlaveBitData *    pucBitData = NULL;
	
	iNReg = (USHORT)(ucNBits / BITS_UCHAR) + 1;
    usNPreBits = (USHORT)(ucNBits % BITS_UCHAR);

	while (iNReg > 0)
	{
		*ucByteBuf = 0;
		if( (iNReg == 1) && ( usNPreBits > 0) ) //未满8个bit
        {
            iBits = usNPreBits;
        }
        else    //超过8bits
        {
            iBits = BITS_UCHAR;
        }
        for ( i = 0; i < iBits; i++ )
        {
            switch(eDataType)
            {					
#if MB_FUNC_READ_COILS_ENABLED > 0 || MB_FUNC_WRITE_COIL_ENABLED > 0 || MB_FUNC_WRITE_MULTIPLE_COILS_ENABLED > 0					
				case CoilData:		   
                    (void)eMBSlaveCoilsMap(psMBSlaveInfo, usAddress, &pucBitData);         //扫描线圈字典，找到对应的线圈
				break;
#endif
				   
#if MB_FUNC_READ_DISCRETE_INPUTS_ENABLED > 0
				case DiscInData:
                    (void)eMBSlaveDiscreteMap(psMBSlaveInfo, usAddress, &pucBitData);
                break;
#endif
                default:break;					
			}
            if( (pucBitData != NULL) && (pucBitData->pvValue != NULL) && (pucBitData->ucAccessMode != WO) )
            {
                if( (*(UCHAR*)pucBitData->pvValue) > 0)
                {
                    *ucByteBuf |= ( 1 << i );
                }			
            }
            else
            {
                eStatus = MB_ENOREG;
                return eStatus;
            }
            usAddress++;
        }
		iNReg--;
		ucByteBuf++;	
	}
    return eStatus;
}

/***********************************************************************************
 * @brief  错误代码转异常码
 * @param  eMBErrorCode  mb错误代码
 * @return eMBException  异常码
 * @author laoc
 * @date 2019.01.22
 *************************************************************************************/
eMBException prveMBSlaveError2Exception( eMBErrorCode eErrorCode )
{
    eMBException    eStatus;

    switch ( eErrorCode )
    {
        case MB_ENOERR:
            eStatus = MB_EX_NONE;
            break;

        case MB_ENOREG:
            eStatus = MB_EX_ILLEGAL_DATA_ADDRESS;
            break;

		case MB_EINVAL:
            eStatus = MB_EX_ILLEGAL_DATA_VALUE;
            break;
		
        case MB_ETIMEDOUT:
            eStatus = MB_EX_SLAVE_BUSY;
            break;

        default:
            eStatus = MB_EX_SLAVE_DEVICE_FAILURE;
            break;
    }

    return eStatus;
}
