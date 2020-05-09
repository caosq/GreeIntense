#ifndef _USER_MB_DICT_M_H
#define _USER_MB_DICT_M_H

#include "mbframe.h"
#include "mbconfig.h"
#include "port.h"

/* -----------------------Master Defines -------------------------------------*/
#define DTU_PROTOCOL_VERSIPON     0x0B    //DTU通讯协议版本号 V1.1

#define MB_MASTER_MIN_DEV_ADDR    1     //主栈从设备最小通讯地址
#define MB_MASTER_MAX_DEV_ADDR    20    //主栈从设备最大通讯地址


typedef enum      /* 测试模式 */        
{
	READ_REG_IN,
    READ_REG_HOLD,
	WRITE_REG_HOLD,
}eMasterCmdMode;

typedef struct        /* 主栈保持寄存器数据结构 */
{
	const USHORT      usAddr;            //地址
    const UCHAR       ucDataType;        //数据类型
    volatile USHORT   usPreVal;          //先前值
    const LONG        lMinVal;           //最小值
    const LONG        lMaxVal;           //最大值
    const UCHAR       ucAccessMode;      //访问权限
    const float       fTransmitMultiple; //传输因子
    void * const      pvValue;           //变量指针    
}sMasterRegHoldData;     		

typedef struct        /* 主栈字典保持寄存器数据结构 */
{
	const USHORT    usAddr;             //地址
    const UCHAR     ucDataType;         //数据类型
    const LONG      lMinVal;            //最小值
    const LONG      lMaxVal;            //最大值
    const UCHAR     ucAccessMode;       //访问权限
    const float     fTransmitMultiple;  //传输因子
    void * const    pvValue;            //变量指针    
}sMasterRegInData; 

typedef struct       /* 主栈字典线圈数据结构 */
{
    const USHORT    usAddr;           //地址
    volatile UCHAR  ucPreVal;         //先前值
    const UCHAR     ucAccessMode;     //访问权限
	UCHAR* const    pvValue;          //变量指针   
}sMasterBitCoilData;  

typedef struct       /* 主栈字典离散量数据结构 */
{
    const USHORT   usAddr;           //地址
    const UCHAR    ucAccessMode;     //访问权限
	UCHAR* const   pvValue;          //变量指针      
}sMasterBitDiscData;  


typedef struct   /* 用于测试从栈的指令数据结构 */       
{
	const USHORT          usValue;       //数值
	const eMasterCmdMode  eCmdMode;      //测试模式
	const UCHAR           ucAddr;        //测试点位通讯地址
}sMBTestDevCmd; 

typedef struct   /* 主栈字典数据列表结构 */
{
    const void* const  pvDataBuf;             //协议数据域
	const USHORT       usStartAddr;           //起始地址
	const USHORT       usEndAddr;             //末尾地址
    const USHORT       usDataCount;           //协议点位总数
}sMBDevDataTable;

typedef USHORT (*psMBDevDataMapIndex)(eDataType eDataType, UCHAR ucProtocolID,  USHORT usAddr); //字典映射函数

typedef struct sMBSlaveDevCommData   /* 从设备通讯字典数据结构 */  
{
	sMBDevDataTable      sMBRegInTable;       //输入寄存器数据表
	sMBDevDataTable      sMBRegHoldTable;     //保持寄存器数据表
	sMBDevDataTable      sMBCoilTable;        //线圈数据表
	sMBDevDataTable      sMBDiscInTable;      //离散量数据表
    sMBTestDevCmd        sMBDevCmdTable;      //用于测试从设备状态命令表
    
    const  UCHAR         ucProtocolID;        //协议ID
    psMBDevDataMapIndex  psMBDevDataMapIndex; //字典映射函数
    
    struct sMBSlaveDevCommData*   pNext;         //下一个数据表
}sMBSlaveDevCommData; 

typedef struct sMBSlaveDev   /* 从设备信息列表 */   
{
    UCHAR   ucDevAddr;             //设备通讯地址
    UCHAR   ucDevCurTestAddr;      //设备当前测试地址
	UCHAR   ucRetryTimes;          //测试间隔
	UCHAR   ucOnLine;              //是否在线
	UCHAR   ucDataReady;           //数据是否准备好
    UCHAR   ucProtocolID;          //协议ID
	UCHAR   ucSynchronized;        //是否同步
    UCHAR   ucDevOnTimeout;        //是否处于延时
    OS_TMR  sDevOfflineTmr;        //设备掉线定时器
    
    sMBSlaveDevCommData* psDevDataInfo;     //从设备数据域
    sMBSlaveDevCommData* psDevCurData;      //从设备当前数据域   
    
    struct sMBSlaveDev*  pNext;             //下一个设备节点
    struct sMBSlaveDev*  pLast;             //尾设备节点
}sMBSlaveDev; 

typedef struct    /* 主栈从设备状态结构  */
{
    UCHAR         ucSlaveDevCount;    //从设备总数量        
    UCHAR         ucSlaveDevMinAddr;  //从设备最小通讯地址
	UCHAR         ucSlaveDevMaxAddr;  //从设备最大通讯地址
    
    UCHAR         ucDevAddrOccupy[MB_MASTER_MAX_DEV_ADDR - MB_MASTER_MIN_DEV_ADDR];
    
	sMBSlaveDev*  psMBSlaveDevsList;  //当前在线从设备列表
    sMBSlaveDev*  psMBSlaveDevCur;    //当前活动的设备
       
}sMBMasterDevsInfo; 

#endif