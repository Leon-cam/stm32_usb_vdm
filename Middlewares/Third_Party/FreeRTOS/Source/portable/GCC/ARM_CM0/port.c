/*
 * FreeRTOS Kernel V10.3.1
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 */

#include "FreeRTOS.h"
#include "task.h"

#ifndef configSYSTICK_CLOCK_HZ
    #define configSYSTICK_CLOCK_HZ configCPU_CLOCK_HZ
#endif

#define portNVIC_SYSTICK_CTRL_REG           ( *( ( volatile uint32_t * ) 0xe000e010 ) )
#define portNVIC_SYSTICK_LOAD_REG           ( *( ( volatile uint32_t * ) 0xe000e014 ) )
#define portNVIC_SYSTICK_CURRENT_VALUE_REG  ( *( ( volatile uint32_t * ) 0xe000e018 ) )
#define portNVIC_SHPR3_REG                  ( *( ( volatile uint32_t * ) 0xe000ed20 ) )
#define portNVIC_SYSTICK_CLK_BIT            ( 1UL << 2UL )
#define portNVIC_SYSTICK_INT_BIT            ( 1UL << 1UL )
#define portNVIC_SYSTICK_ENABLE_BIT         ( 1UL << 0UL )
#define portMIN_INTERRUPT_PRIORITY          ( 255UL )
#define portNVIC_PENDSV_PRI                 ( portMIN_INTERRUPT_PRIORITY << 16UL )
#define portNVIC_SYSTICK_PRI                ( portMIN_INTERRUPT_PRIORITY << 24UL )
#define portINITIAL_XPSR                    ( 0x01000000 )
#define portSTART_ADDRESS_MASK              ( ( StackType_t ) 0xfffffffeUL )

static UBaseType_t uxCriticalNesting = 0xaaaaaaaa;

extern void vPortStartFirstTask( void );

void vPortSetupTimerInterrupt( void )
{
    portNVIC_SYSTICK_CTRL_REG = 0UL;
    portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;
    portNVIC_SYSTICK_LOAD_REG = ( configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ ) - 1UL;
    portNVIC_SYSTICK_CTRL_REG = ( portNVIC_SYSTICK_CLK_BIT |
                                   portNVIC_SYSTICK_INT_BIT |
                                   portNVIC_SYSTICK_ENABLE_BIT );
}

StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters )
{
    pxTopOfStack--;
    *pxTopOfStack = portINITIAL_XPSR;
    pxTopOfStack--;
    *pxTopOfStack = ( ( StackType_t ) pxCode ) & portSTART_ADDRESS_MASK;
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) 0;
    pxTopOfStack -= 5;
    *pxTopOfStack = ( StackType_t ) pvParameters;
    pxTopOfStack -= 8;
    return pxTopOfStack;
}

void xPortPendSVHandler( void ) __attribute__(( naked ));
void xPortPendSVHandler( void )
{
    __asm volatile
    (
        ".syntax unified                        \n"
        "   mrs r0, psp                         \n"
        "   ldr r3, pxCurrentTCBConst2          \n"
        "   ldr r2, [r3]                        \n"
        "                                       \n"
        "   subs r0, #32                        \n"
        "   str r0, [r2]                        \n"
        "   stmia r0!, {r4-r7}                  \n"
        "   mov r4, r8                          \n"
        "   mov r5, r9                          \n"
        "   mov r6, r10                         \n"
        "   mov r7, r11                         \n"
        "   stmia r0!, {r4-r7}                  \n"
        "                                       \n"
        "   push {r3, r14}                      \n"
        "   cpsid i                             \n"
        "   bl vTaskSwitchContext                \n"
        "   cpsie i                             \n"
        "   pop {r2, r3}                        \n"
        "                                       \n"
        "   ldr r1, [r2]                        \n"
        "   ldr r0, [r1]                        \n"
        "   adds r0, #16                        \n"
        "   ldmia r0!, {r4-r7}                  \n"
        "   mov r8, r4                          \n"
        "   mov r9, r5                          \n"
        "   mov r10, r6                         \n"
        "   mov r11, r7                         \n"
        "                                       \n"
        "   msr psp, r0                         \n"
        "                                       \n"
        "   subs r0, #32                        \n"
        "   ldmia r0!, {r4-r7}                  \n"
        "                                       \n"
        "   bx r3                               \n"
        "                                       \n"
        "   .align 4                            \n"
        "pxCurrentTCBConst2: .word pxCurrentTCB \n"
    );
}

void xPortSysTickHandler( void )
{
    uint32_t ulPreviousMask;
    ulPreviousMask = portSET_INTERRUPT_MASK_FROM_ISR();
    {
        if( xTaskIncrementTick() != pdFALSE )
        {
            *( ( volatile uint32_t * ) 0xe000ed04 ) = ( 1UL << 28UL );
        }
    }
    portCLEAR_INTERRUPT_MASK_FROM_ISR( ulPreviousMask );
}

void vPortSVCHandler( void ) __attribute__(( naked ));
void vPortSVCHandler( void )
{
    __asm volatile
    (
        ".syntax unified                        \n"
        "   ldr r3, pxCurrentTCBConst           \n"
        "   ldr r1, [r3]                        \n"
        "   ldr r0, [r1]                        \n"
        "   adds r0, #16                        \n"
        "   ldmia r0!, {r4-r7}                  \n"
        "   mov r8, r4                          \n"
        "   mov r9, r5                          \n"
        "   mov r10, r6                         \n"
        "   mov r11, r7                         \n"
        "                                       \n"
        "   msr psp, r0                         \n"
        "                                       \n"
        "   subs r0, #32                        \n"
        "   ldmia r0!, {r4-r7}                  \n"
        "   movs r1, #2                         \n"
        "   msr CONTROL, r1                     \n"
        "   isb                                 \n"
        "   mov r0, lr                          \n" /* r0 = EXC_RETURN set by hw = 0xFFFFFFF9 (thread/MSP) */
        "   movs r1, #4                         \n" /* bit[2] selects PSP on exception return  */
        "   orrs r0, r1                         \n" /* r0 = 0xFFFFFFFD (thread/PSP)            */
        "   bx r0                               \n" /* EXC_RETURN: CPU pops task frame from PSP */
        "                                       \n"
        "   .align 4                            \n"
        "pxCurrentTCBConst: .word pxCurrentTCB  \n"
    );
}

BaseType_t xPortStartScheduler( void )
{
    portNVIC_SHPR3_REG |= portNVIC_PENDSV_PRI;
    portNVIC_SHPR3_REG |= portNVIC_SYSTICK_PRI;
    vPortSetupTimerInterrupt();
    uxCriticalNesting = 0;
    vPortStartFirstTask();
    return 0;
}

void vPortEndScheduler( void )
{
}

void vPortYield( void )
{
    *( ( volatile uint32_t * ) 0xe000ed04 ) = ( 1UL << 28UL );
    __asm volatile ( "dsb" ::: "memory" );
    __asm volatile ( "isb" );
}

void vPortEnterCritical( void )
{
    portDISABLE_INTERRUPTS();
    uxCriticalNesting++;
    __asm volatile ( "dsb" ::: "memory" );
    __asm volatile ( "isb" );
}

void vPortExitCritical( void )
{
    configASSERT( uxCriticalNesting );
    uxCriticalNesting--;
    if( uxCriticalNesting == 0 )
    {
        portENABLE_INTERRUPTS();
    }
}

uint32_t ulPortSetInterruptMask( void )
{
    uint32_t ulReturn;
    __asm volatile ( " mrs %0, PRIMASK \n" " cpsid i " : "=r" ( ulReturn ) :: "memory" );
    return ulReturn;
}

void vPortClearInterruptMask( uint32_t ulNewMask )
{
    __asm volatile ( " msr PRIMASK, %0 " :: "r" ( ulNewMask ) : "memory" );
}

void vPortStartFirstTask( void ) __attribute__(( naked ));
void vPortStartFirstTask( void )
{
    __asm volatile
    (
        ".syntax unified                \n"
        " ldr r0, =0xE000ED08          \n"
        " ldr r0, [r0]                 \n"
        " ldr r0, [r0]                 \n"
        " msr msp, r0                  \n"
        " cpsie i                      \n"
        " svc 0                        \n"
        " nop                          \n"
    );
}
