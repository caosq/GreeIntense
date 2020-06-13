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
 * File: $Id: mbfuncholding.c,v 1.12 2007/02/18 23:48:22 wolti Exp $
 */

/* ----------------------- System includes ----------------------------------*/
#include "stdlib.h"
#include "string.h"

/* ----------------------- Platform includes --------------------------------*/
#include "port.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbframe.h"
#include "mbproto.h"
#include "mbconfig.h"
#include "mbutils.h"
#include "mbdict.h"
#include "mbmap.h"

#include "system.h"

#if MB_SLAVE_RTU_ENABLED > 0 || MB_SLAVE_ASCII_ENABLED > 0 

/* ----------------------- Defines ------------------------------------------*/
#define MB_PDU_FUNC_READ_ADDR_OFF               ( MB_PDU_DATA_OFF + 0)
#define MB_PDU_FUNC_READ_REGCNT_OFF             ( MB_PDU_DATA_OFF + 2 )
#define MB_PDU_FUNC_READ_SIZE                   ( 4 )
#define MB_PDU_FUNC_READ_REGCNT_MAX             ( 0x007D )

#define MB_PDU_FUNC_WRITE_ADDR_OFF              ( MB_PDU_DATA_OFF + 0)
#define MB_PDU_FUNC_WRITE_VALUE_OFF             ( MB_PDU_DATA_OFF + 2 )
#define MB_PDU_FUNC_WRITE_SIZE                  ( 4 )

#define MB_PDU_FUNC_WRITE_MUL_ADDR_OFF          ( MB_PDU_DATA_OFF + 0 )
#define MB_PDU_FUNC_WRITE_MUL_REGCNT_OFF        ( MB_PDU_DATA_OFF + 2 )
#define MB_PDU_FUNC_WRITE_MUL_BYTECNT_OFF       ( MB_PDU_DATA_OFF + 4 )
#define MB_PDU_FUNC_WRITE_MUL_VALUES_OFF        ( MB_PDU_DATA_OFF + 5 )
#define MB_PDU_FUNC_WRITE_MUL_SIZE_MIN          ( 5 )
#define MB_PDU_FUNC_WRITE_MUL_REGCNT_MAX        ( 0x0078 )

#define MB_PDU_FUNC_READWRITE_READ_ADDR_OFF     ( MB_PDU_DATA_OFF + 0 )
#define MB_PDU_FUNC_READWRITE_READ_REGCNT_OFF   ( MB_PDU_DATA_OFF + 2 )
#define MB_PDU_FUNC_READWRITE_WRITE_ADDR_OFF    ( MB_PDU_DATA_OFF + 4 )
#define MB_PDU_FUNC_READWRITE_WRITE_REGCNT_OFF  ( MB_PDU_DATA_OFF + 6 )
#define MB_PDU_FUNC_READWRITE_BYTECNT_OFF       ( MB_PDU_DATA_OFF + 8 )
#define MB_PDU_FUNC_READWRITE_WRITE_VALUES_OFF  ( MB_PDU_DATA_OFF + 9 )
#define MB_PDU_FUNC_READWRITE_SIZE_MIN          ( 9 )


/* ----------------------- Start implementation -----------------------------*/

#if MB_FUNC_WRITE_HOLDING_ENABLED > 0
/***********************************************************************************
 * @brief 写单个保持寄存器功能函数
 * @param pucFrame       Modbus的PDU缓冲区数据指针
 * @param  usLen         缓冲区长度
 * @return eMBException  异常码
 * @author laoc
 * @date 2019.01.22
 *************************************************************************************/
eMBException 
eMBSlaveFuncWriteHoldingRegister(sMBSlaveInfo* psMBSlaveInfo, UCHAR* pucFrame, USHORT* usLen)
{
    USHORT          usRegAddress;
    eMBErrorCode    eRegStatus;
    eMBException    eStatus = MB_EX_NONE;
    
    if( *usLen == ( MB_PDU_FUNC_WRITE_SIZE + MB_PDU_SIZE_MIN ) )
    {
        usRegAddress  = (USHORT)( *(pucFrame + MB_PDU_FUNC_WRITE_ADDR_OFF) << 8 );
        usRegAddress |= (USHORT)( *(pucFrame + MB_PDU_FUNC_WRITE_ADDR_OFF + 1) );
        usRegAddress++;

        /* Make callback to update the value. */
        eRegStatus = eMBSlaveRegHoldingCB(psMBSlaveInfo, pucFrame + MB_PDU_FUNC_WRITE_VALUE_OFF,
                                      usRegAddress, 1, MB_REG_WRITE );

        /* If an error occured convert it into a Modbus exception. */
        if(eRegStatus != MB_ENOERR)
        {
            eStatus = prveMBSlaveError2Exception(eRegStatus);
        }
    }
    else
    {
        /* Can't be a valid request because the length is incorrect. */
        eStatus = MB_EX_ILLEGAL_DATA_VALUE;
    }
    return eStatus;
}
#endif

#if MB_FUNC_WRITE_MULTIPLE_HOLDING_ENABLED > 0
/***********************************************************************************
 * @brief 写多个保持寄存器功能函数
 * @param pucFrame       Modbus的PDU缓冲区数据指针
 * @param  usLen         缓冲区长度
 * @return eMBException  异常码
 * @author laoc
 * @date 2019.01.22
 *************************************************************************************/
eMBException 
eMBSlaveFuncWriteMultipleHoldingRegister(sMBSlaveInfo* psMBSlaveInfo, UCHAR* pucFrame, USHORT* usLen)
{
    USHORT          usRegAddress;
    USHORT          usRegCount;
    UCHAR           ucRegByteCount;
    eMBErrorCode    eRegStatus;
    
    eMBException    eStatus = MB_EX_NONE;

    if( *usLen >= ( MB_PDU_FUNC_WRITE_MUL_SIZE_MIN + MB_PDU_SIZE_MIN ) )
    {
        usRegAddress  = (USHORT)( *(pucFrame + MB_PDU_FUNC_WRITE_MUL_ADDR_OFF) << 8 );
        usRegAddress |= (USHORT)( *(pucFrame + MB_PDU_FUNC_WRITE_MUL_ADDR_OFF + 1) );
        usRegAddress++;

        usRegCount  = (USHORT)( *(pucFrame + MB_PDU_FUNC_WRITE_MUL_REGCNT_OFF) << 8 );
        usRegCount |= (USHORT)( *(pucFrame + MB_PDU_FUNC_WRITE_MUL_REGCNT_OFF + 1) );

        ucRegByteCount = *(pucFrame + MB_PDU_FUNC_WRITE_MUL_BYTECNT_OFF);
        
        if( (usRegCount >= 1) && (usRegCount <= MB_PDU_FUNC_WRITE_MUL_REGCNT_MAX) &&
            (ucRegByteCount == (UCHAR)(2*usRegCount)) )
        {
            /* Make callback to update the register values. */
            eRegStatus = eMBSlaveRegHoldingCB(psMBSlaveInfo, pucFrame + MB_PDU_FUNC_WRITE_MUL_VALUES_OFF,
                                               usRegAddress, usRegCount, MB_REG_WRITE );

            /* If an error occured convert it into a Modbus exception. */
            if(eRegStatus != MB_ENOERR)
            {
                eStatus = prveMBSlaveError2Exception(eRegStatus);
            }
            else
            {
                /* The response contains the function code, the starting
                 * address and the quantity of registers. We reuse the
                 * old values in the buffer because they are still valid.
                 */
                *usLen = MB_PDU_FUNC_WRITE_MUL_BYTECNT_OFF;
            }
        }
        else
        {
            eStatus = MB_EX_ILLEGAL_DATA_VALUE;
        }
    }
    else
    {
        /* Can't be a valid request because the length is incorrect. */
        eStatus = MB_EX_ILLEGAL_DATA_VALUE;
    }
    return eStatus;
}
#endif

#if MB_FUNC_READ_HOLDING_ENABLED > 0
/***********************************************************************************
 * @brief 读多个保持寄存器功能函数
 * @param pucFrame       Modbus的PDU缓冲区数据指针
 * @param  usLen         缓冲区长度
 * @return eMBException  异常码
 * @author laoc
 * @date 2019.01.22
 *************************************************************************************/
eMBException 
eMBSlaveFuncReadHoldingRegister(sMBSlaveInfo* psMBSlaveInfo, UCHAR* pucFrame, USHORT* usLen)
{
    USHORT          usRegAddress;
    USHORT          usRegCount;
    UCHAR          *pucFrameCur;
    eMBErrorCode    eRegStatus;
    
    eMBException    eStatus = MB_EX_NONE;
    
    if( *usLen == ( MB_PDU_FUNC_READ_SIZE + MB_PDU_SIZE_MIN ) )
    {
        usRegAddress  = (USHORT)( *(pucFrame + MB_PDU_FUNC_READ_ADDR_OFF) << 8 );
        usRegAddress |= (USHORT)( *(pucFrame + MB_PDU_FUNC_READ_ADDR_OFF + 1) );
        usRegAddress++;

        usRegCount = (USHORT)( *(pucFrame + MB_PDU_FUNC_READ_REGCNT_OFF) << 8 );
        usRegCount = (USHORT)( *(pucFrame + MB_PDU_FUNC_READ_REGCNT_OFF + 1) );

        /* Check if the number of registers to read is valid. If not
         * return Modbus illegal data value exception. 
         */
        if( (usRegCount >= 1) && (usRegCount <= MB_PDU_FUNC_READ_REGCNT_MAX) )
        {
            /* Set the current PDU data pointer to the beginning. */
            pucFrameCur = pucFrame + MB_PDU_FUNC_OFF;
            *usLen = MB_PDU_FUNC_OFF;

            /* First byte contains the function code. */
            *pucFrameCur++ = MB_FUNC_READ_HOLDING_REGISTER;
            *usLen += 1;

            /* Second byte in the response contain the number of bytes. */
            *pucFrameCur++ = (UCHAR)(usRegCount * 2);
            *usLen += 1;

            /* Make callback to fill the buffer. */
            eRegStatus = eMBSlaveRegHoldingCB(psMBSlaveInfo, pucFrameCur, usRegAddress, usRegCount, MB_REG_READ);
            
            /* If an error occured convert it into a Modbus exception. */
            if(eRegStatus != MB_ENOERR)
            {
                eStatus = prveMBSlaveError2Exception(eRegStatus);
            }
            else
            {
                *usLen += usRegCount * 2;
            }
        }
        else
        {
            eStatus = MB_EX_ILLEGAL_DATA_VALUE;
        }
    }
    else
    {
        /* Can't be a valid request because the length is incorrect. */
        eStatus = MB_EX_ILLEGAL_DATA_VALUE;
    }
    return eStatus;
}

#endif

#if MB_FUNC_READWRITE_HOLDING_ENABLED > 0
/***********************************************************************************
 * @brief 读写保持寄存器功能函数
 * @param pucFrame       Modbus的PDU缓冲区数据指针
 * @param  usLen         缓冲区长度
 * @return eMBException  异常码
 * @author laoc
 * @date 2019.01.22
 *************************************************************************************/
eMBException 
eMBSlaveFuncReadWriteMultipleHoldingRegister(sMBSlaveInfo* psMBSlaveInfo, UCHAR* pucFrame, USHORT* usLen )
{
    USHORT          usRegReadAddress;
    USHORT          usRegReadCount;
    USHORT          usRegWriteAddress;
    USHORT          usRegWriteCount;
    UCHAR           ucRegWriteByteCount;
    UCHAR          *pucFrameCur;
    eMBErrorCode    eRegStatus;
    
    eMBException    eStatus = MB_EX_NONE;
   
    if( *usLen >= (MB_PDU_FUNC_READWRITE_SIZE_MIN + MB_PDU_SIZE_MIN) )
    {
        usRegReadAddress = (USHORT)( *(pucFrame + MB_PDU_FUNC_READWRITE_READ_ADDR_OFF) << 8U );
        usRegReadAddress |= (USHORT)( *(pucFrame + MB_PDU_FUNC_READWRITE_READ_ADDR_OFF + 1) );
        usRegReadAddress++;

        usRegReadCount  = (USHORT)( *(pucFrame + MB_PDU_FUNC_READWRITE_READ_REGCNT_OFF) << 8U );
        usRegReadCount |= (USHORT)( *(pucFrame + MB_PDU_FUNC_READWRITE_READ_REGCNT_OFF + 1) );

        usRegWriteAddress  = (USHORT)( *(pucFrame + MB_PDU_FUNC_READWRITE_WRITE_ADDR_OFF) << 8U );
        usRegWriteAddress |= (USHORT)( *(pucFrame + MB_PDU_FUNC_READWRITE_WRITE_ADDR_OFF + 1) );
        usRegWriteAddress++;

        usRegWriteCount  = (USHORT)( *(pucFrame + MB_PDU_FUNC_READWRITE_WRITE_REGCNT_OFF) << 8U );
        usRegWriteCount |= (USHORT)( *(pucFrame + MB_PDU_FUNC_READWRITE_WRITE_REGCNT_OFF + 1) );

        ucRegWriteByteCount = *(pucFrame + MB_PDU_FUNC_READWRITE_BYTECNT_OFF);

        if( (usRegReadCount >= 1) && (usRegReadCount <= 0x7D) &&(usRegWriteCount >= 1) && (usRegWriteCount <= 0x79) &&
            (2*usRegWriteCount  == ucRegWriteByteCount) )
        {
            /* Make callback to update the register values. */
            eRegStatus = eMBSlaveRegHoldingCB(psMBSlaveInfo, pucFrame + MB_PDU_FUNC_READWRITE_WRITE_VALUES_OFF,
                                              usRegWriteAddress, usRegWriteCount, MB_REG_WRITE);
            if(eRegStatus == MB_ENOERR)
            {
                /* Set the current PDU data pointer to the beginning. */
                pucFrameCur = pucFrame + MB_PDU_FUNC_OFF;
                *usLen = MB_PDU_FUNC_OFF;

                /* First byte contains the function code. */
                *pucFrameCur++ = MB_FUNC_READWRITE_MULTIPLE_REGISTERS;
                *usLen += 1;

                /* Second byte in the response contain the number of bytes. */
                *pucFrameCur++ = (UCHAR)(usRegReadCount * 2);
                *usLen += 1;

                /* Make the read callback. */
                eRegStatus = eMBSlaveRegHoldingCB(psMBSlaveInfo, pucFrameCur, usRegReadAddress, usRegReadCount, MB_REG_READ);
                
                if(eRegStatus == MB_ENOERR)
                {
                    *usLen += 2*usRegReadCount;
                }
            }
            if(eRegStatus != MB_ENOERR)
            {
                eStatus = prveMBSlaveError2Exception(eRegStatus);
            }
        }
        else
        {
            eStatus = MB_EX_ILLEGAL_DATA_VALUE;
        }
    }
    return eStatus;
}
#endif

#if MB_FUNC_WRITE_HOLDING_ENABLED > 0 || MB_FUNC_WRITE_MULTIPLE_HOLDING_ENABLED > 0 \
    || MB_FUNC_READ_HOLDING_ENABLED > 0 || MB_FUNC_READWRITE_HOLDING_ENABLED > 0
/***********************************************************************************
 * @brief 保持寄存器回调函数（读、连续读、写、连续写）
 * @param pucRegBuffer  如果需要更新用户寄存器数值，这个缓冲区必须指向新的寄存器数值。
 *                      如果协议栈想知道当前的数值，回调函数必须将当前值写入这个缓冲区
 * 
 * @param usAddress     寄存器起始地址1~65535
 * @param usNRegs       寄存器数量
 * 
 * @param eMode         如果该参数为eMBRegisterMode::MB_REG_WRITE，用户的应用数值将从pucRegBuffer中得到更新。
 *                      如果该参数为eMBRegisterMode::MB_REG_READ，用户需要将当前的应用数据存储在pucRegBuffer中
 * 
 * @return eMBErrorCode 错误码
 * @author laoc
 * @date 2019.01.22
 *************************************************************************************/
eMBErrorCode eMBSlaveRegHoldingCB(sMBSlaveInfo* psMBSlaveInfo, UCHAR * pucRegBuffer, 
                                  USHORT usAddress, USHORT usNRegs, eMBRegisterMode eMode)
{
    USHORT          iRegIndex, REG_HOLDING_START, REG_HOLDING_END;

	USHORT          usRegHoldValue;
	SHORT           sRegHoldValue;
	int8_t          cRegHoldValue;
	
    OS_ERR            err = OS_ERR_NONE;
    eMBErrorCode  eStatus = MB_ENOERR;
    
    sMBSlaveRegData *      pvRegHoldValue = NULL;
	sMBSlaveDataTable*   psMBRegHoldTable = &psMBSlaveInfo->sMBCommInfo.psSlaveCurData->sMBRegHoldTable;  //从栈通讯协议表
    
    REG_HOLDING_START = psMBRegHoldTable->usStartAddr;
    REG_HOLDING_END = psMBRegHoldTable->usEndAddr;

    /* it already plus one in modbus function method. */
    usAddress--;

    if( (usAddress < REG_HOLDING_START) || (usAddress + usNRegs -1 > REG_HOLDING_END) )
    {
        return  MB_ENOREG;
    }
    
    iRegIndex = usAddress;
    switch (eMode)
    {
    /* read current register values from the protocol stack. */
    case MB_REG_READ:
        while (usNRegs > 0)
        {
            (void)eMBSlaveRegHoldMap(psMBSlaveInfo, iRegIndex, &pvRegHoldValue);
      
            if(pvRegHoldValue->ucAccessMode == WO)
            {
                return MB_ENOREG;
            }
            if( (pvRegHoldValue != NULL) && (pvRegHoldValue->pvValue != NULL))                        
            {	
                /* 根据数据类型进行指针转换，需要特别注意，转错会导致数值异常*/
                if (pvRegHoldValue->ucDataType == uint16)   
                {
                	usRegHoldValue = (USHORT)(*(USHORT*)pvRegHoldValue->pvValue);
                }
                else if(pvRegHoldValue->ucDataType == uint8)
                {
                	usRegHoldValue = (USHORT)(*(UCHAR*)pvRegHoldValue->pvValue);
                }
                else if(pvRegHoldValue->ucDataType == int16)
                {
                	usRegHoldValue = (USHORT)(*(SHORT*)pvRegHoldValue->pvValue);
                }
                else if(pvRegHoldValue->ucDataType == int8)
                {
                	usRegHoldValue = (USHORT)(*(int8_t*)pvRegHoldValue->pvValue);
                }
                    
                if( (pvRegHoldValue->fTransmitMultiple != 0) && (pvRegHoldValue->fTransmitMultiple != 1) )
                {
                    usRegHoldValue *=  pvRegHoldValue->fTransmitMultiple;
                }                        
                *pucRegBuffer++ = (UCHAR)(usRegHoldValue >> 8);
            	*pucRegBuffer++ = (UCHAR)(usRegHoldValue & 0xFF);
            }
            else
            {
                *pucRegBuffer++ = 0;
            	*pucRegBuffer++ = 0;
            }
            iRegIndex++;
            usNRegs--;
            }
    break;
    
    /* write current register values with new values from the protocol stack. */
    case MB_REG_WRITE:
        while(usNRegs > 0)
        {
            (void)eMBSlaveRegHoldMap(psMBSlaveInfo, iRegIndex, &pvRegHoldValue); //扫描保持寄存器字典，取对应的点
            
            if(pvRegHoldValue->ucAccessMode == RO)
            {
                return MB_ENOREG;
            }
            usRegHoldValue  =((LONG)(*pucRegBuffer++)) << 8;
            usRegHoldValue |=((LONG)(*pucRegBuffer++)) & 0xFF;
            
            if( (pvRegHoldValue != NULL) && (pvRegHoldValue->pvValue != NULL) )
            {
                if( (pvRegHoldValue->fTransmitMultiple != 0) && (pvRegHoldValue->fTransmitMultiple != 1))
                {
                    usRegHoldValue /= pvRegHoldValue->fTransmitMultiple;
                }
                if (pvRegHoldValue->ucDataType == uint16)
                {
                    if( (usRegHoldValue >= pvRegHoldValue->lMinVal ) && (usRegHoldValue <= pvRegHoldValue->lMaxVal) )	
                    {		
                        if ( usRegHoldValue != *(USHORT*)pvRegHoldValue->pvValue ) //更新数据
                        {
                            *(USHORT*)pvRegHoldValue->pvValue = (USHORT)usRegHoldValue;	       
                        }	
                    }
                    else
                    {
                        return MB_EINVAL;
                    }		
                }
                else if(pvRegHoldValue->ucDataType == uint8)
                {				
                    if( (usRegHoldValue >= pvRegHoldValue->lMinVal) && (usRegHoldValue <= pvRegHoldValue->lMaxVal) )
                	 {
                	 	 if(usRegHoldValue != *(UCHAR*)pvRegHoldValue->pvValue)
                	 	 {
                             *(UCHAR*)pvRegHoldValue->pvValue = (UCHAR)usRegHoldValue;					
                	 	 }							
                	 }
                	 else
                	 {
                	     return MB_EINVAL;
                	 }
                }
                else if (pvRegHoldValue->ucDataType == int16)
                {
                    sRegHoldValue = (SHORT)usRegHoldValue;
                    if( (sRegHoldValue >= (SHORT)pvRegHoldValue->lMinVal ) && (sRegHoldValue <= (SHORT)pvRegHoldValue->lMaxVal) )	
                    {		
                        if(sRegHoldValue != *(SHORT*)pvRegHoldValue->pvValue)
                        {
                            *(SHORT*)pvRegHoldValue->pvValue = (SHORT)sRegHoldValue;							
                        }		
                    }
                    else
                    {
                    	return MB_EINVAL;
                    }		
                }
                else if(pvRegHoldValue->ucDataType == int8)
                {	
                    cRegHoldValue = (int8_t)usRegHoldValue;						
                    if( (cRegHoldValue >= (int8_t)pvRegHoldValue->lMinVal ) && (cRegHoldValue <= (int8_t)pvRegHoldValue->lMaxVal) )	
                    {
                        if(cRegHoldValue != *(int8_t*)pvRegHoldValue->pvValue)
                        {
                            *(int8_t*)pvRegHoldValue->pvValue = (int8_t)cRegHoldValue;				
                        }					
                    }
                    else
                    {
                        return MB_EINVAL;
                    }
                }	
            }
            iRegIndex++;
            usNRegs--;
        }
    break;
	default: break;
    }
    return eStatus;
}

#endif

#endif
