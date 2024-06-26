
/**********************************************************************************************************************
 * \file    GTM_ADC.c
 * \copyright Copyright (C) Infineon Technologies AG 2019
 *
 * Use of this file is subject to the terms of use agreed between (i) you or the company in which ordinary course of
 * business you are acting and (ii) Infineon Technologies AG or its licensees. If and as long as no such terms of use
 * are agreed, use of this file is subject to following:
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization obtaining a copy of the software and
 * accompanying documentation covered by this license (the "Software") to use, reproduce, display, distribute, execute,
 * and transmit the Software, and to prepare derivative works of the Software, and to permit third-parties to whom the
 * Software is furnished to do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including the above license grant, this restriction
 * and the following disclaimer, must be included in all copies of the Software, in whole or in part, and all
 * derivative works of the Software, unless such copies or derivative works are solely in the form of
 * machine-executable object code generated by a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *********************************************************************************************************************/

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "Ifx_Types.h"
#include "IfxVadc_Adc.h"
#include "IfxGtm_Tom_Pwm.h"
#include "IfxGtm_Trig.h"
#include "IfxScuWdt.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define ISR_PRIORITY_ADC        4                               /* ADC result interrupt's priority                  */
#define PWM_PIN                 IfxGtm_TOM0_13_TOUT15_P00_6_OUT /* Pin driven by the PWM, P00.6                     */
#define PWM_PERIOD              10000                           /* PWM period for the TOM, 100MHz / 10kHz = 10000   */
#define VOLTAGE_SENSE_GAIN      (100.0f / 600.0f * 3.75f)   /* Define the DC-link voltage sensing scaled gain value */
#define VOLTAGE_SENSE_OFFSET    (0)
#define DRCTR_ACCUMULATE_4      3                           /* Value to set the Data Reduction Control Bitfield     */
#define DMM_STD                 0                           /* Value to set the Data Modification Mode Bitfield     */
#define VADC_MAX                4095.0f /* Maximum ADC value (conversion results are expressed in 12-bit format)    */

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/
/* VADC handle */
IfxVadc_Adc g_vadc;
IfxVadc_Adc_Group g_adcGroup;
IfxVadc_Adc_Channel g_adcChannel;

/* Timer handle */
IfxGtm_Tom_Pwm_Config g_tomConfig;                                  /* Timer configuration structure                */
IfxGtm_Tom_Pwm_Driver g_tomDriver;                                  /* Timer Driver structure                       */

/* Support variables */
uint32 g_conversionResult;
uint32 g_cnt = 0;

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
void setDutyCycle(uint32 dutyCycle);                                /* Function to set the duty cycle of the PWM    */

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

/* This function initializes the TOM */
void initGtmTomPwm(void)
{
    IfxGtm_enable(&MODULE_GTM);                                     /* Enable GTM                                   */

    IfxGtm_Cmu_enableClocks(&MODULE_GTM, IFXGTM_CMU_CLKEN_FXCLK);   /* Enable the FXU clock                         */

    /* Initialize the configuration structure with default parameters */
    IfxGtm_Tom_Pwm_initConfig(&g_tomConfig, &MODULE_GTM);

    g_tomConfig.tom = PWM_PIN.tom;                                  /* Select the TOM (depending on the LED)        */
    g_tomConfig.tomChannel = PWM_PIN.channel;                       /* Select the channel (depending on the LED)    */
    g_tomConfig.period = PWM_PERIOD;                                /* Set the timer period                         */
    g_tomConfig.pin.outputPin = &PWM_PIN;                           /* Set the LED port pin as output               */
    g_tomConfig.synchronousUpdateEnabled = TRUE;                    /* Enable synchronous update                    */

    IfxGtm_Tom_Pwm_init(&g_tomDriver, &g_tomConfig);                /* Initialize the GTM TOM                       */
    IfxGtm_Tom_Pwm_start(&g_tomDriver, TRUE);                       /* Start the PWM                                */

    /* Configure the GTM to trigger the VADC, the User Manual of the device lists all the possible interconnections */
    Ifx_GTM* gtm = &MODULE_GTM;
    IfxGtm_Trig_toVadc(gtm, IfxGtm_Trig_AdcGroup_0, IfxGtm_Trig_AdcTrig_0, IfxGtm_Trig_AdcTrigSource_tom0, IfxGtm_Trig_AdcTrigChannel_13);
}

/* Configuration and initialization of the VADC module and its group */
void initVadc(void)
{
    /* Create configuration */
    IfxVadc_Adc_Config adcConfig;
    IfxVadc_Adc_initModuleConfig(&adcConfig, &MODULE_VADC);         /* Assign default values to the configuration   */

    /* Initialize module */
    IfxVadc_Adc_initModule(&g_vadc, &adcConfig);

    /* Create and initialize the group configuration with default values */
    IfxVadc_Adc_GroupConfig adcGroupConfig;
    IfxVadc_Adc_initGroupConfig(&adcGroupConfig, &g_vadc);

    /* Setting the user configuration using group 0 */
    adcGroupConfig.groupId = IfxVadc_GroupId_0;                     /* Select the VADC group                        */
    adcGroupConfig.master = IfxVadc_GroupId_0;                      /* Select the master group                      */
    adcGroupConfig.arbiter.requestSlotScanEnabled = TRUE;           /* Enable scan source                           */
    adcGroupConfig.scanRequest.autoscanEnabled = FALSE;             /* Disable auto scan                            */

    /* Queue is enabled, enable the request slot for the arbiter */
    adcGroupConfig.arbiter.requestSlotQueueEnabled = TRUE;

    /* Configure the queue */
    adcGroupConfig.queueRequest.requestSlotPrio = IfxVadc_RequestSlotPriority_highest;
    adcGroupConfig.queueRequest.requestSlotStartMode = IfxVadc_RequestSlotStartMode_cancelInjectRepeat;

    adcGroupConfig.queueRequest.triggerConfig.gatingMode = IfxVadc_GatingMode_always;   /* No gate signal is required               */
    adcGroupConfig.queueRequest.triggerConfig.gatingSource = IfxVadc_GatingSource_0;    /* Use GTM Trigger 0                        */

    adcGroupConfig.queueRequest.triggerConfig.triggerMode = IfxVadc_TriggerMode_uponRisingEdge;
    adcGroupConfig.queueRequest.triggerConfig.triggerSource = IfxVadc_TriggerSource_15; /* Trigger source taken from Gating Input   */

    /* Initialize the VADC group */
    IfxVadc_Adc_initGroup(&g_adcGroup, &adcGroupConfig);

    /* Initialize the channel AN00 (G0.CH0) for Voltage Sense */

    /* Create channel configuration */
    IfxVadc_Adc_ChannelConfig adcChannelConfig;
    IfxVadc_Adc_initChannelConfig(&adcChannelConfig, &g_adcGroup);

    adcChannelConfig.channelId = IfxVadc_ChannelId_0;               /* Select the VADC channel                      */
    adcChannelConfig.resultRegister = IfxVadc_ChannelResult_0;      /* Use dedicated result register                */

    /* Interrupt configuration */
    adcChannelConfig.resultPriority = ISR_PRIORITY_ADC;             /* Set the VADC interrupt priority              */
    adcChannelConfig.resultServProvider = IfxSrc_Tos_cpu0;          /* Set the VADC interrupt service provider      */
    adcChannelConfig.backgroundChannel = TRUE;

    /* Initialize the channel */
    IfxVadc_Adc_initChannel(&g_adcChannel, &adcChannelConfig);

    /* Add Over-sampling for the current measurement */
    IfxVadc_Adc_addToQueue(&g_adcChannel, IFXVADC_QUEUE_REFILL);    /* Trigger just refills the queue               */

    /* Enable data reduction */
    {
        IfxVadc_GroupId groupId = g_adcChannel.group -> groupId;
        Ifx_VADC_G *adcGroupSFRs = &g_adcChannel.group -> module.vadc -> G[groupId];
        adcGroupSFRs->RCR[0].B.DRCTR = DRCTR_ACCUMULATE_4;          /* Accumulate 4 result                          */
        adcGroupSFRs->RCR[0].B.DMM = DMM_STD;                       /* Standard data reduction                      */
    }

    /* Add the selected channel to the scan sequence */
    uint32 channels = (1 << adcChannelConfig.channelId);    /* Set the bit corresponding to the input channel of the*/
    uint32 mask = channels;                                 /* respective group to take part in the scan sequence.  */
    IfxVadc_Adc_setScan(&g_adcGroup, channels, mask);
}

/* ADC Interrupt Service Routine */
IFX_INTERRUPT(ISRresultADC, 0, ISR_PRIORITY_ADC);

void ISRresultADC(void)
{
    /* Get the result from the VADC result register */
    g_cnt++;
    g_conversionResult = IfxVadc_Adc_getResult(&g_adcChannel).B.RESULT;

    uint32 duty = g_conversionResult / VADC_MAX * PWM_PERIOD + 1;

    if(duty >= PWM_PERIOD)
    {
        duty = PWM_PERIOD - 1;
    }
    setDutyCycle(duty);
}

/* This function sets the duty cycle of the PWM */
void setDutyCycle(uint32 dutyCycle)
{
    g_tomConfig.dutyCycle = dutyCycle;                      /* Change the value of the duty cycle                   */
    IfxGtm_Tom_Pwm_init(&g_tomDriver, &g_tomConfig);        /* Re-initialize the PWM                                */
}
