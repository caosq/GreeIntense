#include "sensor.h"

#define TMR_TICK_PER_SECOND     OS_CFG_TMR_TASK_RATE_HZ
#define SENSOR_TIME_OUT_S       1

#define MAX_TEMP  700
#define MIN_TEMP  -300

#define MAX_HUMI  100
#define MIN_HUMI  0

#define MAX_CO2_PPM  2000
#define MIN_CO2_PPM  0


/*************************************************************
*                         传感器                             *
**************************************************************/

/*向通讯主栈中注册设备*/
void vSensor_RegistDev(IDevCom* pt)
{
    Sensor* pThis = SUB_PTR(pt, IDevCom, Sensor);
    (void)xMBMasterRegistDev(pThis->psMBMasterInfo, &pThis->sMBSlaveDev);
}

void vSensor_Init(Sensor* pt, sMBMasterInfo* psMBMasterInfo)
{
    OS_ERR err = OS_ERR_NONE;
    
    Sensor*  pThis   = (Sensor*)pt;
    IDevCom* pDevCom = SUPER_PTR(pThis, IDevCom);
    
    ULONG i = SENSOR_TIME_OUT_S * TMR_TICK_PER_SECOND; 
    
    pThis->psMBMasterInfo = psMBMasterInfo; //所属通讯主栈
    
    pDevCom->initDevCommData(pDevCom);
    pDevCom->registDev(pDevCom);            //向通讯主栈中注册设备
    
    OSTmrCreate(&pThis->sSensorTmr,
			    "sSensorTmr",
		        0,   
			    i,
			    OS_OPT_TMR_PERIODIC,
			    pThis->timeoutInd,
			    (void*)pThis,
			    &err);
}


ABS_CTOR(Sensor)  //传感器抽象类构造函数
    SUPER_CTOR(Device);
    FUNCTION_SETTING(IDevCom.registDev, vSensor_RegistDev);
END_CTOR


/*************************************************************
*                         CO2传感器                          *
**************************************************************/

/*通讯映射函数*/
BOOL xCO2Sensor_DevDataMapIndex(eDataType eDataType, UCHAR ucProtocolID, USHORT usAddr, USHORT* psIndex)
{
    USHORT i = 0;
    switch(ucProtocolID)
	{
        case 0:
            if(eDataType == RegHoldData)
            {
                switch(usAddr)
                {
                    case 1	  :  i = 0 ;  break;
                    case 0X30 :  i = 1 ;  break;
                       
                    default:
                		return FALSE;
                	break;
                }
            }
        break;
        default: break;
	}
    *psIndex = i;
    return TRUE;
}

/*通讯数据表初始化*/
void vCO2Sensor_InitDevCommData(IDevCom* pt)
{
    Sensor*      pThis = SUB_PTR(pt, IDevCom, Sensor);
    CO2Sensor* pCO2Sen = SUB_PTR(pThis, Sensor, CO2Sensor);
   
    sMBDevDataTable* psMBRegHoldTable = &pThis->sDevCommData.sMBRegHoldTable; 
    sMBTestDevCmd*            psMBCmd = &pThis->sDevCommData.sMBDevCmdTable;
    
    
MASTER_PBUF_INDEX_ALLOC()
MASTER_TEST_CMD_INIT(psMBCmd, 0x30, READ_REG_HOLD, pThis->sMBSlaveDev.ucDevAddr, FALSE)  
    
    /******************************保持寄存器数据域*************************/
MASTER_BEGIN_DATA_BUF(pThis->sSensor_RegHoldBuf, psMBRegHoldTable)      
    MASTER_REG_HOLD_DATA(1, int16,  MIN_CO2_PPM, MAX_CO2_PPM, 0, RO, 0, (void*)&pCO2Sen->usCO2PPM)

    MASTER_REG_HOLD_DATA(0x30, uint8, 1, 255, 0, RW, pThis->sMBSlaveDev.ucDevAddr, (void*)&pThis->sMBSlaveDev.ucDevAddr)
MASTER_END_DATA_BUF(1, 0x30)
    
    
    pCO2Sen->usMaxPPM = MAX_CO2_PPM;
    pCO2Sen->usMinPPM = MIN_CO2_PPM;
    
    pThis->sDevCommData.pxDevDataMapIndex = xCO2Sensor_DevDataMapIndex;    //绑定映射函数
    pThis->sMBSlaveDev.psDevDataInfo = &(pThis->sDevCommData);
}


void vCO2Sensor_TimeoutInd(void * p_tmr, void * p_arg)  //定时器中断服务函数
{
    Sensor*       pThis = (Sensor*)p_arg;
    CO2Sensor* psCO2Sen = SUB_PTR(pThis, Sensor, CO2Sensor);
    
    uint8_t*        pcSampleIndex = &pThis->ucSampleIndex;
    
    if(*pcSampleIndex< SENSOR_SAMPLE_NUM)
    {
        psCO2Sen->usTotalCO2PPM = psCO2Sen->usTotalCO2PPM - psCO2Sen->usSampleCO2PPM[*pcSampleIndex];
        psCO2Sen->usSampleCO2PPM[*pcSampleIndex]  = psCO2Sen->usCO2PPM;
        psCO2Sen->usTotalCO2PPM  = psCO2Sen->usTotalCO2PPM + psCO2Sen->usCO2PPM;
     
        (*pcSampleIndex)++;
    }
    else if(*pcSampleIndex == SENSOR_SAMPLE_NUM)
    {
        *pcSampleIndex = 0;
    }
    psCO2Sen->usAvgCO2PPM  = psCO2Sen->usTotalCO2PPM  / SENSOR_SAMPLE_NUM;
    
    if( (psCO2Sen->usAvgCO2PPM < psCO2Sen->usMinPPM) || (psCO2Sen->usAvgCO2PPM > psCO2Sen->usMaxPPM))
    {
        psCO2Sen->xCO2Error = TRUE;
    }
    else
    {
        psCO2Sen->xCO2Error = FALSE;
    }
}


CTOR(CO2Sensor)   //CO2传感器构造函数
    SUPER_CTOR(Sensor);
    FUNCTION_SETTING(Sensor.IDevCom.initDevCommData, vCO2Sensor_InitDevCommData);
    FUNCTION_SETTING(Sensor.timeoutInd, vCO2Sensor_TimeoutInd);
END_CTOR


/*************************************************************
*                         温湿度传感器                       *
**************************************************************/

/*通讯映射函数*/
BOOL xTempHumiSensor_DevDataMapIndex(eDataType eDataType, UCHAR ucProtocolID, USHORT usAddr, USHORT* psIndex)
{
    USHORT i = 0;
    switch(ucProtocolID)
	{
        case 0:
            if(eDataType == RegHoldData)
            {
                switch(usAddr)
                {
                    case 1	  :  i = 0 ;  break;
                    case 2	  :  i = 1 ;  break;
                    case 0X30 :  i = 2 ;  break;
                       
                    default:
                		return FALSE;
                	break;
                }
            }
        break;
        default: break;
	}
    *psIndex = i;
    return TRUE;
    
    
}

/*通讯数据表初始化*/
void vTempHumiSensor_InitDevCommData(IDevCom* pt)
{
    Sensor*                pThis = SUB_PTR(pt, IDevCom, Sensor);
    TempHumiSensor* pTempHumiSen = SUB_PTR(pThis, Sensor, TempHumiSensor);
    
    sMBDevDataTable* psMBRegHoldTable = &pThis->sDevCommData.sMBRegHoldTable; 
    sMBTestDevCmd*            psMBCmd = &pThis->sDevCommData.sMBDevCmdTable;
  
    
MASTER_PBUF_INDEX_ALLOC()
MASTER_TEST_CMD_INIT(psMBCmd, 0x30, READ_REG_HOLD, pThis->sMBSlaveDev.ucDevAddr, FALSE)  
    
    /******************************保持寄存器数据域*************************/
MASTER_BEGIN_DATA_BUF(pThis->sSensor_RegHoldBuf, psMBRegHoldTable)      
    MASTER_REG_HOLD_DATA(1, int16,  MIN_TEMP, MAX_TEMP, 0, RO, 0, (void*)&pTempHumiSen->sTemp)
    MASTER_REG_HOLD_DATA(2, uint16, MIN_HUMI, MAX_HUMI, 0, RO, 0, (void*)&pTempHumiSen->usHumi)

    MASTER_REG_HOLD_DATA(0x30, uint8, 1, 255, 0, RW, pThis->sMBSlaveDev.ucDevAddr, (void*)&pThis->sMBSlaveDev.ucDevAddr)
MASTER_END_DATA_BUF(1, 0x30)
    




    
    pTempHumiSen->sMaxTemp = MAX_TEMP;
    pTempHumiSen->sMinTemp = MIN_TEMP; 
    
    pTempHumiSen->usMaxHumi = MAX_HUMI;
    pTempHumiSen->usMinHumi = MIN_HUMI;
    
    pThis->sDevCommData.ucProtocolID = 0;
    pThis->sDevCommData.pxDevDataMapIndex = xTempHumiSensor_DevDataMapIndex;    //绑定映射函数
    pThis->sMBSlaveDev.psDevDataInfo = &(pThis->sDevCommData);
}

void vTempHumiSensor_TimeoutInd(void * p_tmr, void * p_arg)  //定时器中断服务函数
{
    Sensor*                pThis = (Sensor*)p_arg;
    TempHumiSensor* pTempHumiSen = SUB_PTR(pThis, Sensor, TempHumiSensor);
    
    uint8_t*        pcSampleIndex = &pThis->ucSampleIndex;
    
    if(*pcSampleIndex< SENSOR_SAMPLE_NUM)
    {
        pTempHumiSen->sTotalTemp  = pTempHumiSen->sTotalTemp - pTempHumiSen->sSampleTemp[*pcSampleIndex];
        pTempHumiSen->usTotalHumi = pTempHumiSen->usTotalHumi - pTempHumiSen->usSampleHumi[*pcSampleIndex];
        
        pTempHumiSen->sSampleTemp[*pcSampleIndex]  = pTempHumiSen->sTemp;
        pTempHumiSen->usSampleHumi[*pcSampleIndex] = pTempHumiSen->usHumi;
        
        pTempHumiSen->sTotalTemp  = pTempHumiSen->sTotalTemp + pTempHumiSen->sTemp;
        pTempHumiSen->usTotalHumi = pTempHumiSen->usTotalHumi + pTempHumiSen->usHumi;
        (*pcSampleIndex)++;
    }
    else if(*pcSampleIndex == SENSOR_SAMPLE_NUM)
    {
        *pcSampleIndex = 0;
    }
    pTempHumiSen->sAvgTemp  = pTempHumiSen->sTotalTemp  / SENSOR_SAMPLE_NUM;
    pTempHumiSen->usAvgHumi = pTempHumiSen->usTotalHumi / SENSOR_SAMPLE_NUM;
    
    if( (pTempHumiSen->sAvgTemp < pTempHumiSen->sMinTemp) || (pTempHumiSen->sAvgTemp > pTempHumiSen->sMaxTemp))
    {
        pTempHumiSen->xTempError = TRUE;
    }
    else
    {
        pTempHumiSen->xTempError = FALSE;
    }
    if( (pTempHumiSen->usAvgHumi < pTempHumiSen->usMinHumi) || (pTempHumiSen->usAvgHumi > pTempHumiSen->usMaxHumi))
    {
        pTempHumiSen->xHumiError = TRUE;
    }
    else
    {
        pTempHumiSen->xHumiError = FALSE;
    }
}


CTOR(TempHumiSensor)   //温湿度传感器构造函数
    SUPER_CTOR(Sensor);
    FUNCTION_SETTING(Sensor.IDevCom.initDevCommData, vTempHumiSensor_InitDevCommData);
    FUNCTION_SETTING(Sensor.timeoutInd, vTempHumiSensor_TimeoutInd);
END_CTOR



