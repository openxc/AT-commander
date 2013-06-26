#include "LPC17xx.h"
#include "lpc17xx_uart.h"
#include "lpc_types.h"
#include "lpc17xx_timer.h"
#include "lpc17xx_pinsel.h"
#include "debug_frmwrk.h"
#include "emqueue.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "atcommander.h"

#define DESIRED_BAUDRATE 115200

#define DELAY_TIMER LPC_TIM0
#define UART1_DEVICE (LPC_UART_TypeDef*)LPC_UART1

#define UART1_FUNCNUM 1
#define UART1_PORTNUM 0
#define UART1_TX_PINNUM 15
#define UART1_RX_PINNUM 16
#define UART1_FLOW_PORTNUM 2
#define UART1_FLOW_FUNCNUM 2
#define UART1_RTS1_PINNUM 2
#define UART1_CTS1_PINNUM 7

extern const AtCommanderPlatform AT_PLATFORM_RN42;

QUEUE_DECLARE(uint8_t, 8);
QUEUE_DEFINE(uint8_t);
QUEUE_TYPE(uint8_t) receive_queue;

void debug(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char message[512];
    vsnprintf(message, 512, format, args);
    _printf(message);
    va_end(args);
}

void delayMs(long unsigned int delayInMs) {
    TIM_TIMERCFG_Type delayTimerConfig;
    TIM_ConfigStructInit(TIM_TIMER_MODE, &delayTimerConfig);
    TIM_Init(DELAY_TIMER, TIM_TIMER_MODE, &delayTimerConfig);

    DELAY_TIMER->PR  = 0x00;        /* set prescaler to zero */
    DELAY_TIMER->MR0 = (SystemCoreClock / 4) / (1000 / delayInMs);  //enter delay time
    DELAY_TIMER->IR  = 0xff;        /* reset all interrupts */
    DELAY_TIMER->MCR = 0x04;        /* stop timer on match */

    TIM_Cmd(DELAY_TIMER, ENABLE);

    /* wait until delay time has elapsed */
    while (DELAY_TIMER->TCR & 0x01);
}

void configurePins() {
    PINSEL_CFG_Type PinCfg;

    PinCfg.Funcnum = UART1_FUNCNUM;
    PinCfg.OpenDrain = 0;
    PinCfg.Pinmode = 0;
    PinCfg.Portnum = UART1_PORTNUM;
    PinCfg.Pinnum = UART1_TX_PINNUM;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Pinnum = UART1_RX_PINNUM;
    PINSEL_ConfigPin(&PinCfg);

    PinCfg.Portnum = UART1_FLOW_PORTNUM;
    PinCfg.Funcnum = UART1_FLOW_FUNCNUM;
    PinCfg.Pinnum = UART1_CTS1_PINNUM;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Pinnum = UART1_RTS1_PINNUM;
    PINSEL_ConfigPin(&PinCfg);
}

void configureUart(int baud) {
    UART_CFG_Type UARTConfigStruct;
    UART_ConfigStructInit(&UARTConfigStruct);
    UARTConfigStruct.Baud_rate = baud;
    UART_Init(UART1_DEVICE, &UARTConfigStruct);

    UART_FIFO_CFG_Type fifoConfig;
    UART_FIFOConfigStructInit(&fifoConfig);
    UART_FIFOConfig(UART1_DEVICE, &fifoConfig);


    UART_IntConfig(UART1_DEVICE, UART_INTCFG_RBR, ENABLE);
    /* UART_IntConfig(UART1_DEVICE, UART_INTCFG_RLS, ENABLE); */
    /* preemption = 1, sub-priority = 1 */
    NVIC_SetPriority(UART1_IRQn, ((0x01<<3)|0x01));
    NVIC_EnableIRQ(UART1_IRQn);

    UART_FullModemConfigMode(LPC_UART1, UART1_MODEM_MODE_AUTO_RTS, ENABLE);
    UART_FullModemConfigMode(LPC_UART1, UART1_MODEM_MODE_AUTO_CTS, ENABLE);

    UART_TxCmd(UART1_DEVICE, ENABLE);
}

void writeByte(uint8_t byte) {
    /* debug("Sending %d\r\n", byte); */
    UART_SendByte(UART1_DEVICE, byte);
}

void handleReceiveInterrupt() {
    while(!QUEUE_FULL(uint8_t, &receive_queue)) {
        uint8_t byte;
        uint32_t received = UART_Receive(UART1_DEVICE, &byte, 1, NONE_BLOCKING);
        if(received > 0) {
            /* debug("Received %c\r\n", byte); */
            QUEUE_PUSH(uint8_t, &receive_queue, byte);
        } else {
            break;
        }
    }
}

void UART1_IRQHandler() {
    uint32_t interruptSource = UART_GetIntId(UART1_DEVICE)
        & UART_IIR_INTID_MASK;
    switch(interruptSource) {
        case UART_IIR_INTID_RDA:
        case UART_IIR_INTID_CTI:
            handleReceiveInterrupt();
            break;
    }
}

int readByte() {
    if(!QUEUE_EMPTY(uint8_t, &receive_queue)) {
        return QUEUE_POP(uint8_t, &receive_queue);
    }
    return -1;
}

int main (void) {
    debug_frmwrk_init();
    _printf("About to change baud rate of RN-42 to %d\r\n", DESIRED_BAUDRATE);

    QUEUE_INIT(uint8_t, &receive_queue);

    bool configured = false;
    AtCommanderConfig config = {AT_PLATFORM_RN42};

    config.baud_rate_initializer = configureUart;
    config.write_function = writeByte;
    config.read_function = readByte;
    config.delay_function = delayMs;
    config.log_function = debug;

    configurePins();

    delayMs(1000);
    while(true) {
        if(!configured) {
            if(at_commander_set_baud(&config, DESIRED_BAUDRATE)) {
                configured = true;
                at_commander_reboot(&config);
            } else {
                delayMs(1000);
            }
        } else {
            char* message = "Sending data over the RN-42";
            UART_Send(UART1_DEVICE, (uint8_t*)message, strlen(message), BLOCKING);
        }
    }

    return  0;
}
