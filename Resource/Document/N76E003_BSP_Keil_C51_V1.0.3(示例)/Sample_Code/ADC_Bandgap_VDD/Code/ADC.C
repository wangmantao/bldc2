/*---------------------------------------------------------------------------------------------------------*/
/*                                                                                                         */
/* Copyright(c) 2017 Nuvoton Technology Corp. All rights reserved.                                         */
/*                                                                                                         */
/*---------------------------------------------------------------------------------------------------------*/

//***********************************************************************************************************
//  Nuvoton Technoledge Corp. 
//  Website: http://www.nuvoton.com
//  E-Mail : MicroC-8bit@nuvoton.com
//  Date   : Apr/21/2017
//***********************************************************************************************************

//***********************************************************************************************************
//  File Function: N76E003 ADC demo code
//***********************************************************************************************************

#include "N76E003.h"
#include "SFR_Macro.h"
#include "Function_define.h"
#include "Common.h"
#include "Delay.h"


//*****************  The Following is in define in Fucntion_define.h  ***************************
//****** Always include Function_define.h call the define you want, detail see main(void) *******
//***********************************************************************************************

double  Bandgap_Voltage,VDD_Voltage;			//please always use "double" mode for this
		
void ADC_Bypass (void)										// First three times band-gap convert result bypass
{
		unsigned char ozc;
		for (ozc=0;ozc<0x03;ozc++)
		{
			clr_ADCF;
			set_ADCS;
			while(ADCF == 0);
		}
}
		
void READ_BANDGAP()												// Read band-gap actually value after UID two byte.
{																					// Detail description see datasheet V1.02 band-gap chapter.
		UINT8 BandgapHigh,BandgapLow;
		double Bandgap_Value;									// always use double mode define.
	
		set_IAPEN;
		IAPAL = 0x0C;
    IAPAH = 0x00;
    IAPCN = READ_UID;
    set_IAPGO;
		BandgapHigh = IAPFD;
		IAPAL = 0x0d;
    IAPAH = 0x00;
    IAPCN = READ_UID;
    set_IAPGO;
		BandgapLow = IAPFD;
		BandgapLow = BandgapLow&0x0F;
		clr_IAPEN;
		Bandgap_Value = (BandgapHigh<<4)+BandgapLow;
		Bandgap_Voltage = 3072/(0x0fff/Bandgap_Value);
}
			
/******************************************************************************
The main C function.  Program execution starts
here after stack initialization.
******************************************************************************/
void main (void) 
{

		double bgvalue;
		unsigned int i;
		set_CLOEN;
		P12_Quasi_Mode;														//For GPIO1 output, Find in "Function_define.h" - "GPIO INIT"
		InitialUART0_Timer1(115200);
		READ_BANDGAP();
		Enable_ADC_BandGap;												//Find in "Function_define.h" - "ADC INIT"
		ADC_Bypass();

		for(i=0;i<5;i++)
    {
			Timer0_Delay1ms(20);
			clr_ADCF;
			set_ADCS;									// ADC start trig signal
      while(ADCF == 0);
			ADCdataH[i] = ADCRH;
			ADCdataL[i] = ADCRL;
		}		
		
		for(i=0;i<5;i++)
    {
			ADCsumH = ADCsumH + ADCdataH[i];
			ADCsumL = ADCsumL + ADCdataL[i];
		}				

		ADCavgH = ADCsumH/5;
		ADCavgL = ADCsumL/5;
	
		bgvalue = (ADCavgH<<4) + ADCavgL;
		VDD_Voltage = (0x1000/bgvalue)*Bandgap_Voltage;
//		printf ("\n Bandgap voltage = %e", Bandgap_Voltage); 
//		printf ("\n VDD voltage = %e", VDD_Voltage); 
    while(1);
}


