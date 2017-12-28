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
#define PWMOUT 80 			//预定位（proLoc）占空比
#define PWMOUT2 5 			//启动（）占空比
#define DOWN_A_ON set_P00
#define DOWN_B_ON set_P11
#define DOWN_C_ON set_P03
#define DOWN_A_OFF clr_P00
#define DOWN_B_OFF clr_P11
#define DOWN_C_OFF clr_P03

/* ------- 定义变量 --------------*/
unsigned int i = 0;
unsigned char workstep = 0;
unsigned char startOk = 0;
// PWM 占空比的寄存器值 （PWMPH/PWMPL)
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
	PWM0_P12_OUTPUT_ENABLE; 	// upA
	PWM4_P01_OUTPUT_ENABLE; 	// upB
	PWM2_P10_OUTPUT_ENABLE; 	// upC

	P00_PushPull_Mode; 		//downA
	P11_PushPull_Mode; 		// downB
	P03_PushPull_Mode; 		// downC

	P04_PushPull_Mode; 		// downC
}

void pwmConf(){
	set_CLRPWM;     		// clrear pwm counter
					//PWM_CLOCK_DIV_8;	// sysClk / 8 for pwm
	PWM_GP_MODE_ENABLE;		// use group mode
	PWMPH = (unsigned char) (pwm >> 8);  // set pwm frequnce	
	PWMPL = (unsigned char) pwm; 
	
	PMD = 0XFF;			// wen masked, 00: ground / FF: vcc
	PWM_OUTPUT_ALL_NORMAL;		// set on_duty is hight/low & _INVERSE
}

void pwmStart(){
	//set_EPWM;    			//Enable PWM interrupt
	set_EA;
	set_LOAD;
	set_PWMRUN;
}

void pwmStop(){
	//clr_EPWM;    			//Enable PWM interrupt
	clr_EA;
	clr_LOAD;
	clr_PWMRUN;
}

/*
void setDuty(unsigned int dutyv){
	PWM0H =(unsigned char) (dutyv >> 8);    // set duty 
	PWM0L =(unsigned char) dutyv; 
}
*/

//void setCommutation(unsigned char workstep, unsigned int dutyv){
void setCommutation(unsigned char workstep){
	PMEN = 0xFF;				// pwm output all off

	if(workstep!=3&&workstep!=4)
	    DOWN_A_OFF;
	if(workstep!=0&&workstep!=5)
	    DOWN_B_OFF;
	if(workstep!=1&&workstep!=2)
	    DOWN_C_OFF;

	//setDuty(dutyv);
	PWM0H =(unsigned char) (duty >> 8);    // set duty 
	PWM0L =(unsigned char) duty; 

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
	//duty=pwm*PWMOUT/100;      	// duty cicy(location stage)
	duty =(unsigned int) (pwm*PWMOUT/100);      	// duty cicy(location stage)
	//setCommutation(workstep, pwmduty);
	setCommutation(workstep);
 	//for(i=0; i<16000; i++); 	// delay
}

unsigned char  bldcStart(){
	unsigned int actCount;
	startOk = 0;
	do{					   // only one pass, not need do this
		workstep++; actCount++;
		if(workstep >=6 ) workstep = 0;
		setCommutation(workstep);	
 		for(i=0; i<6000; i++); 	// delay
	}while(actCount < 200 && startOk == 0);   // action not enough, and start fail 

	if(actCount >= 200){
		return 0; 	// ng
	}	
	return 1;		// ok
}

/* 定义功能函数 */
void main(){
	unsigned char step = 0;
	Set_All_GPIO_Quasi_Mode;
	for (i = 0; i < 5000; ++i); 	// deylay 

	clkConf();
	ioConf();
	pwmConf();
	preLoc();			// the time is not sure

	clr_P04;
 	//for(i=0; i<8000; i++); 	// delay
	Timer0_Delay1ms(50);

	set_P04;
	pwmStart();

	//clr_P04;
 	//for(i=0; i<8000; i++); 	// delay
	Timer0_Delay1ms(50);
	pwmStop();
	//set_P04;
	duty =(unsigned int)(pwm*PWMOUT2/100);      	// duty cicy(location stage)
	pwmStart();
	if(bldcStart() == 0){
		//pwmStop();
		while(1);		// no started to stop all
	}	
	while (1){
	}
}
