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
 * File: $Id: mbrtu_m.c,v 1.60 2013/08/20 11:18:10 Armink Add Master Functions $
 */

/* ----------------------- System includes ----------------------------------*/
#include "stdlib.h"
#include "string.h"
#include "os.h"

/* ----------------------- Platform includes --------------------------------*/
#include "port.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb_m.h"
#include "mbconfig.h"
#include "mbframe.h"
#include "mbproto.h"
#include "mbfunc_m.h"
#include "mbdict_m.h"
#include "mbscan_m.h"
#include "mbtest_m.h"
#include "mbmap_m.h"

#if MB_MASTER_RTU_ENABLED == 1
#include "mbrtu_m.h"
#endif
#if MB_MASTER_ASCII_ENABLED == 1
#include "mbascii_m.h"
#endif
#if MB_MASTER_TCP_ENABLED == 1
#include "mbtcp_m.h"
#endif

#if MB_MASTER_RTU_ENABLED > 0 || MB_MASTER_ASCII_ENABLED > 0

#ifndef MB_PORT_HAS_CLOSE
#define MB_PORT_HAS_CLOSE 0
#endif

#define TMR_TICK_PER_SECOND                  OS_CFG_TMR_TASK_RATE_HZ
#define MB_MASTER_POLL_INTERVAL_MS           50

/* ----------------------- Static variables ---------------------------------*/
static sMBMasterInfo*      psMBMasterList = NULL;    

/* Functions pointer which are initialized in eMBInit( ). Depending on the
 * mode (RTU or ASCII) the are set to the correct implementations.
 * Using for Modbus Master,Add by Armink 20130813
 */
static peMBMasterFrameSend     peMBMasterFrameSendCur;
static pvMBMasterFrameStart    pvMBMasterFrameStartCur;
static pvMBMasterFrameStop     pvMBMasterFrameStopCur;
static peMBMasterFrameReceive  peMBMasterFrameReceiveCur;
static pvMBMasterFrameClose    pvMBMasterFrameCloseCur;

/* Callback functions required by the porting layer. They are called when
 * an external event has happend which includes a timeout or the reception
 * or transmission of a character.
 * Using for Modbus Master,Add by Armink 20130813
 */
pxMBMasterFrameCBByteReceived      pxMBMasterFrameCBByteReceivedCur;
pxMBMasterFrameCBTransmitterEmpty  pxMBMasterFrameCBTransmitterEmptyCur ;
pxMBMasterFrameCBTimerExpired      pxMBMasterFrameCBTimerExpiredCur;

pvMBMasterFrameReceiveCallback     pvMBMasterReceiveCallback;
pvMBMasterFrameSendCallback        pvMBMasterSendCallback;


/* An array of Modbus functions handlers which associates Modbus function
 * codes with implementing functions.
 */
static xMBMasterFunctionHandler xMasterFuncHandlers[MB_FUNC_HANDLERS_MAX] = {
#if MB_FUNC_OTHER_REP_SLAVEID_ENABLED > 0
	//TODO Add Master function define
    {MB_FUNC_OTHER_REPORT_SLAVEID, eMBFuncReportSlaveID},
#endif
#if MB_FUNC_READ_INPUT_ENABLED > 0
    {MB_FUNC_READ_INPUT_REGISTER, eMBMasterFuncReadInputRegister},
#endif
#if MB_FUNC_READ_HOLDING_ENABLED > 0
    {MB_FUNC_READ_HOLDING_REGISTER, eMBMasterFuncReadHoldingRegister},
#endif
#if MB_FUNC_WRITE_MULTIPLE_HOLDING_ENABLED > 0
    {MB_FUNC_WRITE_MULTIPLE_REGISTERS, eMBMasterFuncWriteMultipleHoldingRegister},
#endif
#if MB_FUNC_WRITE_HOLDING_ENABLED > 0
    {MB_FUNC_WRITE_REGISTER, eMBMasterFuncWriteHoldingRegister},
#endif
#if MB_FUNC_READWRITE_HOLDING_ENABLED > 0
    {MB_FUNC_READWRITE_MULTIPLE_REGISTERS, eMBMasterFuncReadWriteMultipleHoldingRegister},
#endif
#if MB_FUNC_READ_COILS_ENABLED > 0
    {MB_FUNC_READ_COILS, eMBMasterFuncReadCoils},
#endif
#if MB_FUNC_WRITE_COIL_ENABLED > 0
    {MB_FUNC_WRITE_SINGLE_COIL, eMBMasterFuncWriteCoil},
#endif
#if MB_FUNC_WRITE_MULTIPLE_COILS_ENABLED > 0
    {MB_FUNC_WRITE_MULTIPLE_COILS, eMBMasterFuncWriteMultipleCoils},
#endif
#if MB_FUNC_READ_DISCRETE_INPUTS_ENABLED > 0
    {MB_FUNC_READ_DISCRETE_INPUTS, eMBMasterFuncReadDiscreteInputs},
#endif
};

/* ----------------------- Start implementation -----------------------------*/

/**********************************************************************
 * @brief  MODBUS??????????????????
 * @param  psMBMasterInfo  ???????????????
 * @return eMBErrorCode    ?????????
 * @author laoc
 * @date 2019.01.22
 *********************************************************************/
eMBErrorCode eMBMasterInit(sMBMasterInfo* psMBMasterInfo)
{
    eMBErrorCode       eStatus = MB_ENOERR;
    sMBMasterPort* psMBPort= &psMBMasterInfo->sMBPort;
	
	switch(psMBMasterInfo->eMode)
	{
#if MB_MASTER_RTU_ENABLED > 0
	case MB_RTU:
		pvMBMasterFrameStartCur   = eMBMasterRTUStart;
		pvMBMasterFrameStopCur    = eMBMasterRTUStop;
		peMBMasterFrameSendCur    = eMBMasterRTUSend;
		peMBMasterFrameReceiveCur = eMBMasterRTUReceive;
		pvMBMasterFrameCloseCur   = MB_PORT_HAS_CLOSE ? vMBMasterPortClose : NULL;
	
		pxMBMasterFrameCBByteReceivedCur     = xMBMasterRTUReceiveFSM;
		pxMBMasterFrameCBTransmitterEmptyCur = xMBMasterRTUTransmitFSM;
		pxMBMasterFrameCBTimerExpiredCur     = xMBMasterRTUTimerT35Expired;

		eStatus = eMBMasterRTUInit(psMBMasterInfo);
	
		break;
#endif
    
#if MB_MASTER_ASCII_ENABLED > 0
    case MB_ASCII:
		pvMBMasterFrameStartCur   = eMBMasterASCIIStart;
		pvMBMasterFrameStopCur    = eMBMasterASCIIStop;
		peMBMasterFrameSendCur    = eMBMasterASCIISend;
		peMBMasterFrameReceiveCur = eMBMasterASCIIReceive;
		pvMBMasterFrameCloseCur   = MB_PORT_HAS_CLOSE ? vMBMasterPortClose : NULL;
    
		pxMBMasterFrameCBByteReceived     = xMBMasterASCIIReceiveFSM;
		pxMBMasterFrameCBTransmitterEmpty = xMBMasterASCIITransmitFSM;
		pxMBMasterPortCBTimerExpired      = xMBMasterASCIITimerT1SExpired;

		eStatus = eMBMasterASCIIInit(ucPort, ulBaudRate, eParity );
		break;
#endif
	default:
		eStatus = MB_EINVAL;
		break;
	}
	if (eStatus == MB_ENOERR)
	{
		if(xMBMasterPortEventInit(psMBPort) == FALSE)
		{
			/* port dependent event module initalization failed. */
			eStatus = MB_EPORTERR;
		}
		else
		{
			psMBMasterInfo->eMBState = STATE_DISABLED;
		}
		/* initialize the OS resource for modbus master. */
		vMBMasterOsResInit();
	}
	return eStatus;
}

/**********************************************************************
 * @brief  MODBUS???????????????
 * @param  psMBMasterInfo  ???????????????
 * @return eMBErrorCode    ?????????
 * @author laoc
 * @date 2019.01.22
 *********************************************************************/
eMBErrorCode eMBMasterClose(sMBMasterInfo* psMBMasterInfo)
{
    eMBErrorCode    eStatus = MB_ENOERR;
    sMBMasterPort* psMBPort = &psMBMasterInfo->sMBPort;

    if( psMBMasterInfo->eMBState == STATE_DISABLED )
    {
        if( pvMBMasterFrameCloseCur != NULL )
        {
            pvMBMasterFrameCloseCur(psMBPort);
        }
    }
    else
    {
        eStatus = MB_EILLSTATE;
    }
    return eStatus;
}

/**********************************************************************
 * @brief  MODBUS???????????????
 * @param  psMBMasterInfo  ???????????????
 * @return eMBErrorCode    ???????????????
 * @author laoc
 * @date 2019.01.22
 *********************************************************************/
eMBErrorCode eMBMasterEnable(sMBMasterInfo* psMBMasterInfo)
{
    eMBErrorCode   eStatus  = MB_ENOERR;
    OS_ERR         err     = OS_ERR_NONE;
    
    sMBMasterPort* psMBPort = &psMBMasterInfo->sMBPort;
    
    if( psMBMasterInfo->eMBState == STATE_DISABLED )
    {
        /* Activate the protocol stack. */
        pvMBMasterFrameStartCur(psMBMasterInfo);
        psMBMasterInfo->eMBState = STATE_ENABLED;
        (void)OSSemPost(&psMBPort->sMBIdleSem, OS_OPT_POST_ALL, &err);
    }
    else
    {
        eStatus = MB_EILLSTATE;
    }
    return eStatus;
}

/**********************************************************************
 * @brief  MODBUS???????????????
 * @param  psMBMasterInfo  ???????????????
 * @return eMBErrorCode    ???????????????
 * @author laoc
 * @date 2019.01.22
 *********************************************************************/
eMBErrorCode eMBMasterDisable(sMBMasterInfo* psMBMasterInfo)
{
    eMBErrorCode eStatus = MB_ENOERR;

    if( psMBMasterInfo->eMBState == STATE_ENABLED )
    {
        pvMBMasterFrameStopCur(psMBMasterInfo);
        psMBMasterInfo->eMBState = STATE_DISABLED;
        eStatus = MB_ENOERR;
    }
    else if( psMBMasterInfo->eMBState == STATE_DISABLED )
    {
        eStatus = MB_ENOERR;
    }
    else
    {
        eStatus = MB_EILLSTATE;
    }
    return eStatus;
}

/**********************************************************************
 * @brief  MODBUS???????????????
 *          1. ????????????????????????????????????eMBState?????????STATE_NOT_INITIALIZED???
 *             ???eMBInit()?????????????????????STATE_DISABLED,???eMBEnable?????????????????????STATE_ENABLE??
 *          2. ??????EV_FRAME_RECEIVED??????????????????EV_FRAME_RECEIVED???????????????
 *             ?????????????????????????????????EV_EXECUTE????????????????????????????????????(??????)???????????????????????
 * @param  psMBMasterInfo  ???????????????
 * @return eMBErrorCode    ???????????????
 * @author laoc
 * @date 2019.01.22
*********************************************************************/
eMBErrorCode eMBMasterPoll(sMBMasterInfo* psMBMasterInfo)
{
    static UCHAR   *pucMBFrame;
    static UCHAR    ucRcvAddress;
    static UCHAR    ucFunctionCode;
    static USHORT   usLength;
    static eMBException eException;

    USHORT i, j;
    USHORT  usAddr, usDataVal;
    
    eMBMasterEventType      eEvent;
    eMBMasterErrorEventType errorType;
    
    eMBErrorCode       eStatus      = MB_ENOERR;  
	sMBMasterPort*     psMBPort     = &psMBMasterInfo->sMBPort;   //????????????
	sMBMasterDevsInfo* psMBDevsInfo = &psMBMasterInfo->sMBDevsInfo;   //??????????????????
    UCHAR*             pcPDUCur     = NULL;
     
//    pucMBFrame = NULL;
    
    /* Check if the protocol stack is ready. */
    if(psMBMasterInfo->eMBState != STATE_ENABLED)
    {
        return MB_EILLSTATE;
    }
    /* Check if there is a event available. If not return control to caller.
     * Otherwise we will handle the event. */
    if( xMBMasterPortEventGet(psMBPort, &eEvent) == TRUE )
    {
//        myprintf("eMBMasterPoll\n");
        switch (eEvent)
        {
        case EV_MASTER_READY:
        break;

        case EV_MASTER_FRAME_RECEIVED:
            
			eStatus = peMBMasterFrameReceiveCur(psMBMasterInfo, &ucRcvAddress, &pucMBFrame, &usLength);  
			/* Check if the frame is for us. If not ,send an error process event. */
			if ( (eStatus == MB_ENOERR) && (ucRcvAddress == ucMBMasterGetDestAddr(psMBMasterInfo)) )
			{
                psMBMasterInfo->pucMasterPDUCur = pucMBFrame;
				(void) xMBMasterPortEventPost(psMBPort, EV_MASTER_EXECUTE);
			}
			else
			{
				vMBMasterSetErrorType(psMBMasterInfo, EV_ERROR_RECEIVE_DATA);
				(void) xMBMasterPortEventPost(psMBPort, EV_MASTER_ERROR_PROCESS);
			}
            
            if(pvMBMasterReceiveCallback != NULL)
            {
                pvMBMasterReceiveCallback((void*)psMBMasterInfo);
            }
        break;
          
        case EV_MASTER_EXECUTE:
            ucFunctionCode = *(pucMBFrame + MB_PDU_FUNC_OFF);
            eException = MB_EX_NONE;
            /* If receive frame has exception .The receive function code highest bit is 1.*/
            if(ucFunctionCode >> 7) 
			{
            	eException = (eMBException)( *(pucMBFrame + MB_PDU_DATA_OFF) );
            }
			else
			{
				for (i = 0; i < MB_FUNC_HANDLERS_MAX; i++)
				{
					/* No more function handlers registered. Abort. */
					if (xMasterFuncHandlers[i].ucFunctionCode == 0)	
					{
						break;
					}
					else if (xMasterFuncHandlers[i].ucFunctionCode == ucFunctionCode)
					{
						/* If master request is broadcast,
						 * the master need execute function for all slave.
						 */
						if(xMBMasterRequestIsBroadcast(psMBMasterInfo)) 
						{
							usLength = usMBMasterGetPDUSndLength(psMBMasterInfo);
							for(j = psMBDevsInfo->ucSlaveDevMinAddr; j <= psMBDevsInfo->ucSlaveDevMaxAddr; j++)
							{
								vMBMasterSetDestAddress(psMBMasterInfo, j);
								eException = xMasterFuncHandlers[i].pxHandler(psMBMasterInfo, pucMBFrame, &usLength);
							}
						}
						else 
						{
							eException = xMasterFuncHandlers[i].pxHandler(psMBMasterInfo, pucMBFrame, &usLength);
						}
						break;
					}
				}
			}
            /* If master has exception ,Master will send error process.Otherwise the Master is idle.*/
            if (eException != MB_EX_NONE) 
			{
            	vMBMasterSetErrorType(psMBMasterInfo, EV_ERROR_EXECUTE_FUNCTION);
            	(void) xMBMasterPortEventPost(psMBPort, EV_MASTER_ERROR_PROCESS );
            }
            else 
			{
            	vMBMasterCBRequestSuccess(psMBPort);
            	vMBMasterRunResRelease();
            }
        break;

        case EV_MASTER_FRAME_SENT:     //??????????????????
        	/* Master is busy now. */
        	vMBMasterGetPDUSndBuf( psMBMasterInfo, &pucMBFrame );
		
#if MB_MASTER_RTU_ENABLED > 0 || MB_MASTER_ASCII_ENABLED > 0		
			eStatus = peMBMasterFrameSendCur( psMBMasterInfo,ucMBMasterGetDestAddr(psMBMasterInfo), 
		                                      pucMBFrame, usMBMasterGetPDUSndLength(psMBMasterInfo) );    //???????????????     
#endif
            if(pvMBMasterSendCallback != NULL)
            {
                pvMBMasterSendCallback((void*)psMBMasterInfo);
            }
		break;

        case EV_MASTER_ERROR_PROCESS:    //??????????????????
        	/* Execute specified error process callback function. */
			errorType = eMBMasterGetErrorType(psMBMasterInfo);
			vMBMasterGetPDUSndBuf(psMBMasterInfo, &pucMBFrame);
			switch(errorType) 
			{
			    case EV_ERROR_RESPOND_TIMEOUT:    //????????????
			    	vMBMasterErrorCBRespondTimeout( psMBPort, ucMBMasterGetDestAddr(psMBMasterInfo),
			    			                        pucMBFrame, usMBMasterGetPDUSndLength(psMBMasterInfo));
			    	break;
			    case EV_ERROR_RECEIVE_DATA:      //??????????????????
			    	vMBMasterErrorCBReceiveData( psMBPort, ucMBMasterGetDestAddr(psMBMasterInfo), pucMBFrame, 
			                                     usMBMasterGetPDUSndLength(psMBMasterInfo));
			    	break;
			    case EV_ERROR_EXECUTE_FUNCTION:   //??????????????????
			    	vMBMasterErrorCBExecuteFunction( psMBPort, ucMBMasterGetDestAddr(psMBMasterInfo), pucMBFrame, 
			                                         usMBMasterGetPDUSndLength(psMBMasterInfo));
			    	break;
			    case EV_ERROR_RESPOND_DATA:      //????????????????????????
			    	vMBMasterErrorCBRespondData( psMBPort, ucMBMasterGetDestAddr(psMBMasterInfo), pucMBFrame, 
			                                     usMBMasterGetPDUSndLength(psMBMasterInfo));
			    	break;
			    default:break;
			}
			vMBMasterRunResRelease();
        break;	
		default:break;
        }
    }
    return MB_ENOERR;
}

/**********************************************************************
 * @brief  MODBUS????????????
 * @param  psMBMasterInfo  ???????????????   
 * @return BOOL   
 * @author laoc
 * @date 2019.01.22
 *********************************************************************/
BOOL xMBMasterRegistNode(sMBMasterInfo* psMBMasterInfo, sMBMasterNodeInfo* psMasterNode)
{
    OS_ERR err = OS_ERR_NONE;
    
    sMBMasterInfo*         psMBInfo   = NULL;
    sMBMasterPort*         psMBPort   = &psMBMasterInfo->sMBPort;      //????????????????????????
	sMBMasterTask*         psMBTask   = &psMBMasterInfo->sMBTask;      //???????????????????????????
    sMBMasterDevsInfo* psMBDevsInfo   = &psMBMasterInfo->sMBDevsInfo;  //?????????????????????
    
    if(psMBMasterInfo == NULL || psMasterNode == NULL)
    {
        return FALSE;
    }
	if((psMBInfo = psMBMasterFindNodeByPort(psMasterNode->pcMBPortName)) == NULL)
    {
        psMBMasterInfo->pNext = NULL;
        psMBMasterInfo->eMode = psMasterNode->eMode;
       
        /***************************??????????????????***************************/
        psMBPort = (sMBMasterPort*)(&psMBMasterInfo->sMBPort);
        if(psMBPort != NULL)
        {
            psMBPort->psMBMasterInfo = psMBMasterInfo;
            psMBPort->psMBMasterUart = psMasterNode->psMasterUart;
            psMBPort->pcMBPortName   = psMasterNode->pcMBPortName;
        }
        /***************************?????????????????????***************************/
        psMBDevsInfo = (sMBMasterDevsInfo*)(&psMBMasterInfo->sMBDevsInfo);
        if(psMBDevsInfo != NULL)
        {
            psMBDevsInfo->ucSlaveDevMinAddr = psMasterNode->ucMinAddr;
            psMBDevsInfo->ucSlaveDevMaxAddr = psMasterNode->ucMaxAddr;
        }
        /***************************??????????????????????????????***************************/
        psMBTask = (sMBMasterTask*)(&psMBMasterInfo->sMBTask);
        if(psMBTask != NULL)
        {
            psMBTask->ucMasterPollPrio = psMasterNode->ucMasterPollPrio;
            psMBTask->ucMasterScanPrio = psMasterNode->ucMasterScanPrio;
            
#if MB_MASTER_HEART_BEAT_ENABLED > 0
            psMBTask->ucMasterHeartBeatPrio = psMasterNode->ucMasterHeartBeatPrio;
#endif	 
        }
#if MB_MASTER_DTU_ENABLED > 0         
        /******************************GPRS??????????????????****************************/
        psMBMasterInfo->bDTUEnable = psMasterNode->bDTUEnable;
#endif   
        /*******************************???????????????????????????*************************/
        if(xMBMasterCreatePollTask(psMBMasterInfo) == FALSE)   
        {
            return FALSE;
        }
        /*******************************????????????????????????*************************/
        if(xMBMasterCreateScanSlaveDevTask(psMBMasterInfo) == FALSE)   
        {
            return FALSE;
        }
        /*******************************??????????????????????????????*************************/
        if(xMBMasterCreateDevHeartBeatTask(psMBMasterInfo) == FALSE)   
        {
            return FALSE;
        }
	    if(psMBMasterList == NULL)   //????????????
	    {
            psMBMasterList = psMBMasterInfo;
	    }
	    else if(psMBMasterList->pLast != NULL)
	    {
            psMBMasterList->pLast->pNext = psMBMasterInfo;
	    }
        psMBMasterList->pLast = psMBMasterInfo;
        
        return TRUE;
    }
    return FALSE;
}

/**********************************************************************
 * @brief  MODBUS??????????????????????????????
 * @param  pcMBPortName    ????????????
 * @return sMBMasterInfo   ???????????????
 * @author laoc
 * @date 2019.01.22
 *********************************************************************/
sMBMasterInfo* psMBMasterFindNodeByPort(const CHAR* pcMBPortName)
{
	sMBMasterInfo* psMBMasterInfo = NULL;
	
	for( psMBMasterInfo = psMBMasterList; psMBMasterInfo != NULL; psMBMasterInfo = psMBMasterInfo->pNext )
	{    
        if(strcmp(psMBMasterInfo->sMBPort.pcMBPortName, pcMBPortName) == 0)
        {
        	return psMBMasterInfo;
        }
	}
	return psMBMasterInfo;		
}

/**********************************************************************
 * @brief  MODBUS???????????????????????????
 * @param  psMBMasterInfo  ???????????????   
 * @return BOOL   
 * @author laoc
 * @date 2019.01.22
 *********************************************************************/
BOOL xMBMasterCreatePollTask(sMBMasterInfo* psMBMasterInfo)
{
    OS_ERR err = OS_ERR_NONE;
    
    CPU_STK_SIZE   stk_size = MB_MASTER_SCAN_TASK_STK_SIZE; 
    sMBMasterTask* psMBTask = &(psMBMasterInfo->sMBTask);
    
    OS_PRIO             prio = psMBTask->ucMasterPollPrio;
    OS_TCB*            p_tcb = (OS_TCB*)(&psMBTask->sMasterPollTCB);  
    CPU_STK*      p_stk_base = (CPU_STK*)(psMBTask->usMasterPollStk);
    
    OSTaskCreate(p_tcb, "vMBMasterPollTask", vMBMasterPollTask, (void*)psMBMasterInfo, prio, p_stk_base,
                 stk_size/10u, stk_size, 0u, 0u, 0u, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR ), &err);
    return (err == OS_ERR_NONE);
}

/**********************************************************************
 * @brief   ???????????????
 * @param   *p_arg    
 * @return	none
 * @author  laoc
 * @date    2019.01.22
 *********************************************************************/
void vMBMasterPollTask(void *p_arg)
{
	CPU_TS ts = 0;
	OS_ERR err = OS_ERR_NONE;
    
	eMBErrorCode  eStatus = MB_ENOERR;
	sMBMasterInfo* psMBMasterInfo = (sMBMasterInfo*)p_arg;
	
	if(eMBMasterInit(psMBMasterInfo) == MB_ENOERR)
	{
		if(eMBMasterEnable(psMBMasterInfo) == MB_ENOERR)
		{
			while (DEF_TRUE)
			{	
				(void)OSTimeDlyHMSM(0, 0, 0, MB_MASTER_POLL_INTERVAL_MS, OS_OPT_TIME_HMSM_STRICT, &err);
				(void)eMBMasterPoll(psMBMasterInfo);                     
			}
		}			
	}	
}

/**********************************************************************
 * @brief   ???????????????
 * @param   psMBMasterInfo  ???????????????
 * @param   Address         ???????????????   
 * @return	sMBSlaveDev 
 * @author  laoc
 * @date    2019.01.22
 *********************************************************************/
sMBSlaveDev* psMBMasterGetDev(const sMBMasterInfo* psMBMasterInfo, UCHAR ucDevAddr)
{
    sMBSlaveDev*  psMBDev = NULL;
    const sMBMasterDevsInfo* psMBDevsInfo = &(psMBMasterInfo->sMBDevsInfo);    //?????????????????????
    
    if( psMBDevsInfo->psMBSlaveDevsList == NULL)   //???????????????
    {
        return NULL;
    }
    else
    {
        for(psMBDev = psMBDevsInfo->psMBSlaveDevsList; psMBDev != NULL; psMBDev = psMBDev->pNext)
        {
            if(psMBDev->ucDevAddr == ucDevAddr)
            {
                return psMBDev;
            }
        }
    }
    return psMBDev;
}

/**********************************************************************
 * @brief   ???????????????????????????
 * @param   psMBMasterInfo  ??????????????? 
 * @param   Address         ??????????????? 
 * @return	sMBSlaveDev
 * @author  laoc
 * @date    2019.01.22
 *********************************************************************/
BOOL xMBMasterRegistDev(sMBMasterInfo* psMBMasterInfo, sMBSlaveDev* psMBNewDev)
{
    sMBSlaveDev*       psMBDev      = NULL;
    sMBMasterDevsInfo* psMBDevsInfo = NULL;
    
    if(psMBMasterInfo == NULL || psMBNewDev == NULL)
    {
        return FALSE;
    }
    psMBDevsInfo =&psMBMasterInfo->sMBDevsInfo;    //???????????????
    psMBNewDev->psMBMasterInfo = psMBMasterInfo;
    psMBNewDev->pNext = NULL;
    
    if(psMBDevsInfo->psMBSlaveDevsList == NULL)   //???????????????
    {
        psMBDevsInfo->psMBSlaveDevsList = psMBNewDev;
    }
    else //????????????
    {
        psMBDev = psMBDevsInfo->psMBSlaveDevsList->pLast;   //?????????
        psMBDev->pNext = psMBNewDev;
    }
    psMBDevsInfo->psMBSlaveDevsList->pLast = psMBNewDev;
    psMBDevsInfo->ucSlaveDevCount++;
   
    return TRUE;
}

/**********************************????????????******************************************/

/* Get whether the Modbus Master is run in master mode.*/
eMasterRunMode eMBMasterGetCBRunInMode(const sMBMasterInfo* psMBMasterInfo)
{
	return psMBMasterInfo->eMBRunMode;
}
/* Set whether the Modbus Master is run in master mode.*/
void vMBMasterSetCBRunInScanMode(sMBMasterInfo* psMBMasterInfo)
{
	psMBMasterInfo->eMBRunMode = STATE_SCAN_DEV;
}
/* Get Modbus Master send destination address. */
UCHAR ucMBMasterGetDestAddr( const sMBMasterInfo* psMBMasterInfo )
{
	return psMBMasterInfo->ucMBDestAddr;
}
/* Set Modbus Master send destination address. */
void vMBMasterSetDestAddress( sMBMasterInfo* psMBMasterInfo, UCHAR Address )
{
	psMBMasterInfo->ucMBDestAddr = Address;
}
/* Get Modbus Master current error event type. */
eMBMasterErrorEventType eMBMasterGetErrorType( const sMBMasterInfo* psMBMasterInfo )
{
	return psMBMasterInfo->eCurErrorType;
}
/* Set Modbus Master current error event type. */
void vMBMasterSetErrorType( sMBMasterInfo* psMBMasterInfo, eMBMasterErrorEventType errorType )
{
	psMBMasterInfo->eCurErrorType = errorType;
}

#endif
