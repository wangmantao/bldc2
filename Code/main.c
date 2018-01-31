/*
   FOR BLDC 2017
   --Pin configure--
   HA   P12
   HB   P01
   HC   P10

   LA   P00
   LB   (P11)  --> P13 (原P11要做为CS电流检测)
   LC   P03

   EA   P30 AIN1 -> P05 AIN4
   EB   P07 AIN2 => P06 AIN3 (PCB 设计适应)
   EC   P06 AIN3 => P07 AIN2 (PCB 设计适应)
   MV   P17 AIN0

   HV   (P17 AIN0) -> P30 AIN1 方向信号
   CS   P11 AIN7
   SC   P04 AIN5

*/

#include "N76E003.h"
#include "Common.h"
#include "SFR_Macro.h"
#include "Function_define.h"

/* ---------- 定义常量 --------------*/
#define STM8_FREQ_MHZ 16 
#define PWM_FREQUENCY 18000 		//PWM频率16K, 将被计算出pwm值,写入PWMPH/L
//#define PWMOUT 15 			//预定位（proLoc）占空比

/* ------------------------------------
   给ADC触发的PWM信号源，用于悬空绕组测量时，可以用其它未空绕组的PWM信号
   ETGSEL1 ETGSEL0
   00 = PWM0.
   01 = PWM2.
   10 = PWM4.
   -----------------------------------*/
#define PWM0_ETGSEL_EN clr_ETGSEL1; clr_ETGSEL0;
#define PWM2_ETGSEL_EN clr_ETGSEL1; set_ETGSEL0;
#define PWM4_ETGSEL_EN set_ETGSEL1; clr_ETGSEL0;

#define DOWN_A_ON set_P00			// this for high level avalable
#define DOWN_B_ON set_P13
#define DOWN_C_ON set_P03
#define DOWN_A_OFF clr_P00
#define DOWN_B_OFF clr_P13
#define DOWN_C_OFF clr_P03

/*
#define DOWN_A_ON clr_P00			// this for  low level avalable
#define DOWN_B_ON clr_P13
#define DOWN_C_ON clr_P03
#define DOWN_A_OFF set_P00
#define DOWN_B_OFF set_P13
#define DOWN_C_OFF set_P03
*/

/* ------- 定义变量 --------------*/
static bit zOk= 0;            // 奇偶开关，偶取中点电压（先），奇取端电压
static bit demag = 0;
static bit dir = 0;
static char adcSwitch = 0;			// adc开关，0:中点电压（先），1:取端电压, 3:母线电流检测
static char startLevel = 0;            // 0  启动前
static unsigned int i = 0;
static unsigned char startOk = 0;
static unsigned char workstep = 0;
static unsigned int actCount = 0;
static unsigned int hv = 0;     // 中点电压 (在startLevel小于1，定方向时，存临时值)
static unsigned int ev = 0;     // 悬空端电压
static unsigned int ocpv = 0;     //过电流检测
static unsigned int evLast = 0xFFFF;     // 悬空端电压
static unsigned int cv = 0;     // 过0时间统计值
static unsigned int cv1 = 0;     // 过0时间统计值
static unsigned int cv2 = 0;     // 过0时间统计值
static unsigned char zCount = 0;
static unsigned char PWMOUT2 = 20;              //设置占空比
static unsigned char PWMOUT = 20;              //系统动态自变的占空比
static unsigned int up =0;
static unsigned int down = 0;
// PWM 占空比的寄存器值 （PWM0H/PWML)
static unsigned int duty = 0;
// PWM 频率的寄存器值 （PWMPH/PWMPL)
static const unsigned long pwm = ((unsigned int)((STM8_FREQ_MHZ * (unsigned long)1000000)/PWM_FREQUENCY) ); // PWM周期
const unsigned char PWM_MARSK_TABLE[6]={0x01, 0x01, 0x10, 0x10, 0x04, 0x04};

// Pid有关
static unsigned int Target = 800;
static unsigned int Real = 0;
static float PP=0.5,II=0.5,DD=0;
static double SumError=0,PrevError=0,LastError=0;
static int dError=0,Error=0;


void nextStep(){
	if (dir ==1){
		workstep++;
		if(workstep >=6 ) workstep = 0;
	}
	else {
		if(workstep == 0)
			workstep = 5;
		else
			workstep--;
	}
}

void PID_init(void)
{
	Error=0;SumError=0;LastError=0;
}

/*--------------------------------------------------------------------
  根据设定及采集值进行计算PID调节，计算pwm输出值
  --------------------------------------------------------------------*/
//void PIDcompute(unsigned int Target,unsigned int Real)
void PIDcompute()
{
	float j=0.0,ii;
	Error =(Target-Real);           
	SumError +=Error;                   
	dError=Error-LastError;
	LastError=Error;

	ii=PP;
	j=Error*ii;
	ii=II;
	j=j+SumError*ii;
	ii=DD;
	j=j+dError*ii;

	j=j/100;

	if(j >= 98)
		PWMOUT2 = 98;
	else if(j<1) 
		PWMOUT2=1;
	else 
		PWMOUT2=j;

	duty =(unsigned int)(pwm*PWMOUT2/100);          // duty for start
	PWM0H =(unsigned char) (duty >> 8);    // set duty 
	PWM0L =(unsigned char) duty; 
	set_LOAD;
}

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
	P13_PushPull_Mode; 			// downB
	P03_PushPull_Mode; 			// downC

	// 定义模拟采样输入
	P05_Input_Mode;		// AIN4 EA
	clr_P05DIDS;

	P07_Input_Mode;		// AIN3 EB
	clr_P07DIDS;

	P06_Input_Mode;	 // AIN2 EC
	clr_P06DIDS;

	P17_Input_Mode;		// 中点
	clr_P17DIDS;

	P04_Input_Mode;		// 速度控制 sc speed control _5V分压
	clr_P17DIDS;

	P11_Input_Mode;		// 过电流检测 cs current sence AIN7
	clr_P11DIDS;

	P30_Input_Mode;		// 方向转换 hv 
	clr_P30DIDS;

	P14_PushPull_Mode; 			// downC
	clr_P14;

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
	//set_EPWM;    			//Enable PWM interrupt
	set_LOAD;         // load PWM duty from buffer
	set_PWMRUN;
}

void pwmStop(){
	//clr_EPWM;    			//Enable PWM interrupt
	clr_PWMRUN;
}

void setDuty(){
	duty =(unsigned int)(pwm*PWMOUT2/100);          // duty for start
	PWM0H =(unsigned char) (duty >> 8);    // set duty 
	PWM0L =(unsigned char) duty; 
	set_LOAD;
}

void setCommutation(){
	demag = 0;  evLast = 0xffff;         //消磁检测参考
	adcSwitch = 0; Enable_ADC_AIN0;  //每次换相以取中点电压为最先
	if ( workstep != 3 && workstep != 4)
		DOWN_A_OFF;
	if ( workstep != 0 && workstep != 5)
		DOWN_B_OFF;
	if ( workstep != 1 && workstep != 2)
		DOWN_C_OFF;
	/* ----------------------------------------------
	   | AB AC BC BA CA CB    no work:   !a   !b   !c  |
	   | 0   1  2  3  4  5       step:   2 5  1 4  0 3 |
	   |                       PWM上管：  B C  A C  A B |
	   |                       ex_PWM:   4 2  0 2  0 4 |
	   -----------------------------------------------*/
	// (for ADC:选择悬空绕组; 及其对应的高管PWM信号源)
	// (for commuation: 开启下管)
	switch (workstep) { 
		case 0:                            // AB
			DOWN_B_ON; PWM0_ETGSEL_EN; 
			break;
		case 1:                            // AC
			DOWN_C_ON; PWM0_ETGSEL_EN; 
			break;
		case 2:                            // BC
			DOWN_C_ON; PWM4_ETGSEL_EN; 
			break;
		case 3:                            // BA
			DOWN_A_ON; PWM4_ETGSEL_EN; 
			break;
		case 4:                            // CA
			DOWN_A_ON; PWM2_ETGSEL_EN; 
			break;
		case 5:                            // CB
			DOWN_B_ON; PWM2_ETGSEL_EN; 
			break;
	}
	PMEN = ~ (PWM_MARSK_TABLE[workstep]);	// one chanel pwm open
}

void preLoc(){
	if(dir == 1)
		workstep =5;
	else
		workstep = 1;
	PWMOUT2 = 20;
	setDuty();
	setCommutation();
}

unsigned char  bldcStart(){
	unsigned int timerT = 300, i2, i3;
	while (1) {
		nextStep();
		setCommutation();
		// 延迟，以50uS为一单位，延迟timer个单位
		for(i2=0; i2<timerT; i2++){
			for(i3=0; i3<280; i3++);
		}
		timerT -=  timerT/15+1;
		if (timerT < 25) return 0;
	}
}

void keepAllOff(){
	//clr_ET3;
	clr_ADCEN;
	clr_TR0; clr_TR1; clr_TR3; clr_ET3;
	clr_P12; clr_P01; clr_P10; 	// upA  upB  upC OFF
	PWMOUT2 = 0; setDuty(); 	// keep low for PWM mode start
	DOWN_A_OFF; DOWN_B_OFF; DOWN_C_OFF;

}

void keepUpAllOff(){
	clr_ADCEN;
	clr_TR0; clr_TR1; clr_TR3;
	clr_P12; clr_P01; clr_P10; 	// upA  upB  upC OFF
	PWMOUT2 = 0; setDuty(); 	// keep low for PWM mode start
	DOWN_A_ON; DOWN_B_ON; DOWN_C_ON;
}

void reKeepUpAllOff(){
	set_ADCEN;
	set_TR0; set_TR1; set_TR3;
	// 其它项换相中自动设置
}

void adcConf(){
	// adc
	ADCCON0 = 0X00;	 // ETGSEL =00  外部触发源选择:PWM0  每次PWM0脉冲都比较一次
	ADCCON1 = 0X02;  // ETGTYP = 00 falling; ADCEX = 1; ADCEN = 0;
	// AD finish, ADCF set 1 automatic, and clear by manually
	//adc 比较器
	ADCDLY = 0X00;
	clr_ADCS;        // 暂不启动ADC转换
	//ADCCON2 = 0x20;	// 7.ADFBEN(break) 6.ADCMPOP(ADCR>=ADCOMP->1) 5.ADCMPEN 4.ADCMPO(转换结果)
}

/*
   由workstep确定去取对应的相端悬空电压
   */
void reAIN(){
	switch (workstep){
		case 0:	                  // AB
			Enable_ADC_AIN2;  // ->2
			break;
		case 1:                            // AC
			Enable_ADC_AIN3;  // ->3
			break;
		case 2:                            // BC
			Enable_ADC_AIN4; 
			break;
		case 3:                            // BA
			Enable_ADC_AIN2;  // ->2
			break;
		case 4:                            // CA
			Enable_ADC_AIN3;  // ->3
			break;
		case 5:                            // CB
			Enable_ADC_AIN4; 
			break;
	}
}

// 定时3器溢出，用于定时pid 计算
void forPid() interrupt 16 {
	Real = ( 1/( (cv + cv1+ cv2)/3*0.75*6 )*60) / 5*1000000;
	PIDcompute ();
}

// 定时1器溢出，用于换相延迟
void forCommuation() interrupt 3 {
	nextStep();
	setCommutation();
	set_ADCEX;      //继续pwm触发ADC
	clr_ADCF;      // 只有在没有过0时，才会启用ADC；当过0后，还未换相前不启用ADC
}

// 上升反电势过0检查
void eUpChk(){
	if (demag == 0){
		if (ev > evLast) {
			demag = 1;
		}
	}
	else if (ev > hv) {
		zOk = 1;
	}
	evLast = ev;
}

// 下降反电势过0检查
void eDownChk(){
	if(demag == 0) {
		if(ev > 350) demag = 1;   //350 =0.4v 消磁的上限
	}
	else if( ev < hv ) {
		zOk = 1;
	}
}

void setDir(){
	if (hv == 0)
		dir = 1;
	else
		dir = 0;
}

void setSpeed(){
	if (hv == 0)                            // <1v 819
		Target = 800;
	else if (hv > 1638 && hv < 2457)          // 2 （1638） ~ 3v （2457）
		Target = 1000;
	else if (hv > 2457 && hv < 3276)          // 3 ~ 4v （3276）
		Target = 1200;
	else if (hv >= 3276)          // >= 4v (3276)
		Target = 1400;

}

/* 中断－ADC转换完成 [一直使能]  */
void adcHandle() interrupt 11 {
	unsigned int t1pv = 0;                      // timer1初值
	switch (startLevel) {
		case 0: {              // 定方向
        startLevel = 1;    // 下一次取5v分压 speed control
				Enable_ADC_AIN5;
				hv = ADCRH; hv <<= 4; hv |= (ADCRL & 0X0F);  // 得HV分压值
				setDir();
				clr_ADCF;
				set_ADCS;
				break;
			}
		case 1: {             // 定speed
        startLevel = 2;     // 下一次为检过0,某一项ev
				hv = ADCRH; hv <<= 4; hv |= (ADCRL & 0X0F);  // 得5v分压值
				setSpeed();
				clr_EADC;  // 不再启动ADC，停用ADC中断
				clr_EA;
				clr_ADCF;
				break;
			}
   case 2: {             //过0换相, 与过电流检测
     zOk = 0;                 // 重置为未过0 // 只有在没有过0时，才会启用ADC；当过0后，还未换相前不启用ADC

				if (adcSwitch == 0) // 第1测中点电压
				{
					reAIN();
					hv = ADCRH; hv <<= 4; hv |= (ADCRL & 0X0F);  // 得中点值
				}

				else if (adcSwitch == 1) 			// 第2测端电压(ev)
        {
					//Enable_ADC_AIN0; // 如果过0 由换相函数设置取中点电压，未过0则由过电流部分设置AIN0
          Enable_ADC_AIN7; // cs
					ev = ADCRH; ev <<= 4; ev |= (ADCRL & 0X0F);
					switch (workstep){
						case 1: case 3: case 5:   //  反电势上升
							if (dir == 1) eUpChk();
							else eDownChk();
							break;
						case 0: case 2: case 4:   // 反电势下降
							if (dir == 1) eDownChk();
							else eUpChk();
							break;
					}
          // 有过0  注： 只有在没有过0时，才会启用ADC；当过0后，还未换相前不启用ADC
					if ( zOk == 1) {
						clr_ADCEX;  //过0点后暂停PWM触发ADC
						// 得到过0点间的定时值
						clr_TR0;            // 停止term0
						cv=cv1; cv1=cv2;
						cv2 = TH0; cv2 <<= 8; cv2 |= TL0; 
						TH0 = 0; TL0 = 0;
						// 用间隔时间值的一半让timer1定时换相
						clr_TR1;            // 停止term1
						t1pv = 0xFFFF - (cv/2.8);                // set timer1 preValue
						TL1 = t1pv; TH1 = t1pv >> 8;
						set_TR1;                        // 让timer1去换相  
						set_TR0;                        // 继续过0计时
					}
          // 没有过0 本case  break之前 cle_ADCF
				}

        // 第3测过电流
				else if (adcSwitch == 2)
        {
          //adcSwitch = 0; Enable_ADC_AIN0;  // 每检完总电流后，接着测中点电压
          Enable_ADC_AIN0;  //中点
					ocpv = ADCRH; ocpv <<= 4; ocpv |= (ADCRL & 0X0F);
          if( ocpv > 102) {                // 如果过电流关闭一切 (102 = 0.125v / 2.5A@50mO)
            keepAllOff();
          }
        }

        // ------------------------ adcSwitch 变更 ----------------------
				//adcSwitch = ~adcSwitch;  // 取ev电压和hv中点电压，交互进行;每次换相时，初始化为0,即先取中点
        if (adcSwitch == 0){
          adcSwitch = 1;
        }
        else if(adcSwitch == 1){
          adcSwitch = 2;
        }
        else if(adcSwitch == 2){
          adcSwitch = 0;
        }
        clr_ADCF;
				break;
			}
	}
}

void countConf(){
	// T0
	TIMER0_MODE1_ENABLE;		// 16位定时器
	TIMER1_MODE1_ENABLE;		// 16位定时器
	//注: 读写TH0,TL0时，要clr_TR0

	// T3
	clr_TR3;    // timer3 stop
	T3CON = 0X04; // FOR T3PS = 0 SYS/16 频率 (T3PS:100)
	RL3 = 0X00;
	RH3 = 0X00;  //自动重载的值 0xffff - 50ms ()
}

/*
   void itemTest(unsigned int speed) {
   unsigned int i2;
// 启动, 设定转速 
Target = speed;
// 运行时间
for (i2 = 0; i2<10000; i2++){
for (i = 0; i < 615; ++i); // wait 20s
} 
// 停
keepAllOff();
// 等一会
for (i2 = 0; i2<8000; i2++){    // wait 10s
for (i = 0; i < 615; ++i); 
} 
IE = 0X00;
// 退出
}
*/


void main(){
	unsigned int i2;
	/*开机*/
	Set_All_GPIO_Quasi_Mode;
	keepAllOff();		// 防止MOS误动

	/*配置*/
	clkConf();
	ioConf();
	adcConf(); 		// 同时配置adc比较
	pwmConf();
	countConf();
	for (i2 = 0; i2<50; i2++){
		for (i = 0; i < 615; ++i); // wait 1ms
	}

	/*系统配置检验*/
	Enable_ADC_AIN1; // HV for dir信号检测 (startLevel =0)
	set_EADC;
	set_EA;
	clr_ADCF;
	set_ADCS;        // interrupt 11 去执行中断处理....

	/*定位*/
	preLoc(); 		// 内含duty设置
	pwmStart();
	for (i2 = 0; i2<50; i2++){
		for (i = 0; i < 615; ++i); // wait 1ms
	}

	/*启动*/
	PWMOUT2=35; //35
	setDuty();
	bldcStart();
	nextStep();
	setCommutation();
	//startLevel = 2;

	PID_init();
	set_ADCEN;              //强制加速后开启ADC, 即开启自动换相
	set_ADCS;
	IE = 0XCA;           // Enable EA EADC | ET1 ET0  (p194, 中断向量表）
	set_ET3;             // Enable timer3 中断 for PID定时调控
	set_TR3;

	//itemTest(800);
	while (1){
	}
}
