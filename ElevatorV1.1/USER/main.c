#include "led.h"
#include "delay.h"
#include "sys.h"
#include "usart.h"
#include "includes.h"
#include "lcd_init.h"
#include "lcd.h"
#include "pic.h"
#include "key.h"

//LED0����
//�����������ȼ�
#define UART_TASK_PRIO			7
//���������ջ��С
#define UART_STK_SIZE			128
//�����ջ
OS_STK UART_TASK_STK[UART_STK_SIZE];
//������
void uart_task(void *pdata);



//LED1����
//�����������ȼ�
#define MOTION_TASK_PRIO			6
//���������ջ��С
#define MOTION_STK_SIZE			128
//�����ջ
OS_STK MOTION_TASK_STK[MOTION_STK_SIZE];
//������
void motion_task(void *pdata);




//�����������
#define FLOAT_TASK_PRIO			5
//���������ջ��С
#define FLOAT_STK_SIZE			128
//�����ջ
//���������ʹ��printf����ӡ�������ݵĻ�һ��Ҫ8�ֽڶ���
__align(8) OS_STK FLOAT_TASK_STK[FLOAT_STK_SIZE]; 
//������
void float_task(void *pdata);



//key����ɨ������
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
	
	printf("Ӳ����ʼ�����\n");
	
	
	
	OSInit();  				//UCOS��ʼ��
  mbox=OSMboxCreate((void *)0);
	
	OSTaskCreate(uart_task,(void*)0,(OS_STK*)&UART_TASK_STK[UART_STK_SIZE-1],5);//��������ָ���������
	OSTaskCreate(motion_task,(void*)0,(OS_STK*)&MOTION_TASK_STK[MOTION_STK_SIZE-1],7);//����LED1����
	OSTaskCreate(float_task,(void*)0,(OS_STK*)&FLOAT_TASK_STK[FLOAT_STK_SIZE-1],8);//���������������
	
	OSTaskCreate(scan_task,(void*)0,(OS_STK*)&SCAN_TASK_STK[SCAN_STK_SIZE-1],6);//¥��ɨ��
	OSStart(); 				//��ʼ����
}


_Bool direct = 0;		//�����û�����0/����1
uint16_t correct_floor = 1;	//��ǰ�����û�����¥��

uint16_t destination_floor = 1; //Ŀ��¥��

//UART����
void uart_task(void *pdata)
{
	while(1)
	{
		if(rx_flag)
		{
			rx_flag=0;
			if(rx_buf[0] == 0x01)
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
				printf("¥�� %d , ���� %d \r\n",correct_floor,direct);
			}
			else if(rx_buf[0] == 0x02)
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

//�����˶���������

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
			
			
			
//			printf("���ݵ�ǰ������%d��\r\n",realtime_floor);
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
					point = 8-realtime_floor;		//ɨ�����ӵ�ǰ¥�㿪ʼ
				}
				else if(pend_dir)
				{
					des[realtime_floor + 7] = 0;
					point = realtime_floor + 7; 				
				}
				printf("�����ѵ����%d��\r\n",realtime_floor);
				point = realtime_floor;		//ɨ�����ӵ�ǰ¥�㿪ʼ
				OSTimeDly(1000);
				OSTaskResume(6);
				
				
				
			}
			else
			{
				if(pend_dir == 0)
				{
					
					point = 8-realtime_floor;		//ɨ�����ӵ�ǰ¥�㿪ʼ
				}
				else if(pend_dir)
				{
					
					point = realtime_floor + 7; 				
				}
				
				
				OSTaskResume(6);
				
				printf("���ݵ�ǰ������%d��,����Ϊ%d\r\n",realtime_floor,realtime_direction);
			}
			
		}
		if(realtime_floor == motion_floor)
			OSTimeDly(20);
	}
}

//�����������
void float_task(void *pdata)
{
	OS_CPU_SR cpu_sr=0;
	static float float_num=0.01;
	while(1)
	{
//		OSMboxPost(mbox,"123");
		float_num+=0.01f;
		
		//OS_ENTER_CRITICAL();	//�����ٽ���(�ر��ж�)
//		printf("float_num��ֵΪ: %.4f\r\n",float_num); //���ڴ�ӡ���
		//OS_EXIT_CRITICAL();		//�˳��ٽ���(���ж�)
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
			if(des[point])			//ɨ�赽������
			{			
				OSMboxPost(mbox,(INT32U *)point);
				OSTaskSuspend(OS_PRIO_SELF);
			}
			
		}
		
		
		
	
OSTimeDly(20);
}

