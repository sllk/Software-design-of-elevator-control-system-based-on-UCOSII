/**
  ****************************(C) COPYRIGHT 2023 ShaoYang****************************
  * @file      	main.c/h
  * @brief     	电梯控制系统（系统层）
  * @note      	系统包含三个任务，分别进行指令接收、运动控制、响应判优
  * @history
  *  Version    Date            Author          Modification
  *  V1.0.0     April-20-2023   ShaoYang        1. 添加了基本的电梯控制逻辑
												2. 完成串口协议，接收以及发送
												
	 V1.1.0		April-22-2023	ShaoYang		1. 初步实现了响应判优部分，但电梯控制仍存在BUG
	 V1.1.1		May-13-2023		ShaoYang		1. 完成
  * @blog		http://47.96.8.41/
  @verbatim
  ==============================================================================

  ==============================================================================
  @endverbatim
  ****************************(C) COPYRIGHT 2023 ShaoYang****************************
  */
#include "led.h"
#include "delay.h"
#include "sys.h"
#include "usart.h"
#include "includes.h"
#include "lcd_init.h"
#include "lcd.h"
#include "pic.h"
#include "key.h"

//串口接收任务
//设置任务优先级
#define UART_TASK_PRIO			7
//设置任务堆栈大小
#define UART_STK_SIZE			128
//任务堆栈
OS_STK UART_TASK_STK[UART_STK_SIZE];
//任务函数
void uart_task(void *pdata);


//电梯运动控制任务
//设置任务优先级
#define MOTION_TASK_PRIO			6
//设置任务堆栈大小
#define MOTION_STK_SIZE			128
//任务堆栈
OS_STK MOTION_TASK_STK[MOTION_STK_SIZE];
//任务函数
void motion_task(void *pdata);


//优先级判优任务
//设置任务优先级
#define SCAN_TASK_PRIO			4
//设置任务堆栈大小
#define SCAN_STK_SIZE			128
//任务堆栈
OS_STK SCAN_TASK_STK[SCAN_STK_SIZE];
//任务函数
void scan_task(void *pdata);




OS_EVENT *mbox;
INT8U err;
INT32U send_data=0;

uint16_t realtime_floor = 1;
uint16_t realtime_direction = 1;

uint16_t motion_floor = 1;

static uint8_t des[16];		//扫描空间，其中为1的单元为请求的楼层
uint16_t point = 0;

int main(void)
{
	delay_init();       	//延时初始化
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); //中断分组配置
	uart_init(115200);    	//串口波特率设置
	LED_Init();  			//LED初始化
	
	LCD_Init();//LCD初始化
	LCD_Fill(0,0,LCD_W,LCD_H,WHITE);
	
	LCD_ShowString(40,0,"elevator",RED,WHITE,16,0);
	Key_Init();
	
	printf("硬件初始化完毕\r\n");
	
	
	OSInit();  				//UCOS初始化
  mbox=OSMboxCreate((void *)0);
	
	OSTaskCreate(uart_task,(void*)0,(OS_STK*)&UART_TASK_STK[UART_STK_SIZE-1],5);//创建串口指令接收任务
	OSTaskCreate(motion_task,(void*)0,(OS_STK*)&MOTION_TASK_STK[MOTION_STK_SIZE-1],7);//创建LED1任务

	
	OSTaskCreate(scan_task,(void*)0,(OS_STK*)&SCAN_TASK_STK[SCAN_STK_SIZE-1],6);//楼层扫描
	OSStart(); 				//开始任务
}


_Bool direct = 0;		//梯外用户下行0/上行1
uint16_t correct_floor = 1;	//当前梯外用户所在楼层

uint16_t destination_floor = 1; //目标楼层


/******************************************************************************
      任务说明：（任务0）串口接收任务
				接收串口数据，并判断指令类型，将指令中的数据发送传递给任务2
      入口数据：无
								
      返回值：  无
******************************************************************************/
void uart_task(void *pdata)
{
	while(1)
	{
		if(rx_flag)
		{
			rx_flag=0;
			if(rx_buf[0] == 0x01)			//接收到指令1
			{
				correct_floor = rx_buf[1];	//提取用户楼层
				direct = rx_buf[2];			//提取用户方向
				if(direct == 0)
				{
					des[8-correct_floor] = 1;		
				}
				else if(direct == 1)
				{
					des[correct_floor+7] = 1;
				}
				if(direct == 0)
				{
					printf("%d 楼层有人要下楼 \r\n",correct_floor);
				}
				if(direct == 1)
				{
					printf("%d 楼层有人要上楼 \r\n",correct_floor);
				}
				
			}
			else if(rx_buf[0] == 0x02)			//接收到指令2
			{
				destination_floor = rx_buf[1];	//提取目标楼层
				if(realtime_direction)
				{
					des[destination_floor+7] = 1;
				}
				else if(realtime_direction == 0)
				{
					des[8 - destination_floor] = 1;
				}
				printf("目标楼层 %d \r\n",destination_floor);
			}
		}	
	  OSTimeDly(10);		
	}
}


/******************************************************************************
      任务说明：（任务1）电梯运动状态控制任务
				从任务2接收实时的目的楼层，控制电梯前往目的楼层
      入口数据：无
								
      返回值：  无
******************************************************************************/
uint16_t pend_dir;
uint16_t pend_floor;
void motion_task(void *pdata)
{
	
	
	while(1)
	{
		motion_floor = (INT32U)(INT32U *)OSMboxPend(mbox,0,&err);		//从任务2接收实时目的楼层
		if(point<=7)
		{
			pend_dir = 0;
			pend_floor = 8 - motion_floor;		
		}
		else
		{
			pend_dir = 1;
			pend_floor = motion_floor - 7;			
		}
		if(realtime_floor != pend_floor)
		{
			OSTimeDly(1000);
			

			if(realtime_floor > pend_floor)
			{
				realtime_direction = 0;
				realtime_floor--;
			}
			else
			{
				realtime_direction = 1;
				realtime_floor++;
			}
			

			if(realtime_floor == pend_floor)
			{
				if(pend_dir == 0)
				{
					des[8-realtime_floor] = 0;
					point = 8-realtime_floor;		//设置扫描起点
				}
				else if(pend_dir)
				{
					des[realtime_floor + 7] = 0;
					point = realtime_floor + 7; 	//设置扫描起点			
				}
				printf("电梯已到达第%d层\r\n",realtime_floor);
				point = realtime_floor;		
				OSTimeDly(1000);			//延时模拟电梯运动时间
				OSTaskResume(6);			//将任务2解挂，进行下一次楼层响应判优
			}
			else
			{
				if(pend_dir == 0)
				{		
					point = 8-realtime_floor;		//设置扫描起点
				}
				else if(pend_dir)
				{
					point = realtime_floor + 7; 	//设置扫描起点		
				}
				
				
				OSTaskResume(6);
				if(realtime_direction == 0)		//舱体每次经过楼层时，向上位机返回楼层以及运动方向信息
				{
					printf("电梯当前运行至%d层,方向为向下\r\n",realtime_floor);
				}
				else if(realtime_direction == 1)
				{
					printf("电梯当前运行至%d层,方向为向上\r\n",realtime_floor);
				}
			}
			
		}
		if(realtime_floor == motion_floor)
			OSTimeDly(20);
	}
}

/******************************************************************************
      任务说明：（任务2）楼层响应判优任务
				从任务0接收到响应的楼层号，并进行优先级判断，将实时最优楼层号传递给任务1
      入口数据：无
								
      返回值：  无
******************************************************************************/
void scan_task(void *pdata)
{	
	while(1)
	{	
			point++;			//扫面指针循环
			if(point > 15)
			{
				point = 0;
			}
			if(des[point])			//扫描到有请求
			{			
				OSMboxPost(mbox,(INT32U *)point);	//将响应楼层号传递给任务1
				OSTaskSuspend(OS_PRIO_SELF);		//挂起自己
			}
			
		}
		
		
		
	
OSTimeDly(20);
}

