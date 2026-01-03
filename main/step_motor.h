#ifndef step_motor_h
#define step_motor_h

#include "driver/mcpwm_prelude.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct
{
    gpio_num_t dir_pin;
    gpio_num_t step_pin;
    mcpwm_timer_handle_t timer;
    mcpwm_timer_config_t timer_cfg;
    mcpwm_oper_handle_t oper;
    mcpwm_cmpr_handle_t comporator;
    mcpwm_gen_handle_t generator;
    volatile int steps_left;

} step_motor;


bool IRAM_ATTR step_motor_callback(mcpwm_timer_handle_t timer, 
                                   const mcpwm_timer_event_data_t *edata, 
                                   void *user_ctx) {
    step_motor *motor = (step_motor *)user_ctx;
    
    if (motor->steps_left > 0) {
        motor->steps_left--;
        return true;
    }
    
    mcpwm_timer_start_stop(timer, MCPWM_TIMER_STOP_FULL);
    gpio_set_level(motor->step_pin, 0);
    return false;
}

void gpio_init(step_motor *current_motor)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << current_motor->dir_pin) | (1ULL << current_motor->step_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    gpio_set_level(current_motor->dir_pin, 1);
    gpio_set_level(current_motor->step_pin, 1);
}

void timer_init(step_motor *current_motor)
{
    mcpwm_timer_config_t new_timer_cfg = {0};  // Все поля = 0

    new_timer_cfg.group_id = 0;
    new_timer_cfg.clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT;
    new_timer_cfg.resolution_hz = 1000000;
    new_timer_cfg.period_ticks = 90;
    new_timer_cfg.count_mode = MCPWM_TIMER_COUNT_MODE_UP;
    new_timer_cfg.flags.update_period_on_empty = true;

    current_motor->timer_cfg = new_timer_cfg;
    ESP_ERROR_CHECK(mcpwm_new_timer(&current_motor->timer_cfg, &current_motor->timer));

    mcpwm_timer_event_callbacks_t cbs = {
        .on_full = step_motor_callback,  // для X
    };
    ESP_ERROR_CHECK(mcpwm_timer_register_event_callbacks(current_motor->timer, &cbs, current_motor));
}

void operator_init(step_motor *current_motor)
{
    ESP_ERROR_CHECK(mcpwm_new_operator(&(mcpwm_operator_config_t){.group_id = 0}, &current_motor->oper));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(current_motor->oper, current_motor->timer));
}

void comporator_init(step_motor *current_motor)
{
    ESP_ERROR_CHECK(mcpwm_new_comparator(current_motor->oper, &(mcpwm_comparator_config_t){}, &current_motor->comporator));
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(current_motor->comporator, current_motor->timer_cfg.period_ticks / 2));
}
void generator_init(step_motor *current_motor)
{
    ESP_ERROR_CHECK(mcpwm_new_generator(current_motor->oper, &(mcpwm_generator_config_t){.gen_gpio_num = current_motor->step_pin}, &current_motor->generator));

    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(
        current_motor->generator,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(
        current_motor->generator,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, current_motor->comporator, MCPWM_GEN_ACTION_LOW)));
}

static esp_err_t step_motor_init(step_motor *new_motor, gpio_num_t dir, gpio_num_t step)
{
    esp_err_t err = ESP_OK;
    new_motor->dir_pin = dir;
    new_motor->step_pin = step;
    new_motor->steps_left = 0;
    gpio_init(new_motor);
    timer_init(new_motor);
    operator_init(new_motor);
    comporator_init(new_motor);
    generator_init(new_motor);

    return err;
}
static void step_motor_move(step_motor *motor, int steps, bool direction, uint32_t speed){
    gpio_set_level(motor->dir_pin, direction);
    motor->steps_left = steps;
    ESP_ERROR_CHECK(mcpwm_timer_set_period(motor->timer, speed));
    ESP_ERROR_CHECK(mcpwm_timer_enable(motor->timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(motor->timer, MCPWM_TIMER_START_NO_STOP));

}

#endif