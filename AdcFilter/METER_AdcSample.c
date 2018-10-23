/******************************************************************************************************
*            METER_AdcSample.c
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
*    FILE NAME: METER_AdcSample.c
*
*    DESCRIPTION: Filter ADC result and convert to physical meters
*
*    ORIGINATOR: DYH
*
*    DATE: 2018/6/4 10:05:45
*
*             Version   Author     Modification
*    HISTORY:  V1.0      DYH       Initial Version, copy from Solar project
******************************************************************************************************/


/**********************************************************************************************
* Includes
**********************************************************************************************/
/* Standard lib include */

/* Chip lib include */
/* user include */
#include "METER_AdcSample.h"
#include "DCDC_Control.h"


#define ADC_VBATT_MAX_MV        16500L
#define ADC_LVOUT_MAX_10MV      6600L
#define ADC_HVOUT_MAX_100MV      5770L
#define ADC_VOUT_1V_10MV        62u
#define ADC_TEMP_OFFSET_FACTOR  500

#ifndef NULL
#define NULL (0)
#endif

/********************************************************************************
* Global DECLARATIONS
********************************************************************************/
void f_AdcFastFilter(Uint16 adcCnt, enAdcIndex index, Uint16 autoZero);
void f_AdcSlowFilter(enAdcIndex index);
Uint16 f_GetAdcResult(enAdcIndex index);
Uint16 ISampleAutoZero(void);
void ADC_FilterIniital(void);
void ADC_EnableAutoZero(Uint16 enableOrNot);

/********************************************************************************
* LOCAL FUNCTION PROTOTYPES
********************************************************************************/
static void f_LvLCurrentSlowFilterCb(enAdcIndex adcIndex);
static void f_PackVoltsSlowFilterCb(enAdcIndex adcIndex);
static void f_LvVoltsFilterCb(enAdcIndex adcIndex);
static void f_HvCurrrentFilterCb(enAdcIndex adcIndex);
static void f_HvVoltsFilterCb(enAdcIndex adcIndex);
static void f_BoardTemperatureFilterCb(enAdcIndex adcIndex);
static void f_AuxPowerFilterCb(enAdcIndex adcIndex);


/********************************************************************************
* Global VARIABLES
********************************************************************************/
stAdcFilter_t stAdcFilter[ADC_END] =
{
//  Raw fastG fastPre fastCur zeroCnt zeroOffset SlowShift SlowCnt SlowSum SlowFilterCb

    {0, 2,    0,      0,      0,      0,         4,       0,      0,    &f_PackVoltsSlowFilterCb},  //PACK_VOLTS
    {0, 1,    0,      0,      0,      0,         4,       0,      0,    &f_LvVoltsFilterCb},        //LVOUT_VOLTS
    
    {0, 2,    0,      0,  CURRENT_ZERO_CNT,      0,         3,       0,      0,    &f_LvLCurrentSlowFilterCb},    //CURRENT1
    {0, 2,    0,      0,  CURRENT_ZERO_CNT,      0,         3,       0,      0,    &f_LvLCurrentSlowFilterCb},    //CURRENT2
    {0, 2,    0,      0,  CURRENT_ZERO_CNT,      0,         3,       0,      0,    &f_LvLCurrentSlowFilterCb},    //CURRENT3
    {0, 2,    0,      0,  CURRENT_ZERO_CNT,      0,         3,       0,      0,    &f_LvLCurrentSlowFilterCb},    //CURRENT4
    {0, 2,    0,      0,  CURRENT_ZERO_CNT,      0,         3,       0,      0,    &f_LvLCurrentSlowFilterCb},    //CURRENT5
    
    {0, 2,    0,      0,      0,      0,         4,       0,      0,    &f_HvVoltsFilterCb},        //HVOUT_VOLTS
    {0, 2,    0,      0,  CURRENT_ZERO_CNT,      0,         4,       0,      0,    &f_HvCurrrentFilterCb},     //HV_CURRENT

    {0, 3,    0,      0,      0,      0,         4,       0,      0,    &f_BoardTemperatureFilterCb},        //TEMP1
    {0, 3,    0,      0,      0,      0,         4,       0,      0,    &f_BoardTemperatureFilterCb},        //TEMP2
    {0, 3,    0,      0,      0,      0,         4,       0,      0,    &f_BoardTemperatureFilterCb},        //TEMP3
    {0, 3,    0,      0,      0,      0,         4,       0,      0,    &f_AuxPowerFilterCb},        //AUX_POWER
};


stADCMeter_t stAdcMeters = {0};


/********************************************************************************
* LOCAL VARIABLES
********************************************************************************/
static Uint16 ADC_EnableAutoZeroFlag = 0;

/****************************************************************************
*
*  Function: ADC_FilterIniital
*
*  Purpose :    Initial ADC filter structure
*
*  Parms Passed   :   Nothing
*
*  Returns        :   Nothing
*
*  Description    :   
*
****************************************************************************/
void ADC_FilterIniital(void)
{
    Uint16 i = 0;
    
    for(i = 0; i < ADC_END; i++)
    {
        if((stAdcFilter[i].SlowAvgShift == 0) || ((1 << stAdcFilter[i].SlowAvgShift) >= 1024))
        {
            stAdcFilter[i].SlowAvgShift = 5;
        }
        
        stAdcFilter[i].SlowAvgCnt = 0;
        stAdcFilter[i].SlowAvgSum = 0;
    }
}


/****************************************************************************
*
*  Function: f_AdcSampleSWICallback
*
*  Purpose :    ADC sequence sample finish ISR call back function
*
*  Parms Passed   :   Nothing
*
*  Returns        :   Nothing
*
*  Description    :   Note: this call back function should not take much time, don't add any
*                     operation containing OS delay API or hard code delay;
*
****************************************************************************/
void f_AdcSampleSWICallback(void)
{

}


static void f_LvLCurrentSlowFilterCb(enAdcIndex adcIndex)
{
    Uint32 u32tempVal = 0;

    int16 *ps16Current = &stAdcMeters.ADC_Current1Meter_100mA;
    int16 s16tempVal = 0;
    int16 s16tempMinVal = 0;
    int16 s16tempMaxVal = 0;

    Uint16 i = 0;


    if(stAdcFilter[adcIndex].SlowAvgCnt >=  (1 << stAdcFilter[adcIndex].SlowAvgShift))
    {
        u32tempVal = stAdcFilter[adcIndex].SlowAvgSum >> stAdcFilter[adcIndex].SlowAvgShift;

        /* AdcCounter / 2^12 * 3.3 * 3 / 2 = 2.5 - 0.02 * I_A
           ==> 500 * AdcCounter / 2^12 * 3.3 * 3 / 2 = 500 * (2.5 +- 0.02 * I_A)
           ==> AdcCounter / 2^13 * 4950 = 1250 +- I_100mA  ==> I_100mA = AdcCounter / 2^13 * 4950 +- 1250*/
        ps16Current[adcIndex - CURRENT1] = (int16)((u32tempVal * (Uint32)4950) >> 13) - (int16)1250;
    }

    /* last current channel, then calc overall meter */
    if(adcIndex == CURRENT5)
    {
        s16tempVal =  stAdcMeters.ADC_Current1Meter_100mA
                    + stAdcMeters.ADC_Current2Meter_100mA
                    + stAdcMeters.ADC_Current3Meter_100mA
                    + stAdcMeters.ADC_Current4Meter_100mA
                    + stAdcMeters.ADC_Current5Meter_100mA;

        /* for display, use abs value */
        if(s16tempVal < 0)
        {
            stAdcMeters.ADC_absTotalCurrMeter_100mA = (-s16tempVal);
            stAdcMeters.ADC_BattDischgPower = (Uint16)(Uint32)stAdcMeters.ADC_absTotalCurrMeter_100mA * (Uint32)stAdcMeters.ADC_VPackMeter_mV / (Uint32)10000;
            stAdcMeters.ADC_BattChgPower = 0u;
        }
        else
        {
            stAdcMeters.ADC_absTotalCurrMeter_100mA = s16tempVal;
            stAdcMeters.ADC_BattChgPower = (Uint16)(Uint32)stAdcMeters.ADC_absTotalCurrMeter_100mA * (Uint32)stAdcMeters.ADC_VPackMeter_mV / (Uint32)10000;
            stAdcMeters.ADC_BattDischgPower = 0u;
        }
        stAdcMeters.ADC_TotalCurrMeter_100mA = s16tempVal;

        stAdcMeters.ADC_LegAvgCurrMeter_100mA = stAdcMeters.ADC_TotalCurrMeter_100mA / (int16)LEG_NUM;

        s16tempMinVal = stAdcMeters.ADC_Current1Meter_100mA;
        s16tempMaxVal = stAdcMeters.ADC_Current1Meter_100mA;

        /* find the max and min leg current */
        for(i = 1; i < LEG_NUM; i++)
        {
            if(s16tempMinVal > ps16Current[i])
            {
                s16tempMinVal = ps16Current[i];
            }

            if(s16tempMaxVal < ps16Current[i])
            {
                s16tempMaxVal = ps16Current[i];
            }
        }

        stAdcMeters.ADC_LegMaxCurrMeter_100mA = s16tempMaxVal;
        stAdcMeters.ADC_LegMinCurrMeter_100mA = s16tempMinVal;
    }
}


static void f_PackVoltsSlowFilterCb(enAdcIndex adcIndex)
{
    Uint32 tempVal = stAdcFilter[adcIndex].SlowAvgSum >> stAdcFilter[adcIndex].SlowAvgShift;

    stAdcMeters.ADC_VPackMeter_mV = (Uint16)((tempVal * (Uint32)ADC_VBATT_MAX_MV) >> 12);
}


static void f_LvVoltsFilterCb(enAdcIndex adcIndex)
{
    Uint32 tempVal = stAdcFilter[adcIndex].SlowAvgSum >> stAdcFilter[adcIndex].SlowAvgShift;

    stAdcMeters.ADC_VOutLvMeter_10mV = (Uint16)((tempVal * (Uint32)ADC_LVOUT_MAX_10MV) >> 12);
}

static void f_HvCurrrentFilterCb(enAdcIndex adcIndex)
{
    Uint32 tempVal = stAdcFilter[adcIndex].SlowAvgSum >> stAdcFilter[adcIndex].SlowAvgShift;

    /* AdcCounter / 2^12 * 3.3 = 2.5 - 0.04 * I_A
       ==> 250 * AdcCounter / 2^12 * 3.3 / 0.667 = 250 * (2.5 +- 0.02 * I_A)
       ==> AdcCounter / 2^13 * 2475 = 625 +- I_100mA  ==> I_100mA = AdcCounter / 2^13 * 4950 +- 1250 */
    stAdcMeters.ADC_IOutHvMeter_100mA = (int16)((tempVal * (Uint32)2475) >> 13) - (int16)625;
}


static void f_HvVoltsFilterCb(enAdcIndex adcIndex)
{
    Uint32 tempVal = stAdcFilter[adcIndex].SlowAvgSum >> stAdcFilter[adcIndex].SlowAvgShift;

    stAdcMeters.ADC_VOutHvMeter_100mV = (Uint16)((tempVal * (Uint32)ADC_HVOUT_MAX_100MV) >> 12);
}


static void f_BoardTemperatureFilterCb(enAdcIndex adcIndex)
{
    Uint32 tempVal = stAdcFilter[adcIndex].SlowAvgSum >> stAdcFilter[adcIndex].SlowAvgShift;
    int16  *pTemperature = &stAdcMeters.ADC_CtrlTempMeter_100mC;

    pTemperature[adcIndex - TEMP1] = (int16)((tempVal * (Uint32)3300) >> 12) - (int16)ADC_TEMP_OFFSET_FACTOR;
}


static void f_AuxPowerFilterCb(enAdcIndex adcIndex)
{
    Uint32 tempVal = stAdcFilter[adcIndex].SlowAvgSum >> stAdcFilter[adcIndex].SlowAvgShift;

    stAdcMeters.ADC_UAuxPowerSupply_10mV = (Uint16)(tempVal * (Uint32)10 / (Uint32)113);
}

/****************************************************************************
*
*  Function: f_AdcSampleSlowTask
*
*  Purpose :    Run ADC slow task, mainly for slow ADC channel, such as temperature
*
*  Parms Passed   :   Nothing
*
*  Returns        :   Nothing
*
*  Description    :   Suggest to run every <= 100ms
*
****************************************************************************/
void f_AdcSampleSlowTask(void)
{
    f_AdcFastFilter(AdcResult.ADCRESULT7, TEMP1, 0);
    f_AdcFastFilter(AdcResult.ADCRESULT8, TEMP2, 0);
    f_AdcFastFilter(AdcResult.ADCRESULT9, TEMP3, 0);
    f_AdcFastFilter(AdcResult.ADCRESULT12, AUX_POWER, 0);

#if 0
    Uint32 tempVal = 0;

    int32 tempCurrentVal = 0;

    Uint16 i = 0u;

    int16 s16tempVal = 0;
    int16 s16tempMaxVal = 0;
    int16 s16tempMinVal = 0;
    int16 *ps16Current = NULL;

    f_AdcFastFilter(AdcResult.ADCRESULT7, TEMP1, 0);
    f_AdcFastFilter(AdcResult.ADCRESULT8, TEMP2, 0);
    f_AdcFastFilter(AdcResult.ADCRESULT9, TEMP3, 0);

    /********************** convert ADC counter into meters ********************/
    /* Battery voltage display meters */
    tempVal = ((Uint32)stAdcFilter[PACK_VOLTS].fastCurrVal <<  stAdcFilter[PACK_VOLTS].slowGain)
            + (Uint32)stAdcFilter[PACK_VOLTS].slowPreVal * ((1u << (Uint32)stAdcFilter[PACK_VOLTS].slowGain) - 1);
    stAdcFilter[PACK_VOLTS].slowCurrVal =  (Uint16)(tempVal >> stAdcFilter[PACK_VOLTS].slowGain);
    stAdcFilter[PACK_VOLTS].slowPreVal = stAdcFilter[PACK_VOLTS].slowCurrVal;

    stAdcMeters.ADC_VPackMeter_mV = ((Uint32)stAdcFilter[PACK_VOLTS].slowCurrVal * ADC_VBATT_MAX_MV) >> 12;

    /* Vout voltage display meters */
    tempVal = ((Uint32)stAdcFilter[LVOUT_VOLTS].fastCurrVal << stAdcFilter[LVOUT_VOLTS].slowGain)
            + (Uint32)stAdcFilter[LVOUT_VOLTS].slowPreVal * ((1u << (Uint32)stAdcFilter[LVOUT_VOLTS].slowGain) - 1);
    stAdcFilter[LVOUT_VOLTS].slowCurrVal =  (Uint16)(tempVal >> stAdcFilter[LVOUT_VOLTS].slowGain);
    stAdcFilter[LVOUT_VOLTS].slowPreVal = stAdcFilter[LVOUT_VOLTS].slowCurrVal;

    stAdcMeters.ADC_VOutLvMeter_10mV = ((Uint32)stAdcFilter[LVOUT_VOLTS].slowCurrVal * ADC_LVOUT_MAX_10MV) >> 12;

    /* 500mV+Temp*10mV/¡æ */
    stAdcMeters.ADC_CtrlTempMeter_100mC = (int16)(((Uint32)stAdcFilter[TEMP1].fastCurrVal * (Uint32)3300) >> 12) - (int16)ADC_TEMP_OFFSET_FACTOR;
    stAdcMeters.ADC_LvTempMeter_100mC = (int16)(((Uint32)stAdcFilter[TEMP2].fastCurrVal * (Uint32)3300) >> 12) - (int16)ADC_TEMP_OFFSET_FACTOR;

    /* current1 */
    tempVal = ((Uint32)stAdcFilter[CURRENT1].fastCurrVal << stAdcFilter[CURRENT1].slowGain)
            + (Uint32)stAdcFilter[CURRENT1].slowPreVal * ((1u << (Uint32)stAdcFilter[CURRENT1].slowGain) - 1);
    stAdcFilter[CURRENT1].slowCurrVal =  (Uint16)(tempVal >> stAdcFilter[CURRENT1].slowGain);
    stAdcFilter[CURRENT1].slowPreVal = stAdcFilter[LVOUT_VOLTS].slowCurrVal;

    stAdcMeters.ADC_Current1Meter_100mA = (int16)(((Uint32)stAdcFilter[CURRENT1].slowCurrVal * (Uint32)9900) >> 13) - (int16)2500;

    /* current2 */
    tempVal = ((Uint32)stAdcFilter[CURRENT2].fastCurrVal << stAdcFilter[CURRENT2].slowGain)
            + (Uint32)stAdcFilter[CURRENT2].slowPreVal * ((1u << (Uint32)stAdcFilter[CURRENT2].slowGain) - 1);
    stAdcFilter[CURRENT2].slowCurrVal =  (Uint16)(tempVal >> stAdcFilter[CURRENT2].slowGain);
    stAdcFilter[CURRENT2].slowPreVal = stAdcFilter[CURRENT2].slowCurrVal;

    stAdcMeters.ADC_Current2Meter_100mA = (int16)(((Uint32)stAdcFilter[CURRENT2].slowCurrVal * (Uint32)9900) >> 13) - (int16)2500;

    /* current3 */
    tempVal = ((Uint32)stAdcFilter[CURRENT3].fastCurrVal << stAdcFilter[CURRENT3].slowGain)
            + (Uint32)stAdcFilter[CURRENT3].slowPreVal * ((1u << (Uint32)stAdcFilter[CURRENT3].slowGain) - 1);
    stAdcFilter[CURRENT3].slowCurrVal =  (Uint16)(tempVal >> stAdcFilter[CURRENT3].slowGain);
    stAdcFilter[CURRENT3].slowPreVal = stAdcFilter[CURRENT3].slowCurrVal;
    stAdcMeters.ADC_Current3Meter_100mA = (int16)(((Uint32)stAdcFilter[CURRENT3].slowCurrVal * (Uint32)9900) >> 13) - (int16)2500;

    /* current4 */
    tempVal = ((Uint32)stAdcFilter[CURRENT4].fastCurrVal << stAdcFilter[CURRENT4].slowGain)
            + (Uint32)stAdcFilter[CURRENT4].slowPreVal * ((1u << (Uint32)stAdcFilter[CURRENT4].slowGain) - 1);
    stAdcFilter[CURRENT4].slowCurrVal =  (Uint16)(tempVal >> stAdcFilter[CURRENT4].slowGain);
    stAdcFilter[CURRENT4].slowPreVal = stAdcFilter[CURRENT4].slowCurrVal;

    stAdcMeters.ADC_Current4Meter_100mA = (int16)(((Uint32)stAdcFilter[CURRENT4].slowCurrVal * (Uint32)9900) >> 13) - (int16)2500;

    /* current5 */
    tempVal = ((Uint32)stAdcFilter[CURRENT5].fastCurrVal << stAdcFilter[CURRENT5].slowGain)
            + (Uint32)stAdcFilter[CURRENT5].slowPreVal * ((1u << (Uint32)stAdcFilter[CURRENT5].slowGain) - 1);
    stAdcFilter[CURRENT5].slowCurrVal =  (Uint16)(tempVal >> stAdcFilter[CURRENT5].slowGain);
    stAdcFilter[CURRENT5].slowPreVal = stAdcFilter[CURRENT4].slowCurrVal;

    stAdcMeters.ADC_Current5Meter_100mA = (int16)(((Uint32)stAdcFilter[CURRENT5].slowCurrVal * (Uint32)9900) >> 13) - (int16)2500;
    s16tempVal =  stAdcMeters.ADC_Current1Meter_100mA
                + stAdcMeters.ADC_Current2Meter_100mA
                + stAdcMeters.ADC_Current3Meter_100mA
                + stAdcMeters.ADC_Current4Meter_100mA
                + stAdcMeters.ADC_Current5Meter_100mA;

    /* for display, use abs value */
    if(s16tempVal < 0)
    {
        stAdcMeters.ADC_absTotalCurrMeter_100mA = (-s16tempVal);
        stAdcMeters.ADC_BattDischgPower = (Uint16)(Uint32)stAdcMeters.ADC_absTotalCurrMeter_100mA * (Uint32)stAdcMeters.ADC_VPackMeter_mV / (Uint32)10000;
        stAdcMeters.ADC_BattChgPower = 0u;
    }
    else
    {
        stAdcMeters.ADC_absTotalCurrMeter_100mA = s16tempVal;
        stAdcMeters.ADC_BattChgPower = (Uint16)(Uint32)stAdcMeters.ADC_absTotalCurrMeter_100mA * (Uint32)stAdcMeters.ADC_VPackMeter_mV / (Uint32)10000;
        stAdcMeters.ADC_BattDischgPower = 0u;
    }
    stAdcMeters.ADC_TotalCurrMeter_100mA = s16tempVal;

    stAdcMeters.ADC_LegAvgCurrMeter_100mA = stAdcMeters.ADC_TotalCurrMeter_100mA / 5;

    ps16Current = &stAdcMeters.ADC_Current1Meter_100mA;
    s16tempMinVal = stAdcMeters.ADC_Current1Meter_100mA;
    s16tempMaxVal = stAdcMeters.ADC_Current1Meter_100mA;

    /* find the max and min leg current */
    for(i = 1; i < LEG_NUM; i++)
    {
        if(s16tempMinVal > ps16Current[i])
        {
            s16tempMinVal = ps16Current[i];
        }

        if(s16tempMaxVal < ps16Current[i])
        {
            s16tempMaxVal = ps16Current[i];
        }
        tempCurrentVal += (int32)ps16Current[i];
    }

    stAdcMeters.ADC_LegMaxCurrMeter_100mA = s16tempMaxVal;
    stAdcMeters.ADC_LegMinCurrMeter_100mA = s16tempMinVal;

    /* high resolution current calc for SOC calc */
    stAdcMeters.ADC_TotalCurrent_mA = (tempCurrentVal * (int32)121) - (int32)250000;
#endif
}


/****************************************************************************
*
*  Function: f_AdcFastFilter
*
*  Purpose :    ADC filter common API
*
*  Parms Passed   :   index: ADC channel
*
*  Returns        :   Nothing
*
*  Description    :   This filter is used as fast ADC FIR filter
*
****************************************************************************/
void f_AdcFastFilter(Uint16 adcCnt, enAdcIndex index, Uint16 autoZero)
{
    int32 tempVal = (int32)adcCnt; /* get current ADC counter */

    stAdcFilter_t *pAdcFilter = &stAdcFilter[index];

    if(autoZero)
    {
        pAdcFilter->zeroOffsetCali =  ((tempVal - pAdcFilter->zeroCntVal) >> 1)
                                + (pAdcFilter->zeroOffsetCali >> 1);

        if(pAdcFilter->zeroOffsetCali > 40)
        {
            pAdcFilter->zeroOffsetCali = 40;
        }
        else if(pAdcFilter->zeroOffsetCali < -40)
        {
            pAdcFilter->zeroOffsetCali = -40;
        }
    }

    tempVal -= pAdcFilter->zeroOffsetCali;

    if(tempVal < 0)
    {
        tempVal = 0;
    }

    /* Y(n) = (X(n) + (1<<fast - 1)Y(n-1)) >> fastGain */
    //tempVal = tempVal + (Uint32)pAdcFilter->fastPreVal * ((1u << (Uint32)pAdcFilter->fastGain) - 1);
    tempVal = tempVal + ((Uint32)pAdcFilter->fastPreVal << pAdcFilter->fastGain) - (Uint32)pAdcFilter->fastPreVal;
    pAdcFilter->fastCurrVal =  (Uint16)(tempVal >> pAdcFilter->fastGain);
    pAdcFilter->fastPreVal = pAdcFilter->fastCurrVal;

    /* uncomment this line to allow slow filter running if not called by ISR */
    /* call slow filter */
    f_AdcSlowFilter(index);
}


/****************************************************************************
*
*  Function: f_AdcSlowFilter
*
*  Purpose :    ADC slow filter
*
*  Parms Passed   :   index: ADC channel
*
*  Returns        :   Nothing
*
*  Description    :   The filter method is calculating average counter
*
****************************************************************************/
void f_AdcSlowFilter(enAdcIndex index)
{
    /* sum the ADC value */
    stAdcFilter[index].SlowAvgSum += stAdcFilter[index].fastCurrVal;
    stAdcFilter[index].SlowAvgCnt++;

    /* Average counter has arrived, then call cb function to handle them */
    if(stAdcFilter[index].SlowAvgCnt >=  (1 << stAdcFilter[index].SlowAvgShift))
    {
        if(NULL != stAdcFilter[index].SlowFilterCb)
        {
            stAdcFilter[index].SlowFilterCb(index);
        }

        /* Clear temp result */
        stAdcFilter[index].SlowAvgCnt = 0;
        stAdcFilter[index].SlowAvgSum = 0;
    }
}


/****************************************************************************
*
*  Function: f_GetAdcResult
*
*  Purpose :    Get ADC result which already pass through filter
*
*  Parms Passed   :   index: ADC channel
*
*  Returns        :   Nothing
*
*  Description    :
*
****************************************************************************/
Uint16 f_GetAdcResult(enAdcIndex index)
{
    return stAdcFilter[index].fastCurrVal;
}



/****************************************************************************
*
*  Function: ISampleAutoZero
*
*  Purpose :    To detect whether it is the time to zero offset calc for I sample
*
*  Parms Passed   :   None
*
*  Returns        :   1 means ok to zero calc, 0 means not
*
*  Description    :
*
****************************************************************************/
Uint16 ISampleAutoZero(void)
{
    return(LoopContrlInfo.PWMGating == 0u && (ADC_EnableAutoZeroFlag == 1));
}


void ADC_EnableAutoZero(Uint16 enableOrNot)
{
    ADC_EnableAutoZeroFlag = enableOrNot;
}


