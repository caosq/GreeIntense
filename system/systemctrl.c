#include "bms.h"
#include "system.h"
#include "systemctrl.h"
#include "md_event.h"
#include "md_modbus.h"
#include "md_timer.h"

/*系统开启*/
void vSystem_SwitchOpen(System* pt)
{
    System* pThis = (System*)pt;

    vSystem_OpenUnits(pThis);   
    pThis->eSwitchCmd = CMD_OPEN; 
}

/*系统关闭*/
void vSystem_SwitchClose(System* pt)
{
    uint8_t  n = 0; 
    System* pThis = (System*)pt;
    
    ExAirFan*    pExAirFan    = NULL;
    ModularRoof* pModularRoof = NULL;
    
    vSystem_CloseExAirFans(pThis);
    vSystem_CloseUnits(pThis);
    
    pThis->eSwitchCmd = CMD_CLOSE; 
}

/*切换系统模式*/
void vSystem_ChangeSystemMode(System* pt, eSystemMode eSystemMode)
{
    uint8_t  n = 0; 
    System* pThis = (System*)pt;
    
    ExAirFan*    pExAirFan    = NULL;
    ModularRoof* pModularRoof = NULL;
          
    if(eSystemMode == MODE_AUTO)    //自动模式
    {
        //若室内温度>室内目标温度+ T0（默认1.5℃），机组送风模式开启；否则，机组制热模式开启；
        if(pThis->sAmbientIn_T > pThis->usTempSet + pThis->usModeAdjustTemp_0)
        {
            vSystem_SetUnitRunningMode(pThis, RUN_MODE_FAN);
        }
        else
        {
            vSystem_SetUnitRunningMode(pThis, RUN_MODE_HEAT);
        }
        vSystem_SwitchOpen(pThis);  //开启系统
    }
    if(eSystemMode == MODE_CLOSE)    //关闭模式
    {
        vSystem_SwitchClose(pThis);
    }
    if(eSystemMode == MODE_EMERGENCY) //紧急模式
    {
        vSystem_SwitchOpen(pThis);    //开启系统
        vSystem_SetUnitRunningMode(pThis, RUN_MODE_FAN); //开启送风模式
    }
    pThis->eSystemMode = eSystemMode;
    
#if DEBUG_ENABLE > 0  
    myprintf("vSystem_ChangeSystemMode %d\n", pThis->eSystemMode);
#endif    
}

/*设定系统目标温度值*/
void vSystem_SetTemp(System* pt, uint16_t usTempSet)
{
    uint8_t  n = 0; 
    System* pThis = (System*)pt;
    
    ModularRoof* pModularRoof = NULL;
    
    for(n=0; n < MODULAR_ROOF_NUM; n++)
    {
        pModularRoof = pThis->psModularRoofList[n];
        pModularRoof->usCoolTempSet = usTempSet;
        pModularRoof->usHeatTempSet = usTempSet;
    }
    pThis->usTempSet = usTempSet;
  
#if DEBUG_ENABLE > 0
    myprintf("vSystem_SetTemp %d\n", pThis->usTempSet);
#endif
    vSystem_ChangeUnitRunningMode(pThis);
    
}

/*设定系统目标新风量*/
void vSystem_SetFreAir(System* pt, uint16_t usFreAirSet_Vol_H, uint16_t usFreAirSet_Vol_L)
{
    uint8_t  n, ucUnitNum; 
    System* pThis = (System*)pt;
    
    ModularRoof* pModularRoof = NULL;
    ModularRoof* pUnit        = NULL;
    
    uint16_t usFreAirSet_Vol = 0;
    uint32_t ulFreAirSet_Vol = (uint32_t)usFreAirSet_Vol_H*65535 + (uint32_t)usFreAirSet_Vol_L;
    
    for(n=0; n < MODULAR_ROOF_NUM; n++)
    {
        pModularRoof = pThis->psModularRoofList[n];
        if(pModularRoof->sMBSlaveDev.xOnLine == TRUE && pModularRoof->xStopErrFlag == FALSE)  //机组在线且无故障
        {
            ucUnitNum++;           
        }
    } 
    if(ucUnitNum > 0 && ucUnitNum < MODULAR_ROOF_NUM ) //只有一台机组可用
    {
        if(pThis->ulFreAirSet_Vol > 65000)    //保证新风量
        {
            pUnit->usFreAirSet_Vol = 65000;
        }
        else
        {
            pUnit->usFreAirSet_Vol = pThis->ulFreAirSet_Vol;
        }
    }
    else if(ucUnitNum == MODULAR_ROOF_NUM)  //两台
    {
        usFreAirSet_Vol = (uint16_t)(ulFreAirSet_Vol / MODULAR_ROOF_NUM);
        
        for(n=0; n < MODULAR_ROOF_NUM; n++)
        {
            pModularRoof = pThis->psModularRoofList[n];
            pModularRoof->usFreAirSet_Vol = usFreAirSet_Vol;
        }  
    }
    pThis->ulFreAirSet_Vol = ulFreAirSet_Vol;
    
#if DEBUG_ENABLE > 0
    myprintf("vSystem_SetFreAir  ulFreAirSet_Vol %ld \n", pThis->ulFreAirSet_Vol);
#endif    
    vSystem_ExAirSet_Vol(pThis); //系统排风需求量变化
}

/*设定系统排风机额定风量*/
void vSystem_SetExAirFanRated(System* pt, uint16_t usExAirFanRated_Vol_H, uint16_t usExAirFanRated_Vol_L)  
{
    uint8_t  n, ucUnitNum; 
    System* pThis = (System*)pt;
    
    pThis->ulExAirFanRated_Vol = usExAirFanRated_Vol_H*65535 + usExAirFanRated_Vol_L;
    
#if DEBUG_ENABLE > 0
    myprintf("vSystem_SetExAirFanRated  usExAirFanRated_Vol_L %d  usExAirFanRated_Vol_H %d\n", usExAirFanRated_Vol_L, usExAirFanRated_Vol_H);
#endif    
    
}

/*设定系统湿度阈值*/
void vSystem_SetHumidity(System* pt, uint16_t usHumidityMin, uint16_t usHumidityMax)
{
    uint8_t  n = 0; 
    System* pThis = (System*)pt;
    
    ModularRoof* pModularRoof = NULL;
    for(n=0; n < MODULAR_ROOF_NUM; n++)
    {
        pModularRoof = pThis->psModularRoofList[n]; 
        pModularRoof->usHumidityMin = usHumidityMin;
        pModularRoof->usHumidityMax = usHumidityMax;
    }
    pThis->usHumidityMin = usHumidityMin;
    pThis->usHumidityMax = usHumidityMax;
    
#if DEBUG_ENABLE > 0
    myprintf("vSystem_SetHumidity  usHumidityMin %d  usHumidityMax %d\n", pThis->usHumidityMin, pThis->usHumidityMax);
#endif    
    
}

/*设定系统目标CO2浓度值*/
void vSystem_SetCO2AdjustThr_V(System* pt, uint16_t usCO2AdjustThr_V)
{
    uint8_t  n = 0; 
    System* pThis = (System*)pt;
    
    ModularRoof* pModularRoof = NULL;
    for(n=0; n < MODULAR_ROOF_NUM; n++)
    {
        pModularRoof = pThis->psModularRoofList[n]; 
        pModularRoof->usCO2AdjustThr_V = usCO2AdjustThr_V;
    }
    pThis->usCO2AdjustThr_V = usCO2AdjustThr_V;
    
#if DEBUG_ENABLE > 0
    myprintf("vSystem_SetCO2AdjustThr_V  usCO2AdjustThr_V %d  \n", pThis->usCO2AdjustThr_V);
#endif    
    
}

/*设定系统CO2浓度偏差*/
void vSystem_SetCO2AdjustDeviat(System* pt, uint16_t usCO2AdjustDeviat)
{
    uint8_t  n = 0; 
    System* pThis = (System*)pt;
    
    ModularRoof* pModularRoof = NULL;
    for(n=0; n < MODULAR_ROOF_NUM; n++)
    {
        pModularRoof = pThis->psModularRoofList[n]; 
        pModularRoof->usCO2AdjustDeviat = usCO2AdjustDeviat;
    }
    pThis->usCO2AdjustDeviat = usCO2AdjustDeviat;
    
#if DEBUG_ENABLE > 0
    myprintf("vSystem_SetCO2AdjustDeviat  usCO2AdjustDeviat %d  \n", pThis->usCO2AdjustDeviat);
#endif
    
}

/*注册声光报警启停接口*/
void vSystem_RegistAlarmIO(System* pt, uint8_t ucSwitch_DO)
{
    System* pThis = (System*)pt;  
    pThis->sAlarm_DO.ucChannel = ucSwitch_DO;
}

/*声光报警*/
void vSystem_SetAlarm(System* pt)
{
    System* pThis = (System*)pt; 
    
    if(pThis->xAlarmEnable)  //声光报警使能     
    {
        vDigitalOutputCtrl(pThis->sAlarm_DO.ucChannel, ON); //输出开启,继电器闭合
    }
}

/*清除声光报警*/
void vSystem_DelAlarm(System* pt)
{
    System* pThis = (System*)pt; 
    vDigitalOutputCtrl(pThis->sAlarm_DO.ucChannel, OFF);   //输出关闭，继电器断开
}

/*清除声光报警请求*/
void vSystem_DelAlarmRequst(System* pt)
{
    uint8_t  n = 0; 
    System* pThis = (System*)pt;
    
    ModularRoof* pModularRoof = NULL;
    ExAirFan*    pExAirFan    = NULL;
    
    if(pThis->xAlarmEnable == FALSE)
    {
        vSystem_DelAlarm(pThis); //【声光报警使能】关闭。
    }
    for(n=0; n < MODULAR_ROOF_NUM; n++)
    {
        pModularRoof = pThis->psModularRoofList[n]; 

       //(1)群控控制器与空调机组通讯故障,  (8)空调机组停机保护。声光报警        
        if( (pModularRoof->sMBSlaveDev.xOnLine == FALSE) || (pModularRoof->xStopErrFlag) ) 
        {
            return;
        } 
        //(3)当送风温度大于【送风温度最大值】（默认45℃）,声光报警
        if(pModularRoof->sSupAir_T > pThis->usSupAirMax_T)
        {
            return;            
        }         
    }
    for(n=0; n < EX_AIR_FAN_NUM; n++)
    {
        pExAirFan = pThis->psExAirFanList[n]; 
        if(pExAirFan->xExAirFanErr == TRUE)
        {
            return;
        }
    }
    
    //(2)当室内CO2浓度大于【CO2报警浓度指标值】（默认3000PPM），声光报警
    if( pThis->usCO2PPM >= pThis->usCO2PPMAlarm)  
    {
        return;
    }
    //(4)同类全部传感器通讯故障,声光报警
    if( (pThis->xCO2SenErr == TRUE) || (pThis->xTempHumiSenOutErr == TRUE) || (pThis->xTempHumiSenInErr == TRUE) ) 
    {
        return;
    }   
    vSystem_DelAlarm(pThis); //不满足报警条件，清除声光报警
}
