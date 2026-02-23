#pragma once

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "drivers/axis.h"
#include "drivers/my_timer.h"
#define LIMIT_X GPIO_NUM_11
#define LIMIT_Y GPIO_NUM_10
#define LIMIT_Z GPIO_NUM_2

static bool isr_service_installed = false;

TaskHandle_t motion_task_handle = NULL;

void ensure_gpio_isr_service(void) {
    if (!isr_service_installed) {
        gpio_install_isr_service(0);
        isr_service_installed = true;
    }
}

typedef struct
{
    gpio_num_t switch_pin;
    TaskHandle_t switch_handle;
    void (*task_fn)(void *pvParameters);
} Super_switch;

int current_pin = 0;
void button_task(void *pvParameters)
{
    Super_switch *lim_switch = (Super_switch *)pvParameters;
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (gpio_get_level(lim_switch->switch_pin) == 0)
        {
           char tag[10];
            snprintf(tag, sizeof(tag), "SW%d", lim_switch->switch_pin);
            ESP_LOGI(tag, "Button pressed");

            gpio_intr_disable(lim_switch->switch_pin);

            // Отправляем номер пина в очередь аварийной остановки
            current_pin = lim_switch->switch_pin;

            xTaskNotifyGive(motion_task_handle);
            vTaskDelay(pdMS_TO_TICKS(1000));
        } 
    }
}

void button_isr_handler(void *arg)
{
    Super_switch *lim_switch = (Super_switch *)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(lim_switch->switch_handle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void gpio_switch_init(Super_switch *limit_switch)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE, // срабатывание на спад (при подтяжке вверх)
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << (limit_switch->switch_pin)),
        .pull_up_en = GPIO_PULLUP_ENABLE, // обязательно для кнопки без внешней подтяжки
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };

    gpio_config(&io_conf);

    // 2. Установка ISR-сервиса (приоритет по умолчанию — безопасен)
    ensure_gpio_isr_service();

    // 3. Добавление обработчика
    gpio_isr_handler_add((limit_switch->switch_pin), button_isr_handler, limit_switch);
}

void init_switch(Super_switch *limit_switch, gpio_num_t pin)
{
    limit_switch->switch_pin = pin;
    limit_switch->switch_handle = NULL;
    limit_switch->task_fn = button_task; 

    xTaskCreate(button_task, "btn_task", 2048, limit_switch, 5, &limit_switch->switch_handle);

    gpio_switch_init(limit_switch);
}

void check_limit_switches(){
    switch (current_pin)
    {
    case LIMIT_X:
        setup_axis(&x_axis);
        break;
    case LIMIT_Y:
        setup_axis(&y_axis);
        break;
    case LIMIT_Z:
        z_axis.max_steps = 0 * STEPS_PER_MM_Z;
        z_axis.min_steps = (-50) * STEPS_PER_MM_Z;
        z_axis.steps_position = 0;
        z_axis.current_position = 0;
        z_axis.limit_set = true;
        break;
    default:
        break;
    }
    
    mcpwm_timer_start_stop(motor_timer.timer, MCPWM_TIMER_STOP_FULL);
    current_pin = -1;
}

Super_switch limit_x_switch;
Super_switch limit_y_switch;
Super_switch limit_z_switch;