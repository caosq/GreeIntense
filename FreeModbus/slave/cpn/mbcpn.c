/* 
 * FreeModbus Libary: A portable Modbus implementation for Modbus ASCII/RTU.
 * Copyright (c) 2006 Christian Walter <wolti@sil.at>
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
 * File: $Id: mbrtu.c,v 1.18 2007/09/12 10:15:56 wolti Exp $
 */

/* ----------------------- System includes ----------------------------------*/
#include "stdlib.h"
#include "string.h"

/* ----------------------- Platform includes --------------------------------*/
#include "port.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbcpn.h"
#include "mbframe.h"
#include "my_rtt_printf.h"
#include "mbport.h"
#include "app_led.h"

#if MB_SLAVE_CPN_ENABLED > 0

/* ----------------------- Defines ------------------------------------------*/
#define MB_CPN_FRAME_SIZE_MIN          22      /*!< Minimum size of a Modbus CPN frame. */
#define MB_CPN_FRAME_SIZE_MAX          1022    /*!< Maximum size of a Modbus CPN frame. */
#define MB_CPN_FRAME_DATA_SIZE_MAX     1000    /*!< Maximum size of data size of a Modbus CPN frame. */
#define MB_CPN_FRAME_SIZE_CRC          2       /*!< Size of CRC field in CPN frame. */
#define MB_CPN_FRAME_HEAD_SIZE         18      /*!< Size of CPN frame head in CPN frame. */
#define MB_CPN_FRAME_TERM_SIZE         2       /*!< Size of CPN frame terminal in CPN frame. */

#define MB_CPN_FRAME_DATA_SIZE_OFF     4       /*!< Offset of data size in CPN frame. */
#define MB_CPN_FRAME_CRC_OFF           10      /*!< Offset of crc in CPN frame. */
#define MB_CPN_FRAME_NAME_OFF          12      /*!< Offset of frame name in CPN frame. */
#define MB_CPN_FRAME_FUNC_CODE_OFF     18      /*!< Offset of function code in CPN frame. */
#define MB_CPN_FRAME_SOUR_ADDR_OFF     20      /*!< Offset of source address in CPN frame. */
#define MB_CPN_FRAME_DEST_ADDR_OFF     21      /*!< Offset of destination address in CPN frame. */

#define MB_CPN_FRAME_PDU_OFF           18      /*!< Offset of PDU in cpn frame. */
#define MB_CPN_FRAME_NAME_VALUE_MAX    60000   /*!< Maximum value of a Modbus CPN frame name. */


/* ----------------------- Type definitions ---------------------------------*/
typedef enum
{
    STATE_RX_INIT,              /*!< Receiver is in initial state. */
    STATE_RX_IDLE,              /*!< Receiver is in idle state. */
    STATE_RX_RCV,               /*!< Frame is beeing received. */
    STATE_RX_ERROR              /*!< If the frame is invalid. */
} eMBRcvState;

typedef enum
{
    STATE_TX_IDLE,              /*!< Transmitter is in idle state. */
    STATE_TX_XMIT               /*!< Transmitter is in transfer state. */
} eMBSndState;

/* ----------------------- Static variables ---------------------------------*/
static volatile eMBSndState eSndState;
static volatile eMBRcvState eRcvState;

volatile UCHAR  ucCPNBuf[MB_CPN_FRAME_SIZE_MAX];

static volatile UCHAR *pucSndBufferCur;
static volatile USHORT usSndBufferCount;

static volatile USHORT usRcvBufferPos;

static volatile UCHAR *pucCPNHead;

static volatile USHORT usCPNName = 0;

/**********************************************************************
 * @brief  cpn模式协议栈初始化
 * @param  *Uart           UART配置
 * @return eMBErrorCode    协议栈错误
 * @author laoc
 * @date 2019.01.22
 *********************************************************************/
eMBErrorCode
eMBCPNInit( UART_Def *Uart )
{
    eMBErrorCode    eStatus = MB_ENOERR;
    ULONG           usTimerT35_50us;

    ENTER_CRITICAL_SECTION( );

    /* Modbus RTU uses 8 Databits. */
    if( xMBPortSerialInit( Uart ) != TRUE )           //串口初始化
    {
        eStatus = MB_EPORTERR;
    }
    else
    {
        /* If baudrate > 19200 then we should use the fixed timer values
         * t35 = 1750us. Otherwise t35 must be 3.5 times the character time.
         */
        if( (Uart->UARTCfg.Baud_rate)  > 19200 )
        {
            usTimerT35_50us = 35;       /* 1800us. */
        }
        else
        {
            /* The timer reload value for a character is given by:
             *
             * ChTimeValue = Ticks_per_1s / ( Baudrate / 11 )
             *             = 11 * Ticks_per_1s / Baudrate
             *             = 220000 / Baudrate
             * The reload for t3.5 is 1.5 times this value and similary
             * for t3.5.
             */
            usTimerT35_50us = ( 7UL * 220000UL ) / ( 2UL * (Uart->UARTCfg.Baud_rate) );
        }
        if( xMBPortTimersInit( ( USHORT ) usTimerT35_50us ) != TRUE )           //t35超时定时器
        {
            eStatus = MB_EPORTERR;
        }
    }
    EXIT_CRITICAL_SECTION(  );

    return eStatus;
}

/**********************************************************************
 * @brief  启动CPN模式协议栈
 *         1. 设置接收状态机eRcvState为STATE_RX_INIT；
 *         2. 使能串口接收,禁止串口发送,作为从机,等待主机传送的数据;
 *         3. 开启定时器，3.5T时间后定时器发生第一次中断,此时eRcvState为STATE_RX_INIT,
 *            上报初始化完成事件,然后设置eRcvState为空闲STATE_RX_IDLE;

 *         4. 每次进入3.5T定时器中断,定时器被禁止，等待串口有字节接收后，才使能定时器;
 * @author laoc
 * @date 2019.01.22
 *********************************************************************/
void
eMBCPNStart( void )
{
    ENTER_CRITICAL_SECTION(  );              //关全局中断
    /* Initially the receiver is in the state STATE_RX_INIT. we start
     * the timer and if no character is received within t3.5 we change
     * to STATE_RX_IDLE. This makes sure that we delay startup of the
     * modbus protocol stack until the bus is free.
     */
    eRcvState = STATE_RX_INIT;
    vMBPortSerialEnable( TRUE, FALSE );    //从栈等待数据，开启串口接收，发送未开启
    vMBPortTimersEnable(  );               //启动定时器

    EXIT_CRITICAL_SECTION(  );             //开全局中断
}

/**********************************************************************
 * @brief  关闭CPN模式协议栈
 * @author laoc
 * @date 2019.01.22
 *********************************************************************/
void
eMBCPNStop( void )
{
    ENTER_CRITICAL_SECTION(  );
    vMBPortSerialEnable( FALSE, FALSE );
    vMBPortTimersDisable(  );
    EXIT_CRITICAL_SECTION(  );
}

/**********************************************************************
 * @brief  报文接收处理
 *         1. eMBPoll函数轮询到EV_FRAME_RECEIVED事件时,调用peMBFrameReceiveCur()
 *         2. 从一帧数据报文中，取得CPN源地址给pucSourAddress、目标地址给pucDestAddress、PDU报文的长度给pusLength
 *         3. PDU报文的首地址给pucFrame，函数*形参全部为地址传递
 * @param  pucSourAddress  源地址
 * @param  pucDestAddress  目标地址
 * @param  pucFrame        CPN数据帧指针
 * @param  pusLength       PDU长度
 * @return eMBErrorCode    协议栈错误 
 * @author laoc
 * @date 2019.01.22
 *********************************************************************/
eMBErrorCode
eMBCPNReceive( UCHAR * pucSourAddress, UCHAR * pucDestAddress, UCHAR ** pucFrame, USHORT * pusLength )
{
   /*  eMBCPNReceive函数完成了CRC校验、帧数据地址和长度的赋值，便于给上层进行处理！之后
    *  eMBPoll函数发送 ( void )xMBPortEventPost( EV_EXECUTE )事件。在EV_EXECUTE 事件中，从
    *  站对接收到的数据进行处理，包括根据功能码寻找功能函数处理报文和调用eStatus =
    *  peMBFrameSendCur(ucSourAddress, ucDestAddress, ucMBFrame, usLength ) 发送应答报文。*/

    BOOL            xFrameReceived = FALSE;
    eMBErrorCode    eStatus = MB_ENOERR;
    USHORT          usCRCData = 0;
	
    ENTER_CRITICAL_SECTION(  );
    assert_param( usRcvBufferPos < MB_CPN_FRAME_SIZE_MAX );     //断言宏，判断接收到的字节数<256，如果>256，终止程序

    /* Length and CRC check */
    if( usRcvBufferPos >= MB_CPN_FRAME_SIZE_MIN )
    {
		/* Check the CRC data*/
		
		usCRCData =  ( (USHORT) ucCPNBuf[MB_CPN_FRAME_CRC_OFF] ) | ( (USHORT) ucCPNBuf[MB_CPN_FRAME_CRC_OFF + 1] << 8 );
		
		ucCPNBuf[MB_CPN_FRAME_CRC_OFF] = 0;                     
		ucCPNBuf[MB_CPN_FRAME_CRC_OFF + 1] = 0;
		
		if( usCRCData == usMBCRC16( ( UCHAR * )( ucCPNBuf + MB_CPN_FRAME_TERM_SIZE ), usRcvBufferPos - MB_CPN_FRAME_TERM_SIZE ) )
		{
			/* Save the address field. All frames are passed to the upper layed
			 * and the decision if a frame is used is done there.
			 */
			*pucSourAddress = ucCPNBuf[MB_CPN_FRAME_SOUR_ADDR_OFF];    //取源地址和目标地址
			*pucDestAddress = ucCPNBuf[MB_CPN_FRAME_DEST_ADDR_OFF];
			
			/* Total length of Modbus-PDU is Modbus-Serial-Line-PDU minus
			 * size of address field and CRC checksum.
			 */
			*pusLength = ( USHORT )( usRcvBufferPos - MB_CPN_FRAME_HEAD_SIZE );  

			/* Return the start of the Modbus PDU to the caller. */
			*pucFrame = ( UCHAR * ) &ucCPNBuf[MB_CPN_FRAME_HEAD_SIZE];      //pucFrame指向PDU起始位置
			pucCPNHead =( UCHAR * ) ucCPNBuf;
			xFrameReceived = TRUE;
	    }
		else
		{
			eStatus = MB_EIO;
		}
    }
    else
    {
        eStatus = MB_EIO;
    }

    EXIT_CRITICAL_SECTION(  );
    return eStatus;
}

/**********************************************************************
 * @brief  报文发送处理
 *         1. 对响应报文PDU前面加上从机地址;
 *         2. 对响应报文PDU后加上CRC校；
 *         3. 使能发送，启动传输;
 * @param  pucSourAddress   源地址
 * @param  pucDestAddress   目标地址
 * @param  pucFrame         CPN数据帧指针
 * @param  pusLength        PDU长度
 * @return eMBErrorCode     协议栈错误  
 * @author laoc
 * @date 2019.01.22
 *********************************************************************/
eMBErrorCode
eMBCPNSend( UCHAR ucSourAddress, UCHAR ucDestAddress, const UCHAR * pucFrame, USHORT usLength )
{
    /* 在 eMBCPNSend函数中会调用串口发送数据，在进入串口发送中断后会调用xMBCPNTransmitFSM
     * 发送状态机函数发送应答报文。*/

    eMBErrorCode    eStatus = MB_ENOERR;
    USHORT          usCRC16;
    OS_ERR err = OS_ERR_NONE;
	
    ENTER_CRITICAL_SECTION(  );

    /* Check if the receiver is still in idle state. If not we where to
     * slow with processing the received frame and the master sent another
     * frame on the network. We have to abort sending the frame.
     */
	
	/* 检查接收器是否空闲。STATE_RX_IDLE状态在T35定时中断中被设置。若接收器不在空闲状态，
     * 说明主机正在向ModBus网络发送另一个帧。我们必须中断本次回应帧的发送。
     */
    if( eRcvState == STATE_RX_IDLE )
    {
       
		/* First before the PDU is the cpn head. */
        pucSndBufferCur = (UCHAR*)(pucFrame - MB_CPN_FRAME_HEAD_SIZE);             
        usSndBufferCount = MB_CPN_FRAME_HEAD_SIZE;
		
		/* Set the data size of PDU. */
		
		if(usLength > MB_CPN_FRAME_DATA_SIZE_MAX)
		{
			return MB_EILLSTATE;
		}
		
		*(pucSndBufferCur + MB_CPN_FRAME_DATA_SIZE_OFF) = (UCHAR)( usLength & 0xFF );    //设置CPN数据长度
		*(pucSndBufferCur + MB_CPN_FRAME_DATA_SIZE_OFF + 1) = (UCHAR)( usLength >> 8 );
		
		/* Set the CRC code as zero of PDU. */
		*(pucSndBufferCur + MB_CPN_FRAME_CRC_OFF)     = 0;         //CRC位先置零
		*(pucSndBufferCur + MB_CPN_FRAME_CRC_OFF + 1) = 0;
		
		if(usCPNName < MB_CPN_FRAME_NAME_VALUE_MAX)                //CPN变量名称，每发一条则增加1, 范围 1~60000
		{
			usCPNName += 1;
		}
		else
		{
			usCPNName = 1;
		}
		*( pucSndBufferCur + MB_CPN_FRAME_FUNC_CODE_OFF ) = MB_CPN_FUNC_WRITE_CODE;    //CPN操作类型 05：写   06：读
		
		*( pucSndBufferCur + MB_CPN_FRAME_NAME_OFF ) = (UCHAR)usCPNName ;
		*( pucSndBufferCur + MB_CPN_FRAME_NAME_OFF + 1 ) = (UCHAR)(usCPNName >> 8) ;
			
        /* Now copy the Modbus-PDU into the Modbus-Serial-Line-PDU. */  
        *( pucSndBufferCur + MB_CPN_FRAME_SOUR_ADDR_OFF) = ucSourAddress;        //在协议数据单元加入源地址（本设备地址）
		*( pucSndBufferCur + MB_CPN_FRAME_DEST_ADDR_OFF) = ucDestAddress;        //在协议数据单元加入目标地址（目标设备地址）
		
        usSndBufferCount += usLength;

		/* Calculate CRC16 checksum for Modbus-Serial-Line-PDU. */
        usCRC16 = usMBCRC16( (UCHAR*)(pucSndBufferCur + MB_CPN_FRAME_TERM_SIZE), usSndBufferCount - MB_CPN_FRAME_TERM_SIZE );
        *( pucSndBufferCur + MB_CPN_FRAME_CRC_OFF )     = ( UCHAR )( usCRC16 & 0xFF );  //CRC校验
        *( pucSndBufferCur + MB_CPN_FRAME_CRC_OFF + 1 ) = ( UCHAR )( usCRC16 >> 8 );

		 /* Activate the transmitter. */
        eSndState = STATE_TX_XMIT;              //发送状态
		vMBPortSerialEnable( FALSE, TRUE );     //使能发送，禁止接收	

		//插入代码启动第一次发送，这样才可以进入发送完成中断
		
        (void)xMBPortSerialPutByte( (CHAR)(*pucSndBufferCur) );
        pucSndBufferCur++;
        usSndBufferCount--;
    }
    else
    {
        eStatus = MB_EIO;
    }
    EXIT_CRITICAL_SECTION(  );
    return eStatus;
}

/**********************************************************************
 * @brief  串口接收数据
 *         1. 将接收到的数据存入ucCPNBuf[]中
 *         2. usRcvBufferPos为全局变量，表示接收数据的个数
 *         3. 每接收到一个字节的数据，3.5T定时器清0
 * @return BOOL   
 * @author laoc
 * @date 2019.01.22
 *********************************************************************/
BOOL
xMBCPNReceiveFSM( void )
{
    /*在串口中断前，状态机为eRcvState=STATE_RX_IDLE，接收状态机开始后，读取uart串口缓存中的数据，并进入STATE_RX_IDLE分支中存储一次数据后开启定时器，
    然后进入STATE_RX_RCV分支继续接收后续的数据，直至定时器超时！如果没有超时的话，状态不会转换，将还可以继续接收数据。超时之后，
    在T3.5超时函数xMBCPNTimerT35Expired 中将发送EV_FRAME_RECEIVED事件。然后eMBPoll函数将会调用eMBCPNReceive函数。*/

    BOOL            xTaskNeedSwitch = FALSE;
    UCHAR           ucByte;

    assert_param( eSndState == STATE_TX_IDLE );    //确保没有数据在发送

    /* Always read the character. */
    ( void )xMBPortSerialGetByte( ( CHAR * ) & ucByte );   //从串口数据寄存器读取一个字节数据

    switch ( eRcvState )
    {
        /* If we have received a character in the init state we have to
         * wait until the frame is finished.
         */
    case STATE_RX_INIT:
        vMBPortTimersEnable( );             //开启3.5T定时器
        break;

        /* In the error state we wait until all characters in the
         * damaged frame are transmitted.
         */
    case STATE_RX_ERROR:                   //数据帧被损坏，重启定时器，不保存串口接收的数据
        vMBPortTimersEnable( );
        break;

        /* In the idle state we wait for a new character. If a character
         * is received the t1.5 and t3.5 timers are started and the
         * receiver is in the state STATE_RX_RECEIVCE.
         */
    case STATE_RX_IDLE:                  // 接收器空闲，开始接收，进入STATE_RX_RCV状态
        usRcvBufferPos = 0;
        ucCPNBuf[usRcvBufferPos++] = ucByte;    //保存数据
        eRcvState = STATE_RX_RCV;

        /* Enable t3.5 timers. */
        vMBPortTimersEnable( );           //重启3.5T定时器
        break;

        /* We are currently receiving a frame. Reset the timer after
         * every character received. If more than the maximum possible
         * number of bytes in a modbus frame is received the frame is
         * ignored.
         */
    case STATE_RX_RCV:
        if( usRcvBufferPos < MB_CPN_FRAME_SIZE_MAX )
        {
            ucCPNBuf[usRcvBufferPos++] = ucByte;     //接收数据
        }
        else
        {
            eRcvState = STATE_RX_ERROR;             //一帧报文的字节数大于最大PDU长度，忽略超出的数据
        }
        vMBPortTimersEnable();                      //每收到一个字节，都重启3.5T定时器
        break;
	default: break;
    }
    return xTaskNeedSwitch;
}

/**********************************************************************
 * @brief  串口发送数据
 *         eMBCPNSend函数中会调用串口发送数据，在进入串口发送中断后会调用xMBCPNTransmitFSM
 *         发送状态机函数发送应答报文。
 * @return BOOL   
 * @author laoc
 * @date 2019.01.22
 *********************************************************************/
BOOL
xMBCPNTransmitFSM( void )
{
    BOOL            xNeedPoll = FALSE;

    assert_param( eRcvState == STATE_RX_IDLE );

    switch ( eSndState )
    {
        /* We should not get a transmitter event if the transmitter is in
         * idle state.  */
    case STATE_TX_IDLE:            //发送器处于空闲状态，使能接收，禁止发送
        /* enable receiver/disable transmitter. */
        vMBPortSerialEnable( TRUE, FALSE );
        break;

    case STATE_TX_XMIT:            //发送器处于发送状态,在从机发送函数eMBCPNSend中赋值STATE_TX_XMIT
        /* check if we are finished. */
        if( usSndBufferCount != 0 )
        {
            (void)xMBPortSerialPutByte( ( CHAR )*pucSndBufferCur );         //发送数据
            pucSndBufferCur++;  /* next byte in sendbuffer. */
            usSndBufferCount--;
        }
        else                //传递任务，发送完成
        {
            xNeedPoll = xMBPortEventPost( EV_FRAME_SENT );          //协议栈事件状态赋值为EV_FRAME_SENT,发送完成事件,eMBPoll函数会对此事件进行处理
            
			/* Disable transmitter. This prevents another transmit buffer
             * empty interrupt. */
            eSndState = STATE_TX_IDLE;               //发送器状态为空闲状态
        }
        break;
		default: break;
    }
    return xNeedPoll;
}

/**********************************************************************
 * @brief  T3.5超时函数
 * @return BOOL   
 * @author laoc
 * @date 2019.01.22
 *********************************************************************/
BOOL
xMBCPNTimerT35Expired( void )
{
    BOOL            xNeedPoll = FALSE;
	
    switch ( eRcvState )             //上报modbus协议栈的事件状态给poll函数
    {
        /* Timer t35 expired. Startup phase is finished. */
    case STATE_RX_INIT:
        xNeedPoll = xMBPortEventPost( EV_READY );          //初始化完成事件
        break;

        /* A frame was received and t35 expired. Notify the listener that
         * a new frame was received. */
    case STATE_RX_RCV:                                     
      
	    if( usRcvBufferPos >= 5)                                //防止错误数据而导致激发接收事件,该芯片存在bug，发送完数据后会自动接收上次发送的数据
		{                                                         
	         xNeedPoll = xMBPortEventPost( EV_FRAME_RECEIVED );   //一帧数据接收完成，上报协议栈事件,接收到一帧完整的数据 
			 myprintf("EV_FRAME_RECEIVED******************\n");
		}
	    else
		{
			myprintf("EV_FRAME_RECEIVED_ERROR******************\n");
		}
        break;
		
        /* An error occured while receiving the frame. */
    case STATE_RX_ERROR:
        break;

        /* Function called in an illegal state. */
    default:
        assert_param( ( eRcvState == STATE_RX_INIT ) ||
                ( eRcvState == STATE_RX_RCV ) || ( eRcvState == STATE_RX_ERROR ) );
	    break;
    }

	vMBPortTimersDisable(  );         //当接收到一帧数据后，禁止3.5T定时器，直到接受下一帧数据开始，开始计时
    eRcvState = STATE_RX_IDLE;        //处理完一帧数据，接收器状态为空闲

    return xNeedPoll;
}

#endif