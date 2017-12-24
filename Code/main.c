#include "N76E003.h"
#include "Common.h"
#include "SFR_Macro.h"
#include "Function_define.h"
#include "Delay.h"

/* ---------- 定义常量 --------------*/
#define STM8_FREQ_MHZ 16 
#define PWM_FREQUENCY 16000 //16K
#define PWMOUT 15 			//占空比

/* ---------- 定义变量 --------------*/
// PWM信号周期
//static const u16 hArrPwmVal = ((u16)((STM8_FREQ_MHZ * (u32)1000000)/PWM_FREQUENCY));


/* ---------- 定义函数 --------------*/
// 系统时钟配置: 内部16M
void Clock_init(){
	Set_All_GPIO_Quasi_Mode;// In Common.h define
    set_CLOEN;
    set_HIRCEN; 			// 高速内部RC 16M 
}

void PWM_IO_init(){
	// 下桥0有效，默认设为1

	/* 上桥1有效，默认设为0 */
	// A pwm output ( pin_13 P1.2 PWM0/IC0 )
	P12_PushPull_Mode;
	//set_P1SR_2;
}


/* 定义功能函数 */
void main(){
	unsigned int i;
	unsigned char step = 0;
	for (i = 0; i < 50000; ++i);
	Clock_init(); 	// 时钟配置
	PWM_IO_init();
	while (1){
		// 实验一个PP端口的反转变化
		set_P12;
		for (i = 0; i < 5; ++i);
		clr_P12;
		for (i = 0; i < 5; ++i);
	}
}
