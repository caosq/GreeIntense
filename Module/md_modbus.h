#ifndef _MD_MODBUS_H_
#define _MD_MODBUS_H_

#define MODBUS_DEBUG                            0 
#define MODBUS_ACCESS_ADDR                      1     //自动寻址
#define MODBUS_ACCESS_ADDR_INTERVAL_TIMES       10     //自动寻址轮询间隔次数

void vMBScanSlaveDevTask(void *p_arg);

#endif
