#include "user_mb_map_m.h"
#include "user_mb_dict_m.h"

#if MB_FUNC_READ_INPUT_ENABLED > 0

/***********************************************************************************
 * @brief  输入寄存器字典映射
* @param   ucSndAddr             从栈地址
 * @param  usRegAddr            寄存器地址
 * @param  pvRegInValue         寄存器指针
 * @return eMBMasterReqErrCode  错误码
 * @author laoc
 * @date 2019.01.22
 *************************************************************************************/
eMBMasterReqErrCode eMBMasterRegInMap(const sMBMasterInfo* psMBMasterInfo, UCHAR ucSndAddr, 
                                      USHORT usRegAddr, sMasterRegInData ** pvRegInValue)
{
	USHORT i;
	
    eMBMasterReqErrCode         eStatus  = MB_MRE_NO_ERR;
    sMBSlaveDevInfo*    psMBSlaveDevCur = psMBMasterInfo->psMBMasterDevsInfo->psMBSlaveDevCur ;     //当前从设备
    const sMBDevDataTable* psRegInputBuf = psMBSlaveDevCur->psDevDataInfo->psMBRegInTable;

    if(psMBSlaveDevCur->ucDevAddr != ucSndAddr) //如果当前从设备地址与要轮询从设备地址不一致，则更新从设备
    {
        psMBSlaveDevCur = psMBMasterGetDev(psMBMasterInfo, ucSndAddr);
        psMBMasterInfo->psMBMasterDevsInfo->psMBSlaveDevCur = psMBSlaveDevCur;
        psRegInputBuf = psMBSlaveDevCur->psDevDataInfo->psMBRegInTable;
    } 
   
	if( (psRegInputBuf->pvDataBuf == NULL) || (psRegInputBuf->usDataCount == 0)) //非空且数据点不为0
	{
		return MB_MRE_ILL_ARG;
	}
	
	switch(psMBSlaveDevCur->ucProtocolID)
	{
		 case DTU247_PROTOCOL_TYPE_ID:	
			    switch(usRegAddr)
			    {
			        case 12	:   i = 0 ;  break;
                    case 59	:   i = 1 ;  break;
                    case 60	:   i = 2 ;  break;
                    case 61	:   i = 3 ;  break;
                    case 62	:   i = 4 ;  break;
                    case 63	:   i = 5 ;  break;
         	
	                default:
			    		return MB_MRE_NO_REG;
			    	break;
			    }
		break;
		default: break;
	}
	
	*pvRegInValue = (sMasterRegInData*)(psRegInputBuf->pvDataBuf) + i; //指针赋值，这里传递的是个地址，指向目标寄存器所在数组位置
	return eStatus;
}
#endif


#if MB_FUNC_WRITE_HOLDING_ENABLED > 0 || MB_FUNC_WRITE_MULTIPLE_HOLDING_ENABLED > 0 \
    || MB_FUNC_READ_HOLDING_ENABLED > 0 || MB_FUNC_READWRITE_HOLDING_ENABLED > 0

/***********************************************************************************
 * @brief  保持寄存器字典映射
 * @param  ucSndAddr      从栈地址
 * @param  usRegAddr      寄存器地址
 * @param  pvRegInValue   寄存器指针
 * @return eMBErrorCode   错误码
 * @author laoc
 * @date 2019.01.22
 *************************************************************************************/
eMBMasterReqErrCode eMBMasterRegHoldingMap(const sMBMasterInfo* psMBMasterInfo, UCHAR ucSndAddr, 
                                           USHORT usRegAddr, sMasterRegHoldData ** pvRegHoldValue)
{
	USHORT i;
    
	eMBMasterReqErrCode         eStatus  = MB_MRE_NO_ERR;
    sMBSlaveDevInfo*    psMBSlaveDevCur = psMBMasterInfo->psMBMasterDevsInfo->psMBSlaveDevCur ;     //当前从设备
    const sMBDevDataTable*  psRegHoldBuf = psMBSlaveDevCur->psDevDataInfo->psMBRegHoldTable;
    
    if(psMBSlaveDevCur->ucDevAddr != ucSndAddr) //如果当前从设备地址与要轮询从设备地址不一致，则更新从设备
    {
        psMBSlaveDevCur = psMBMasterGetDev(psMBMasterInfo, ucSndAddr);
        psMBMasterInfo->psMBMasterDevsInfo->psMBSlaveDevCur = psMBSlaveDevCur;
        psRegHoldBuf = psMBSlaveDevCur->psDevDataInfo->psMBRegHoldTable;
    } 
    
	if( (psRegHoldBuf->pvDataBuf == NULL) || (psRegHoldBuf->usDataCount == 0)) //非空且数据点不为0
	{
		return MB_MRE_ILL_ARG;
	}
	
	switch (psMBSlaveDevCur->ucProtocolID )
	{
		case DTU200_PROTOCOL_TYPE_ID:
			switch(usRegAddr)
			{
		        case 0	 : i = 0  ; break;
                case 1	 : i = 1  ; break;
   
				default:
					return MB_MRE_NO_REG;
				break;
			}
		break;
		
        case SEC_PROTOCOL_TYPE_ID:	
			    switch(usRegAddr)
			    {
			        case 0	:   i = 0 ;  break;
                    case 1	:   i = 1 ;  break;
                    case 9	:   i = 2 ;  break;
                    case 23	:   i = 3 ;  break;
                    case 180:   i = 4 ;  break;

	                default:
			    		return MB_MRE_NO_REG;
			    	break;
			    }
		break;			
	    default: break;
        }
	*pvRegHoldValue = (sMasterRegHoldData*)(psRegHoldBuf->pvDataBuf) + i ;
	return eStatus;
} 
#endif

#if MB_FUNC_READ_COILS_ENABLED > 0 || MB_FUNC_WRITE_COIL_ENABLED > 0 || MB_FUNC_WRITE_MULTIPLE_COILS_ENABLED > 0

/***********************************************************************************
 * @brief  线圈字典映射
 * @param   ucSndAddr      从栈地址
 * @param  usCoilAddr      线圈地址
 * @param  pvCoilValue     线圈指针
 * @return eMBErrorCode    错误码
 * @author laoc
 * @date 2019.01.22
 *************************************************************************************/
eMBMasterReqErrCode eMBMasterCoilMap(const sMBMasterInfo* psMBMasterInfo, UCHAR ucSndAddr, 
                                     USHORT usCoilAddr, sMasterBitCoilData ** pvCoilValue)
{
	UCHAR i;
	USHORT iRegIndex, iRegBitIndex, iBit;
	
	eMBMasterReqErrCode          eStatus = MB_MRE_NO_ERR;
    sMBSlaveDevInfo*    psMBSlaveDevCur = psMBMasterInfo->psMBMasterDevsInfo->psMBSlaveDevCur;     //当前从设备
    const sMBDevDataTable*     psCoilBuf = psMBSlaveDevCur->psDevDataInfo->psMBCoilTable;
	
    if(psMBSlaveDevCur->ucDevAddr != ucSndAddr) //如果当前从设备地址与要轮询从设备地址不一致，则更新从设备
    {
        psMBSlaveDevCur = psMBMasterGetDev(psMBMasterInfo, ucSndAddr);
        psMBMasterInfo->psMBMasterDevsInfo->psMBSlaveDevCur = psMBSlaveDevCur;
        psCoilBuf = psMBSlaveDevCur->psDevDataInfo->psMBCoilTable;
    } 
	if( (psCoilBuf->pvDataBuf == NULL) || (psCoilBuf->pvDataBuf == 0)) //非空且数据点不为0
	{
		return MB_MRE_ILL_ARG;
	}
	switch ( psMBSlaveDevCur->ucProtocolID )
	{
		case SEC_PROTOCOL_TYPE_ID:
			switch(usCoilAddr)
			{
				case 17: i= 0;break;
				case 33: i= 1;break;
				case 37: i= 2;break;
				case 38: i= 3;break;
				case 43: i= 4;break;
				
				case 44: i= 5;break;
				case 47: i= 6;break;
				case 76: i= 7;break;
				
				case 77: i= 8;break;
				case 78: i= 9;break;
				case 79: i= 10;break;
				case 80: i= 11;break;
				case 81: i= 12;break;
				case 82: i= 13;break;
				case 83: i= 14;break;
				case 84: i= 15;break;
				case 85: i= 16;break;
				default:
					return MB_MRE_NO_REG;
				break;
			}
		break;
		default: break;
	}
	
	*pvCoilValue = (sMasterBitCoilData*)(psCoilBuf->pvDataBuf) + i ; 	
	return eStatus;
}    
#endif

#if MB_FUNC_READ_DISCRETE_INPUTS_ENABLED > 0
/***********************************************************************************
 * @brief  离散量字典映射
 * @param  ucSndAddr       从栈地址
 * @param  usCoilAddr      线圈地址
 * @param  pvCoilValue     线圈指针
 * @return eMBErrorCode    错误码
 * @author laoc
 * @date 2019.01.22
 *************************************************************************************/
eMBMasterReqErrCode eMBMasterDiscreteMap(const sMBMasterInfo* psMBMasterInfo, UCHAR ucSndAddr, 
                                         USHORT usDiscreteAddr, sMasterBitDiscData ** pvDiscreteValue)
{
	UCHAR i;
	USHORT iRegIndex, iRegBitIndex,iBit;
	
	eMBMasterReqErrCode          eStatus = MB_MRE_NO_ERR;
    sMBSlaveDevInfo*    psMBSlaveDevCur = psMBMasterInfo->psMBMasterDevsInfo->psMBSlaveDevCur;     //当前从设备
    const sMBDevDataTable*     psDiscInBuf = psMBSlaveDevCur->psDevDataInfo->psMBDiscInTable;
	
    if(psMBSlaveDevCur->ucDevAddr != ucSndAddr) //如果当前从设备地址与要轮询从设备地址不一致，则更新从设备
    {
        psMBSlaveDevCur = psMBMasterGetDev(psMBMasterInfo, ucSndAddr);
        psMBMasterInfo->psMBMasterDevsInfo->psMBSlaveDevCur = psMBSlaveDevCur;
        psDiscInBuf = psMBSlaveDevCur->psDevDataInfo->psMBDiscInTable;
    } 
	if( (psDiscInBuf->pvDataBuf == NULL) || (psDiscInBuf->usDataCount == 0)) //非空且数据点不为0
	{
		return MB_MRE_ILL_ARG;
	}
	switch ( psMBSlaveDevCur->ucProtocolID )
	{
		case 0:
			switch(usDiscreteAddr)
			{
				case 0: i= 0;break;
				default:
					return MB_MRE_NO_REG;
				break;
			}
		break;
			
		default: break;
	}
	*pvDiscreteValue = (sMasterBitDiscData*)(psDiscInBuf->pvDataBuf)  + i;
	return eStatus;
}    

#endif

/***********************************************************************************
 * @brief  字典初始化,初始化各点位的原始值
 * @return eMBErrorCode 错误码
 * @author laoc
 * @date 2019.01.22
 *************************************************************************************/
void eMBMasterTableInit(const sMBMasterInfo* psMBMasterInfo)
{
//	USHORT iIndex, iBit, iTable;
//	eMBMasterReqErrCode    eStatus = MB_MRE_NO_ERR;
//	
//	const sMBDevDataTable*  psRegHoldBuf;
//	sMasterRegHoldData*     psRegHoldValue;
//	
//	const sMBDevDataTable*  psCoilBuf;
//	sMasterBitCoilData*     psCoilValue;

//	sMBMasterDictInfo* psMBDictInfo = psMBMasterInfo->psMBMasterDictInfo;          //通讯字典
//	sMBMasterDevsInfo* psMBDevsInfo = psMBMasterInfo->psMBMasterDevsInfo;          //从设备状态

//	USHORT nSlaveTypes = psMBDevsInfo->ucSlaveDevTypes;                            //从设备类型数
//	
//#if MB_FUNC_WRITE_HOLDING_ENABLED > 0 || MB_FUNC_WRITE_MULTIPLE_HOLDING_ENABLED > 0 \
//    || MB_FUNC_READ_HOLDING_ENABLED > 0 || MB_FUNC_READWRITE_HOLDING_ENABLED > 0
//	
//	for(iTable = 0; iTable < nSlaveTypes; iTable++)
//	{
//		psRegHoldBuf = &(psMBDictInfo->psMBRegHoldTable[iTable]);
//		
//		if( (psRegHoldBuf->pvDataBuf != NULL) && (psRegHoldBuf->usDataCount !=0) )
//		{
//			for(iIndex = 0; iIndex < psRegHoldBuf->usDataCount; iIndex++)
//		    {
//		    	psRegHoldValue = (sMasterRegHoldData*)(psRegHoldBuf->pvDataBuf) + iIndex;
//		    	if(psRegHoldValue->Value != NULL)
//		    	{
//		    		if (psRegHoldValue->DataType == uint16)
//		    		{
//		    			psRegHoldValue->PreValue = *(USHORT*)psRegHoldValue->Value;
//		    		}
//		    		else if(psRegHoldValue->DataType == uint8)
//		    		{
//		    			psRegHoldValue->PreValue = (USHORT)(*(UCHAR*)psRegHoldValue->Value);
//		    		}
//		    		else if (psRegHoldValue->DataType == int16)
//		    		{
//		    			psRegHoldValue->PreValue = (USHORT)(*(SHORT*)psRegHoldValue->Value);
//		    		}
//		    		else if(psRegHoldValue->DataType == int8)
//		    		{
//		    			psRegHoldValue->PreValue = (USHORT)(*(int8_t*)psRegHoldValue->Value);
//		    		}  
//		    	}
//		    }
//		}	
//	}	
//#endif
//	
//#if MB_FUNC_READ_COILS_ENABLED > 0 || MB_FUNC_WRITE_COIL_ENABLED > 0 || MB_FUNC_WRITE_MULTIPLE_COILS_ENABLED > 0
//	
//	for(iTable = 0; iTable < nSlaveTypes; iTable++)
//	{
//		psCoilBuf = &(psMBDictInfo->psMBCoilTable[iTable]);
//		
//		if( (psCoilBuf->pvDataBuf != NULL) || (psCoilBuf->usDataCount !=0) )
//		{
//			for(iIndex = 0; iIndex < psCoilBuf->usDataCount; iIndex++)
//		    {
//		    	psCoilValue = (sMasterBitCoilData*)(psCoilBuf->pvDataBuf) + iIndex;
//				
//                if( psCoilValue->Value  != NULL )
//		    	{
//		    		psCoilValue->PreValue  = *(UCHAR*)psCoilValue->Value;
//		    	}
//		    }
//		}
//	}
//#endif	
		
}
