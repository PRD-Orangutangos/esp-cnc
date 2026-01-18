#pragma once

#include "driver/mcpwm_prelude.h"
#include "driver/gpio.h"
#include "drivers/motor.h"
#include "my_timer.h"

// Setup st/mm for your step motors
#define STEPS_PER_MM_X 1600 //steps count of step motor for move x axis to 1 mm
#define STEPS_PER_MM_Y 1600 //steps count of step motor for move y axis to 1 mm
#define STEPS_PER_MM_Z 1600 //steps count of step motor for move z axis to 1 mm

// Setup max/min position for your mechanic structure, system will not move over it
#define MAX_X_POSITION 100  // max x axis position in mm 
#define MAX_Y_POSITION 100 // max y position in mm
#define MAX_Z_POSITION 30 // max z position in mm

#define MIN_X_POSITION 0 
#define MIN_Y_POSITION 0
#define MIN_Z_POSITION (-29)

// Max and min position in steps, for comfort calculations 
#define MAX_X_STEPS (MAX_X_POSITION * STEPS_PER_MM_X) 
#define MAX_Y_STEPS (MAX_Y_POSITION * STEPS_PER_MM_Y)
#define MAX_Z_STEPS (MAX_Z_POSITION * STEPS_PER_MM_Z)

#define MIN_X_STEPS (MIN_X_POSITION * STEPS_PER_MM_X) 
#define MIN_Y_STEPS (MIN_Y_POSITION * STEPS_PER_MM_Y)
#define MIN_Z_STEPS (MIN_Z_POSITION * STEPS_PER_MM_Z)



// Axis structure with required parametrs
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
} axis_t;


// Function for init axis main parametrs, others will = 0
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

// Checking completion of work current axis
bool check_done(axis_t *axis){
    return (axis->done_position >= axis->target_position);
}

// Make step in interpolstion system
void dda_step(axis_t *axis){
    axis->dda_err_pos += axis->dda_d_pos;
}

// Check if we need to make step for current axis in interpolation system
bool need_step(axis_t *axis){
    if(axis->dda_err_pos >= axis->dda_N && axis->done_position < axis->target_position){
        axis->dda_err_pos -= axis->dda_N;
        axis->done_position++;
        axis->steps_position += (axis->current_direction ? 1 : -1);
        return true;
    }
    return false;
}

// At this moment if current axis need to make step, it will step
void check_and_step(axis_t *axis){
    dda_step(axis);
    make_step(need_step(axis), axis->motor);
}

// Check will current axis going other limits or not
bool check_bounds_ok(axis_t *axis, int32_t targ_s){
    if (axis->limit_set) {
        if(targ_s > axis->max_steps || targ_s < axis->min_steps){
            return false;
        }
    }
    return true;
}

// Set axis params after basing or zero setting
void setup_axis(axis_t *axis){
    axis->min_steps = 0;
    axis->steps_position = 0;
    axis->current_position = 0;
    axis->limit_set = true;
}


axis_t x_axis; 
axis_t y_axis;
axis_t z_axis;


