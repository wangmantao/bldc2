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
       set_CLOEN;				// 启动P2.1时钟输出
    set_HIRCEN; 			// 高速内部RC 16M 
}

void PWM_IO_init(){
	// 下桥0有效，默认设为1

	/* 上桥1有效，默认设为0 */
	P12_PushPull_Mode;  // A pwm output ( pin_13 P1.2 PWM0/IC0 )
		
	//set_P1SR_2;

	/* 临时实验 */
	set_PIO00; 	    // 设置P12->PWM0
}

// 周期－占空比的设置
void PWM_init(){
	//　清０计数器
	set_CLRPWM;
	//　设置周期寄存器

	/**********************************************************************
	PWM frequency 	= Fpwm/((PWMPH,PWMPL) + 1) <Fpwm = Fsys/PWM_CLOCK_DIV> 
			= (16MHz/8)/(0x3FF + 1)
			= 1.9KHz
	***********************************************************************/
	PWM_CLOCK_DIV_8;
	PWMPH = 0x03;		//把PWM预设频率分成若干份，一份即一PWM脉冲的频率
	PWMPL = 0xFF;

	PWM0H = 0x01;          //相对于一份PWM脉冲，占空所占的份额
	PWM0L = 0xFF; 

	set_EPWM;    		//Enable PWM interrupt
	set_EA;                                                                 
	set_LOAD;
	set_PWMRUN;
				
	//　启动计数开始
	//　查看PWM
}

/* 定义功能函数 */
void main(){
	unsigned int i;
	unsigned char step = 0;

	Set_All_GPIO_Quasi_Mode;

	for (i = 0; i < 50000; ++i);
	Clock_init(); 	// 时钟配置
	PWM_IO_init();
	PWM_init();	

	while (1){
		// 实验一个PP端口的反转变化
		// set_P12;
	}
}
