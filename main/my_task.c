#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#include "components/DAP/config/DAP_config.h"

#include "driver/gpio.h"

void DAP_SPI_Deinit();

void my_task()
{
    DAP_SPI_Deinit();
    for(;;){
        for(int i = 0; i < 100000 ; i++){
            PIN_SWCLK_TCK_SET();
            PIN_SWDIO_TMS_SET();
            PIN_SWCLK_TCK_CLR();
            PIN_SWDIO_TMS_CLR();
        }
        vTaskDelay(10);
    }
}