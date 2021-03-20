/**
 * @file spi_switch.c
 * @author windowsair
 * @brief Switching between SPI mode and IO mode
 * @change: 2021-3-7 Support esp32 SPI
 *
 * @version 0.1
 * @date 2021-3-7
 *
 * @copyright Copyright (c) 2021
 *
 */
#include <stdbool.h>

#include "components/DAP/include/cmsis_compiler.h"
#include "components/DAP/include/spi_switch.h"

// soc register
#include "esp32/rom/gpio.h"
#include "esp32/include/soc/gpio_struct.h"
#include "hal/gpio_types.h"

#include "esp32/include/soc/dport_access.h"
#include "esp32/include/soc/dport_reg.h"
#include "esp32/include/soc/periph_defs.h"
#include "esp32/include/soc/spi_struct.h"
#include "esp32/include/soc/spi_reg.h"

//// FIXME: esp32
#define DAP_SPI SPI2

#define ENTER_CRITICAL() portENTER_CRITICAL()
#define EXIT_CRITICAL() portEXIT_CRITICAL()

// Note that the index starts at 0, so we are using function 2(SPI).
#define FUNC_SPI 1

#define SPI2_HOST 1

#define SPI_LL_RST_MASK (SPI_OUT_RST | SPI_IN_RST | SPI_AHBM_RST | SPI_AHBM_FIFO_RST)


typedef enum {
    SPI_40MHz_DIV = 2,
    // SPI_80MHz_DIV = 1, //// FIXME: high speed clock
} spi_clk_div_t;


/**
 * @brief Initialize on first use
 *
 */
void DAP_SPI_Init()
{
    // Enable spi module, We use SPI2(HSPI)
    DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_SPI2_CLK_EN);
    DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_SPI2_RST);


    // We will use IO_MUX to get the maximum speed.
    GPIO.func_in_sel_cfg[HSPID_IN_IDX].sig_in_sel = 0;   // IO_MUX direct connnect
    PIN_INPUT_ENABLE(IO_MUX_GPIO13_REG);                 // MOSI
    GPIO.func_out_sel_cfg[13].oen_sel = 0;               // use output enable signal from peripheral
    GPIO.func_out_sel_cfg[13].oen_inv_sel = 0;           // do not invert the output value
    PIN_FUNC_SELECT(IO_MUX_GPIO13_REG, FUNC_SPI);

    // GPIO.func_in_sel_cfg[HSPIQ_IN_IDX].sig_in_sel = 0;
    // PIN_INPUT_ENABLE(IO_MUX_GPIO12_REG);                 // MISO
    // GPIO.func_out_sel_cfg[12].oen_sel = 0;
    // GPIO.func_out_sel_cfg[12].oen_inv_sel = 0;
    // PIN_FUNC_SELECT(IO_MUX_GPIO12_REG, FUNC_SPI);

    GPIO.func_in_sel_cfg[HSPICLK_IN_IDX].sig_in_sel = 0;
    PIN_INPUT_ENABLE(IO_MUX_GPIO14_REG);                 // SCLK
    GPIO.func_out_sel_cfg[14].oen_sel = 0;
    GPIO.func_out_sel_cfg[14].oen_inv_sel = 0;
    PIN_FUNC_SELECT(IO_MUX_GPIO14_REG, FUNC_SPI);


    // Not using DMA
    // esp32 only
    DPORT_SET_PERI_REG_BITS(DPORT_SPI_DMA_CHAN_SEL_REG, 3, 0, (SPI2_HOST * 2));


    // Reset DMA
    DAP_SPI.dma_conf.val |= SPI_LL_RST_MASK;
    DAP_SPI.dma_out_link.start = 0;
    DAP_SPI.dma_in_link.start = 0;
    DAP_SPI.dma_conf.val &= ~SPI_LL_RST_MASK;
    // Disable DMA
    DAP_SPI.dma_conf.dma_continue = 0;


    // Set to Master mode
    DAP_SPI.slave.slave_mode = false;


    // use all 64 bytes of the buffer
    DAP_SPI.user.usr_mosi_highpart = false;
    DAP_SPI.user.usr_miso_highpart = false;


    // Disable cs pin
    DAP_SPI.user.cs_setup = false;
    DAP_SPI.user.cs_hold = false;

    // Disable CS signal
    DAP_SPI.pin.cs0_dis = 1;
    DAP_SPI.pin.cs1_dis = 1;
    DAP_SPI.pin.cs2_dis = 1;

    // Duplex transmit
    DAP_SPI.user.doutdin = false;  // half dulex


    // Set data bit order
    DAP_SPI.ctrl.wr_bit_order = 1;   // SWD -> LSB
    DAP_SPI.ctrl.rd_bit_order = 1;   // SWD -> LSB
    // Set data byte order
    DAP_SPI.user.wr_byte_order = 0;  // SWD -> litte_endian && Risc V -> litte_endian
    DAP_SPI.user.rd_byte_order = 0;  // SWD -> litte_endian && Risc V -> litte_endian

    // Set dummy
    DAP_SPI.user.usr_dummy = 0; // not use

    // Set spi clk: 40Mhz 50% duty
    // CLEAR_PERI_REG_MASK(PERIPHS_IO_MUX_CONF_U, SPI1_CLK_EQU_SYS_CLK);

    // See esp32 TRM `SPI_CLOCK_REG`
    DAP_SPI.clock.clk_equ_sysclk = false;
    DAP_SPI.clock.clkdiv_pre = 0;
    DAP_SPI.clock.clkcnt_n = SPI_40MHz_DIV - 1;
    DAP_SPI.clock.clkcnt_h = SPI_40MHz_DIV / 2 - 1;
    DAP_SPI.clock.clkcnt_l = SPI_40MHz_DIV - 1;
    // Dummy is not required, but it may still need to be delayed
    // by half a clock cycle (espressif)


    // MISO delay setting
    DAP_SPI.user.ck_i_edge = true; //// TODO: may be used in slave mode?
    DAP_SPI.ctrl2.miso_delay_mode = 0;
    DAP_SPI.ctrl2.miso_delay_num = 0;


    // Set the clock polarity and phase CPOL = CPHA = 1
    DAP_SPI.pin.ck_idle_edge = 1;  // HIGH while idle
    DAP_SPI.user.ck_out_edge = 0;

    // No command and addr for now
    DAP_SPI.user.usr_command = 0;
    DAP_SPI.user.usr_addr = 0;
}


/**
 * @brief Switch to GPIO
 * Note: You may be able to pull the pin high in SPI mode, though you cannot set it to LOW
 */
__FORCEINLINE void DAP_SPI_Deinit()
{
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[14], PIN_FUNC_GPIO);
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[13], PIN_FUNC_GPIO); // MOSI
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[12], PIN_FUNC_GPIO); // MISO

    // enable SWCLK output
    GPIO.enable_w1ts = (0x01 << 14);

    // disable MISO output connect
    GPIO.enable_w1tc = (0x1 << 12);

    // enable MOSI output & input
    GPIO.enable_w1ts |= (0x1 << 13);
    PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[13]);

    // enable MOSI OD output
    //GPIO.pin[13].pad_driver = 1;

    // disable MOSI pull up
    REG_CLR_BIT(GPIO_PIN_MUX_REG[13], FUN_PU);
}


/**
 * @brief Gain control of SPI
 *
 */
__FORCEINLINE void DAP_SPI_Acquire()
{
    PIN_FUNC_SELECT(IO_MUX_GPIO14_REG, FUNC_SPI);
}


/**
 * @brief Release control of SPI
 *
 */
__FORCEINLINE void DAP_SPI_Release()
{
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[14], PIN_FUNC_GPIO);
    GPIO.enable_w1ts = (0x01 << 14);
}


/**
 * @brief Use SPI acclerate
 *
 */
void DAP_SPI_Enable()
{
    // may be unuse
    //// FIXME: esp32 nop?
    //PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_HSPID_MOSI); // GPIO13 is SPI MOSI pin (Master Data Out)
}


/**
 * @brief Disable SPI
 * Drive capability not yet known
 */
__FORCEINLINE void DAP_SPI_Disable()
{
    ;
    //CLEAR_PERI_REG_MASK(PERIPHS_IO_MUX_MTCK_U, (PERIPHS_IO_MUX_FUNC << PERIPHS_IO_MUX_FUNC_S));
    // may be unuse
    // gpio_pin_reg_t pin_reg;
    // GPIO.enable_w1ts |= (0x1 << 13);
    // GPIO.pin[13].driver = 0; // OD Output
    // pin_reg.val = READ_PERI_REG(GPIO_PIN_REG(13));
    // pin_reg.pullup = 1;
    // WRITE_PERI_REG(GPIO_PIN_REG(13), pin_reg.val);
}

