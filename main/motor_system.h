#include "driver/mcpwm_prelude.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include "hal/gpio_ll.h"
#include "hal/gpio_types.h"
#include "esp_log.h"

#define DIR_PIN_X GPIO_NUM_18
#define STEP_PIN_X GPIO_NUM_19

#define DIR_PIN_Y GPIO_NUM_20
#define STEP_PIN_Y GPIO_NUM_21

#define DIR_PIN_Z GPIO_NUM_22
#define STEP_PIN_Z GPIO_NUM_23

#define DEFAULT_SPEED 100

#define STEPS_PER_MM_X 1600 
#define STEPS_PER_MM_Y 1600
#define STEPS_PER_MM_Z 1600

mcpwm_timer_handle_t timer1;
mcpwm_timer_config_t timer_cfg;

mcpwm_oper_handle_t operator_x;
mcpwm_oper_handle_t operator_y;
mcpwm_oper_handle_t operator_z;

mcpwm_cmpr_handle_t comporator_x;
mcpwm_cmpr_handle_t comporator_y;
mcpwm_cmpr_handle_t comporator_z;

mcpwm_gen_handle_t generator_x;
mcpwm_gen_handle_t generator_y;
mcpwm_gen_handle_t generator_z;

float current_x_position = 0;
float current_y_position = 0;
float current_z_position = 0;

int32_t max_x_position = 100 * STEPS_PER_MM_X;
int32_t max_y_position = 100 * STEPS_PER_MM_Y;
int32_t max_z_position = 0 * STEPS_PER_MM_Z;

int32_t min_x_position = 0 * STEPS_PER_MM_X;
int32_t min_y_position = 0 * STEPS_PER_MM_Y;
int32_t min_z_position = -29 * STEPS_PER_MM_Z;

volatile bool current_dir_x = true;
volatile bool current_dir_y = true;
volatile bool current_dir_z = true;

volatile int32_t position_steps_x = 0;
volatile int32_t position_steps_y = 0;
volatile int32_t position_steps_z = 0;


typedef struct {
    float x_mm;
    float y_mm;
    float z_mm;
} motion_cmd_t;  // всегда абсолютная позиция


QueueHandle_t motion_queue;
TaskHandle_t motion_task_handle = NULL;

// целевые шаги
volatile int target_x = 0;
volatile int target_y = 0;
volatile int target_z = 0;
// уже сделанные шаги
volatile int done_x = 0;
volatile int done_y = 0;
volatile int done_z = 0;
// DDA
volatile int dda_err_x = 0;
volatile int dda_err_y = 0;
volatile int dda_err_z = 0;

volatile int dda_dx = 0;
volatile int dda_dy = 0;
volatile int dda_dz = 0;
volatile int dda_N = 0;


void motion_task(void *arg) {
    motion_cmd_t cmd;
    while (1) {
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

            if (target_steps_x > max_x_position || target_steps_x < min_x_position ||
                target_steps_y > max_y_position || target_steps_y < min_y_position ||
                target_steps_z > max_z_position || target_steps_z < min_z_position) {
                ESP_LOGE("motion", "Target out of bounds");
                continue;
            }


            bool dir_x = (dx >= 0);
            bool dir_y = (dy >= 0);
            bool dir_z = (dz >= 0);

            int steps_x = abs((int)roundf(dx * STEPS_PER_MM_X));
            int steps_y = abs((int)roundf(dy * STEPS_PER_MM_Y));
            int steps_z = abs((int)roundf(dz * STEPS_PER_MM_Z));

            gpio_set_level(DIR_PIN_X, dir_x);
            gpio_set_level(DIR_PIN_Y, dir_y);
            gpio_set_level(DIR_PIN_Z, !dir_z);  // инверсия Z

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

            mcpwm_timer_start_stop(timer1, MCPWM_TIMER_START_NO_STOP);
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        }
    }
}

bool IRAM_ATTR timer_callback(
    mcpwm_timer_handle_t timer,
    const mcpwm_timer_event_data_t *edata,
    void *user_ctx
) {
    bool step_x = false;
    bool step_y = false;
    bool step_z = false;

    if (done_x < target_x || done_y < target_y || done_z < target_z) {

        dda_err_x += dda_dx;
        dda_err_y += dda_dy;
        dda_err_z += dda_dz;

        if (dda_err_x >= dda_N && done_x < target_x) {
            dda_err_x -= dda_N;
            step_x = true;
            done_x++;
            if (current_dir_x) {
                position_steps_x++;
            } else {
                position_steps_x--;
            }
        }

        if (dda_err_y >= dda_N && done_y < target_y) {
            dda_err_y -= dda_N;
            step_y = true;
            done_y++;
            if (current_dir_y) {
                position_steps_y++;
            } else {
                position_steps_y--;
            }
        }

        if (dda_err_z >= dda_N && done_z < target_z) {
            dda_err_z -= dda_N;
            step_z = true;
            done_z++;
            if (current_dir_z) {
                position_steps_z++;
            } else {
                position_steps_z--;
            }
        }
    }

    if (step_x) {
        mcpwm_generator_set_action_on_timer_event(
            generator_x,
            MCPWM_GEN_TIMER_EVENT_ACTION(
                MCPWM_TIMER_DIRECTION_UP,
                MCPWM_TIMER_EVENT_EMPTY,
                MCPWM_GEN_ACTION_HIGH));
        mcpwm_generator_set_action_on_compare_event(
            generator_x,
            MCPWM_GEN_COMPARE_EVENT_ACTION(
                MCPWM_TIMER_DIRECTION_UP,
                comporator_x,
                MCPWM_GEN_ACTION_LOW));
    } else {
        mcpwm_generator_set_action_on_timer_event(
            generator_x,
            MCPWM_GEN_TIMER_EVENT_ACTION(
                MCPWM_TIMER_DIRECTION_UP,
                MCPWM_TIMER_EVENT_EMPTY,
                MCPWM_GEN_ACTION_LOW));
    }

    if (step_y) {
        mcpwm_generator_set_action_on_timer_event(
            generator_y,
            MCPWM_GEN_TIMER_EVENT_ACTION(
                MCPWM_TIMER_DIRECTION_UP,
                MCPWM_TIMER_EVENT_EMPTY,
                MCPWM_GEN_ACTION_HIGH));
        mcpwm_generator_set_action_on_compare_event(
            generator_y,
            MCPWM_GEN_COMPARE_EVENT_ACTION(
                MCPWM_TIMER_DIRECTION_UP,
                comporator_y,
                MCPWM_GEN_ACTION_LOW));
    } else {
        mcpwm_generator_set_action_on_timer_event(
            generator_y,
            MCPWM_GEN_TIMER_EVENT_ACTION(
                MCPWM_TIMER_DIRECTION_UP,
                MCPWM_TIMER_EVENT_EMPTY,
                MCPWM_GEN_ACTION_LOW));
    }

    if (step_z) {
        mcpwm_generator_set_action_on_timer_event(
            generator_z,
            MCPWM_GEN_TIMER_EVENT_ACTION(
                MCPWM_TIMER_DIRECTION_UP,
                MCPWM_TIMER_EVENT_EMPTY,
                MCPWM_GEN_ACTION_HIGH));
        mcpwm_generator_set_action_on_compare_event(
            generator_z,
            MCPWM_GEN_COMPARE_EVENT_ACTION(
                MCPWM_TIMER_DIRECTION_UP,
                comporator_z,
                MCPWM_GEN_ACTION_LOW));
    } else {
        mcpwm_generator_set_action_on_timer_event(
            generator_z,
            MCPWM_GEN_TIMER_EVENT_ACTION(
                MCPWM_TIMER_DIRECTION_UP,
                MCPWM_TIMER_EVENT_EMPTY,
                MCPWM_GEN_ACTION_LOW));
    }

    if (done_x >= target_x && done_y >= target_y && done_z >= target_z) {
        mcpwm_timer_start_stop(timer, MCPWM_TIMER_STOP_FULL);

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(motion_task_handle, &xHigherPriorityTaskWoken);
        return xHigherPriorityTaskWoken == pdTRUE;
    }

    return true;
}


void gpios_init()
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << DIR_PIN_X) | (1ULL << STEP_PIN_X) | (1ULL << DIR_PIN_Y) | (1ULL << STEP_PIN_Y) | (1ULL << DIR_PIN_Z) | (1ULL << STEP_PIN_Z),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    gpio_set_level(DIR_PIN_X, 1);
    gpio_set_level(STEP_PIN_X, 1);
    gpio_set_level(DIR_PIN_Y, 1);
    gpio_set_level(STEP_PIN_Y, 1);
    gpio_set_level(DIR_PIN_Z, 1);
    gpio_set_level(STEP_PIN_Z, 1);
}

void timer_init()
{
    timer_cfg.group_id = 0;
    timer_cfg.clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT;
    timer_cfg.resolution_hz = 1000000;
    timer_cfg.period_ticks = DEFAULT_SPEED;
    timer_cfg.count_mode = MCPWM_TIMER_COUNT_MODE_UP;
    timer_cfg.flags.update_period_on_empty = true;

    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_cfg, &timer1));

    mcpwm_timer_event_callbacks_t cbs = {
        .on_full = timer_callback,  // для X
    };
    ESP_ERROR_CHECK(mcpwm_timer_register_event_callbacks(timer1, &cbs, NULL));
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer1));
}
void operators_init()
{
    ESP_ERROR_CHECK(mcpwm_new_operator(&(mcpwm_operator_config_t){.group_id = 0}, &operator_x));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(operator_x, timer1));

    ESP_ERROR_CHECK(mcpwm_new_operator(&(mcpwm_operator_config_t){.group_id = 0}, &operator_y));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(operator_y, timer1));

    ESP_ERROR_CHECK(mcpwm_new_operator(&(mcpwm_operator_config_t){.group_id = 0}, &operator_z));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(operator_z, timer1));
}

void comporators_init()
{
    ESP_ERROR_CHECK(mcpwm_new_comparator(operator_x, &(mcpwm_comparator_config_t){}, &comporator_x));
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comporator_x, timer_cfg.period_ticks / 2));

    ESP_ERROR_CHECK(mcpwm_new_comparator(operator_y, &(mcpwm_comparator_config_t){}, &comporator_y));
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comporator_y, timer_cfg.period_ticks / 2));

    ESP_ERROR_CHECK(mcpwm_new_comparator(operator_z, &(mcpwm_comparator_config_t){}, &comporator_z));
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comporator_z, timer_cfg.period_ticks / 2));
}
void generators_init()
{
    ESP_ERROR_CHECK(mcpwm_new_generator(operator_x, &(mcpwm_generator_config_t){.gen_gpio_num = STEP_PIN_X}, &generator_x));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(
        generator_x,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(
        generator_x,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comporator_x, MCPWM_GEN_ACTION_LOW)));


    ESP_ERROR_CHECK(mcpwm_new_generator(operator_y, &(mcpwm_generator_config_t){.gen_gpio_num = STEP_PIN_Y}, &generator_y));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(
        generator_y,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(
        generator_y,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comporator_y, MCPWM_GEN_ACTION_LOW)));

    
    ESP_ERROR_CHECK(mcpwm_new_generator(operator_z, &(mcpwm_generator_config_t){.gen_gpio_num = STEP_PIN_Z}, &generator_z));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(
        generator_z,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(
        generator_z,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comporator_z, MCPWM_GEN_ACTION_LOW)));
}
void init_motors()
{
    gpios_init();
    timer_init();
    operators_init();
    comporators_init();
    generators_init();

    motion_queue = xQueueCreate(10, sizeof(motion_cmd_t));
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

void get_position(void) {
    current_x_position = (float)position_steps_x / STEPS_PER_MM_X;
    current_y_position = (float)position_steps_y / STEPS_PER_MM_Y;
    current_z_position = (float)position_steps_z / STEPS_PER_MM_Z;
}


void move_to_position(float x, float y, float z) {
    motion_cmd_t cmd = { .x_mm = x, .y_mm = y, .z_mm = z };
    xQueueSend(motion_queue, &cmd, portMAX_DELAY);
}


