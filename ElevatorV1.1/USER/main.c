#include "led.h"
#include "delay.h"
#include "sys.h"
#include "usart.h"
#include "includes.h"
#include "lcd_init.h"
#include "lcd.h"
#include "pic.h"
#include "key.h"

//LED0任务
//设置任务优先级
#define UART_TASK_PRIO			7
//设置任务堆栈大小
#define UART_STK_SIZE			128
//任务堆栈
OS_STK UART_TASK_STK[UART_STK_SIZE];
//任务函数
void uart_task(void *pdata);



//LED1任务
//设置任务优先级
#define MOTION_TASK_PRIO			6
//设置任务堆栈大小
#define MOTION_STK_SIZE			128
//任务堆栈
OS_STK MOTION_TASK_STK[MOTION_STK_SIZE];
//任务函数
void motion_task(void *pdata);




//浮点测试任务
#define FLOAT_TASK_PRIO			5
//设置任务堆栈大小
#define FLOAT_STK_SIZE			128
//任务堆栈
//如果任务中使用printf来打印浮点数据的话一点要8字节对齐
__align(8) OS_STK FLOAT_TASK_STK[FLOAT_STK_SIZE]; 
//任务函数
void float_task(void *pdata);



//key按键扫描任务
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
	
	printf("硬件初始化完毕\n");
	
	
	
	OSInit();  				//UCOS初始化
  mbox=OSMboxCreate((void *)0);
	
	OSTaskCreate(uart_task,(void*)0,(OS_STK*)&UART_TASK_STK[UART_STK_SIZE-1],5);//创建串口指令接收任务
	OSTaskCreate(motion_task,(void*)0,(OS_STK*)&MOTION_TASK_STK[MOTION_STK_SIZE-1],7);//创建LED1任务
	OSTaskCreate(float_task,(void*)0,(OS_STK*)&FLOAT_TASK_STK[FLOAT_STK_SIZE-1],8);//创建浮点测试任务
	
	OSTaskCreate(scan_task,(void*)0,(OS_STK*)&SCAN_TASK_STK[SCAN_STK_SIZE-1],6);//楼层扫描
	OSStart(); 				//开始任务
}


_Bool direct = 0;		//梯外用户下行0/上行1
uint16_t correct_floor = 1;	//当前梯外用户所在楼层

uint16_t destination_floor = 1; //目标楼层

//UART任务
void uart_task(void *pdata)
{
	while(1)
	{
		if(rx_flag)
		{
			rx_flag=0;
			if(rx_buf[0] == 0x01)
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
				printf("楼层 %d , 方向 %d \r\n",correct_floor,direct);
			}
			else if(rx_buf[0] == 0x02)
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

//电梯运动控制任务

uint16_t pend_dir;
uint16_t pend_floor;
void motion_task(void *pdata)
{
	
	
	while(1)
	{
		motion_floor = (INT32U)(INT32U *)OSMboxPend(mbox,0,&err);
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
			
			
			
//			printf("电梯当前运行至%d层\r\n",realtime_floor);
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
					point = 8-realtime_floor;		//扫描起点从当前楼层开始
				}
				else if(pend_dir)
				{
					des[realtime_floor + 7] = 0;
					point = realtime_floor + 7; 				
				}
				printf("电梯已到达第%d层\r\n",realtime_floor);
				point = realtime_floor;		//扫描起点从当前楼层开始
				OSTimeDly(1000);
				OSTaskResume(6);
				
				
				
			}
			else
			{
				if(pend_dir == 0)
				{
					
					point = 8-realtime_floor;		//扫描起点从当前楼层开始
				}
				else if(pend_dir)
				{
					
					point = realtime_floor + 7; 				
				}
				
				
				OSTaskResume(6);
				
				printf("电梯当前运行至%d层,方向为%d\r\n",realtime_floor,realtime_direction);
			}
			
		}
		if(realtime_floor == motion_floor)
			OSTimeDly(20);
	}
}

//浮点测试任务
void float_task(void *pdata)
{
	OS_CPU_SR cpu_sr=0;
	static float float_num=0.01;
	while(1)
	{
//		OSMboxPost(mbox,"123");
		float_num+=0.01f;
		
		//OS_ENTER_CRITICAL();	//进入临界区(关闭中断)
//		printf("float_num的值为: %.4f\r\n",float_num); //串口打印结果
		//OS_EXIT_CRITICAL();		//退出临界区(开中断)
		//OSSemPend(sem,0,&err);
		
		//OSTimeDly(5);
		OSTimeDly(200);
	}
}


void scan_task(void *pdata)
{
	
	while(1)
	{
		
		
			point++;
			if(point > 15)
			{
				point = 0;
			}
			if(des[point])			//扫描到有请求
			{			
				OSMboxPost(mbox,(INT32U *)point);
				OSTaskSuspend(OS_PRIO_SELF);
			}
			
		}
		
		
		
	
OSTimeDly(20);
}

