/*
FOR BLDC 2017
*/
#include "N76E003.h"
#include "Common.h"
#include "SFR_Macro.h"
#include "Function_define.h"
#include "Delay.h"

/* ---------- 定义常量 --------------*/
#define STM8_FREQ_MHZ 16 
#define PWM_FREQUENCY 16000 		//PWM频率16K, 将被计算出pwm值,写入PWMPH/L
#define PWMOUT 30 			//预定位（proLoc）占空比
#define PWMOUT2 50 			//启动（）占空比
/*
#define DOWN_A_ON set_P00			// this for high level avalable
#define DOWN_B_ON set_P11
#define DOWN_C_ON set_P03
#define DOWN_A_OFF clr_P00
#define DOWN_B_OFF clr_P11
#define DOWN_C_OFF clr_P03
*/

#define DOWN_A_ON clr_P00			// this for  low level avalable
#define DOWN_B_ON clr_P11
#define DOWN_C_ON clr_P03
#define DOWN_A_OFF set_P00
#define DOWN_B_OFF set_P11
#define DOWN_C_OFF set_P03

/* ------- 定义变量 --------------*/
static unsigned char startOk = 0;
static unsigned char workstep = 0;
static	unsigned int actCount = 0;
static	unsigned int change_delay = 50;
// PWM 占空比的寄存器值 （PWM0H/PWML)
static unsigned int duty = 0;
// PWM 频率的寄存器值 （PWMPH/PWMPL)
static const unsigned long pwm = ((unsigned int)((STM8_FREQ_MHZ * (unsigned long)1000000)/PWM_FREQUENCY) ); 
const unsigned char PWM_MARSK_TABLE[6]={0x01, 0x01, 0x10, 0x10, 0x04, 0x04};
/* ---------- 定义函数 --------------*/
// 系统时钟配置: 内部16M
void clkConf(){
	set_HIRCEN; 		// sysClk = 16M
}

void ioConf(){
	PWM_GP_MODE_ENABLE;		// use group mode
	PWM0_P12_OUTPUT_ENABLE; 	// upA
	PWM4_P01_OUTPUT_ENABLE; 	// upB
	PWM2_P10_OUTPUT_ENABLE; 	// upC

	P00_PushPull_Mode; 			//downA
	P11_PushPull_Mode; 			// downB
	P03_PushPull_Mode; 			// downC

	//P04_PushPull_Mode; 			// for test referrence 
}

void pwmConf(){
	set_CLRPWM;     		// clrear pwm counter //PWM_CLOCK_DIV_8;	// sysClk / 8 for pwm
	PWM_GP_MODE_ENABLE;		// use group mode
	PWMPH = (unsigned char) (pwm >> 8);  // set pwm frequnce	
	PWMPL = (unsigned char) pwm; 
	/* 上管高电平有效，平时不动作为低电平 */
	PMD = 0X00;				//(PWM掩码数据) wen masked, 00: ground / FF: vcc (note: upFET-off)
	PWM_OUTPUT_ALL_NORMAL;	//(占空输出的极性) set on_duty is [default]hight:low ( [default]NORMAL: INVERSE )
}

void pwmStart(){
	set_EPWM;    			//Enable PWM interrupt
	set_EA;
	set_LOAD;
	set_PWMRUN;
}

void pwmStop(){
	clr_EPWM;    			//Enable PWM interrupt
	clr_EA;
	clr_PWMRUN;
}

void setDuty(){
	PWM0H =(unsigned char) (duty >> 8);    // set duty 
	PWM0L =(unsigned char) duty; 
	//while( !(LOAD == 0X00) );				// whait LOAD reset 0 by self
	set_LOAD;
}

//void setCommutation(unsigned char workstep, unsigned int dutyv){
void setCommutation(unsigned char workstep){
	//PMEN = 0xFF;				// pwm output all off

	if(workstep!=3&&workstep!=4)
	    DOWN_A_OFF;
	if(workstep!=0&&workstep!=5)
	    DOWN_B_OFF;
	if(workstep!=1&&workstep!=2)
	    DOWN_C_OFF;

	setDuty();

	if(workstep==0) {			//AB
		DOWN_B_ON;
	}
  	else if(workstep==1) {			//AC
	  	DOWN_C_ON;
	}
	else if(workstep==2) {			//BC
	  	DOWN_C_ON;
	}
	else if(workstep==3) {			//BA
	  	DOWN_A_ON;
	}
	else if(workstep==4) {  		//CA
	  	DOWN_A_ON;
	}
	else if(workstep==5) {			//CB
	  	DOWN_B_ON;
	}
	
	PMEN = ~ (PWM_MARSK_TABLE[workstep]);	// one chanel pwm open
}

void preLoc(){
	workstep = 5;		
	duty=pwm*PWMOUT/100;      	// duty cicy(location stage)
	duty =(unsigned int) (pwm*PWMOUT/100);      	// duty cicy(location stage)
	//setDuty();
	setCommutation(workstep);
}

unsigned char  bldcStart(){
	actCount = 0;
	startOk = 0;
	do{					   // only one pass, not need do this
		workstep++; actCount++;
		if(workstep >=6 ) workstep = 0;
		setCommutation(workstep);	
		switch (actCount){
			case 24: change_delay = 50;
			break;
			case 48: change_delay = 40;
			break;
			//case 52: change_delay = 30;
			//break;
			//case 76: change_delay = 20;
			//break;
			//case 100: change_delay = 10;
			//break;
			//case 140: change_delay = 5;
			//break;
		}

		Timer0_Delay1ms(change_delay);
		//Timer0_Delay100us(50);
	}while(actCount <1000 && startOk == 0);   // action not enough, and start fail 

	if(actCount >= 1000){
		return 0; 	// ng
	}	
	return 1;		// ok
}

void keepAllOff(){
	clr_P12; clr_P01; clr_P10; 	// upA  upB  upC OFF
	duty = 0x00; setDuty(); 	// keep low for PWM mode start
	DOWN_A_OFF; DOWN_B_OFF; DOWN_C_OFF;
	//pwmStop();
}

void keepUpAllOff(){
	clr_P12; clr_P01; clr_P10; 	// upA  upB  upC OFF
	duty = 0x00; setDuty(); 	// keep low for PWM mode start
	DOWN_A_ON; DOWN_B_ON; DOWN_C_ON;
	//pwmStop();
}

/* 定义功能函数 */
void main(){
	Set_All_GPIO_Quasi_Mode;
	keepAllOff();
	clkConf();
	ioConf();
	pwmConf();
	Timer0_Delay1ms(50);
	preLoc();			
	pwmStart();
	Timer0_Delay1ms(500);		// preLocation delay 
	//pwmStop();
	duty =(unsigned int)(pwm*PWMOUT2/100);      	// duty cicy(location stage)
	setDuty();
	//pwmStart();
	if(bldcStart() == 0){
		//keepAllOff();
		keepUpAllOff();
		while(1);		// no started to stop all
	}	
	while (1){
	}
}
