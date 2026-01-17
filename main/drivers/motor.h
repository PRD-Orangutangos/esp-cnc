#include "driver/mcpwm_prelude.h"
#include "driver/gpio.h"
#include "my_timer.h"

#define DIR_PIN_X GPIO_NUM_18
#define STEP_PIN_X GPIO_NUM_19

#define DIR_PIN_Y GPIO_NUM_20
#define STEP_PIN_Y GPIO_NUM_21

#define DIR_PIN_Z GPIO_NUM_22
#define STEP_PIN_Z GPIO_NUM_23


typedef struct{
    gpio_num_t dir_pin;
    gpio_num_t step_pin;
    mcpwm_timer_handle_t timer;
    uint32_t period;
    mcpwm_oper_handle_t operator;
    mcpwm_cmpr_handle_t comparator;
    mcpwm_gen_handle_t generator;
} motor_t;

void motor_gpio_init(motor_t *current_motor){
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << current_motor->dir_pin) | (1ULL << current_motor->step_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    gpio_set_level(current_motor->dir_pin, 1);
    gpio_set_level(current_motor->step_pin, 0);
}

void motor_operator_init(motor_t *current_motor){
    ESP_ERROR_CHECK(mcpwm_new_operator(&(mcpwm_operator_config_t){.group_id = 0}, &current_motor->operator));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(current_motor->operator, current_motor->timer));
}

void motor_comparator_init(motor_t *current_motor){
    ESP_ERROR_CHECK(mcpwm_new_comparator(current_motor->operator, &(mcpwm_comparator_config_t){}, &current_motor->comparator));
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(current_motor->comparator, current_motor->period/ 2));
}

void motor_generator_init(motor_t *current_motor){
    ESP_ERROR_CHECK(mcpwm_new_generator(current_motor->operator, &(mcpwm_generator_config_t){.gen_gpio_num = current_motor->step_pin}, &current_motor->generator));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(
        current_motor->generator,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(
        current_motor->generator,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, current_motor->comparator, MCPWM_GEN_ACTION_LOW)));

}
void make_step(bool high_state, motor_t *current_motor){
    if (high_state) {
        mcpwm_generator_set_action_on_timer_event(current_motor->generator,
            MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH));
        mcpwm_generator_set_action_on_compare_event(current_motor->generator,
            MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, current_motor->comparator, MCPWM_GEN_ACTION_LOW));
    } else {
        mcpwm_generator_set_action_on_timer_event(current_motor->generator,
            MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_LOW));
    }
}

void set_speed(motor_t *current_motor, uint32_t new_speed){
    current_motor->period = new_speed;
    mcpwm_comparator_set_compare_value(current_motor->comparator, current_motor->period / 2);    
}

motor_t new_motor(my_timer_t *m_timer, gpio_num_t d_pin, gpio_num_t s_pin){
    motor_t new_motor;
    new_motor.dir_pin = d_pin;
    new_motor.step_pin = s_pin;
    new_motor.timer = m_timer->timer;
    new_motor.period = m_timer->timer_cfg.period_ticks;
    motor_gpio_init(&new_motor);
    motor_operator_init(&new_motor);
    motor_comparator_init(&new_motor);
    motor_generator_init(&new_motor);

    return new_motor;
}

motor_t motor_x;
motor_t motor_y;
motor_t motor_z;
