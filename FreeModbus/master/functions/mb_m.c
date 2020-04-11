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
#include "user_mb_dict_m.h"
#include "user_mb_scan_m.h"
#include "user_mb_map_m.h"

#if MB_MASTER_RTU_ENABLED == 1
#include "mbrtu_m.h"
#endif
#if MB_MASTER_ASCII_ENABLED == 1
#include "mbascii_m.h"
#endif
#if MB_MASTER_TCP_ENABLED == 1
#include "mbtcp_m.h"
#endif

#define TMR_TICK_PER_SECOND  OS_CFG_TMR_TASK_RATE_HZ
#define MB_MASTER_DEV_OFFLINE_TMR_S              30

#if MB_MASTER_RTU_ENABLED > 0 || MB_MASTER_ASCII_ENABLED > 0

#ifndef MB_PORT_HAS_CLOSE
#define MB_PORT_HAS_CLOSE 0
#endif

#define MB_MASTER_POLL_INTERVAL_MS              50

/* ----------------------- Static variables ---------------------------------*/
static sMBMasterNodeInfo*      sMBMasterNodeList = NULL;    

/* Functions pointer which are initialized in eMBInit( ). Depending on the
 * mode (RTU or ASCII) the are set to the correct implementations.
 * Using for Modbus Master,Add by Armink 20130813
 */
static peMBMasterFrameSend peMBMasterFrameSendCur;
static pvMBMasterFrameStart pvMBMasterFrameStartCur;
static pvMBMasterFrameStop pvMBMasterFrameStopCur;
static peMBMasterFrameReceive peMBMasterFrameReceiveCur;
static pvMBMasterFrameClose pvMBMasterFrameCloseCur;

/* Callback functions required by the porting layer. They are called when
 * an external event has happend which includes a timeout or the reception
 * or transmission of a character.
 * Using for Modbus Master,Add by Armink 20130813
 */
pxMBMasterFrameCBByteReceived pxMBMasterFrameCBByteReceivedCur;
pxMBMasterFrameCBTransmitterEmpty pxMBMasterFrameCBTransmitterEmptyCur ;
pxMBMasterFrameCBTimerExpired pxMBMasterFrameCBTimerExpiredCur;

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

/* ----------------------- static functions ---------------------------------*/
static void vMBMasterDevOfflineTimeout( void * p_tmr, void * p_arg);

/* ----------------------- Start implementation -----------------------------*/

/**********************************************************************
 * @brief  MODBUS协议栈初始化
 * @param  psMBMasterInfo  主栈信息块
 * @return eMBErrorCode    错误码
 * @author laoc
 * @date 2019.01.22
 *********************************************************************/
eMBErrorCode eMBMasterInit ( sMBMasterInfo* psMBMasterInfo)
{
    eMBErrorCode       eStatus = MB_ENOERR;
    sMBMasterPortInfo* psMBPortInfo= psMBMasterInfo->psMasterPortInfo;
	
	switch(psMBMasterInfo->eMode)
	{
#if MB_MASTER_RTU_ENABLED > 0
	case MB_RTU:
		pvMBMasterFrameStartCur = eMBMasterRTUStart;
		pvMBMasterFrameStopCur = eMBMasterRTUStop;
		peMBMasterFrameSendCur = eMBMasterRTUSend;
		peMBMasterFrameReceiveCur = eMBMasterRTUReceive;
		pvMBMasterFrameCloseCur = MB_PORT_HAS_CLOSE ? vMBMasterPortClose : NULL;
	
		pxMBMasterFrameCBByteReceivedCur = xMBMasterRTUReceiveFSM;
		pxMBMasterFrameCBTransmitterEmptyCur = xMBMasterRTUTransmitFSM;
		pxMBMasterFrameCBTimerExpiredCur = xMBMasterRTUTimerT35Expired;

		eStatus = eMBMasterRTUInit(psMBMasterInfo);
	
	    (void)eMBMasterTableInit(psMBMasterInfo);
	
		break;
#endif
#if MB_MASTER_ASCII_ENABLED > 0
		case MB_ASCII:
		pvMBMasterFrameStartCur = eMBMasterASCIIStart;
		pvMBMasterFrameStopCur = eMBMasterASCIIStop;
		peMBMasterFrameSendCur = eMBMasterASCIISend;
		peMBMasterFrameReceiveCur = eMBMasterASCIIReceive;
		pvMBMasterFrameCloseCur = MB_PORT_HAS_CLOSE ? vMBMasterPortClose : NULL;
		pxMBMasterFrameCBByteReceived = xMBMasterASCIIReceiveFSM;
		pxMBMasterFrameCBTransmitterEmpty = xMBMasterASCIITransmitFSM;
		pxMBMasterPortCBTimerExpired = xMBMasterASCIITimerT1SExpired;

		eStatus = eMBMasterASCIIInit(ucPort, ulBaudRate, eParity );
		break;
#endif
	default:
		eStatus = MB_EINVAL;
		break;
	}
	if (eStatus == MB_ENOERR)
	{
		if (!xMBMasterPortEventInit(psMBPortInfo))
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
 * @brief  MODBUS协议栈关闭
 * @param  psMBMasterInfo  主栈信息块
 * @return eMBErrorCode    错误码
 * @author laoc
 * @date 2019.01.22
 *********************************************************************/
eMBErrorCode eMBMasterClose( sMBMasterInfo* psMBMasterInfo )
{
    eMBErrorCode       eStatus = MB_ENOERR;
    sMBMasterPortInfo* psMBPortInfo = psMBMasterInfo->psMasterPortInfo;

    if( psMBMasterInfo->eMBState == STATE_DISABLED )
    {
        if( pvMBMasterFrameCloseCur != NULL )
        {
            pvMBMasterFrameCloseCur(psMBPortInfo);
        }
    }
    else
    {
        eStatus = MB_EILLSTATE;
    }
    return eStatus;
}

/**********************************************************************
 * @brief  MODBUS协议栈使能
 * @param  psMBMasterInfo  主栈信息块
 * @return eMBErrorCode    协议栈错误
 * @author laoc
 * @date 2019.01.22
 *********************************************************************/
eMBErrorCode eMBMasterEnable(sMBMasterInfo* psMBMasterInfo)
{
    eMBErrorCode    eStatus = MB_ENOERR;

    if( psMBMasterInfo->eMBState == STATE_DISABLED )
    {
        /* Activate the protocol stack. */
        pvMBMasterFrameStartCur(psMBMasterInfo);
        psMBMasterInfo->eMBState = STATE_ENABLED;
    }
    else
    {
        eStatus = MB_EILLSTATE;
    }
    return eStatus;
}

/**********************************************************************
 * @brief  MODBUS协议栈禁止
 * @param  psMBMasterInfo  主栈信息块
 * @return eMBErrorCode    协议栈错误
 * @author laoc
 * @date 2019.01.22
 *********************************************************************/
eMBErrorCode eMBMasterDisable(sMBMasterInfo* psMBMasterInfo)
{
    eMBErrorCode    eStatus;

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
 * @brief  MODBUS协议栈轮询
 *          1. 检查协议栈状态是否使能，eMBState初值为STATE_NOT_INITIALIZED，
 *             在eMBInit()函数中被赋值为STATE_DISABLED,在eMBEnable函数中被赋值为STATE_ENABLE;
 *          2. 轮询EV_FRAME_RECEIVED事件发生，若EV_FRAME_RECEIVED事件发生，
 *             接收一帧报文数据，上报EV_EXECUTE事件，解析一帧报文，响应(发送)一帧数据给主机;
 * @param  psMBMasterInfo  主栈信息块
 * @return eMBErrorCode    协议栈错误
 * @author laoc
 * @date 2019.01.22
 *********************************************************************/
eMBErrorCode eMBMasterPoll(sMBMasterInfo* psMBMasterInfo)
{
    static UCHAR   *ucMBFrame;
    static UCHAR    ucRcvAddress;
    static UCHAR    ucFunctionCode;
    static USHORT   usLength;
    static eMBException eException;

    int             i , j;
    eMBErrorCode    eStatus = MB_ENOERR;
    eMBMasterEventType    eEvent;
    eMBMasterErrorEventType errorType;

	sMBMasterPortInfo* psMBPortInfo = psMBMasterInfo->psMasterPortInfo;     //硬件结构
	sMBMasterDevsInfo* psMBDevsInfo = psMBMasterInfo->psMBMasterDevsInfo;   //从设备状态表
	
    /* Check if the protocol stack is ready. */
    if( psMBMasterInfo->eMBState != STATE_ENABLED )
    {
        return MB_EILLSTATE;
    }

    /* Check if there is a event available. If not return control to caller.
     * Otherwise we will handle the event. */
    if( xMBMasterPortEventGet( psMBPortInfo, &eEvent ) == TRUE )
    {
        switch ( eEvent )
        {
        case EV_MASTER_READY:
            break;

        case EV_MASTER_FRAME_RECEIVED:
			eStatus = peMBMasterFrameReceiveCur( psMBMasterInfo, &ucRcvAddress, &ucMBFrame, &usLength );
            psMBMasterInfo->pucMasterPDUCur = ucMBFrame;
        
			/* Check if the frame is for us. If not ,send an error process event. */
			if ( (eStatus == MB_ENOERR) && (ucRcvAddress == ucMBMasterGetDestAddress(psMBMasterInfo)) )
			{
				( void ) xMBMasterPortEventPost( psMBPortInfo, EV_MASTER_EXECUTE );
				
			}
			else
			{
				vMBMasterSetErrorType(psMBMasterInfo, EV_ERROR_RECEIVE_DATA);
				( void ) xMBMasterPortEventPost( psMBPortInfo, EV_MASTER_ERROR_PROCESS );
			}
			break;
          
        case EV_MASTER_EXECUTE:
            ucFunctionCode = *(ucMBFrame + MB_PDU_FUNC_OFF);
            eException = MB_EX_ILLEGAL_FUNCTION;
            /* If receive frame has exception .The receive function code highest bit is 1.*/
            if(ucFunctionCode >> 7) 
			{
            	eException = (eMBException)( *(ucMBFrame + MB_PDU_DATA_OFF) );
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
						vMBMasterSetCBRunInMasterMode(psMBMasterInfo, TRUE);
						/* If master request is broadcast,
						 * the master need execute function for all slave.
						 */
						if ( xMBMasterRequestIsBroadcast(psMBMasterInfo) ) 
						{
							usLength = usMBMasterGetPDUSndLength(psMBMasterInfo);
							for(j = psMBDevsInfo->ucSlaveMinAddr; j <= psMBDevsInfo->ucSlaveMaxAddr; j++)
							{
								vMBMasterSetDestAddress(psMBMasterInfo, j);
								eException = xMasterFuncHandlers[i].pxHandler(psMBMasterInfo, ucMBFrame, &usLength);
							}
						}
						else 
						{
							eException = xMasterFuncHandlers[i].pxHandler(psMBMasterInfo, ucMBFrame, &usLength);
						}
						vMBMasterSetCBRunInMasterMode(psMBMasterInfo, FALSE);
						break;
					}
				}
			}
            /* If master has exception ,Master will send error process.Otherwise the Master is idle.*/
            if (eException != MB_EX_NONE) 
			{
            	vMBMasterSetErrorType(psMBMasterInfo, EV_ERROR_EXECUTE_FUNCTION);
            	( void ) xMBMasterPortEventPost( psMBPortInfo, EV_MASTER_ERROR_PROCESS );
            }
            else 
			{
            	vMBMasterCBRequestSuccess(psMBPortInfo);
            	vMBMasterRunResRelease();
            }
            break;

        case EV_MASTER_FRAME_SENT:     //主栈发送请求
        	/* Master is busy now. */
        	vMBMasterGetPDUSndBuf( psMBMasterInfo, &ucMBFrame );
		
#if MB_MASTER_RTU_ENABLED > 0 || MB_MASTER_ASCII_ENABLED > 0		
			eStatus = peMBMasterFrameSendCur( psMBMasterInfo,ucMBMasterGetDestAddress(psMBMasterInfo), 
		                                      ucMBFrame, usMBMasterGetPDUSndLength(psMBMasterInfo) );    //发送数据帧     
#endif
		break;

        case EV_MASTER_ERROR_PROCESS:    //主栈处理错误
        	/* Execute specified error process callback function. */
			errorType = eMBMasterGetErrorType(psMBMasterInfo);
			vMBMasterGetPDUSndBuf( psMBMasterInfo, &ucMBFrame );
			switch (errorType) 
			{
			    case EV_ERROR_RESPOND_TIMEOUT:    //等待超时
			    	vMBMasterErrorCBRespondTimeout( psMBPortInfo, ucMBMasterGetDestAddress(psMBMasterInfo),
			    			                        ucMBFrame, usMBMasterGetPDUSndLength(psMBMasterInfo));
			    	break;
			    case EV_ERROR_RECEIVE_DATA:      //接收数据出错
			    	vMBMasterErrorCBReceiveData( psMBPortInfo, ucMBMasterGetDestAddress(psMBMasterInfo), ucMBFrame, 
			                                     usMBMasterGetPDUSndLength(psMBMasterInfo));
			    	break;
			    case EV_ERROR_EXECUTE_FUNCTION:   //处理数据出错
			    	vMBMasterErrorCBExecuteFunction( psMBPortInfo, ucMBMasterGetDestAddress(psMBMasterInfo), ucMBFrame, 
			                                         usMBMasterGetPDUSndLength(psMBMasterInfo));
			    	break;
			    case EV_ERROR_RESPOND_DATA:      //接收响应数据出错
			    	vMBMasterErrorCBRespondData( psMBPortInfo, ucMBMasterGetDestAddress(psMBMasterInfo), ucMBFrame, 
			                                     usMBMasterGetPDUSndLength(psMBMasterInfo));
			    	break;
			    default:break;
			}
			vMBMasterRunResRelease();
        break;
			
		default: 
		break;
        }
    }
    return MB_ENOERR;
}

/**********************************************************************
 * @brief  MODBUS注册节点
 * @param  psMBMasterInfo  主栈信息块   
 * @return BOOL   
 * @author laoc
 * @date 2019.01.22
 *********************************************************************/
BOOL vMBMasterRegisterNode(sMBMasterInfo* psMBMasterInfo)
{
    sMBMasterNodeInfo* psMBMasterNodeInfo = NULL;
	const CHAR*        pcMBPortName       = psMBMasterInfo->psMasterPortInfo->pcMasterPortName;
	
	if(psMBMasterFindNodeByPort(pcMBPortName) == NULL)
    {
        psMBMasterNodeInfo =(sMBMasterNodeInfo*) calloc(1, sizeof(sMBMasterNodeInfo));
        
        if(psMBMasterNodeInfo == NULL)
        {
            return FALSE;
        }
        
        if(xMBMasterCreatePollTask(psMBMasterInfo))   //创建主栈状态机任务
        {
            return FALSE;
        }
	    psMBMasterNodeInfo->psMBMasterInfo = psMBMasterInfo;
	    psMBMasterNodeInfo->pNext = NULL; 
	    
	    if(sMBMasterNodeList->pNext == NULL)
	    {
            sMBMasterNodeList = psMBMasterNodeInfo;
	    }
	    else
	    {
            sMBMasterNodeList->pNext = psMBMasterNodeInfo;
	    }
    }
    return TRUE;
}

/**********************************************************************
 * @brief  MODBUS通过硬件找到所属主栈
 * @param  pcMBPortName    硬件名称
 * @return sMBMasterInfo   主栈信息块
 * @author laoc
 * @date 2019.01.22
 *********************************************************************/
sMBMasterInfo* psMBMasterFindNodeByPort(const CHAR* pcMBPortName)
{
	char*              pcPortName = NULL;
	sMBMasterInfo*     psMBMasterInfo = NULL;
	sMBMasterNodeInfo* psMBMasterNodeInfo = NULL;
    
	for( psMBMasterNodeInfo = sMBMasterNodeList; psMBMasterNodeInfo != NULL;
	     psMBMasterNodeInfo = psMBMasterNodeInfo->pNext )
	{
		if( psMBMasterNodeInfo->psMBMasterInfo != NULL)
		{
			psMBMasterInfo = psMBMasterNodeInfo->psMBMasterInfo;
		    strcpy(pcPortName, psMBMasterInfo->psMasterPortInfo->pcMasterPortName);
		    
		    if( strcmp(pcPortName,pcMBPortName) == 0 )
		    {
		    	return psMBMasterInfo;
		    }
		}
	}
	return psMBMasterInfo;		
}

/**********************************************************************
 * @brief  MODBUS创建主栈状态机任务
 * @param  psMBMasterInfo  主栈信息块   
 * @return BOOL   
 * @author laoc
 * @date 2019.01.22
 *********************************************************************/
BOOL xMBMasterCreatePollTask(const sMBMasterInfo* psMBMasterInfo)
{
    OS_ERR err = OS_ERR_NONE;
    sMBMasterTaskInfo* psMBTaskInfo = psMBMasterInfo->psMBMasterTaskInfo;
    
    OSTaskCreate(&psMBTaskInfo->p_tcb,
                  "vMBMasterPollTask",
                  vMBMasterPollTask,
                  (void*)psMBMasterInfo,
                  psMBTaskInfo->prio,
                  psMBTaskInfo->p_stk_base ,
                  psMBTaskInfo->stk_size / 10u,
                  psMBTaskInfo->stk_size,
                  0u,
                  0u,
                  0u,
                 (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR ),
                 &err);
    return (err == OS_ERR_NONE);
}

/**********************************************************************
 * @brief   主栈状态机
 * @param   *p_arg    
 * @return	none
 * @author  laoc
 * @date    2019.01.22
 *********************************************************************/
void vMBMasterPollTask(void *p_arg)
{
	CPU_SR_ALLOC();
	
	CPU_TS          ts = 0;
	OS_ERR          err = OS_ERR_NONE;
	eMBErrorCode    eStatus = MB_ENOERR;
	sMBMasterInfo*  psMBMasterInfo = (sMBMasterInfo*)p_arg;
	
	eStatus = eMBMasterInit(psMBMasterInfo);
	
	if(eStatus == MB_ENOERR)
	{
		eStatus = eMBMasterEnable(psMBMasterInfo);
		
		if(eStatus == MB_ENOERR)
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
 * @brief   获取设备状态
 * @param   psMBMasterInfo  主栈信息块
 * @param   Address         从设备地址   
 * @return	sMBSlaveDevInfo 
 * @author  laoc
 * @date    2019.01.22
 *********************************************************************/
sMBSlaveDevInfo* psMBMasterGetDev( const sMBMasterInfo* psMBMasterInfo, UCHAR Address)
{
    sMBSlaveDevInfo*  psMBDev = NULL;
    sMBMasterDevsInfo* psMBDevsInfo = psMBMasterInfo->psMBMasterDevsInfo;    //从设备状态信息
    
    if( psMBDevsInfo->psMBSlaveDevsList == NULL)   //无任何结点
    {
        return NULL;
    }
    else
    {
        for(psMBDev = psMBDevsInfo->psMBSlaveDevsList; psMBDev != NULL; psMBDev = psMBDev->pNext)
        {
            if(psMBDev->ucDevAddr == Address)
            {
                return psMBDev;
            }
        }
    }
    return psMBDev;
}

/**********************************************************************
 * @brief   注册设备进设备链表
 * @param   psMBMasterInfo  主栈信息块 
 * @param   Address         从设备地址 
 * @return	sMBSlaveDevInfo
 * @author  laoc
 * @date    2019.01.22
 *********************************************************************/
sMBSlaveDevInfo* psMBMasterRegisterDev( const sMBMasterInfo* psMBMasterInfo, const sMBSlaveDevDataInfo* psDevDataInfo)
{
    sMBSlaveDevInfo*  psMBDev = NULL;
    sMBSlaveDevInfo*  psMBNewDev = NULL;
 
    sMBMasterDevsInfo* psMBDevsInfo = psMBMasterInfo->psMBMasterDevsInfo;    //从设备信息
    
    if( (psMBNewDev = (sMBSlaveDevInfo*) calloc(1, sizeof(sMBSlaveDevInfo))) != NULL ) //申请成功
    {
        (void)xMBMasterInitDevTimer(psMBNewDev, MB_MASTER_DEV_OFFLINE_TMR_S);   //初始化掉线定时器
         psMBNewDev->pNext = NULL;
        
        if( psMBDevsInfo->psMBSlaveDevsList == NULL)   //无任何结点
        {
            psMBDevsInfo->psMBSlaveDevsList = psMBNewDev;
        }
        else //有节结点
        {
            psMBDev = psMBDevsInfo->psMBSlaveDevsList->pLast;   //尾节点
            psMBDev->pNext = psMBNewDev;
            psMBDevsInfo->psMBSlaveDevsList->pLast = psMBNewDev;
        }
        psMBDevsInfo->ucSlaveDevCount++;
    }
    return psMBNewDev;
}

/**********************************************************************
 * @brief   移除从设备
 * @param   psMBMasterInfo  主栈信息块 
 * @param   Address         从设备地址 
 * @return	BOOL
 * @author  laoc
 * @date    2019.01.22
 *********************************************************************/
BOOL xMBMasterRemoveDev( const sMBMasterInfo* psMBMasterInfo, UCHAR Address)
{
    sMBSlaveDevInfo*  psMBDev = NULL;
    sMBSlaveDevInfo*  psMBPreDev = NULL;
    sMBMasterDevsInfo* psMBDevsInfo = psMBMasterInfo->psMBMasterDevsInfo;    //从设备状态信息
    
    if( psMBDevsInfo->psMBSlaveDevsList == NULL)   //无任何结点
    {
        return FALSE;
    }
    else
    {
        for(psMBDev = psMBDevsInfo->psMBSlaveDevsList; psMBDev != NULL; psMBDev = psMBDev->pNext)
        {
            if(psMBDev->ucDevAddr == Address) //找到结点
            {             
                if(psMBDev->pNext == NULL) //只有一个结点或者最后节点
                {
                    if(psMBPreDev == NULL) //只有一个结点
                    {
                        psMBDevsInfo->psMBSlaveDevsList = NULL;
                    }
                    else //最后节点
                    {
                        psMBPreDev->pNext = NULL;
                    }
                }
                else  //有多个结点或非最后节点
                {
                    if(psMBPreDev == NULL) //第一个结点
                    {
                        psMBDevsInfo->psMBSlaveDevsList = psMBDev->pNext;
                    }
                    else  //非第一个结点
                    {
                        psMBPreDev->pNext = psMBDev->pNext; 
                    }  
                }
                
                vMBMasterDevOfflineTmrDel(psMBDev);
                (void)free(psMBDev); 
                return TRUE;         
            }
            psMBPreDev = psMBDev;
        }
    }
    return FALSE; 
}



/**********************************************************************
 * @brief   从设备定时器初始化
 * @param   psMBDev   从设备状态
 * @param   usTimerSec     定时器延迟时间 s
 * @return	BOOL
 * @author  laoc
 * @date    2019.01.22
 *********************************************************************/
BOOL xMBMasterInitDevTimer( sMBSlaveDevInfo* psMBDev, USHORT usTimerSec)
{
    OS_ERR err = OS_ERR_NONE;
	OS_TICK i = (OS_TICK)(usTimerSec * TMR_TICK_PER_SECOND);  //延时30s
    
    OSTmrCreate( &(psMBDev->DevOfflineTmr),       //主定时器
			      "DevOfflineTmr",
			      i,      
			      0,
			      OS_OPT_TMR_ONE_SHOT,
			      vMBMasterDevOfflineTimeout,
			      (void*)psMBDev,
			      &err);
    return (err == OS_ERR_NONE);
}

/**********************************************************************
 * @brief   从设备定时器中断
 * @param   *p_tmr     定时器
 * @param   *p_arg     参数
 * @return	none
 * @author  laoc
 * @date    2019.01.22
 *********************************************************************/
void vMBMasterDevOfflineTimeout( void * p_tmr, void * p_arg)
{
    OS_ERR err = OS_ERR_NONE;
	OS_TICK i = (OS_TICK)(MB_MASTER_DEV_OFFLINE_TMR_S / TMR_TICK_PER_SECOND);
    
    sMBSlaveDevInfo* psMBDev = (sMBSlaveDevInfo*)p_arg;
    psMBDev->ucDevOnTimeout = FALSE;    
}

/**********************************************************************
 * @brief   从设备定时器使能
 * @param   psMBDev   从设备状态
 * @return	none
 *********************************************************************/
void vMBMasterDevOfflineTmrEnable( sMBSlaveDevInfo* psMBDev)
{
    OS_ERR err = OS_ERR_NONE;
    (void)OSTmrStart( &(psMBDev->DevOfflineTmr), &err);
    psMBDev->ucDevOnTimeout = TRUE;
}

/**********************************************************************
 * @brief   删除从设备定时器
 * @param   psMBDev   从设备状态  
 * @return	none
 * @author  laoc
 * @date    2019.01.22
 *********************************************************************/
void vMBMasterDevOfflineTmrDel( sMBSlaveDevInfo* psMBDev)
{
    OS_ERR err = OS_ERR_NONE;
    (void)OSTmrDel( &(psMBDev->DevOfflineTmr), &err);
}




/**********************************接口函数******************************************/

/* Get whether the Modbus Master is run in master mode.*/
BOOL xMBMasterGetCBRunInMasterMode( const sMBMasterInfo* psMBMasterInfo )
{
	return psMBMasterInfo->xMBRunInMasterMode;
}
/* Set whether the Modbus Master is run in master mode.*/
void vMBMasterSetCBRunInMasterMode( sMBMasterInfo* psMBMasterInfo, BOOL IsMasterMode )
{
	psMBMasterInfo->xMBRunInMasterMode = IsMasterMode;
}
/* Get Modbus Master send destination address. */
UCHAR ucMBMasterGetDestAddress( const sMBMasterInfo* psMBMasterInfo )
{
	return psMBMasterInfo->ucMBMasterDestAddr;
}
/* Set Modbus Master send destination address. */
void vMBMasterSetDestAddress( sMBMasterInfo* psMBMasterInfo, UCHAR Address )
{
	psMBMasterInfo->ucMBMasterDestAddr = Address;
}
/* Get Modbus Master current error event type. */
eMBMasterErrorEventType eMBMasterGetErrorType( const sMBMasterInfo* psMBMasterInfo )
{
	return psMBMasterInfo->eMBMasterCurErrorType;
}
/* Set Modbus Master current error event type. */
void vMBMasterSetErrorType( sMBMasterInfo* psMBMasterInfo, eMBMasterErrorEventType errorType )
{
	psMBMasterInfo->eMBMasterCurErrorType = errorType;
}

#endif