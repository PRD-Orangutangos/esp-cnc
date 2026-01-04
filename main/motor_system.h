#include "driver/mcpwm_prelude.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

#define DIR_PIN_X GPIO_NUM_18
#define STEP_PIN_X GPIO_NUM_19

#define DIR_PIN_Y GPIO_NUM_20
#define STEP_PIN_Y GPIO_NUM_21

#define DIR_PIN_Z GPIO_NUM_22
#define STEP_PIN_Z GPIO_NUM_23

#define DEFAULT_SPEED 90


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



// целевые шаги
volatile int target_x = 0;
volatile int target_y = 0;

// уже сделанные шаги
volatile int done_x = 0;
volatile int done_y = 0;

// DDA
volatile int dda_err_x = 0;
volatile int dda_err_y = 0;

volatile int dda_dx = 0;
volatile int dda_dy = 0;
volatile int dda_N = 0;


int steps_x = 0;
int steps_y = 0;
int steps_z = 0;

bool IRAM_ATTR timer_callback(
    mcpwm_timer_handle_t timer,
    const mcpwm_timer_event_data_t *edata,
    void *user_ctx
) {
    bool step_x = false;
    bool step_y = false;

    // --- DDA ---
    if (done_x < target_x || done_y < target_y) {

        dda_err_x += dda_dx;
        dda_err_y += dda_dy;

        if (dda_err_x >= dda_N && done_x < target_x) {
            dda_err_x -= dda_N;
            step_x = true;
            done_x++;
        }

        if (dda_err_y >= dda_N && done_y < target_y) {
            dda_err_y -= dda_N;
            step_y = true;
            done_y++;
        }
    }

    // --- X ось ---
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

    // --- Y ось ---
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

    // --- завершение ---
    if (done_x >= target_x && done_y >= target_y) {
        mcpwm_timer_start_stop(timer, MCPWM_TIMER_STOP_FULL);
        return false;
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
}

void move_linear_xy(int steps_x, int steps_y, bool dir_x, bool dir_y)
{
    gpio_set_level(DIR_PIN_X, dir_x);
    gpio_set_level(DIR_PIN_Y, dir_y);

    target_x = abs(steps_x);
    target_y = abs(steps_y);

    done_x = 0;
    done_y = 0;

    dda_dx = target_x;
    dda_dy = target_y;
    dda_N  = (target_x > target_y) ? target_x : target_y;

    dda_err_x = 0;
    dda_err_y = 0;

    mcpwm_timer_start_stop(timer1, MCPWM_TIMER_START_NO_STOP);
}