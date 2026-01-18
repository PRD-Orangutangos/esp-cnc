#pragma once


#include "driver/mcpwm_prelude.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include "esp_log.h"
#include "interrupt_switch.h"
#include "drivers/axis.h"


#define ACCEL_STEPS           300   

static TaskHandle_t gcode_task_handle = NULL;  // ← глобальный


typedef struct {
    float x_mm;
    float y_mm;
    float z_mm;
} motion_cmd_t;

QueueHandle_t motion_queue;


uint32_t DEFAULT_START_PERIOD = 600;
uint32_t MAX_SPEED_PERIOD = 400;

volatile int32_t total_steps = 0;
volatile int32_t accel_steps = 0;
volatile uint32_t current_period = 600;


void motion_task(void *arg);

void get_position(void) {
    x_axis.current_position = (float)x_axis.steps_position / STEPS_PER_MM_X;
    y_axis.current_position = (float)y_axis.steps_position / STEPS_PER_MM_Y;
    z_axis.current_position = (float)z_axis.steps_position / STEPS_PER_MM_Z;
}

void move_to_position(float x, float y, float z) {
    DEFAULT_START_PERIOD = 200;
    MAX_SPEED_PERIOD = 100;
    motion_cmd_t cmd = { .x_mm = x, .y_mm = y, .z_mm = z };
    xQueueSend(motion_queue, &cmd, portMAX_DELAY);
}

void move_to_base(){
    DEFAULT_START_PERIOD = 200;
    MAX_SPEED_PERIOD = 100;
    x_axis.limit_set = false;
    y_axis.limit_set = false;
    x_axis.min_steps = -150 * STEPS_PER_MM_X;
    y_axis.min_steps = -150 * STEPS_PER_MM_Y;
    // min_z_position = -150 * STEPS_PER_MM_Z;
    motion_cmd_t cmd1 = { .x_mm = 5, .y_mm = 5, .z_mm = 0 };
    xQueueSend(motion_queue, &cmd1, portMAX_DELAY);
    motion_cmd_t cmd2 = { .x_mm = -150, .y_mm = 0, .z_mm = 0 };
    xQueueSend(motion_queue, &cmd2, portMAX_DELAY);
    motion_cmd_t cmd3 = { .x_mm = 0, .y_mm = -150, .z_mm = 0 };
    xQueueSend(motion_queue, &cmd3, portMAX_DELAY);
    // motion_cmd_t cmd_z = { .x_mm = 0, .y_mm = 0, .z_mm = 100}; //invert
    // xQueueSend(motion_queue, &cmd_z, portMAX_DELAY);
}

bool IRAM_ATTR timer_callback(
    mcpwm_timer_handle_t timer,
    const mcpwm_timer_event_data_t *edata,
    void *user_ctx
) {

    // if (check_done(&x_axis) && check_done(&y_axis) && check_done(&z_axis)) {
    //     goto stop_motion;
    // }

    // --- DDA шаг ---
    check_and_step(&x_axis);
    check_and_step(&y_axis);
    check_and_step(&z_axis);

    if (check_done(&x_axis) && check_done(&y_axis) && check_done(&z_axis)) {
        goto stop_motion;
    }
    
    // --- Управление скоростью (трапеция) ---
    int32_t steps_done = (int32_t)MAX(x_axis.done_position, MAX(y_axis.done_position, z_axis.done_position));
    int32_t steps_to_go = total_steps - steps_done;

    uint32_t new_period = current_period;

    if (steps_done < accel_steps) {
        // Разгон
        uint32_t period_delta = (DEFAULT_START_PERIOD - MAX_SPEED_PERIOD) / (accel_steps ? accel_steps : 1);
        new_period = DEFAULT_START_PERIOD - period_delta * steps_done;
    }
    else if (steps_to_go <= accel_steps) {
        // Торможение
        uint32_t period_delta = (DEFAULT_START_PERIOD - MAX_SPEED_PERIOD) / (accel_steps ? accel_steps : 1);
        new_period = DEFAULT_START_PERIOD - period_delta * steps_to_go;
    }
    else {
        // Крейсерская скорость
        new_period = MAX_SPEED_PERIOD;
    }

    // Ограничения
    if (new_period < MAX_SPEED_PERIOD) new_period = MAX_SPEED_PERIOD;
    if (new_period > DEFAULT_START_PERIOD) new_period = DEFAULT_START_PERIOD;

    if (new_period != current_period) {
        current_period = new_period;
        mcpwm_timer_set_period(motor_timer.timer, new_period);

        set_speed(&motor_x, new_period);
        set_speed(&motor_y, new_period);
        set_speed(&motor_z, new_period);
    }

    // if (check_done(&x_axis) && check_done(&y_axis) && check_done(&z_axis)) {
    //     goto stop_motion;
    // }

    return true;

stop_motion:
    mcpwm_timer_start_stop(timer, MCPWM_TIMER_STOP_FULL);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(motion_task_handle, &xHigherPriorityTaskWoken);
    return xHigherPriorityTaskWoken == pdTRUE;
}



void init_axis_system(){

    motor_timer = create_timer(timer_callback);

    motor_x = new_motor(&motor_timer, DIR_PIN_X, STEP_PIN_X);
    motor_y = new_motor(&motor_timer, DIR_PIN_Y, STEP_PIN_Y);
    motor_z = new_motor(&motor_timer, DIR_PIN_Z, STEP_PIN_Z);

    axis_init(&x_axis, &motor_x, STEPS_PER_MM_X, MIN_X_STEPS, MAX_X_STEPS);
    axis_init(&y_axis, &motor_y, STEPS_PER_MM_Y, MIN_Y_STEPS, MAX_Y_STEPS);
    axis_init(&z_axis, &motor_z, STEPS_PER_MM_Z, MIN_Z_STEPS, MAX_Z_STEPS);

    init_switch(&limit_x_switch, LIMIT_X);
    init_switch(&limit_y_switch, LIMIT_Y);

    motion_queue = xQueueCreate(20, sizeof(motion_cmd_t));
    assert(motion_queue);

    xTaskCreatePinnedToCore(
        motion_task,
        "motion_task",
        4096,
        NULL,
        10,
        &motion_task_handle,
        0
    );
}

void basing_axis(){
    DEFAULT_START_PERIOD = 200;
    MAX_SPEED_PERIOD = 100;
    x_axis.limit_set = false;
    y_axis.limit_set = false;

    motion_cmd_t cmd_x = { .x_mm = -150, .y_mm = 0, .z_mm = 0 };
    xQueueSend(motion_queue, &cmd_x, portMAX_DELAY);
    motion_cmd_t cmd_y = { .x_mm = 0, .y_mm = -150, .z_mm = 0 };
    xQueueSend(motion_queue, &cmd_y, portMAX_DELAY);
    // motion_cmd_t cmd_z = { .x_mm = 0, .y_mm = 0, .z_mm = 100}; //invert
    // xQueueSend(motion_queue, &cmd_z, portMAX_DELAY);
}

void motion_task(void *arg) {
    motion_cmd_t cmd;
    while (1) {
        if(current_pin != -1){
            check_limit_switches();
            continue;
        }
        if (xQueueReceive(motion_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            

            int32_t delta_x = (int32_t)roundf(cmd.x_mm * STEPS_PER_MM_X) - x_axis.steps_position;
            int32_t delta_y = (int32_t)roundf(cmd.y_mm * STEPS_PER_MM_Y) - y_axis.steps_position;
            int32_t delta_z = (int32_t)roundf(cmd.z_mm * STEPS_PER_MM_Z) - z_axis.steps_position;

            int32_t target_steps_x = x_axis.steps_position + delta_x;
            int32_t target_steps_y = y_axis.steps_position + delta_y;
            int32_t target_steps_z = z_axis.steps_position + delta_z;

            if (!check_bounds_ok(&x_axis, target_steps_x) ||
                !check_bounds_ok(&y_axis, target_steps_y) ||
                !check_bounds_ok(&z_axis, target_steps_z)) {
                continue;
            }

            bool dir_x = (delta_x >= 0);
            bool dir_y = (delta_y >= 0);
            bool dir_z = (delta_z >= 0);

            int32_t steps_x = abs(delta_x);
            int32_t steps_y = abs(delta_y);
            int32_t steps_z = abs(delta_z);

            gpio_set_level(DIR_PIN_X, dir_x); // need to implement like set_direction(*motor, dir)
            gpio_set_level(DIR_PIN_Y, dir_y);
            gpio_set_level(DIR_PIN_Z, !dir_z); // инверсия Z


            x_axis.current_direction = dir_x;
            y_axis.current_direction = dir_y;
            z_axis.current_direction = dir_z;

            x_axis.target_position = steps_x;
            y_axis.target_position = steps_y;
            z_axis.target_position = steps_z;

            x_axis.done_position = 0;
            y_axis.done_position = 0;
            z_axis.done_position = 0;

            x_axis.dda_d_pos = steps_x;
            y_axis.dda_d_pos = steps_y;
            z_axis.dda_d_pos = steps_z;

            int32_t dda_NN = MAX(steps_x, MAX(steps_y, steps_z));

            x_axis.dda_N = dda_NN;
            y_axis.dda_N = dda_NN;
            z_axis.dda_N = dda_NN;

            x_axis.dda_err_pos = 0;
            y_axis.dda_err_pos = 0;
            z_axis.dda_err_pos = 0;

            // Инициализация профиля скорости
            total_steps = dda_NN;
            accel_steps = (total_steps < 2 * ACCEL_STEPS) ? (total_steps / 2) : ACCEL_STEPS;
            current_period = DEFAULT_START_PERIOD;
            mcpwm_timer_set_period(motor_timer.timer, current_period);
            // Обновляем компараторы под начальный период
            set_speed(&motor_x, current_period);
            set_speed(&motor_y, current_period);
            set_speed(&motor_z, current_period);

            mcpwm_timer_start_stop(motor_timer.timer, MCPWM_TIMER_START_NO_STOP);

            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            if (gcode_task_handle != NULL) {
                xTaskNotifyGive(gcode_task_handle);
            }
        }
    }
}
