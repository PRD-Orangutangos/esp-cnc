#include "driver/mcpwm_prelude.h"
#include "driver/gpio.h"
#include "drivers/motor.h"
#include "my_timer.h"


#define STEPS_PER_MM_X 1600 
#define STEPS_PER_MM_Y 1600
#define STEPS_PER_MM_Z 1600

#define MAX_X_POSITION 100 
#define MAX_Y_POSITION 100
#define MAX_Z_POSITION 30

#define MIN_X_POSITION 0 
#define MIN_Y_POSITION 0
#define MIN_Z_POSITION (-29)

#define MAX_X_STEPS (MAX_X_POSITION * STEPS_PER_MM_X) 
#define MAX_Y_STEPS (MAX_Y_POSITION * STEPS_PER_MM_Y)
#define MAX_Z_STEPS (MAX_Z_POSITION * STEPS_PER_MM_Z)

#define MIN_X_STEPS (MIN_X_POSITION * STEPS_PER_MM_X) 
#define MIN_Y_STEPS (MIN_Y_POSITION * STEPS_PER_MM_Y)
#define MIN_Z_STEPS (MIN_Z_POSITION * STEPS_PER_MM_Z)

typedef struct {
    motor_t *motor;
    uint32_t steps_per_mm;
    float current_position;
    int32_t max_steps;
    int32_t min_steps;
    volatile bool current_direction;
    bool limit_set;
    volatile int32_t steps_position;
    volatile int32_t target_position;
    volatile int32_t done_position;
    volatile int32_t dda_err_pos;
    volatile int32_t dda_d_pos;
    volatile int32_t dda_N;
    volatile int32_t total_steps;
    volatile int32_t accel_steps;
} axis_t;

void axis_init(axis_t *axis,
               motor_t *motor,
               uint32_t steps_per_mm,
               int32_t min_steps,
               int32_t max_steps)
{
    axis->motor = motor;
    axis->steps_per_mm = steps_per_mm;
    axis->min_steps = min_steps;
    axis->max_steps = max_steps;
    axis->current_direction = true;
}

bool check_done(axis_t *axis){
    return (axis->done_position >= axis->target_position);
}

void dda_step(axis_t *axis){
    axis->dda_err_pos += axis->dda_d_pos;
}

bool need_step(axis_t *axis){
    if(axis->dda_err_pos >= axis->dda_N && axis->done_position < axis->target_position){
        axis->dda_err_pos -= axis->dda_N;
        axis->done_position++;
        axis->steps_position += (axis->current_direction ? 1 : -1);
        return true;
    }
    return false;
}

axis_t x_axis;
axis_t y_axis;
axis_t z_axis;

