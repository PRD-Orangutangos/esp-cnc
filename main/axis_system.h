
#include "driver/mcpwm_prelude.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include "esp_log.h"
#include "interrupt_switch.h"
#include "drivers/axis.h"

// #define DIR_PIN_X GPIO_NUM_18
// #define STEP_PIN_X GPIO_NUM_19

// #define DIR_PIN_Y GPIO_NUM_20
// #define STEP_PIN_Y GPIO_NUM_21

// #define DIR_PIN_Z GPIO_NUM_22
// #define STEP_PIN_Z GPIO_NUM_23




#define ACCEL_STEPS           300   

static TaskHandle_t gcode_task_handle = NULL;  // ← глобальный

float current_x_position = 0;
float current_y_position = 0;
float current_z_position = 0;

int32_t max_x_position = 100 * STEPS_PER_MM_X;
int32_t max_y_position = 100 * STEPS_PER_MM_Y;
int32_t max_z_position = 30 * STEPS_PER_MM_Z;

int32_t min_x_position = 0 * STEPS_PER_MM_X;
int32_t min_y_position = 0 * STEPS_PER_MM_Y;
int32_t min_z_position = -29 * STEPS_PER_MM_Z;

volatile bool current_dir_x = true;
volatile bool current_dir_y = true;
volatile bool current_dir_z = true;
bool x_limit_set = false;
bool y_limit_set = false;
bool z_limit_set = false;
volatile int32_t position_steps_x = 0;
volatile int32_t position_steps_y = 0;
volatile int32_t position_steps_z = 0;

typedef struct {
    float x_mm;
    float y_mm;
    float z_mm;
} motion_cmd_t;

QueueHandle_t motion_queue;



volatile int32_t target_x = 0;
volatile int32_t target_y = 0;
volatile int32_t target_z = 0;


volatile int32_t done_x = 0;
volatile int32_t done_y = 0;
volatile int32_t done_z = 0;


volatile int32_t dda_err_x = 0;
volatile int32_t dda_err_y = 0;
volatile int32_t dda_err_z = 0;
volatile int32_t dda_dx = 0;
volatile int32_t dda_dy = 0;
volatile int32_t dda_dz = 0;
volatile int32_t dda_N = 0;

uint32_t DEFAULT_START_PERIOD = 600;
uint32_t MAX_SPEED_PERIOD = 400;

volatile int32_t total_steps = 0;
volatile int32_t accel_steps = 0;
volatile uint32_t current_period = 600;









my_timer_t motor_timer;
Super_switch limit_x_switch;
Super_switch limit_y_switch;

void motion_task(void *arg);

void get_position(void) {
    current_x_position = (float)position_steps_x / STEPS_PER_MM_X;
    current_y_position = (float)position_steps_y / STEPS_PER_MM_Y;
    current_z_position = (float)position_steps_z / STEPS_PER_MM_Z;
}

void move_to_position(float x, float y, float z) {
    DEFAULT_START_PERIOD = 200;
    MAX_SPEED_PERIOD = 100;
    motion_cmd_t cmd = { .x_mm = x, .y_mm = y, .z_mm = z };
    xQueueSend(motion_queue, &cmd, portMAX_DELAY);
}

bool IRAM_ATTR timer_callback(
    mcpwm_timer_handle_t timer,
    const mcpwm_timer_event_data_t *edata,
    void *user_ctx
) {
    bool step_x = false, step_y = false, step_z = false;

    if (check_done(&x_axis) && check_done(&y_axis) && check_done(&z_axis)) {
        goto stop_motion;
    }

    // --- DDA шаг ---
    dda_step(&x_axis);
    dda_step(&y_axis);
    dda_step(&z_axis);

    step_x = need_step(&x_axis);
    step_y = need_step(&y_axis);
    step_z = need_step(&z_axis);

    make_step(step_x, &motor_x);
    make_step(step_y, &motor_y);
    make_step(step_z, &motor_z);
    

    // --- Управление скоростью (трапеция) ---
    int32_t steps_done = (int32_t)MAX(done_x, MAX(done_y, done_z));
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

    if (done_x >= target_x && done_y >= target_y && done_z >= target_z) {
        goto stop_motion;
    }

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
    x_limit_set = false;
    y_limit_set = false;
    min_x_position = -150 * STEPS_PER_MM_X;
    min_y_position = -150 * STEPS_PER_MM_Y;
    // min_z_position = -150 * STEPS_PER_MM_Z;

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
            if(current_pin == LIMIT_X){
                min_x_position = 0 * STEPS_PER_MM_X;
                position_steps_x = 0;
                current_x_position = 0;
                x_limit_set = true;
            }
            if(current_pin == LIMIT_Y){
                min_y_position = 0 * STEPS_PER_MM_Y;
                position_steps_y = 0;
                current_y_position = 0;
                y_limit_set = true;
            }
            if(current_pin == LIMIT_Z){
                min_z_position = -5 * STEPS_PER_MM_Z;
                max_z_position = 0 * STEPS_PER_MM_Z;
                position_steps_z = 0;
                current_z_position = 0;
                z_limit_set = true;
            }
            mcpwm_timer_start_stop(motor_timer.timer, MCPWM_TIMER_STOP_FULL);
            current_pin = -1; // сброс
            continue;
        }
        if (xQueueReceive(motion_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            
            float curr_x = (float)position_steps_x / STEPS_PER_MM_X;
            float curr_y = (float)position_steps_y / STEPS_PER_MM_Y;
            float curr_z = (float)position_steps_z / STEPS_PER_MM_Z;

            float dx = cmd.x_mm - curr_x;
            float dy = cmd.y_mm - curr_y;
            float dz = cmd.z_mm - curr_z;

            int32_t target_steps_x = position_steps_x + (int32_t)roundf(dx * STEPS_PER_MM_X);
            int32_t target_steps_y = position_steps_y + (int32_t)roundf(dy * STEPS_PER_MM_Y);
            int32_t target_steps_z = position_steps_z + (int32_t)roundf(dz * STEPS_PER_MM_Z);
            if (x_limit_set) {
                if (target_steps_x > max_x_position || target_steps_x < min_x_position) {
                    ESP_LOGE("motion", "X out of bounds!");
                    continue;
                }
            }

            // Проверяем Y, если его лимит установлен
            if (y_limit_set) {
                if (target_steps_y > max_y_position || target_steps_y < min_y_position) {
                    ESP_LOGE("motion", "Y out of bounds!");
                    continue;
                }
            }
            if (z_limit_set) {
            // Z проверяем всегда (если нужно)
                if (target_steps_z > max_z_position || target_steps_z < min_z_position) {
                    ESP_LOGE("motion", "Z out of bounds!");
                    continue;
                }
            }   
            bool dir_x = (dx >= 0);
            bool dir_y = (dy >= 0);
            bool dir_z = (dz >= 0);

            int32_t steps_x = abs((int32_t)roundf(dx * STEPS_PER_MM_X));
            int32_t steps_y = abs((int32_t)roundf(dy * STEPS_PER_MM_Y));
            int32_t steps_z = abs((int32_t)roundf(dz * STEPS_PER_MM_Z));

            gpio_set_level(DIR_PIN_X, dir_x);
            gpio_set_level(DIR_PIN_Y, dir_y);
            gpio_set_level(DIR_PIN_Z, !dir_z); // инверсия Z

            current_dir_x = dir_x;
            current_dir_y = dir_y;
            current_dir_z = dir_z;

            target_x = steps_x;
            target_y = steps_y;
            target_z = steps_z;

            done_x = done_y = done_z = 0;
            dda_dx = target_x; dda_dy = target_y; dda_dz = target_z;
            dda_N = MAX(target_x, MAX(target_y, target_z));
            dda_err_x = dda_err_y = dda_err_z = 0;

            // Инициализация профиля скорости
            total_steps = dda_N;
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
