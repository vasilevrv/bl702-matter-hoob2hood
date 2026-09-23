#include "HoodUart.h"

#include <bl702_glb.h>
#include <bl702_hbn.h>
#include <bl702_uart.h>
#include <bl_timer.h>
#include <hosal_uart.h>

#define HOOD_UART_ID 1U
#define HOOD_UART_TX_PIN 23U
#define HOOD_UART_RX_PIN 25U
#define HOOD_UART_BAUD_RATE 1200U
#define HOOD_UART_IDLE_TIMEOUT_US 100000U

/*
 * UART0 and UART1 use one shared clock.  HOSAL selects the 144 MHz FCLK,
 * which is too fast for 1200 baud because BL702's baud divider is only
 * 16 bits wide.  Use 96 MHz / 8 = 12 MHz instead; this gives an exact
 * divider for 1200 baud and a sufficiently accurate divider for the
 * 115200-baud debug console.
 */
#define SHARED_UART_CLOCK_DIV 7U
#define SHARED_UART_CLOCK_HZ 12000000U

#ifndef CHIP_UART_BAUDRATE
#define CHIP_UART_BAUDRATE 115200U
#endif

static hosal_uart_dev_t sHoodUart = {
    .config =
        {
            .uart_id = HOOD_UART_ID,
            .tx_pin = HOOD_UART_TX_PIN,
            .rx_pin = HOOD_UART_RX_PIN,
            .cts_pin = 255,
            .rts_pin = 255,
            .baud_rate = HOOD_UART_BAUD_RATE,
            .data_width = HOSAL_DATA_WIDTH_8BIT,
            .parity = HOSAL_NO_PARITY,
            .stop_bits = HOSAL_STOP_BITS_1,
            .flow_control = HOSAL_FLOW_CONTROL_DISABLED,
            .mode = HOSAL_UART_MODE_POLL,
        },
};

static void configure_uart(UART_ID_Type uartId, uint32_t baudRate)
{
    UART_CFG_Type config = {
        SHARED_UART_CLOCK_HZ,
        baudRate,
        UART_DATABITS_8,
        UART_STOPBITS_1,
        UART_PARITY_NONE,
        DISABLE,
        DISABLE,
        DISABLE,
        DISABLE,
        DISABLE,
        DISABLE,
        0,
        UART_LSB_FIRST,
    };

    UART_Init(uartId, &config);
    UART_TxFreeRun(uartId, ENABLE);
}

int hood_uart_init(void)
{
    int result = hosal_uart_init(&sHoodUart);

    if (result != 0)
    {
        return result;
    }

    /* Finish any pending console character before changing the shared clock. */
    const uint64_t waitStartedAt = bl_timer_now_us64();
    while (UART_GetTxBusBusyStatus(UART0_ID) == SET)
    {
        if ((bl_timer_now_us64() - waitStartedAt) >= HOOD_UART_IDLE_TIMEOUT_US)
        {
            return -1;
        }
    }

    UART_Disable(UART0_ID, UART_TXRX);
    UART_Disable(UART1_ID, UART_TXRX);

    GLB_Set_UART_CLK(ENABLE, HBN_UART_CLK_96M, SHARED_UART_CLOCK_DIV);
    configure_uart(UART0_ID, CHIP_UART_BAUDRATE);
    configure_uart(UART1_ID, HOOD_UART_BAUD_RATE);

    UART_Enable(UART0_ID, UART_TXRX);
    UART_Enable(UART1_ID, UART_TXRX);

    return 0;
}

int hood_uart_write(uint8_t byte)
{
    /* Unlike the HOSAL polling path, UART_SendData has a finite FIFO timeout. */
    return UART_SendData(UART1_ID, &byte, 1U) == SUCCESS ? 0 : -1;
}
