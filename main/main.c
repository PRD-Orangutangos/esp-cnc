
#include "SDstorage.h"
#include "server.h"
#include "wifi.h"

void app_main(void)
{
     if (!initStorage()) {
        ESP_LOGE(TAG, "Storage not available");
        return;
    }
   
    init_axis_system();
    // move_to_base();

    // while(!x_axis.limit_set || !y_axis.limit_set || !z_axis.limit_set){
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    //     ESP_LOGW("info:", "now is basing...");
    // }


    wifi_init();
    server = start_webserver(); 

    
}
