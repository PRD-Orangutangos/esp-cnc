
// #include "SDstorage.h"





#include "server.h"
#include "wifi.h"




// void app_main(void)P
// {
    

    // if (!initStorage()) {
    //     ESP_LOGE(TAG, "Storage init failed");
    //     return;
    // }

    // init_motors();
    // move_to_base();
    // while(!x_limit_set || !y_limit_set){
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    //     ESP_LOGW("info:", "now is basing...");
    // }
    // begin_read_gcode();
    // Запускаем G-code асинхронно
    // // Основной цикл свободен
    // while(1) {
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    //     // get_position();
    //     // ESP_LOGW("pos: ", "%f, %f, %f", current_x_position, current_y_position, current_z_position);
    // }
    
// }


















void app_main(void)
{
   
    init_motors();

    wifi_init();
    server = start_webserver(); // важно: присваиваем глобальной переменной

    ESP_LOGI(TAG, "✅ Ready! Open http://<ESP_IP>/ to upload files to SD.");
}
