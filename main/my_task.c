#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#include "components/DAP/config/DAP_config.h"

#include "driver/gpio.h"

void DAP_SPI_Deinit();
static portMUX_TYPE my_mutex;
static volatile int a = 0;
void my_task()
{
    vPortCPUInitializeMutex(&my_mutex);
    portENTER_CRITICAL(&my_mutex);
    // portDISABLE_INTERRUPTS();

    DAP_SPI_Deinit();
    for(;;){
        //a++;
        //if(a%20000 == 0)
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        {
            PIN_SWCLK_TCK_SET();
            PIN_SWCLK_TCK_CLR();
            PIN_SWCLK_TCK_SET();
            PIN_SWCLK_TCK_CLR();
            PIN_SWCLK_TCK_SET();
            PIN_SWCLK_TCK_CLR();
            PIN_SWCLK_TCK_SET();
            PIN_SWCLK_TCK_CLR();
            PIN_SWCLK_TCK_SET();
            PIN_SWCLK_TCK_CLR();
        }
    }
}