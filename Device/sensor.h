#ifndef _SENSOR_H_
#define _SENSOR_H_

#include "device.h"

#define SENSOR_SAMPLE_NUM   10
#define SENSOR_REG_HOLD_NUM 3

typedef enum   /*传感器类型*/
{
   TYPE_IO = 0,     //IO传输
   TYPE_COM = 1     //通讯
}eSensorType;


ABS_CLASS(Sensor)          /*传感器*/ 
{
    EXTENDS(Device);
    IMPLEMENTS(IDevCom);            //设备通讯接口
    
    uint8_t              ucSampleIndex;    //采样次数
    
    eSensorType          eType;            //传感器类型
    OS_TMR               sSensorTmr;       //传感器内部定时器
    
    sMBMasterInfo*       psMBMasterInfo;   //通讯主栈
    sMBSlaveDevCommData  sDevCommData;     //通讯数据表
    sMBSlaveDev          sMBSlaveDev;      //本通讯设备
    
    OS_SEM               sSensorValChange; //变量变化事件
    
    sMasterRegHoldData   sSensor_RegHoldBuf[SENSOR_REG_HOLD_NUM];  //保持寄存器数据域
    
    void (*init)(Sensor* pt, sMBMasterInfo* psMBMasterInfo);
    void (*monitorRegist)(Sensor* pt);
    
    void (*timeoutInd)(void * p_tmr, void * p_arg);  //定时器中断服务函数
};

CLASS(CO2Sensor)          /*CO2传感器*/  
{
    EXTENDS(Sensor);
    
    uint16_t     usMaxPPM;        //量程上限 = 实际值*10
    uint16_t     usMinPPM;        //量程下限 = 实际值*10
    uint16_t     usCO2PPM;        //实际值   
    uint16_t     usAvgCO2PPM;     //平均值
    uint16_t     usTotalCO2PPM;   //所有采样值之和
    BOOL         xCO2Error;       //CO2故障
    
    uint16_t     usSampleCO2PPM[SENSOR_SAMPLE_NUM];    
};

CLASS(TempHumiSensor)          /*温湿度传感器*/  
{
    EXTENDS(Sensor);
    
    int16_t      sMaxTemp;     //量程上限 = 实际值*10
    int16_t      sMinTemp;     //量程下限 = 实际值*10
    int16_t      sTemp;        //实际值  
    int16_t      sAvgTemp;     //平均值
    int16_t      sTotalTemp;   //所有采样值之和
    BOOL         xTempError;   //温度故障
     
    uint16_t     usMaxHumi;        //量程上限 = 实际值*10
    uint16_t     usMinHumi;        //量程下限 = 实际值*10
    uint16_t     usHumi;           //实际值  
    uint16_t     usAvgHumi;        //平均值
    uint16_t     usTotalHumi;      //所有采样值之和
    BOOL         xHumiError;       //湿度故障
    
    int16_t      sSampleTemp[SENSOR_SAMPLE_NUM];
    uint16_t     usSampleHumi[SENSOR_SAMPLE_NUM];

};

#endif