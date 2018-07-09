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
   EB   P07 AIN2
   EC   P06 AIN3
   HV   (P17 AIN0) -> P30 AIN1
   MV   P17 AIN0
   CS   P11 AIN7
   SC   P04 AIN5

*/

#include "N76E003.h"
#include "Common.h"
#include "SFR_Macro.h"
#include "Function_define.h"
#include "Delay.h"

/* ---------- 定义常量 --------------*/
#define STM8_FREQ_MHZ 16 
#define PWM_FREQUENCY 16000 		//PWM频率16K, 将被计算出pwm值,写入PWMPH/L
//#define PWMOUT 15 			//预定位（proLoc）占空比
// 

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
static bit joSwitch = 0;			// 奇偶开关，偶取中点电压（先），奇取端电压
static char started = 0;            // 0  启动前
static unsigned char i = 0;
static unsigned char startOk = 0;
static unsigned char workstep = 0;
static unsigned int actCount = 0;
static unsigned int hv = 0;     // 中点电压
static unsigned int ev = 0;     // 悬空端电压
static unsigned int cv = 0;     // 过0时间统计值
static unsigned int cv1 = 0;     // 过0时间统计值
static unsigned int cv2 = 0;     // 过0时间统计值
static unsigned char zCount = 0;
static unsigned char PWMOUT2 = 20;              //启动（）占空比
static unsigned char PWMOUT = 20;              //启动（）占空比
static unsigned int up =0;
static unsigned int down = 0;
// PWM 占空比的寄存器值 （PWM0H/PWML)
static unsigned int duty = 0;
static unsigned char setComCount = 0;
// PWM 频率的寄存器值 （PWMPH/PWMPL)
static const unsigned long pwm = ((unsigned int)((STM8_FREQ_MHZ * (unsigned long)1000000)/PWM_FREQUENCY) ); 
const unsigned char PWM_MARSK_TABLE[6]={0x01, 0x01, 0x10, 0x10, 0x04, 0x04};


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

	/*
	   P30_Input_Mode;
	   clr_P30DIDS;
	   */
	P05_Input_Mode;		// AIN4 A
	clr_P05DIDS; 

	P06_Input_Mode;	// AIN1 B
	clr_P06DIDS;

	P07_Input_Mode;		// AIN2 C
	clr_P07DIDS;

	P17_Input_Mode;		// 中点
	clr_P17DIDS;

	P04_PushPull_Mode; 			// for test referrence 
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
	clr_EPWM;    			//Enable PWM interrupt
	clr_PWMRUN;
}

void setDuty(){
    if (PWMOUT2 > PWMOUT && started == 1) {
        if(PWMOUT == PWMOUT2) started =3;   // let setCommuation 不在setDuty
        if(setComCount >= 4){
            PWMOUT++;
            setComCount = 0;
        }
        duty =(unsigned int)(pwm*PWMOUT/100);          // duty for start
        PWM0H =(unsigned char) (duty >> 8);    // set duty 
        PWM0L =(unsigned char) duty; 
        set_LOAD;
    }
    else {
        duty =(unsigned int)(pwm*PWMOUT2/100);          // duty for start
    	PWM0H =(unsigned char) (duty >> 8);    // set duty 
    	PWM0L =(unsigned char) duty; 
    	set_LOAD;
    }
}

void setCommutation(){
    setComCount ++;
	joSwitch = 0; Enable_ADC_AIN0;  //第次换相以取中点电压为最先
	if ( workstep != 3 && workstep != 4)
		DOWN_A_OFF;
	if ( workstep != 0 && workstep != 5)
		DOWN_B_OFF;
	if ( workstep != 1 && workstep != 2)
		DOWN_C_OFF;
	if (started == 1 ) setDuty();   // 启动之后，加速之前
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
			//Enable_ADC_AIN3; 
			break;
		case 1:                            // AC
			DOWN_C_ON; PWM0_ETGSEL_EN; 
			//Enable_ADC_AIN2; 
			break;
		case 2:                            // BC
			DOWN_C_ON; PWM4_ETGSEL_EN; 
			//Enable_ADC_AIN4; 
			break;
		case 3:                            // BA
			DOWN_A_ON; PWM4_ETGSEL_EN; 
			//Enable_ADC_AIN3; 
			break;
		case 4:                            // CA
			DOWN_A_ON; PWM2_ETGSEL_EN; 
			//Enable_ADC_AIN2; 
			break;
		case 5:                            // CB
			DOWN_B_ON; PWM2_ETGSEL_EN; 
			//Enable_ADC_AIN4; 
			break;
	}
	PMEN = ~ (PWM_MARSK_TABLE[workstep]);	// one chanel pwm open
}

void preLoc(){
	workstep =5;
	//duty =(unsigned int) (pwm*PWMOUT/100);      	// duty cicy(location stage)
    PWMOUT2 = 20;
	setDuty();
	setCommutation();
}

unsigned char  bldcStart2(){
	unsigned int timerT = 300, i2, i3;
	while (1) {
		workstep++;
		if(workstep >=6 ) workstep = 0;
		setCommutation();
		// 延迟，以50uS为一单位，延迟timer个单位
		for(i2=0; i2<timerT; i2++){
			for(i3=0; i3<280; i3++);
		}
		timerT -=  timerT/15+1;
		if (timerT < 25) return 0;
	/*
		   if(timerT>25){
		      timerT -=  timerT/15+1;
		   }
*/
	}
}

/*
   unsigned char  bldcStart(){
   actCount = 0;
   startOk = 0;
   do{					   // only one pass, not need do this
   workstep++; actCount++;
   if(workstep >=6 ) workstep = 0;
   zCount =0;
   setCommutation();	
   switch (actCount){
   case 24: change_delay = 100;
   break;

   case 48: change_delay = 80;
   break;
   }
   Timer2_Delay500us(change_delay);

   }while(actCount <1000 && startOk == 0);   // action not enough, and start fail 


   if(actCount >= 1000 ){
   return 0; 	// ng
   }
   return 1;		// ok
   }
   */

void keepAllOff(){
	clr_P12; clr_P01; clr_P10; 	// upA  upB  upC OFF
	PWMOUT2 = 0; setDuty(); 	// keep low for PWM mode start
	DOWN_A_OFF; DOWN_B_OFF; DOWN_C_OFF;
}

void keepUpAllOff(){
	clr_P12; clr_P01; clr_P10; 	// upA  upB  upC OFF
	PWMOUT2 = 0; setDuty(); 	// keep low for PWM mode start
	DOWN_A_ON; DOWN_B_ON; DOWN_C_ON;
}

void adcConf(){
	// adc
	ADCCON0 = 0X00;	 // ETGSEL =00  外部触发源选择:PWM0  每次PWM0脉冲都比较一次
	ADCCON1 = 0X02;  // ETGTYP = 00 falling; ADCEX = 1; ADCEN = 0;
	// AD finish, ADCF set 1 automatic, and clear by manually
	//adc 比较器
	ADCDLY = 0X00;
	set_ADCS;        // 启动ADC转换
	//ADCCON2 = 0x20;	// 7.ADFBEN(break) 6.ADCMPOP(ADCR>=ADCOMP->1) 5.ADCMPEN 4.ADCMPO(转换结果)
}

/*
由workstep确定去取对应的相端悬空电压
*/
void reAIN(){
	switch (workstep){
		case 0:	                  // AB
			Enable_ADC_AIN3; 
			break;
		case 1:                            // AC
			Enable_ADC_AIN2; 
			break;
		case 2:                            // BC
			Enable_ADC_AIN4; 
			break;
		case 3:                            // BA
			Enable_ADC_AIN3; 
			break;
		case 4:                            // CA
			Enable_ADC_AIN2; 
			break;
		case 5:                            // CB
			Enable_ADC_AIN4; 
			break;
	}
}

// 定时1器溢出，用于换相延迟
void commDelay() interrupt 3 {
    //P04 = ~P04;   
    workstep++; if(workstep >=6 ) workstep = 0;
    setCommutation();
    set_ADCEX;      //继续pwm触发ADC
    clr_ADCF;      // 只有在没有过0时，才会启用ADC；当过0后，还未换相前不启用ADC
}

/* 中断－ADC转换完成 [一直使能]  */
void adcHandle() interrupt 11 {

    unsigned int t1pv = 0;                      // timer1初值
	/* ------------ PWM ON 方案 -----*/
	if (joSwitch == 0)		// 先测中点
	{
		reAIN();	// 备下一步取悬空端电压
		hv = ADCRH; hv <<= 4; hv |= (ADCRL & 0X0F);  // 得中点值
        clr_ADCF;       // 只有在没有过0时，才会启用ADC；当过0后，还未换相前不启用ADC
	}
	else {					// 再测悬空端点
		Enable_ADC_AIN0; // 备下一步取中点电压
		ev = ADCRH; ev <<= 4; ev |= (ADCRL & 0X0F);

        if (PWMOUT <= 30)
        {
            up= hv * 1.05; down = hv * 0.95;
        }
        else if (PWMOUT <= 40)
        {
            up= hv * 1.07; down = hv * 0.93;
        }
        else if (PWMOUT <= 50)
        {
            up= hv * 1.10; down = hv * 0.90;
        }
        else if (PWMOUT <= 98)
        {
            up= hv * 1.15; down = hv * 0.85;
        }
        /*
            过0点判断有两种情况
            悬空端由低往高（反电动势上升）
                检测ev在范围内
            悬空端由高往低（反电动势下降）
                检测ev在范围外, 在ev > 0.2的情况下，ev< 中点电压hv
                检测ev在范围内, 
        */
		if( (ev > down) && (ev < up) ) {  // 过0点有效
            clr_ADCEX;  //过0点后暂停PWM触发ADC
   			P04 = ~P04;	
            // 得到过0点间的定时值
            clr_TR0;            // 停止term0
            cv=cv1; cv1=cv2;
            cv2 = TH0; cv2 <<= 8; cv2 |= TL0; 
            TH0 = 0; TL0 = 0;
            // 用间隔时间值的一半让timer1定时换相
            clr_TR1;            // 停止term1
            t1pv = 0xFFFF - (cv/2);                // set timer1 preValue
            TL1 = t1pv; TH1 = t1pv >> 8;
            set_TR1;                        // 让timer1去换相  
            set_TR0;                        // 继续过0计时
   		}
        else {                         // 没有产生过0
            clr_ADCF;     // 只有在没有过0时，才会启用ADC；当过0后，还未换相前不启用ADC
        }
	}
	// Reset some thing
	joSwitch = ~joSwitch;

	/* --------------- PWM OFF 检测方案

	ev = ADCRH; ev <<= 4; ev |= (ADCRL & 0X0F);
	switch (workstep) {
		case 0: case 2: case 4:   // 反电势下降
			if(ev == 0 )    // 80 = 0.1V
				zCount ++;
			//else
				//zCount = 0;
		break;
		case 1: case 3: case 5:   // 反电势上升
			if(ev > 80) // && (ev < 819))    // 80 = 0.1V 819= 1v
				zCount ++;
			else
				zCount = 0;
		break;
	}

    if (zCount > 40) {
		workstep++;
		if(workstep >=6 ) workstep = 0;
		setCommutation();
    	P04=~P04;
		zCount = 0;
		//actCount++;
    }

	clr_ADCF; set_ADCS;
	--------------------------------------------*/
	/*
	set_P04;	
	if (adchv == 0 ){	                      //AIN0  2end interrupt
		ev = ADCRH; ev <<= 4; ev |= (ADCRL & 0X0F);
		Enable_ADC_AIN0;
		adchv = 1;
		clr_ADCF; set_ADCS;
	}
	else{                                 // AINx 1st interrupt (相电势)
		hv = ADCRH; hv <<= 4; hv |= (ADCRL & 0X0F);  // 得中点值
		reAIN();    // 重新启用下相电动势检测
		adchv = 0;
		clr_ADCF;
		// 比较两个电压，控制换相
		if(hv < ev )
			zCount ++;
		else
			zCount = 0;
	    if (zCount > 10) {
			workstep++;
			if(workstep >=6 ) workstep = 0;
			setCommutation();
			zCount = 0;
	    }
	}
	*/
}


/* 执行两次ADC中断 */
/*
   void adcHandle() interrupt 11 {
   unsigned int cv = 0; // count value
   if (StepZok == ST_MARK[workstep]){
   clr_ADCF;	
   return;
   }    //该step 标记是否已有过0点
   if (adchv == 1 ){	                      //AIN0  2end interrupt
   hv = ADCRH; hv <<= 4; hv |= (ADCRL & 0X0F); hv = hv/2;
   if( (hv>800)  && (ev > hv*0.99) && (ev < hv*1.01)){ // for cross 0 point constantly
   zCount ++;
   if ( ( zCount >3)   ){ // && (workstep = laststep) ){                  // cross avalble, write the time of tow 0 pint At zzTime[2]
//P04=~P04;
zCount = 0; 
StepZok = ST_MARK[workstep];

clr_TR0;            // reset cross 0 counter & stop timer
cv = TH0; cv <<= 8; cv |= TL0;  zzTime[2] = cv;  // get timer value

if ((zzTime[2] != 0) && (TR1 == 0)){                 // 开启延时换相
cv = 0xFFFF - (zzTime[2]/2);                // set timer1 preValue
TL1 = cv; TH1 = cv >> 8;
set_TR1; set_P04;
}

TH0 = 0X00; TL0 = 0X00;
set_TR0;  // reset timer0 & restart it
}
}

reAIN();    // 重新启用下相电动势检测
adchv = 0;
clr_ADCF;

}
else{                                 // AINx 1st interrupt (相电势)
ev = ADCRH; ev <<= 4; ev |= (ADCRL & 0X0F);
Enable_ADC_AIN0;
adchv = 1;
clr_ADCF; set_ADCS;
}

}
*/

void countConf(){
	TIMER0_MODE1_ENABLE;		// 16位定时器
	TIMER1_MODE1_ENABLE;		// 16位定时器
	// 读写TH0,TL0时，要clr_TR0
}
void main(){
	/*开机*/
	Set_All_GPIO_Quasi_Mode;
	clr_P04;
	keepAllOff();		// 防止MOS误动

	/*配置*/
	clkConf();
	ioConf();
	adcConf(); 		// 同时配置adc比较
	pwmConf();
	countConf();
	Timer2_Delay500us(2000);

	/*定位*/
	preLoc(); 		// 内含duty设置
	pwmStart();
	Timer2_Delay500us(1000);



	/*启动*/
    PWMOUT2=35;
	setDuty();
	bldcStart2();
    started = 1;
    workstep++; if(workstep >=6 ) workstep = 0;
	setCommutation();	
	/*使能比较中断进入自换相*/
    set_ADCEN;              //preLoc 后开启ADC
	IE = 0XCA;           // Enable EA EADC | ET1 ET0  (p194, 中断向量表）
    set_P04;            //  启动结束，进入下一个setp BA,看C的反电势
    //IE = 0XC8;           // Enable EA EADC ET1  (p194, 中断向量表）
	//IE = 0XC0;           // Enable all interrupt EADC ET1 (p194, 中断向量表）

	//duty =(unsigned int)(pwm*80/100);      	// duty for start
    PWMOUT2 =95;
 	//setDuty();

	while (1){
	}
}
