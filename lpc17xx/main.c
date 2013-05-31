#include "LPC17xx.h"
#include "lpc17xx_uart.h"
#include "lpc_types.h"
#include "lpc17xx_timer.h"
#include "lpc17xx_pinsel.h"
#include "debug_frmwrk.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "atcommander.h"

#define DELAY_TIMER LPC_TIM0
#define UART1_DEVICE (LPC_UART_TypeDef*)LPC_UART1

#define UART1_FUNCNUM 1
#define UART1_PORTNUM 0
#define UART1_TX_PINNUM 15
#define UART1_RX_PINNUM 16

extern const AtCommanderPlatform AT_PLATFORM_RN42;

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
}

void configureFifo() {
    UART_FIFO_CFG_Type fifoConfig;
    UART_FIFOConfigStructInit(&fifoConfig);
    UART_FIFOConfig(UART1_DEVICE, &fifoConfig);
}

void configureUart(int baud) {
    UART_CFG_Type UARTConfigStruct;
    UART_ConfigStructInit(&UARTConfigStruct);
    UARTConfigStruct.Baud_rate = baud;
    UART_Init(UART1_DEVICE, &UARTConfigStruct);
}

void writeByte(uint8_t byte) {
    UART_SendByte(UART1_DEVICE, byte);
}

int readByte() {
    return UART_ReceiveByte(UART1_DEVICE);
}

void debug(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char message[512];
    vsnprintf(message, 512, format, args);
    _printf(message);
    va_end(args);
}

int main (void) {
    debug_frmwrk_init();
    _printf("About to change baud rate of RN-42 to 115200\r\n");


    bool configured = false;
    AtCommanderConfig config = {AT_PLATFORM_RN42};

    config.baud_rate_initializer = configureUart;
    config.write_function = writeByte;
    config.read_function = readByte;
    config.delay_function = delayMs;
    config.log_function = debug;

    configurePins();
    configureFifo();

    while(true) {
        if(!configured) {
            if(at_commander_set_baud(&config, 115200)) {
                configured = true;
                at_commander_reboot(&config);
            } else {
                delayMs(5000);
            }
        } else {
            char*  message = "Sending data over the RN-42 at 115200 baud!";
            UART_Send(UART1_DEVICE, (uint8_t*)message, strlen(message), BLOCKING);
        }
    }

    return  0;
}
