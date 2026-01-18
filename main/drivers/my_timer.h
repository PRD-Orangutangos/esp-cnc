#pragma once

#include "driver/mcpwm_prelude.h"



// Special type for include callback function into to the structure
typedef bool (*timer_event_cb_t)(
    mcpwm_timer_handle_t timer,
    const mcpwm_timer_event_data_t *edata,
    void *user_ctx
);


// Time structure, it will useful for transfer data to other structures, bcs you cannot read period data from timer handle safe
typedef struct {
    mcpwm_timer_handle_t timer;
    mcpwm_timer_config_t timer_cfg;
    timer_event_cb_t callback;   
} my_timer_t;


// It return timer with preset of params
my_timer_t create_timer(timer_event_cb_t cb){
    my_timer_t new_timer;
    new_timer.timer_cfg.group_id = 0; // check, which group_id can use your esp32, with wrong id will not work
    new_timer.timer_cfg.clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT;
    new_timer.timer_cfg.resolution_hz = 1000000;
    new_timer.timer_cfg.period_ticks = 5000;
    new_timer.timer_cfg.count_mode = MCPWM_TIMER_COUNT_MODE_UP;
    new_timer.timer_cfg.flags.update_period_on_empty = true; // for callback

    ESP_ERROR_CHECK(mcpwm_new_timer(&new_timer.timer_cfg, &new_timer.timer));

    new_timer.callback = cb;

    mcpwm_timer_event_callbacks_t cbs = {
        .on_empty = new_timer.callback,
    };

    ESP_ERROR_CHECK(mcpwm_timer_register_event_callbacks(new_timer.timer, &cbs, NULL)); // bound callback for timer, in last parametr you can transfer data into callback

    mcpwm_timer_enable(new_timer.timer);

    return new_timer;
}

my_timer_t motor_timer;