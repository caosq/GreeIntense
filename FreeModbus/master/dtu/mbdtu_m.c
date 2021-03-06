#include "mbdtu_m.h"
#include "mbfunc_m.h"
#include "mbdict_m.h"
#include "mbscan_m.h"

#ifdef MB_MASTER_DTU_ENABLED     //GPRS模块功能支持

static USHORT usDTUInitCmd[2]={DTU_PROTOCOL_VERSIPON, INIT_DTU247_REG_HOLD_VALUE};
static USHORT usDTUInitedCmd[2]={DTU_PROTOCOL_VERSIPON, INITED_DTU247_REG_HOLD_VALUE};	

static void vDTUDevTest(sMBMasterInfo* psMBMasterInfo);
static BOOL xDTUTimerTimeoutInit(sMBMasterDTUInfo* psDTUInfo, USHORT usTimerout);
static void vDTUTimeoutInd(void * p_tmr, void * p_arg);
static void vDTUTimerTimeoutEnable(sMBMasterDTUInfo* psDTUInfo);

/**********************************************************************
 * @brief   DTU轮询
 * @return	none
 *********************************************************************/
void vDTUScanDev(sMBMasterInfo* psMBMasterInfo)
{
    UCHAR ucSlaveDevMaxAddr, ucSlaveDevMinAddr;
    
    eMBMasterReqErrCode  errorCode     = MB_MRE_EILLSTATE;
	sMBMasterDevsInfo*   psMBDevsInfo  = &psMBMasterInfo->sMBDevsInfo;   //从设备状态表
	sMBMasterDTUInfo*    psDTUInfo     = &psMBMasterInfo->sMBDTUInfo;
                         
    sMBSlaveDevInfo*     psDevDTU247   = &psDTUInfo->sDevDTU247;
    sMBSlaveDevInfo*     psDevDTU200   = &psDTUInfo->sDevDTU200;
    
    if(psMBMasterInfo->bDTUEnable == FALSE)
    {
        return;
    }
    if(!psDTUInfo->ucDTUInited || psDevDTU247 == NULL || psDevDTU200 == NULL)  //完成初始化
    {
        return;
    }
   
    ucSlaveDevMaxAddr = psMBDevsInfo->ucSlaveDevMaxAddr;  //切换最大，最小地址
    ucSlaveDevMinAddr = psMBDevsInfo->ucSlaveDevMinAddr;
    
    psMBDevsInfo->ucSlaveDevMaxAddr = DTU247_SLAVE_ADDR;
    psMBDevsInfo->ucSlaveDevMinAddr = DTU200_SLAVE_ADDR;
    
    vDTUDevTest(psMBMasterInfo);
    
    /***********************开始轮询DTU*************************************/
	if( psDevDTU200->ucOnLine == TRUE)   //轮询DTU模块数据表
	{
        psMBDevsInfo->psMBSlaveDevCur = psDevDTU200;
        
        if(psMBDevsInfo->psMBSlaveDevCur->psDevCurData == NULL)               //数据表为空则不进行轮询
        {
            return;
        }
#if  MB_FUNC_WRITE_MULTIPLE_HOLDING_ENABLED > 0 		
		errorCode =eMBMasterScanWriteHoldingRegister(psMBMasterInfo, DTU200_SLAVE_ADDR, FALSE);
#endif
	}
    
    if( (psDevDTU247->ucOnLine == TRUE) && (psDevDTU247->ucDataReady == TRUE) )   //
    {
        psMBDevsInfo->psMBSlaveDevCur = psDevDTU247;
        if(psMBDevsInfo->psMBSlaveDevCur->psDevCurData == NULL)               //数据表为空则不进行轮询
        {
            return;
        }       
#if MB_FUNC_READ_INPUT_ENABLED > 0				
        errorCode = eMBMasterScanReadInputRegister(psMBMasterInfo, DTU247_SLAVE_ADDR);						
#endif   
	}
    psMBDevsInfo->ucSlaveDevMaxAddr = ucSlaveDevMaxAddr;
    psMBDevsInfo->ucSlaveDevMinAddr = ucSlaveDevMinAddr;
}

/**********************************************************************
 * @brief   DTU测试
 * @return	none
 *********************************************************************/
void vDTUDevTest(sMBMasterInfo* psMBMasterInfo)
{
    UCHAR n;
    UCHAR ucSlaveDevMaxAddr, ucSlaveDevMinAddr;
    
	OS_ERR                      err     = OS_ERR_NONE;
    eMBMasterReqErrCode   errorCode     = MB_MRE_EILLSTATE;
    
	sMBMasterDevsInfo*    psMBDevsInfo  = &psMBMasterInfo->sMBDevsInfo;   //从设备状态表
	sMBMasterDTUInfo*     psDTUInfo     = &psMBMasterInfo->sMBDTUInfo;
    
    sMBSlaveDevInfo*      psDevDTU247   = &psDTUInfo->sDevDTU247;
    sMBSlaveDevInfo*      psDevDTU200   = &psDTUInfo->sDevDTU200;
    
    if(psMBMasterInfo->bDTUEnable == FALSE)
    {
        return;
    }
    if(!psDTUInfo->ucDTUInited || psDevDTU247 == NULL || psDevDTU200 == NULL)  //完成初始化
    {
        return;
    }
    /***********************开始测试DTU的状态*************************************/
    ucSlaveDevMaxAddr = psMBDevsInfo->ucSlaveDevMaxAddr;  //切换最大，最小地址
    ucSlaveDevMinAddr = psMBDevsInfo->ucSlaveDevMinAddr;
    
    psMBDevsInfo->ucSlaveDevMaxAddr = DTU247_SLAVE_ADDR;
    psMBDevsInfo->ucSlaveDevMinAddr = DTU200_SLAVE_ADDR;
    
    psMBMasterInfo->xMBRunInTestMode = TRUE;  //处于测试从设备状态
    for(n = 0; n < 5; n++)
    {	
#if  MB_FUNC_WRITE_MULTIPLE_HOLDING_ENABLED > 0 		
        errorCode = eMBMasterReqWriteMultipleHoldingRegister(psMBMasterInfo, DTU247_SLAVE_ADDR, DTU247_INIT_START_REG_HOLD_ADDR, 
            								                 2, psDTUInfo->psDTUInitedCmd, MB_MASTER_WAITING_DELAY); //查看GPRS模块是否已完成初始化                                
#endif       
   	    if(errorCode == MB_MRE_NO_ERR)
   	    {
   	    	break;
   	    }
        else if(errorCode == MB_MRE_ILL_ARG)
        {
            continue;
        }
        (void)OSTimeDlyHMSM(0, 0, 0, 200, OS_OPT_TIME_HMSM_STRICT, &err);
    }
    
    if (errorCode != MB_MRE_NO_ERR)  //未完成初始化
    {
	
#if  MB_FUNC_WRITE_MULTIPLE_HOLDING_ENABLED > 0    
      	errorCode = eMBMasterReqWriteMultipleHoldingRegister(psMBMasterInfo, DTU247_SLAVE_ADDR, DTU247_INIT_START_REG_HOLD_ADDR, 
                                                             2, psDTUInfo->psDTUInitCmd, MB_MASTER_WAITING_DELAY);	//进行初始化
#endif
        psDevDTU247->ucOnLine = FALSE;
        psDevDTU200->ucOnLine = FALSE;
        
        psDevDTU247->ucDataReady = FALSE;
        psDevDTU200->ucDataReady = FALSE;
        
        vDTUTimerTimeoutEnable(psDTUInfo);
    }
    else   //GPRS模块已经初始化
    {
        if( (psDevDTU247 == NULL) || (psDevDTU200 == NULL))
        {
            return;
        }
        psDevDTU247->ucOnLine = TRUE;  
        psDevDTU200->ucOnLine = TRUE;  
  
#if MB_FUNC_READ_INPUT_ENABLED > 0                          
		errorCode = eMBMasterReqReadInputRegister(psMBMasterInfo, DTU247_SLAVE_ADDR, TEST_DTU247_PROTOCOL_REG_IN_ADDR, 
                                                  1, MB_MASTER_WAITING_DELAY);     //查看GPRS模块参数是否改变	
#endif               
        if (errorCode == MB_MRE_NO_ERR)  //GPRS模块参数改变完毕
        {   
            psDevDTU247->ucDataReady = TRUE;  					
        }    
    }
    psMBMasterInfo->xMBRunInTestMode = FALSE;  //退出测试从设备状态 
    
    psMBDevsInfo->ucSlaveDevMaxAddr = ucSlaveDevMaxAddr;
    psMBDevsInfo->ucSlaveDevMinAddr = ucSlaveDevMinAddr;   
}

/**********************************************************************
 * @brief   DTU初始化
 * @return	none
 *********************************************************************/
BOOL xDTUInit(sMBMasterDTUInfo* psDTUInfo)
{
    sMBSlaveDevInfo* psDevDTU247  = &psDTUInfo->sDevDTU247;
    sMBSlaveDevInfo* psDevDTU200  = &psDTUInfo->sDevDTU200;
    
    psDTUInfo->psDTUInitCmd   = usDTUInitCmd;
    psDTUInfo->psDTUInitedCmd = usDTUInitedCmd;
    
//    (void)xMBMasterRegistDev(psMBMasterInfo, psDevDTU247);   //注册虚拟设备
//    (void)xMBMasterRegistDev(psMBMasterInfo, psDevDTU200);
    
    psDevDTU247->ucDevAddr = DTU247_SLAVE_ADDR;      //DTU247通讯地址
    psDevDTU200->ucDevAddr = DTU200_SLAVE_ADDR;      //DTU200通讯地址
    
//    psDevDTU247->psDevCurData = psDevDataDTU247;     //DTU247当前数据域
//    psDevDTU200->psDevCurData = psDevDataDTU200;     //DTU200当前数据域
    
    return xDTUTimerTimeoutInit(psDTUInfo, DTU_TIMEOUT_S);    
}

/**********************************************************************
 * @brief   DTU注册通讯数据表
 * @return	none
 *********************************************************************/
void vDTURegistCommData(sMBMasterDTUInfo* psDTUInfo, sMBSlaveDevCommData* psDevDataDTU247, 
                        sMBSlaveDevCommData* psDevDataDTU200)
{
    sMBSlaveDevInfo* psDevDTU247  = &psDTUInfo->sDevDTU247;
    sMBSlaveDevInfo* psDevDTU200  = &psDTUInfo->sDevDTU200;
    
    psDevDTU247->psDevCurData = psDevDataDTU247;     //DTU247当前数据域
    psDevDTU200->psDevCurData = psDevDataDTU200;     //DTU200当前数据域
}


/********************************************************************
* @brief    DTU模块初始化定时器
 * @param   usTimerout    s   
 * @return	BOOL
********************************************************************/
BOOL xDTUTimerTimeoutInit(sMBMasterDTUInfo* psDTUInfo, USHORT usTimerout)
{
	OS_ERR err = OS_ERR_NONE;
	ULONG i = 0;
	ULONG n = 0;
	
	i = usTimerout  * TMR_TICK_PER_SECOND; 
    n = 2 * TMR_TICK_PER_SECOND;
	
	OSTmrCreate(&psDTUInfo->DTUTimerTimeout,       //主定时器
			    "DTUTimerTimeout",
			    i,      
			    0,
			    OS_OPT_TMR_ONE_SHOT,
			    vDTUTimeoutInd,
			    (void*)psDTUInfo,
			    &err);
	return (err == OS_ERR_NONE);
}

/********************************************************************
* @brief    DTU模块定时器中断
********************************************************************/
void vDTUTimeoutInd(void * p_tmr, void * p_arg)
{
    sMBMasterDTUInfo* psDTUInfo = (sMBMasterDTUInfo*)p_arg;
    psDTUInfo->ucDTUInited = TRUE;
}

/********************************************************************
* @brief    DTU模块定时器使能
********************************************************************/
void vDTUTimerTimeoutEnable(sMBMasterDTUInfo* psDTUInfo)
{
	OS_ERR err = OS_ERR_NONE;
	psDTUInfo->ucDTUInited = FALSE;
    (void)OSTmrStart(&psDTUInfo->DTUTimerTimeout, &err);
}	

#endif
