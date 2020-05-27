#include "bms.h"
#include "system.h"
#include "systemctrl.h"

#include "md_event.h"
#include "md_modbus.h"
#include "md_timer.h"

/*************************************************************
*                         系统                               *
**************************************************************/
#define SYSTEM_ALARM_DO          5       //系统声光报警DO接口

#define TMR_TICK_PER_SECOND      OS_CFG_TMR_TASK_RATE_HZ
#define RUNNING_TIME_OUT_S       1

#define HANDLE(p_arg1, p_arg2) if((USHORT*)psMsg->pvArg == (USHORT*)(&p_arg1)) {p_arg2;continue;}

static System*   psSystem = NULL;

static OS_PRIO         SystemTaskPrio = 10;
static CPU_STK_SIZE    SystemTaskStkSize = 128;

static OS_TCB*   psSystemTaskTCB = NULL;
static CPU_STK*  psSystemTaskStk = NULL;

/*系统排风机配置信息*/
sFanInfo ExAirFanVariate = {VARIABLE_FREQ, 250, 500, 1, 1, 1, 1};

sFanInfo ExAirFanSet[EX_AIR_FAN_NUM] = { {CONSTANT_FREQ, 0, 0, 0, 0, 2, 0},
                                         {CONSTANT_FREQ, 0, 0, 0, 0, 3, 0},
                                         {CONSTANT_FREQ, 0, 0, 0, 0, 4, 0},
                                         {CONSTANT_FREQ, 0, 0, 0, 0, 5, 0},
                                       };
/*系统排风机类型切换*/
void vSystem_ChangeExAirFanType(System* pt, eExAirFanType eExAirFanType)
{
    System* pThis = (System*)pt; 
    ExAirFan* pExAirFan = pThis->psExAirFanList[0];

    if(eExAirFanType == Type_CONSTANT_VARIABLE)     //定频 + 变频
    {
        pExAirFan->init(pExAirFan, &ExAirFanVariate);
        pThis->pExAirFanVariate = pExAirFan;
    }
    else   //定频
    {
        pExAirFan->init(pExAirFan, &ExAirFanSet[0]);
        pThis->pExAirFanVariate = NULL;
    }  
    pThis->eExAirFanType = eExAirFanType;   
}
                                                                            
/*系统内部消息轮询*/
void vSystem_PollTask(void *p_arg)
{
    uint8_t n;
    
    CPU_TS            ts = 0;
    OS_MSG_SIZE  msgSize = 0;
    OS_ERR           err = OS_ERR_NONE;
    
    System* pThis = (System*)p_arg;
    
    ModularRoof*    pModularRoof    = NULL;
    ExAirFan*       pExAirFan       = NULL;
    TempHumiSensor* pTempHumiSensor = NULL;
    CO2Sensor*      pCO2Sensor      = NULL;
    
    BMS* psBMS = BMS_Core();
    psSystem = System_Core();
    
    while(DEF_TRUE)
	{
        sMsg* psMsg = (sMsg*)OSTaskQPend(0, OS_OPT_PEND_BLOCKING, &msgSize, &ts, &err);
        
        /***********************BMS事件响应***********************/
        HANDLE(psBMS->System.eSystemMode,      vSystem_ChangeSystemMode(psSystem, psBMS->System.eSystemMode))  
        HANDLE(psBMS->System.eRunningMode,     vSystem_SetRunningMode(psSystem, psBMS->System.eRunningMode)) 
        
        HANDLE(psBMS->System.sTempSet,         vSystem_SetTemp(psSystem, psBMS->System.sTempSet))
        HANDLE(psBMS->System.usFreAirSet_Vol,  vSystem_SetFreAir(psSystem, psBMS->System.usFreAirSet_Vol))
        
        HANDLE(psBMS->System.usHumidityMin, vSystem_SetHumidity(psSystem, psBMS->System.usHumidityMin, 
                                                                psBMS->System.usHumidityMax))
        HANDLE(psBMS->System.usHumidityMax, vSystem_SetHumidity(psSystem, psBMS->System.usHumidityMin,
                                                                psBMS->System.usHumidityMax))
        
        HANDLE(psBMS->System.usCO2AdjustThr_V,  vSystem_SetCO2PPM(psSystem, psBMS->System.usCO2AdjustThr_V))
        HANDLE(psBMS->System.usCO2AdjustDeviat, vSystem_SetCO2AdjustDeviat(psSystem, psBMS->System.usCO2AdjustDeviat))
        
        HANDLE(psBMS->System.usExAirFanMinFreq, vSystem_SetExAirFanFreqRange(psSystem, psBMS->System.usExAirFanMinFreq, 
                                                                             psBMS->System.usExAirFanMaxFreq))
        HANDLE(psBMS->System.usExAirFanMaxFreq, vSystem_SetExAirFanFreqRange(psSystem, psBMS->System.usExAirFanMinFreq, 
                                                                             psBMS->System.usExAirFanMaxFreq))
        
        HANDLE(psBMS->System.eExAirFanType, vSystem_ChangeExAirFanType(psSystem, psBMS->System.eExAirFanType))
                                                                             
                                                                             
        /***********************主机事件响应***********************/
        for(n=0; n < MODULAR_ROOF_NUM; n++)
        {
            pModularRoof = pThis->psModularRoofList[n]; 
            
            HANDLE(pModularRoof->sSupAir_T,    vSystem_SupAirTemp(psSystem);break) 
 
            HANDLE(pModularRoof->xStopErrFlag, vSystem_UnitErr(psSystem);break)
            HANDLE(pModularRoof->sMBSlaveDev.xOnLine, vSystem_UnitErr(psSystem);break)
            
            HANDLE(pModularRoof->sAmbientInSelf_T,  vSystem_UnitTempHumiIn(psSystem);break)
            HANDLE(pModularRoof->usAmbientInSelf_H, vSystem_UnitTempHumiIn(psSystem);break)
            
            HANDLE(pModularRoof->sAmbientOutSelf_T,  vSystem_UnitTempHumiOut(psSystem);break)
            HANDLE(pModularRoof->usAmbientOutSelf_H, vSystem_UnitTempHumiOut(psSystem);break)
            
            HANDLE(pModularRoof->usCO2PPMSelf, vSystem_UnitCO2PPM(psSystem);break)
        }
        
        /***********************CO2传感器事件响应***********************/
        for(n=0; n < CO2_SEN_NUM; n++)
        {
            pCO2Sensor = (CO2Sensor*)pThis->psCO2SenList[n];
            
            HANDLE(pCO2Sensor->usAvgCO2PPM, vSystem_CO2PPM(psSystem);break) 
            HANDLE(pCO2Sensor->xCO2Error,   vSystem_CO2SensorErr(psSystem);break)
        }
        
        /***********************室外温湿度传感器事件响应***********************/
        for(n=0; n < TEMP_HUMI_SEN_OUT_NUM; n++)
        {
            pTempHumiSensor = (TempHumiSensor*)pThis->psTempHumiSenOutList[n];
            
            HANDLE(pTempHumiSensor->sAvgTemp,   vSystem_TempHumiOut(psSystem);break) 
            HANDLE(pTempHumiSensor->xTempError, vSystem_TempHumiOutErr(psSystem);break)
            
            HANDLE(pTempHumiSensor->usAvgHumi,  vSystem_TempHumiOut(psSystem);break) 
            HANDLE(pTempHumiSensor->xHumiError, vSystem_TempHumiOutErr(psSystem);break)
        }
        
         /***********************室内温湿度传感器事件响应***********************/
        for(n=0; n < TEMP_HUMI_SEN_IN_NUM; n++)
        {
            pTempHumiSensor = (TempHumiSensor*)pThis->psTempHumiSenOutList[n];
            
            HANDLE(pTempHumiSensor->sAvgTemp,   vSystem_TempHumiIn(psSystem);break) 
            HANDLE(pTempHumiSensor->xTempError, vSystem_TempHumiInErr(psSystem);break)
            
            HANDLE(pTempHumiSensor->usAvgHumi,  vSystem_TempHumiIn(psSystem);break) 
            HANDLE(pTempHumiSensor->xHumiError, vSystem_TempHumiInErr(psSystem);break)
        }
    }
}

/*创建系统内部消息轮询任务*/
BOOL xSystem_CreatePollTask(System* pt)
{
    OS_ERR    err = OS_ERR_NONE;
    System* pThis = (System*)pt;

    err = eTaskCreate(psSystemTaskTCB, vSystem_PollTask, pThis, SystemTaskPrio, psSystemTaskStk, SystemTaskStkSize);
    return (err == OS_ERR_NONE);              
}

void vSystem_RuntimeTmrCallback(void * p_tmr, void * p_arg)
{
    uint8_t n;
    ModularRoof*    pModularRoof    = NULL;
    ExAirFan*       pExAirFan       = NULL;
    
     System* pThis = (System*)p_arg;
    
    /*********************主机运行时间*************************/
    for(n=0; n < MODULAR_ROOF_NUM; n++)
    {
        pModularRoof = pThis->psModularRoofList[n];
        if(pModularRoof->Device.eRunningState == STATE_RUN && pModularRoof->Device.ulRunTime < UINT32_MAX)
        {
            pModularRoof->Device.ulRunTime++;
        }            
    }
    
    /*********************排风风机运行时间*************************/
    for(n=0; n < EX_AIR_FAN_NUM; n++)  
    {
        pExAirFan = pThis->psExAirFanList[n];
        if(pExAirFan->Device.eRunningState == STATE_RUN && pExAirFan->Device.ulRunTime < UINT32_MAX)
        {
            pExAirFan->Device.ulRunTime++;
        }   
        pExAirFan->Device.ulRunTime++;
    }
}
    
/*系统运行时间初始化*/
void vSystem_InitRuntimeTmr(System* pt)
{
    System* pThis = (System*)pt; 
    (void)xTimerRegist(&pThis->sRuntimeTmr, 0, RUNNING_TIME_OUT_S, OS_OPT_TMR_PERIODIC, vSystem_RuntimeTmrCallback, pThis);                       
}
 
/*系统EEPROM数据注册*/
void vSystem_RegistEEPROMData(System* pt)
{
    System* pThis = (System*)pt;
    
    EEPROM_DATA(TYPE_UINT_8, pThis->ucModeChangeTime_1)
    EEPROM_DATA(TYPE_UINT_8, pThis->ucModeChangeTime_2)
    EEPROM_DATA(TYPE_UINT_8, pThis->ucModeChangeTime_3)
    EEPROM_DATA(TYPE_UINT_8, pThis->ucModeChangeTime_4)
    EEPROM_DATA(TYPE_UINT_8, pThis->ucModeChangeTime_5)
    EEPROM_DATA(TYPE_UINT_8, pThis->ucModeChangeTime_6)
    
    EEPROM_DATA(TYPE_UINT_8, pThis->ucAmbientInDeviat_T)
    EEPROM_DATA(TYPE_UINT_8, pThis->ucAmbientOutDeviat_T)
    EEPROM_DATA(TYPE_UINT_8, pThis->ucAmbientInDeviat_H)
    EEPROM_DATA(TYPE_UINT_8, pThis->ucAmbientOutDeviat_H)
    
    EEPROM_DATA(TYPE_UINT_8, pThis->ucChickenGrowDays)
    EEPROM_DATA(TYPE_UINT_8, pThis->ucExAirCoolRatio)
    EEPROM_DATA(TYPE_UINT_8, pThis->ucExAirHeatRatio)
    EEPROM_DATA(TYPE_UINT_8, pThis->eExAirFanType)
    EEPROM_DATA(TYPE_UINT_8, pThis->xAlarmEnable)
    
    
    EEPROM_DATA(TYPE_UINT_16, pThis->usChickenNum)
    EEPROM_DATA(TYPE_UINT_16, pThis->usGrowUpTemp)
    EEPROM_DATA(TYPE_UINT_16, pThis->usAdjustModeTemp)
    EEPROM_DATA(TYPE_UINT_16, pThis->usSupAirMax_T)
    
    EEPROM_DATA(TYPE_UINT_16, pThis->usCO2AdjustThr_V)
    EEPROM_DATA(TYPE_UINT_16, pThis->usCO2AdjustDeviat)
    EEPROM_DATA(TYPE_UINT_16, pThis->usCO2PPMAlarm)
    EEPROM_DATA(TYPE_UINT_16, pThis->usFreAirSet_Vol)
    
    EEPROM_DATA(TYPE_UINT_16, pThis->usHumidityMax)
    EEPROM_DATA(TYPE_UINT_16, pThis->usHumidityMin)
    
    EEPROM_DATA(TYPE_UINT_16, pThis->usExAirFanMinFreq)
    EEPROM_DATA(TYPE_UINT_16, pThis->usExAirFanMaxFreq)
    EEPROM_DATA(TYPE_UINT_16, pThis->usExAirFanRunTimeLeast)
    EEPROM_DATA(TYPE_UINT_16, pThis->usExAirFanCtrlTime)
   
    
    EEPROM_DATA(TYPE_INT_16, pThis->sTempSet)
    
    EEPROM_DATA(TYPE_UINT_32, pThis->ulExAirFanRated_Vol)
    EEPROM_DATA(TYPE_RUNTIME, pThis->Device.ulRunTime)  
}

/*系统初始化*/
void vSystem_Init(System* pt)
{
    uint8_t n;
    System* pThis = (System*)pt;
    
    ModularRoof*    pModularRoof    = NULL;
    ExAirFan*       pExAirFan       = NULL;
    TempHumiSensor* pTempHumiSensor = NULL;
    CO2Sensor*      pCO2Sensor      = NULL;
    DTU*            psDTU           = NULL;
    
    vSystem_RegistAlarmIO(pThis, SYSTEM_ALARM_DO);  //注册报警接口
    vSystem_RegistEEPROMData(pThis);
    
    vSystem_InitRuntimeTmr(pThis);
    
    pThis->eSystemMode      = MODE_CLOSE;
    pThis->psMBMasterInfo   = psMBGetMasterInfo();

    /***********************绑定BMS变量变化事件***********************/
    CONNECT( &(BMS_Core()->sValChange), psSystemTaskTCB);  
    
    /*********************DTU模块*************************/
    psDTU = DTU_new(psDTU);
    psDTU->init(psDTU, pThis->psMBMasterInfo);
    
    /*********************主机*************************/
    for(n=0; n < MODULAR_ROOF_NUM; n++)
    {
        pModularRoof = (ModularRoof*)ModularRoof_new();
        pModularRoof->init(pModularRoof, pThis->psMBMasterInfo); //初始化
        pThis->psModularRoofList[n] = pModularRoof;
        
        CONNECT( &(pModularRoof->sValChange), psSystemTaskTCB);  //绑定主机变量变化事件
    }
    
    /*********************排风风机*************************/
    for(n=0; n < EX_AIR_FAN_NUM; n++)
    {
        pExAirFan = (ExAirFan*)ExAirFan_new();  //实例化对象
        pExAirFan->init(pExAirFan, &ExAirFanSet[n]);
        pThis->psExAirFanList[n] = pExAirFan;        
    }
    
    /***********************CO2传感器***********************/
    for(n=0; n < CO2_SEN_NUM; n++)
    {
        pCO2Sensor = (CO2Sensor*)CO2Sensor_new();     //实例化对象
        pCO2Sensor->Sensor.init( SUPER_PTR(pCO2Sensor, Sensor),  pThis->psMBMasterInfo); //向上转型，由子类转为父类
        pThis->psCO2SenList[n] = pCO2Sensor;
        
        CONNECT( &(pCO2Sensor->Sensor.sValChange), psSystemTaskTCB);  //绑定传感器变量变化事件
    }
    
    /***********************室外温湿度传感器***********************/
    for(n=0; n < TEMP_HUMI_SEN_OUT_NUM; n++)
    {
        pTempHumiSensor = (TempHumiSensor*)TempHumiSensor_new();
        pTempHumiSensor->Sensor.init( SUPER_PTR(pTempHumiSensor, Sensor),  pThis->psMBMasterInfo);
        pThis->psTempHumiSenOutList[n] = pTempHumiSensor;
        
        CONNECT( &(pTempHumiSensor->Sensor.sValChange), psSystemTaskTCB);  //绑定传感器变量变化事件        
    }
    
    /***********************室内温湿度传感器***********************/
    for(n=0; n < TEMP_HUMI_SEN_IN_NUM; n++)
    {
        pTempHumiSensor = (TempHumiSensor*)TempHumiSensor_new();
        pTempHumiSensor->Sensor.init( SUPER_PTR(pTempHumiSensor, Sensor),  pThis->psMBMasterInfo);
        pThis->psTempHumiSenInList[n] = pTempHumiSensor; 
        
        CONNECT( &(pTempHumiSensor->Sensor.sValChange), psSystemTaskTCB);  //绑定传感器变量变化事件
    }
    
    xSystem_CreatePollTask(pThis);
    vReadEEPROMData();                 //同步记忆参数

    vSystem_ChangeExAirFanType(pThis, pThis->eExAirFanType);   //切换风机类型                  
}

CTOR(System)   //系统构造函数
    SUPER_CTOR(Device);
    FUNCTION_SETTING(init, vSystem_Init);
END_CTOR


void vSystemInit(OS_TCB *p_tcb, OS_PRIO prio, CPU_STK *p_stk_base, CPU_STK_SIZE stk_size)
{
    SystemTaskPrio = prio;
    SystemTaskStkSize = stk_size;

    psSystemTaskTCB = p_tcb;
    psSystemTaskStk = p_stk_base;
   
    System_Core();
}


System* System_Core()
{
    if(psSystem == NULL)
    {
        psSystem = (System*)System_new();
        psSystem->init(psSystem);
    }
    return psSystem;
}


