/* 
 * FreeModbus Libary: A portable Modbus implementation for Modbus ASCII/RTU.
 * Copyright (C) 2013 Armink <armink.ztl@gmail.com>
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
 * File: $Id: mbfuncinput_m.c,v 1.60 2013/10/12 14:23:40 Armink Add Master Functions  Exp $
 */

/* ----------------------- System includes ----------------------------------*/
#include "stdlib.h"
#include "string.h"

/* ----------------------- Platform includes --------------------------------*/
#include "port.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb_m.h"
#include "mbframe.h"
#include "mbproto.h"
#include "mbconfig.h"
#include "mbfunc_m.h"
#include "mbutils_m.h"
#include "mbdict_m.h"
#include "mbscan_m.h"
#include "mbmap_m.h"

/* ----------------------- Defines ------------------------------------------*/
#define MB_PDU_REQ_READ_ADDR_OFF            ( MB_PDU_DATA_OFF + 0 )
#define MB_PDU_REQ_READ_REGCNT_OFF          ( MB_PDU_DATA_OFF + 2 )
#define MB_PDU_REQ_READ_SIZE                ( 4 )
#define MB_PDU_FUNC_READ_BYTECNT_OFF        ( MB_PDU_DATA_OFF + 0 )
#define MB_PDU_FUNC_READ_VALUES_OFF         ( MB_PDU_DATA_OFF + 1 )
#define MB_PDU_FUNC_READ_SIZE_MIN           ( 1 )

#define MB_PDU_FUNC_READ_RSP_BYTECNT_OFF    ( MB_PDU_DATA_OFF )

/* ----------------------- Start implementation -----------------------------*/
#if MB_MASTER_RTU_ENABLED > 0 || MB_MASTER_ASCII_ENABLED > 0

#if MB_FUNC_READ_INPUT_ENABLED > 0
 /***********************************************************************************
 * @brief  ????????????????????????
 * @param  ucSndAddr      ????????????
 * @param  usRegAddr      ????????????
 * @param  usNRegs        ??????
 * @param  lTimeOut       ?????????????????????0??????????????????
 * @return error          ?????????
 * @author laoc
 * @date 2019.01.22
 *************************************************************************************/
eMBMasterReqErrCode eMBMasterReqReadInputRegister(sMBMasterInfo* psMBMasterInfo, UCHAR ucSndAddr, USHORT usRegAddr, USHORT usNRegs, LONG lTimeOut)
{
    UCHAR  *pucMBFrame = NULL;
	OS_ERR err = OS_ERR_NONE;
    
    eMBMasterReqErrCode eErrStatus   = MB_MRE_NO_ERR;
	sMBMasterDevsInfo*  psMBDevsInfo = &psMBMasterInfo->sMBDevsInfo;    //?????????????????????
	sMBMasterPort*      psMBPort     = &psMBMasterInfo->sMBPort;      //????????????
    
    vMBMasterPortLock(psMBPort);
    
    if( (ucSndAddr < psMBDevsInfo->ucSlaveDevMinAddr) || (ucSndAddr > psMBDevsInfo->ucSlaveDevMaxAddr) ) 
	{
		eErrStatus = MB_MRE_ILL_ARG;
	}		
    else if ( xMBMasterRunResTake(lTimeOut) == FALSE ) 
	{
		eErrStatus = MB_MRE_MASTER_BUSY;
	}
    else
    {
		vMBMasterGetPDUSndBuf(psMBMasterInfo, &pucMBFrame);
		vMBMasterSetDestAddress(psMBMasterInfo, ucSndAddr);
		
		*(pucMBFrame + MB_PDU_FUNC_OFF)                = MB_FUNC_READ_INPUT_REGISTER;
		*(pucMBFrame + MB_PDU_REQ_READ_ADDR_OFF)       = usRegAddr >> 8;
		*(pucMBFrame + MB_PDU_REQ_READ_ADDR_OFF + 1)   = usRegAddr;
		*(pucMBFrame + MB_PDU_REQ_READ_REGCNT_OFF)     = usNRegs >> 8;
		*(pucMBFrame + MB_PDU_REQ_READ_REGCNT_OFF + 1) = usNRegs;
		 
		vMBMasterSetPDUSndLength( psMBMasterInfo, MB_PDU_SIZE_MIN + MB_PDU_REQ_READ_SIZE );
        
		(void)xMBMasterPortEventPost(psMBPort, EV_MASTER_FRAME_SENT);
		eErrStatus = eMBMasterWaitRequestFinish(psMBPort);
    }
    return eErrStatus;
}

/***********************************************************************************
 * @brief  ??????????????????????????????
 * @param  pucFrame      Modbus???PDU?????????????????????
 * @param  usLen         ???????????????
 * @return eMBException  ?????????
 * @author laoc
 * @date 2019.01.22
 *************************************************************************************/
eMBException
eMBMasterFuncReadInputRegister( sMBMasterInfo* psMBMasterInfo, UCHAR * pucFrame, USHORT * usLen )
{
    UCHAR          *pucMBFrame;
    USHORT          usRegAddress;
    USHORT          usRegCount;

    eMBException    eStatus = MB_EX_NONE;
    eMBErrorCode    eRegStatus;

    /* If this request is broadcast, and it's read mode. This request don't need execute. */
	if ( xMBMasterRequestIsBroadcast(psMBMasterInfo) )
	{
		eStatus = MB_EX_NONE;
	}
	else if( *usLen >= MB_PDU_SIZE_MIN + MB_PDU_FUNC_READ_SIZE_MIN )
    {
		vMBMasterGetPDUSndBuf(psMBMasterInfo, &pucMBFrame);
        usRegAddress  = (USHORT)( pucMBFrame[MB_PDU_REQ_READ_ADDR_OFF] << 8 );
        usRegAddress |= (USHORT)( pucMBFrame[MB_PDU_REQ_READ_ADDR_OFF + 1] );
        usRegAddress++;

        usRegCount  = (USHORT)( pucMBFrame[MB_PDU_REQ_READ_REGCNT_OFF] << 8 );
        usRegCount |= (USHORT)( pucMBFrame[MB_PDU_REQ_READ_REGCNT_OFF + 1] );

        /* Check if the number of registers to read is valid. If not
         * return Modbus illegal data value exception.
         */
        if( (usRegCount >= 1) && (2 * usRegCount == pucFrame[MB_PDU_FUNC_READ_BYTECNT_OFF]) )
        {
            /* Make callback to fill the buffer. */
            eRegStatus = eMBMasterRegInputCB( psMBMasterInfo, &pucFrame[MB_PDU_FUNC_READ_VALUES_OFF], 
			                                  usRegAddress, usRegCount );
            /* If an error occured convert it into a Modbus exception. */
            if( eRegStatus != MB_ENOERR )
            {
                eStatus = prveMBMasterError2Exception(eRegStatus);
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

/**
 * Modbus master input register callback function.
 *
 * @param pucRegBuffer input register buffer
 * @param usAddress input register address
 * @param usNRegs input register number
 *
 * @return result
 */
eMBErrorCode eMBMasterRegInputCB( sMBMasterInfo* psMBMasterInfo, UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNRegs )
{
    USHORT          iRegIndex, n, usProtocolType, nSlaveTypes;
    USHORT          REG_INPUT_START, REG_INPUT_END;
    
	USHORT          usRegInValue;
	SHORT           sRegInValue;
	CHAR            cRegInValue;
    
	eMBErrorCode            eStatus = MB_ENOERR;
	sMasterRegInData*  pvRegInValue = NULL;
    
	sMBSlaveDev*      psMBSlaveDevCur = psMBMasterInfo->sMBDevsInfo.psMBSlaveDevCur ;     //???????????????
    sMBDevDataTable*   psMBRegInTable = &psMBSlaveDevCur->psDevCurData->sMBRegInTable;    //????????????????????????
    UCHAR                ucMBDestAddr = ucMBMasterGetDestAddr(psMBMasterInfo);         //?????????????????????
    
    if(psMBMasterInfo->eMBRunMode != STATE_SCAN_DEV) //????????????????????????
    {
        return MB_ENOERR;
    }	
    if(psMBSlaveDevCur->ucDevAddr != ucMBDestAddr) //????????????????????????????????????????????????????????????????????????????????????
    {
        psMBSlaveDevCur = psMBMasterGetDev(psMBMasterInfo, ucMBDestAddr);
        psMBMasterInfo->sMBDevsInfo.psMBSlaveDevCur = psMBSlaveDevCur;
        psMBRegInTable = &psMBSlaveDevCur->psDevCurData->sMBRegInTable;
    }
	if( (psMBRegInTable->pvDataBuf == NULL) || (psMBRegInTable->usDataCount == 0)) //????????????????????????0
	{
		return MB_ENOREG;
	}

	REG_INPUT_START = psMBRegInTable->usStartAddr;
    REG_INPUT_END = psMBRegInTable->usEndAddr;

    /* it already plus one in modbus function method. */
    usAddress--;
    if( (usAddress < REG_INPUT_START) || (usAddress + usNRegs -1 > REG_INPUT_END) )
    { 
        eStatus = MB_ENOREG;
    }

    iRegIndex = usAddress;
    while (usNRegs > 0)
    {
        (void)eMBMasterRegInMap(psMBMasterInfo, ucMBDestAddr, iRegIndex, &pvRegInValue);    //????????????
    	
    	usRegInValue = ( (USHORT)(*pucRegBuffer++) ) << 8;
    	usRegInValue |=( (USHORT)(*pucRegBuffer++) ) & 0xFF;
    
        if( (pvRegInValue != NULL) && (pvRegInValue->pvValue != NULL) && (pvRegInValue->ucAccessMode != WO) )
    	{
    	    if( (pvRegInValue->fTransmitMultiple != 0) && (pvRegInValue->fTransmitMultiple != 1))
    	    {
    	    	usRegInValue = (USHORT)((float)usRegInValue / (float)pvRegInValue->fTransmitMultiple);      //????????????
    	    }
    		if (pvRegInValue->ucDataType == uint16)
    		{
    			if( (usRegInValue >= pvRegInValue->lMinVal) && (usRegInValue <= pvRegInValue->lMaxVal) )
    			{
    				*(USHORT*)pvRegInValue->pvValue  = (USHORT)usRegInValue;
    			}					
    		}
    		else if(pvRegInValue->ucDataType == uint8)
    		{			
    			if( (usRegInValue >= pvRegInValue->lMinVal) && (usRegInValue <= pvRegInValue->lMaxVal) )
    			{
    				*(UCHAR*)pvRegInValue->pvValue = (UCHAR)usRegInValue;
    			}
    		}
    		else if(pvRegInValue->ucDataType == int16)
    		{	
    			sRegInValue = (SHORT)usRegInValue;
    			if( ( usRegInValue >= (SHORT)pvRegInValue->lMinVal) && ( usRegInValue <= (SHORT)pvRegInValue->lMaxVal) )
    			{		
    				*(SHORT*)pvRegInValue->pvValue = (SHORT)sRegInValue ;			   
    			}
    		}
            else if(pvRegInValue->ucDataType == int8)
    		{	
                cRegInValue = (int8_t)usRegInValue;	
    			if( (cRegInValue >= (int8_t)pvRegInValue->lMinVal) && (cRegInValue <= (int8_t)pvRegInValue->lMaxVal) )
    			{		
    				*(CHAR*)pvRegInValue->pvValue = (int8_t)cRegInValue;				   
    			}
    		}
    	}
        iRegIndex++;
        usNRegs--;
    }
    return eStatus;
}

#endif

#endif
