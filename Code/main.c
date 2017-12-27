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
#define PWMOUT 15 			//预定位（proLoc）占空比
#define PWMOUT_2 15 			//启动（）占空比
#define DOWN_A_OFF set_P00
#define DOWN_B_OFF set_P11
#define DOWN_C_OFF set_P03
#define DOWN_A_ON clr_P00
#define DOWN_B_ON clr_P11
#define DOWN_C_ON clr_P03

/* ------- 定义变量 --------------*/
unsigned int i = 0;
unsigned char workstep = 0;
unsigned int pwmduty = 0;
static const unsigned int pwm = ((unsigned int)((STM8_FREQ_MHZ * (unsigned long)1000000)/PWM_FREQUENCY));
const unsigned char PWM_MARSK_TABLE[6]={0x01, 0x01, 0x10, 0x10, 0x04, 0x04};
/* ---------- 定义函数 --------------*/
// 系统时钟配置: 内部16M
void clkConf(){
	set_HIRCEN; 		// sysClk = 16M
}

void ioConf(){
	PWM0_P12_OUTPUT_ENABLE; 	// upA
	PWM4_P01_OUTPUT_ENABLE; 	// upB
	PWM3_P00_OUTPUT_ENABLE; 	// upC

	P00_PushPull_Mode; set_P00;	//downA
	P11_PushPull_Mode; set_P11;	// downB
	P03_PushPull_Mode; set_P03;	// downC
}

void setDuty(unsigned int dutyvalue){
	/* 可变值 */
	PWM0H =(unsigned char) dutyvalue >> 8;           // set duty 
	PWM0L =(unsigned char) dutyvalue; 
}

void pwmConf(){
	set_CLRPWM;     		// clrear pwm counter
					//PWM_CLOCK_DIV_8;	// sysClk / 8 for pwm
	PWM_GP_MODE_ENABLE;		// use group mode
	PWMPH = (unsigned char) pwm >> 8;  // set pwm frequnce	
	PWMPL = (unsigned char) pwm; 
	
	PMD = 0X00;			// wen masked, 00: ground / FF: vcc
	PWM_OUTPUT_ALL_NORMAL;		// set on_duty is hight/low & _INVERSE
}

void pwmStart(){
	set_EPWM;    		//Enable PWM interrupt
	set_EA;
	set_LOAD;
	set_PWMRUN;
}

void setCommutation(unsigned char workstep, unsigned int dutyv){
	PMEN = 0xFF;				// pwm output all off

	if(workstep!=3&&workstep!=4)
	    DOWN_A_OFF;
	if(workstep!=0&&workstep!=5)
	    DOWN_B_OFF;
	if(workstep!=1&&workstep!=2)
	    DOWN_C_OFF;

	setDuty(dutyv);				// set duty value in each commuation

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
	
	PMEN = ~ PWM_MARSK_TABLE[workstep];	// one chanel pwm open
}

void preLoc(){
	workstep = 5;		
	pwmduty = pwm * PWMOUT /100;      // duty cicy
	setCommutation(workstep, pwmduty);
 	for(i=0; i<1000; i++); 	// delay
}

/* 定义功能函数 */
void main(){
	unsigned char step = 0;
	Set_All_GPIO_Quasi_Mode;
	for (i = 0; i < 50000; ++i); 	// deylay 

	clkConf();
	ioConf();
	pwmConf();
	pwmStart();
	preLoc();			// the time is not sure
	while (1){
	}
}
