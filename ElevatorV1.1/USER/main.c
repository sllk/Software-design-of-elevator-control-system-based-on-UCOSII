/**
  ****************************(C) COPYRIGHT 2023 ShaoYang****************************
  * @file      	main.c/h
  * @brief     	���ݿ���ϵͳ��ϵͳ�㣩
  * @note      	ϵͳ�����������񣬷ֱ����ָ����ա��˶����ơ���Ӧ����
  * @history
  *  Version    Date            Author          Modification
  *  V1.0.0     April-20-2023   ShaoYang        1. ����˻����ĵ��ݿ����߼�
												2. ��ɴ���Э�飬�����Լ�����
												
	 V1.1.0		April-22-2023	ShaoYang		1. ����ʵ������Ӧ���Ų��֣������ݿ����Դ���BUG
	 V1.1.1		May-13-2023		ShaoYang		1. ���
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

//���ڽ�������
//�����������ȼ�
#define UART_TASK_PRIO			7
//���������ջ��С
#define UART_STK_SIZE			128
//�����ջ
OS_STK UART_TASK_STK[UART_STK_SIZE];
//������
void uart_task(void *pdata);


//�����˶���������
//�����������ȼ�
#define MOTION_TASK_PRIO			6
//���������ջ��С
#define MOTION_STK_SIZE			128
//�����ջ
OS_STK MOTION_TASK_STK[MOTION_STK_SIZE];
//������
void motion_task(void *pdata);


//���ȼ���������
//�����������ȼ�
#define SCAN_TASK_PRIO			4
//���������ջ��С
#define SCAN_STK_SIZE			128
//�����ջ
OS_STK SCAN_TASK_STK[SCAN_STK_SIZE];
//������
void scan_task(void *pdata);




OS_EVENT *mbox;
INT8U err;
INT32U send_data=0;

uint16_t realtime_floor = 1;
uint16_t realtime_direction = 1;

uint16_t motion_floor = 1;

static uint8_t des[16];		//ɨ��ռ䣬����Ϊ1�ĵ�ԪΪ�����¥��
uint16_t point = 0;

int main(void)
{
	delay_init();       	//��ʱ��ʼ��
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); //�жϷ�������
	uart_init(115200);    	//���ڲ���������
	LED_Init();  			//LED��ʼ��
	
	LCD_Init();//LCD��ʼ��
	LCD_Fill(0,0,LCD_W,LCD_H,WHITE);
	
	LCD_ShowString(40,0,"elevator",RED,WHITE,16,0);
	Key_Init();
	
	printf("Ӳ����ʼ�����\r\n");
	
	
	OSInit();  				//UCOS��ʼ��
  mbox=OSMboxCreate((void *)0);
	
	OSTaskCreate(uart_task,(void*)0,(OS_STK*)&UART_TASK_STK[UART_STK_SIZE-1],5);//��������ָ���������
	OSTaskCreate(motion_task,(void*)0,(OS_STK*)&MOTION_TASK_STK[MOTION_STK_SIZE-1],7);//����LED1����

	
	OSTaskCreate(scan_task,(void*)0,(OS_STK*)&SCAN_TASK_STK[SCAN_STK_SIZE-1],6);//¥��ɨ��
	OSStart(); 				//��ʼ����
}


_Bool direct = 0;		//�����û�����0/����1
uint16_t correct_floor = 1;	//��ǰ�����û�����¥��

uint16_t destination_floor = 1; //Ŀ��¥��


/******************************************************************************
      ����˵����������0�����ڽ�������
				���մ������ݣ����ж�ָ�����ͣ���ָ���е����ݷ��ʹ��ݸ�����2
      ������ݣ���
								
      ����ֵ��  ��
******************************************************************************/
void uart_task(void *pdata)
{
	while(1)
	{
		if(rx_flag)
		{
			rx_flag=0;
			if(rx_buf[0] == 0x01)			//���յ�ָ��1
			{
				correct_floor = rx_buf[1];	//��ȡ�û�¥��
				direct = rx_buf[2];			//��ȡ�û�����
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
					printf("%d ¥������Ҫ��¥ \r\n",correct_floor);
				}
				if(direct == 1)
				{
					printf("%d ¥������Ҫ��¥ \r\n",correct_floor);
				}
				
			}
			else if(rx_buf[0] == 0x02)			//���յ�ָ��2
			{
				destination_floor = rx_buf[1];	//��ȡĿ��¥��
				if(realtime_direction)
				{
					des[destination_floor+7] = 1;
				}
				else if(realtime_direction == 0)
				{
					des[8 - destination_floor] = 1;
				}
				printf("Ŀ��¥�� %d \r\n",destination_floor);
			}
		}	
	  OSTimeDly(10);		
	}
}


/******************************************************************************
      ����˵����������1�������˶�״̬��������
				������2����ʵʱ��Ŀ��¥�㣬���Ƶ���ǰ��Ŀ��¥��
      ������ݣ���
								
      ����ֵ��  ��
******************************************************************************/
uint16_t pend_dir;
uint16_t pend_floor;
void motion_task(void *pdata)
{
	
	
	while(1)
	{
		motion_floor = (INT32U)(INT32U *)OSMboxPend(mbox,0,&err);		//������2����ʵʱĿ��¥��
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
					point = 8-realtime_floor;		//����ɨ�����
				}
				else if(pend_dir)
				{
					des[realtime_floor + 7] = 0;
					point = realtime_floor + 7; 	//����ɨ�����			
				}
				printf("�����ѵ����%d��\r\n",realtime_floor);
				point = realtime_floor;		
				OSTimeDly(1000);			//��ʱģ������˶�ʱ��
				OSTaskResume(6);			//������2��ң�������һ��¥����Ӧ����
			}
			else
			{
				if(pend_dir == 0)
				{		
					point = 8-realtime_floor;		//����ɨ�����
				}
				else if(pend_dir)
				{
					point = realtime_floor + 7; 	//����ɨ�����		
				}
				
				
				OSTaskResume(6);
				if(realtime_direction == 0)		//����ÿ�ξ���¥��ʱ������λ������¥���Լ��˶�������Ϣ
				{
					printf("���ݵ�ǰ������%d��,����Ϊ����\r\n",realtime_floor);
				}
				else if(realtime_direction == 1)
				{
					printf("���ݵ�ǰ������%d��,����Ϊ����\r\n",realtime_floor);
				}
			}
			
		}
		if(realtime_floor == motion_floor)
			OSTimeDly(20);
	}
}

/******************************************************************************
      ����˵����������2��¥����Ӧ��������
				������0���յ���Ӧ��¥��ţ����������ȼ��жϣ���ʵʱ����¥��Ŵ��ݸ�����1
      ������ݣ���
								
      ����ֵ��  ��
******************************************************************************/
void scan_task(void *pdata)
{	
	while(1)
	{	
			point++;			//ɨ��ָ��ѭ��
			if(point > 15)
			{
				point = 0;
			}
			if(des[point])			//ɨ�赽������
			{			
				OSMboxPost(mbox,(INT32U *)point);	//����Ӧ¥��Ŵ��ݸ�����1
				OSTaskSuspend(OS_PRIO_SELF);		//�����Լ�
			}
			
		}
		
		
		
	
OSTimeDly(20);
}

