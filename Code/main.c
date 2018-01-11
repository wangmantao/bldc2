/*
  FOR BLDC 2017
  --Pin configure--
  HA   P12
  HB   P01
  HC   P10

  LA   P00
  LB   P11
  LC   P03

  EA   P30 AIN1 -> P05 AIN4
  EB   P07 AIN2
  EC   P06 AIN3
  HV   P17 AIN0

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
#define PWMOUT2 20 			//启动（）占空比
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
static unsigned char i = 0;
static unsigned char startOk = 0;
static unsigned char workstep = 0;
static unsigned char laststep = 9;
static unsigned int actCount = 0;
static unsigned int change_delay = 50;
static unsigned int hv = 0;
static unsigned int ev = 0;
static unsigned char adchv = 0;
static unsigned char zCount = 0;
static unsigned int zzTime[3] = {0,0,0};
// PWM 占空比的寄存器值 （PWM0H/PWML)
static unsigned int duty = 0;
static unsigned char StepZok= 0;
// PWM 频率的寄存器值 （PWMPH/PWMPL)
static const unsigned long pwm = ((unsigned int)((STM8_FREQ_MHZ * (unsigned long)1000000)/PWM_FREQUENCY) ); 
const unsigned char PWM_MARSK_TABLE[6]={0x01, 0x01, 0x10, 0x10, 0x04, 0x04};
const unsigned char ST_MARK[6]={0x01, 0x02, 0x04, 0x08, 0x10, 0x20}; // [x x s5 s4 s3 s2 s1]
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

  /*
  P30_Input_Mode;
  clr_P30DIDS;
	*/
  P05_Input_Mode;
  clr_P05DIDS; 
  P06_Input_Mode;
  clr_P06DIDS;
  P07_Input_Mode;
  clr_P07DIDS;
  P06_Input_Mode;
  clr_P06DIDS; 

  P17_Input_Mode;
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
  PWM0H =(unsigned char) (duty >> 8);    // set duty 
  PWM0L =(unsigned char) duty; 
  //while( !(LOAD == 0X00) );				// whait LOAD reset 0 by self
  set_LOAD;
}

void setCommutation(){
  if ( workstep != 3 && workstep != 4)
    DOWN_A_OFF;
  if ( workstep != 0 && workstep != 5)
    DOWN_B_OFF;
  if ( workstep != 1 && workstep != 2)
    DOWN_C_OFF;
  setDuty();
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
    Enable_ADC_AIN3; 
    break;
  case 1:                            // AC
    DOWN_C_ON; PWM0_ETGSEL_EN; 
    Enable_ADC_AIN2; 
    break;
  case 2:                            // BC
    DOWN_C_ON; PWM4_ETGSEL_EN; 
    Enable_ADC_AIN4; 
    break;
  case 3:                            // BA
    DOWN_A_ON; PWM4_ETGSEL_EN; 
    Enable_ADC_AIN3; 
    break;
  case 4:                            // CA
    DOWN_A_ON; PWM2_ETGSEL_EN; 
    Enable_ADC_AIN2; 
    break;
  case 5:                            // CB
    DOWN_B_ON; PWM2_ETGSEL_EN; 
    Enable_ADC_AIN4; 
    break;
  }
  PMEN = ~ (PWM_MARSK_TABLE[workstep]);	// one chanel pwm open
}

void preLoc(){
  workstep =5;
  duty=pwm*PWMOUT/100;      	// duty cicy(location stage)
  duty =(unsigned int) (pwm*PWMOUT/100);      	// duty cicy(location stage)
  setDuty();
  setCommutation();
}

unsigned char  bldcStart2(){
	unsigned int timerT = 300, i2;
	while (1) {
		workstep++;
		if(workstep >=6 ) workstep = 0;
		setCommutation();
		// 延迟，以50uS为一单位，延迟timer个单位
		for(i2=0; i2<timerT; i2++){
			//Timer2_Delay500us(1);
		    //Timer3_Delay10us(5);
			for(i=0; i<190; i++);
		}
		timerT -=  timerT/15+1;
		if (timerT < 25) return 0;
		/*
		if(timerT>25){
			timerT -=  timerT/15+1;
		}
		else
			timerT = 25;
		*/
	}
}
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
      /*

    case 52: change_delay = 60;
      	break;
		case 150: change_delay = 40;
			break;
		case 300: change_delay = 20;
			break;

			case 140: change_delay = 5;
			break;
      */
		}
    Timer2_Delay500us(change_delay);

	}while(actCount <1000 && startOk == 0);   // action not enough, and start fail 


	if(actCount >= 1000 ){
		return 0; 	// ng
	}
	return 1;		// ok
}

void keepAllOff(){
	clr_P12; clr_P01; clr_P10; 	// upA  upB  upC OFF
	duty = 0x00; setDuty(); 	// keep low for PWM mode start
	DOWN_A_OFF; DOWN_B_OFF; DOWN_C_OFF;
}

void keepUpAllOff(){
	clr_P12; clr_P01; clr_P10; 	// upA  upB  upC OFF
	duty = 0x00; setDuty(); 	// keep low for PWM mode start
	DOWN_A_ON; DOWN_B_ON; DOWN_C_ON;
}

void adcConf(){
	ADCCON0 = 0X00;	 // ETGSEL =00  外部触发源选择:PWM0 
 	ADCCON1 = 0X02;  // ETGTYP = 00 falling; ADCEX = 1; ADCEN = 0;
	                 // AD finish, ADCF set 1 automatic, and clear by manual
}

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
  clr_TR1;
  clr_P04;      // 表示执行了换相
  	if (actCount=200)
  	{
  		startOk;
  	}
  	workstep++; 
	if(workstep >=6 ) workstep = 0;
	setCommutation();
}

/* 执行两次ADC中断 */
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


void countConf(){
  TIMER0_MODE1_ENABLE;		// 16位定时器
  TIMER1_MODE1_ENABLE;		// 16位定时器
  // 读写TH0,TL0时，要clr_TR0
}

void main(){
  Set_All_GPIO_Quasi_Mode;
  clr_P04;
  keepAllOff();

  clkConf();
  ioConf();
  adcConf();
  pwmConf();
  countConf();
  Timer2_Delay500us(100);

  preLoc();
  pwmStart();
  Timer2_Delay500us(1000);

 // IE = 0XC8;           // Enable all interrupt EADC ET1 (p194, 中断向量表）
 //IE = 0XC0;           // Enable all interrupt EADC ET1 (p194, 中断向量表）
  duty =(unsigned int)(pwm*PWMOUT2/100);      	// duty for start
  setDuty();
  set_ADCEN;              //preLoc 后开启ADC
  bldcStart2();
  IE = 0XC8;           // Enable all interrupt EADC ET1 (p194, 中断向量表）
  /*
  if(bldcStart2() == 0){
    keepUpAllOff();
    set_P04;
    while(1);		// no started to stop all
  }	
	*/
  while (1){
  }
}
