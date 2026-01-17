#pragma once

#include "driver/mcpwm_prelude.h"




typedef bool (*timer_event_cb_t)(
    mcpwm_timer_handle_t timer,
    const mcpwm_timer_event_data_t *edata,
    void *user_ctx
);

typedef struct {
    mcpwm_timer_handle_t timer;
    mcpwm_timer_config_t timer_cfg;
    timer_event_cb_t callback;   // поле для callback
} my_timer_t;



my_timer_t create_timer(timer_event_cb_t cb){
    my_timer_t new_timer;
    new_timer.timer_cfg.group_id = 0;
    new_timer.timer_cfg.clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT;
    new_timer.timer_cfg.resolution_hz = 1000000;
    new_timer.timer_cfg.period_ticks = 5000;
    new_timer.timer_cfg.count_mode = MCPWM_TIMER_COUNT_MODE_UP;
    new_timer.timer_cfg.flags.update_period_on_empty = true;

    ESP_ERROR_CHECK(mcpwm_new_timer(&new_timer.timer_cfg, &new_timer.timer));

    new_timer.callback = cb;
    mcpwm_timer_event_callbacks_t cbs = {
        .on_empty = new_timer.callback,
    };

    ESP_ERROR_CHECK(mcpwm_timer_register_event_callbacks(new_timer.timer, &cbs, NULL));

    mcpwm_timer_enable(new_timer.timer);

    return new_timer;
}

