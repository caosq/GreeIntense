#include "md_monitor.h"
#include "md_event.h"

#define MONITOR_DATA_MAX_NUM        50     //最大可监控点位数，根据实际情况调整
#define MONITOR_POLL_INTERVAL_MS    50     //

sMonitorInfo* MonitorList = NULL;
sMonitorInfo  MonitorBuf[MONITOR_DATA_MAX_NUM];

uint16_t DataID = 0;

sMonitorInfo* psMonitorRegist(void* pvVal, OS_TCB* psTCB, OS_SEM* psSem)
{
    sMonitorInfo* psMonitor = NULL;
    sMonitorInfo* psMonitorInfo = NULL;
    
    if(DataID >= MONITOR_DATA_MAX_NUM)
    {
        return NULL;
    }
    psMonitorInfo = &MonitorBuf[DataID];
    DataID++;
    
    psMonitorInfo->pvVal    = pvVal;

    psMonitorInfo->psSem    = psSem;
    psMonitorInfo->usDataId = DataID + 1;   //全局标示
    psMonitorInfo->sDataBuf = *(int32_t*)pvVal;
    psMonitorInfo->pNext    = NULL;
 
    if(MonitorList == NULL)
    {
        MonitorList = psMonitorInfo;
    }
    else
    {
        for(psMonitor = MonitorList; psMonitor != NULL; psMonitor = psMonitor->pNext)
        {
            if(psMonitor->pNext == NULL)
            {
                psMonitor->pNext = psMonitorInfo;
                break;
            }
        }
    }
    return psMonitorInfo;
}

void vMonitorPollTask(void *p_arg)
{
    CPU_TS  ts = 0;
    OS_ERR err = OS_ERR_NONE;
    sMonitorInfo* psMonitor = NULL;
    
    OS_TCB* psEventTCB = psEventGetTCB();
    
    while(DEF_TRUE)
	{
        (void)OSTimeDlyHMSM(0, 0, 0, MONITOR_POLL_INTERVAL_MS, OS_OPT_TIME_HMSM_STRICT, &err);	
        for(psMonitor = MonitorList; psMonitor != NULL; psMonitor = psMonitor->pNext)
        {
            if(psMonitor->sDataBuf != *(int32_t*)psMonitor->pvVal)
            {
                (void)OSTaskQPost(psEventTCB, (void*)psMonitor, sizeof(sMonitorInfo), OS_OPT_POST_ALL, &err);  //转发到特定的Task
                psMonitor->sDataBuf = *(int32_t*)psMonitor->pvVal;                
            }
        }
    }
}
