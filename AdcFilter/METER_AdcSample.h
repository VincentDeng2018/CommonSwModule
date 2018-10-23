/******************************************************************************************************
*            METER_AdcSample.h
******************************************************************************************************
******************************************************************************************************
*
* THIS INFORMATION IS PROPRIETARY TO BYD Corporation
*
******************************************************************************************************
*
*
******************************************************************************************************
******************************************************************************************************
*    FILE NAME: METER_AdcSample.h
*
*    DESCRIPTION: Header file for ADC module
*
*    ORIGINATOR: DYH
*
*    DATE: 2018/6/4 10:07:20
*
*             Version   Author     Modification
*    HISTORY:  V1.0      DYH       Initial Version
******************************************************************************************************/


/**********************************************************************************************
* Includes
**********************************************************************************************/
#ifndef _METER_ADCSAMPLE_H
#define _METER_ADCSAMPLE_H


/* Chip lib include */
/* user include */
#include "DSP2803x_Device.h"
#include "CLA_Shared.h"

/*********************************************************************************
* Macros
*********************************************************************************/


/*********************************************************************************
* Data and Structure
*********************************************************************************/
typedef struct
{
    Uint16 ADC_VPackMeter_mV;
    Uint16 ADC_VOutLvMeter_10mV;
    int16 ADC_CtrlTempMeter_100mC;
    int16 ADC_LvTempMeter_100mC;
    
    int16 ADC_HvTempMeter_100mC;

    int16 ADC_Current1Meter_100mA;
    int16 ADC_Current2Meter_100mA;
    int16 ADC_Current3Meter_100mA;
    int16 ADC_Current4Meter_100mA;

    int16 ADC_Current5Meter_100mA;
    int16 ADC_TotalCurrMeter_100mA;
    int16 ADC_absTotalCurrMeter_100mA;
    int16 ADC_LegAvgCurrMeter_100mA;

    int16 ADC_LegMaxCurrMeter_100mA;
    int16 ADC_LegMinCurrMeter_100mA;

    Uint16 ADC_BattChgPower;
    Uint16 ADC_BattDischgPower;
    Uint16 ADC_VOutHvMeter_100mV;
    int16  ADC_IOutHvMeter_100mA;
    
    Uint16 ADC_UAuxPowerSupply_10mV;
}stADCMeter_t;

extern stADCMeter_t stAdcMeters;

typedef void (*f_SlowFilterCallBack)(enAdcIndex enIndex);

typedef struct
{
    Uint16 adcRaw;
    Uint16 fastGain;
    Uint16 fastPreVal;
    Uint16 fastCurrVal;
    int16 zeroCntVal;
    int16 zeroOffsetCali;
    
    Uint16 SlowAvgShift : 4;  /* means 1 << n */
    Uint16 SlowAvgCnt : 12;

    Uint32 SlowAvgSum;
    
    f_SlowFilterCallBack SlowFilterCb;
}stAdcFilter_t;

extern stAdcFilter_t stAdcFilter[ADC_END];


/*********************************************************************************
* Global DECLARATIONS
*********************************************************************************/
extern void f_AdcSampleSlowTask(void);
extern void f_AdcSampleSWICallback(void);
extern Uint16 f_GetAdcResult(enAdcIndex index);
extern void f_AdcFastFilter(Uint16 adcCnt, enAdcIndex index, Uint16 autoZero);
extern Uint16 ISampleAutoZero(void);
extern void ADC_FilterIniital(void);
extern void f_AdcSlowFilter(enAdcIndex index);
extern void ADC_EnableAutoZero(Uint16 enableOrNot);

#endif
