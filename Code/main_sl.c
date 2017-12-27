/* MAIN.C file
摘要：
1.上电，按三段式启动无感运转控制。
2.启正确启动后，指示灯闪烁，以指示电机正常运行。
3.启动失败时，指示灯恒亮。
4.无感运行通过硬件比较过零点方式，软件对过零点IO口进行中断配置，当产生中断时，认为发生过零事件。
5.上电，按指定PWM先预定位，再延时换相，并寻找过零点，
以切换过零时换相功能。
2013.5.8
 */
#include "stm8s.h"

#define STM8_FREQ_MHZ 16
#define PWM_FREQUENCY 16000

//PWM信号周期
static const u16 hArrPwmVal = ((u16)((STM8_FREQ_MHZ * (u32)1000000)/PWM_FREQUENCY));

//预定位PWM值
unsigned char AliPwm=15;

//启动过程中占空比输出
unsigned char PWMOUT=15;

//六步法中，CH1\CH2通道极性及使能配置
const unsigned char PWM_EN1_TAB[6]={0x00,0x00,0x10,0x10,0x01,0x01};
//const unsigned char PWM_EN1_TAB[6]={0x00,0x00,0x30,0x30,0x03,0x03};    // 负极性驱动 

//六步法中，CH3通道极性及使能配置
const unsigned char PWM_EN2_TAB[6]={0x01,0x01,0x00,0x00,0x00,0x00};
//const unsigned char PWM_EN2_TAB[6]={0x0`3,0x03,0x00,0x00,0x00,0x00};    // 负极性驱动 
/* 以上总结分析：
    PWM_EN_TAB 指定了所用到的输出比较寄存器，
    TAB1指定了CC1 (step4 & 5) 和 CC2 (step2 & 3)
    TAB2指定了CC3（step0 & 1), CC4没有用
*/

//上桥臂开关控制端口定义
#define MCO1_PORT GPIOC
#define MCO1_PIN	GPIO_PIN_3
#define MCO3_PORT GPIOC
#define MCO3_PIN	GPIO_PIN_7
#define MCO5_PORT GPIOC
#define MCO5_PIN	GPIO_PIN_6

//下桥臂开关控制端口定义
#define MCO0_PORT GPIOC
#define MCO0_PIN	GPIO_PIN_5  //PC2 -> PC5
#define MCO2_PORT GPIOC
#define MCO2_PIN	GPIO_PIN_4 // PC4 -> PC1
#define MCO4_PORT GPIOD
#define MCO4_PIN	GPIO_PIN_2 // PE2 -> PD2

//下桥臂低电平开关管导通
#define PWM_A_OFF MCO0_PORT->ODR |= (u8)MCO0_PIN; 
#define PWM_B_OFF MCO2_PORT->ODR |= (u8)MCO2_PIN; 
#define PWM_C_OFF MCO4_PORT->ODR |= (u8)MCO4_PIN; 

#define PWM_A_ON MCO0_PORT->ODR &= (u8)(~MCO0_PIN); 
#define PWM_B_ON MCO2_PORT->ODR &= (u8)(~MCO2_PIN); 
#define PWM_C_ON MCO4_PORT->ODR &= (u8)(~MCO4_PIN); 

//反电动势引脚定义 (启动成功后再用)
/*
#define F3_PORT GPIOB->IDR
#define F3_PIN  BIT5

#define F2_PORT GPIOB->IDR
#define F2_PIN  BIT6

#define F1_PORT GPIOB->IDR
#define F1_PIN  BIT7
*/

unsigned char bHallSteps[8]={
7,5,3,4,1,0,2,7
};

unsigned char CheckBEMF[6]=
{0x04,0x06,0x02,0x03,0x01,0x05};

/* Private vars and define */
#define BIT0 0x01
#define BIT1 0x02
#define BIT2 0x04
#define BIT3 0x08
#define BIT4 0x10
#define BIT5 0x20
#define BIT6 0x40
#define BIT7 0x80

//换相子函数声明
void Commutation(unsigned char bHallStartStep,unsigned int OutPwmValue);

#define STCount 30
//正确连续检测30次过零信号时认为启动成功
//此值小于200

unsigned int outpwm=0;
unsigned char bHallStartStep=0;//当前换相步序0-5:AB\AC\BC\BA\CA\CB
unsigned char StOk=0;//无感启动是否成功	
unsigned int StCountComm=0;
//无感启动时，正确检测第三项过零点的个数

//初始化按键，指示灯端口
void GPIO_int(void)
{
	 /* LEDs */
	GPIO_Init(GPIOD, GPIO_PIN_7, GPIO_MODE_OUT_PP_HIGH_FAST);
}

//系统时钟配置：内部16M
void Clock_init(void)
{
	/* Select fCPU = 16MHz */
  CLK_SYSCLKConfig(CLK_PRESCALER_HSIDIV1);
}

//换相电路开关管IO初始化
void PWM_IO_init(void)
{	
  //PB012 下桥臂0有效 ,配置为高电平
	GPIO_Init(MCO0_PORT, MCO0_PIN,GPIO_MODE_OUT_PP_HIGH_FAST);
	GPIO_Init(MCO2_PORT, MCO2_PIN,GPIO_MODE_OUT_PP_HIGH_FAST);
	GPIO_Init(MCO4_PORT, MCO4_PIN,GPIO_MODE_OUT_PP_HIGH_FAST);
	
	//PC123 上桥臂1有效,配置为低电平
	GPIO_Init(MCO1_PORT, MCO1_PIN,GPIO_MODE_OUT_PP_LOW_FAST);
	GPIO_Init(MCO3_PORT, MCO3_PIN,GPIO_MODE_OUT_PP_LOW_FAST);
	GPIO_Init(MCO5_PORT, MCO5_PIN,GPIO_MODE_OUT_PP_LOW_FAST);	
}

void Tim1_init(void)
{
	TIM1_DeInit();
	TIM1_TimeBaseInit(0, TIM1_COUNTERMODE_UP, hArrPwmVal, 0);
	TIM1_OC1Init(TIM1_OCMODE_PWM1, TIM1_OUTPUTSTATE_ENABLE, TIM1_OUTPUTNSTATE_DISABLE, hArrPwmVal*0, TIM1_OCPOLARITY_HIGH, TIM1_OCNPOLARITY_LOW, TIM1_OCIDLESTATE_RESET, TIM1_OCNIDLESTATE_SET); 
	TIM1_OC2Init(TIM1_OCMODE_PWM1, TIM1_OUTPUTSTATE_ENABLE, TIM1_OUTPUTNSTATE_DISABLE, hArrPwmVal*0, TIM1_OCPOLARITY_HIGH, TIM1_OCNPOLARITY_LOW, TIM1_OCIDLESTATE_RESET, TIM1_OCNIDLESTATE_SET); 
	TIM1_OC3Init(TIM1_OCMODE_PWM1, TIM1_OUTPUTSTATE_ENABLE, TIM1_OUTPUTNSTATE_DISABLE, hArrPwmVal*0, TIM1_OCPOLARITY_HIGH, TIM1_OCNPOLARITY_LOW, TIM1_OCIDLESTATE_RESET, TIM1_OCNIDLESTATE_SET); 
	TIM1_CCPreloadControl(DISABLE);
	TIM1_Cmd(ENABLE);
}

// 预充
void ChongD(void)
{	
  	unsigned int tem_c=0;
	PWM_A_ON;PWM_B_ON;PWM_C_ON;			// 下全ON
	for(tem_c=0;tem_c<50000;tem_c++);	// 等待
	PWM_A_OFF;PWM_B_OFF;PWM_C_OFF;		// 下全OFF
}

//初始化无感比较端口
void SensorlessEXTI_INIT(void)
{
	//反电动势过零端口模式配置
	GPIO_Init(GPIOB, GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7,GPIO_MODE_IN_PU_NO_IT);	
	//高低频分压开关	
	GPIO_Init(GPIOD, GPIO_PIN_6,GPIO_MODE_OUT_PP_LOW_FAST);	
}

//启动无感电机运行 (三段式)
// 返回启动状况 0失败 1成功
u8 Sensorless_START(void)
{
	u8 bHStatus = 0;
	u16 tem_c;
	u16 Com_time=0;
	StOk=0;						// 启动OK 标识为假
			
	ChongD();//预充
	GPIOB->CR2|=BIT5|BIT6|BIT7; // PB 控制寄存器2的设置
	EXTI->CR1|=BIT2|BIT3;		// 外部中断的CR1 设置

	/* 预定位 */
	bHallStartStep=5;			// 用第6步驱动状态，进行预定位
	outpwm=hArrPwmVal*AliPwm/100;//预定位PWM值计算
	Commutation(bHallStartStep,outpwm);//按预定位PWM设定预定位(换相至第6步状态驱动)
	for(tem_c=0;tem_c<1000;tem_c++);//较小时间			
	
	/* 启动 */
	outpwm=hArrPwmVal*PWMOUT/100;//启动PWM值计算	
	Com_time=0;
	StCountComm=0;
	enableInterrupts();			// 启用全局中断
		
	do{						
		bHallStartStep++;
		if(bHallStartStep>=6) bHallStartStep=0;
		Commutation(bHallStartStep,outpwm);//输出PWM信号，启动电机				
		for(tem_c=0;tem_c<6000;tem_c++);//延时时间
		Com_time++;
	}while(StOk==0&&Com_time<200);		// StOK是在中断检测过零点成功，才赋1值
		
	if(Com_time>=200)
	{
			TIM1->BKR &= (uint8_t)(~TIM1_BKR_MOE);//禁止PWM输出
			PWM_A_OFF;
			PWM_B_OFF;
			PWM_C_OFF;
			return 0;
	}//200次换相还没启动成功，认为启动失败，并且不再驱动电机
		
	return 1;
}


@far @interrupt void EXTI_PORTB_IRQHandler(void)
{
	u8 bHStatus =0,i;
		
	bHStatus=(GPIO_ReadInputData(GPIOB)>>5)&0x07;
	for(i=0;i<150;i++);//延时滤波
	i=(GPIO_ReadInputData(GPIOB)>>5)&0x07;
	if(bHStatus!=i)return;
	
	bHStatus=0;	
		
	/*
	//A相反电动势过零点检测
	if (F1_PORT & F1_PIN)
	{
		bHStatus |= BIT2;
	}
	
	//B相反电动势过零点检测
	if (F2_PORT & F2_PIN)
	{
		bHStatus |= BIT1;
	}
			
	//C相反电动势过零点检测
	if (F3_PORT & F3_PIN)
	{
		bHStatus |= BIT0;
	}
	*/
	
	if(StOk==0)//启动过程。。。
	 { 
		if(bHStatus==CheckBEMF[bHallStartStep])	//正确检测到对应相序的过零点
			{StCountComm++;GPIO_WriteReverse(GPIOD,GPIO_PIN_7);}//指示灯反转指示
	  	else 
		  {StCountComm=0;}
		 
		if(StCountComm>=STCount)//连续检测过固定数量的过零点时，切换至过零换相
		{
			enableInterrupts();
			StOk=1;//启动成功标志
		}
	 }
	 
	if(StOk==1)
	{
		bHallStartStep = bHallSteps[bHStatus];//得到换相步序
		if (bHallStartStep == 7)//运行过程中，状态读错，为错误，停止PWM输出
		{ //故障，停止输出
			TIM1->BKR &= (uint8_t)(~TIM1_BKR_MOE);//禁止PWM输出
			PWM_A_OFF;
			PWM_B_OFF;
			PWM_C_OFF;
			return;
		}
		Commutation(bHallStartStep,outpwm);//输出PWM信号，启动电机
	}		
}

@far @interrupt void EXTI_PORTD_IRQHandler(void)
{
	/* in order to detect unexpected events during development, 
	   it is recommended to set a breakpoint on the following instruction
	*/	
}



main()
{
	unsigned int tem_c=0;
	unsigned char step=0;
	
	for(tem_c=0;tem_c<50000;tem_c++);//上电延时，等待系统稳定

	Clock_init();//指示灯端口初始化
	GPIO_int();//时钟配置
	
	PWM_IO_init();//开关管控制端口初始化
	Tim1_init();//高级定时器配置	
	
	//初始化无感比较端口 -- B5 D6
	SensorlessEXTI_INIT();//电机启动, 端口初始化配置	

	if(Sensorless_START()==0)//如果启动失败
	{
	 	GPIO_WriteHigh(GPIOD,GPIO_PIN_7);//PD7指示灯亮，表示失败
	 	while(1);
  	}
		
	while (1)//启动成功后，指示灯延时反转
	{
		for(tem_c=0;tem_c<50000;tem_c++);//延时时间
		GPIO_WriteReverse(GPIOD,GPIO_PIN_7);//PD7指示灯反转
	}
}

//换向输出PWM值，
//bHallStartStep:当前换相步序0-5，OutPwmValue 输出PWM值
void Commutation(unsigned char bHallStartStep,unsigned int OutPwmValue)
{	
   TIM1->BKR &= (uint8_t)(~TIM1_BKR_MOE);//禁止PWM输出
	 
	if(bHallStartStep!=3&&bHallStartStep!=4)
	PWM_A_OFF;
	if(bHallStartStep!=0&&bHallStartStep!=5)
	PWM_B_OFF;
	if(bHallStartStep!=1&&bHallStartStep!=2)
	PWM_C_OFF;
	 
	//根据换相步序，打开不同的开关管，并施加正确的PWM信号
	if(bHallStartStep==0)//AB
	{
		TIM1->CCR3H = (uint8_t)(OutPwmValue >> 8);
    	TIM1->CCR3L = (uint8_t)(OutPwmValue);
		PWM_B_ON;
	}
  	else if(bHallStartStep==1)	//AC
	{
		TIM1->CCR3H = (uint8_t)(OutPwmValue >> 8);
    	TIM1->CCR3L = (uint8_t)(OutPwmValue);
	  	PWM_C_ON;
	}
	else if(bHallStartStep==2)	//BC
	{
		TIM1->CCR2H = (uint8_t)(OutPwmValue >> 8);
    	TIM1->CCR2L = (uint8_t)(OutPwmValue);
	  	PWM_C_ON;
	}
	else if(bHallStartStep==3)	//BA
	{
		TIM1->CCR2H = (uint8_t)(OutPwmValue >> 8);
    	TIM1->CCR2L = (uint8_t)(OutPwmValue);
	  	PWM_A_ON;
	}
	else if(bHallStartStep==4)//CA
	{
		TIM1->CCR1H = (uint8_t)(OutPwmValue >> 8);
    	TIM1->CCR1L = (uint8_t)(OutPwmValue);
	  	PWM_A_ON;
	}
	else if(bHallStartStep==5)	//CB
	{
		TIM1->CCR1H = (uint8_t)(OutPwmValue >> 8);
    	TIM1->CCR1L = (uint8_t)(OutPwmValue);
	  	PWM_B_ON;
	}
	
	TIM1->CCER1=PWM_EN1_TAB[bHallStartStep];
	TIM1->CCER2=PWM_EN2_TAB[bHallStartStep];		
	TIM1->BKR|=TIM1_BKR_MOE;//使能PWM输出
}

#ifdef USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *   where the assert_param error has occurred.
  * @param file: pointer to the source file name
  * @param line: assert_param error line source number
  * @retval : None
  */
void assert_failed(u8* file, u32 line)
{ 
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }

}
#endif
