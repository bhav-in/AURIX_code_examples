/**********************************************************************************************************************
 * \file Cpu0_Main.c
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
 /*\title Shell via DAS interface using OneEye
 * \abstract A Shell is used to parse a command line and call the corresponding command execution. A OneEye pipe is used to interface with the Shell through the DAS interface.
 * \description After configuring the OneEye DAS interface, the Shell from iLLDs is used to interpret and manage commands like "info" or "help".
 *              The example creates a OneEye pipe and the corresponding DPipe that is used to interface the shell.
 *
 * \name OneEye_DAS_Shell_1_KIT_TC334_LK
 * \version V1.0.0
 * \board AURIX TC334 lite Kit, KIT_A2G_TC334_LITE, TC33xLP_A-Step
 * \keywords OneEye, DAS, Shell, Pipe, AURIX
 * \documents https://www.infineon.com/aurix-expert-training/Infineon-AURIX_OneEye_DAS_Shell_1_KIT_TC334_LK-TR-v01_00_00-EN.pdf
 * \documents https://www.infineon.com/aurix-expert-training/TC33A_iLLD_UM_1_0_1_12_1.chm
 * \lastUpdated 2022-03-28
 *********************************************************************************************************************/
#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "Ifx_OneEyeDasPipe.h"
#include "Ifx_Shell.h"

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
boolean printShellInfo(pchar args, void *data, IfxStdIf_DPipe *io);

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/

/* OneEye pipe object
 * The special Ifx_OneEyeDasPipe type is recognised by OneEye as a pipe object and enables data streaming between
 * OneEye and the ECU.
 */
Ifx_OneEyeDasPipe g_oneEyeDasPipe;

/* The IfxStdIf_DPipe object enables interaction with the pipe, as for example read / write.
 * It can also be used to connect to other objects that support the interface as, for example, a shell.
 */
IfxStdIf_DPipe  g_dPipe;

/* Shell object */
Ifx_Shell       g_shell;

/* Array that stores the supported Shell commands */
const Ifx_Shell_Command g_shellCommands[] = {
    {"help",   SHELL_HELP_DESCRIPTION_TEXT,    &g_shell, &Ifx_Shell_showHelp },
    {"info",   "     : Dummy info command",    &g_shell, &printShellInfo     },
    IFX_SHELL_COMMAND_LIST_END
};

IFX_ALIGN(4) IfxCpu_syncEvent g_cpuSyncEvent = 0;

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

/* Function called when the shell command "info" is executed */
boolean printShellInfo(pchar args, void *data, IfxStdIf_DPipe *io)
{
    IfxStdIf_DPipe_print(io, "The shell command was called !" ENDL);
    return TRUE;
}

void core0_main(void)
{
    IfxCpu_enableInterrupts();
    
    /* !!WATCHDOG0 AND SAFETY WATCHDOG ARE DISABLED HERE!!
     * Enable the watchdogs and service them periodically if it is required
     */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());
    
    /* Wait for CPU sync event */
    IfxCpu_emitEvent(&g_cpuSyncEvent);
    IfxCpu_waitEvent(&g_cpuSyncEvent, 1);
    
    /* Initialize Ifx_OneEyeDasPipe to enable data streaming between the controller and OneEye */
    Ifx_OneEyeDasPipe_Config pipeCfg;

    Ifx_OneEyeDasPipe_initConfig(&pipeCfg);
    pipeCfg.size = 512; /* Ifx_OneEyeDasPipe requires twice the size in the heap (rx and tx). Maximum size is 1024 i.e. 2kB heap. */

    Ifx_OneEyeDasPipe_init(&g_oneEyeDasPipe, &pipeCfg);

    /* Initialize IfxStdIf_DPipe with the Ifx_OneEyeDasPipe */
    Ifx_OneEyeDasPipe_stdIfDPipeInit(&g_dPipe, &g_oneEyeDasPipe);

    /* Print something directly into the pipe */
    IfxStdIf_DPipe_print(&g_dPipe, "Hello World !" ENDL);
    IfxStdIf_DPipe_print(&g_dPipe, "Enter 'help' to see the command list !" ENDL);

    /* Initialize the shell */
    Ifx_Shell_Config shellConf;
    Ifx_Shell_initConfig(&shellConf);                       /* Initialize the structure with default values         */

    shellConf.standardIo = &g_dPipe;                        /* Set a pointer to the standard interface              */
    shellConf.commandList[0] = &g_shellCommands[0];         /* Set the supported command list                       */

    /* Apply the configuration to the shell */
    Ifx_Shell_init(&g_shell, &shellConf);                   /* Initialize the Shell with the given configuration    */

    while(1)
    {
        /* Process the shell commands */
        Ifx_Shell_process(&g_shell);
    }
}
