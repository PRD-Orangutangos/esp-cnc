#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h> // для strtof
// Пример строк G-code (замените на чтение из файла/UART позже)


typedef struct {
    char cmd[8];        // например, "G00", "G01", "M03"
    float x, y, z;      // координаты
    float f;            // feedrate
    float s;            // spindle speed
    bool has_x, has_y, has_z, has_f, has_s;
} gcode_command_t;


void reset_gcode_cmd(gcode_command_t* cmd) {
    memset(cmd->cmd, 0, sizeof(cmd->cmd));
    cmd->x = cmd->y = cmd->z = cmd->f = cmd->s = 0.0f;
    cmd->has_x = cmd->has_y = cmd->has_z = cmd->has_f = cmd->has_s = false;
}

// Простая функция поиска числа после буквы (без alloca/malloc)
bool extract_float(const char* line, char letter, float* out_val) {
    const char* p = strchr(line, letter);
    if (!p) return false;
    char* endptr;
    float val = strtof(p + 1, &endptr);
    if (p + 1 == endptr) return false; // не число
    *out_val = val;
    return true;
}

void parse_gcode_line(const char* line, gcode_command_t* cmd) {
    reset_gcode_cmd(cmd);

    // Пропускаем пробелы и комментарии (в вашем файле их нет в рабочих строках)
    if (line[0] == '(' || line[0] == ';' || line[0] == '\0') {
        strcpy(cmd->cmd, "COMMENT");
        return;
    }

    // Извлекаем команду: первые 2–4 символа (например, G00, M03, T1)
    int i = 0;
    while (i < (int)sizeof(cmd->cmd) - 1 && line[i] != ' ' && line[i] != '\0' && line[i] != '\r' && line[i] != '\n') {
        cmd->cmd[i] = line[i];
        i++;
    }
    cmd->cmd[i] = '\0';

    // Извлекаем параметры
    if (extract_float(line, 'X', &cmd->x)) cmd->has_x = true;
    if (extract_float(line, 'Y', &cmd->y)) cmd->has_y = true;
    if (extract_float(line, 'Z', &cmd->z)) cmd->has_z = true;
    if (extract_float(line, 'F', &cmd->f)) cmd->has_f = true;
    if (extract_float(line, 'S', &cmd->s)) cmd->has_s = true;
}

const char* gcode_lines[] = {
    "G00 X74.1225 Y147.9206",
    "G01 Z-0.4000",
    "G01 X73.4748 Y147.6291",
    NULL
};



void parse_code(){
     const char* test_lines[] = {
        "G00 X74.1225 Y147.9206",
        "G01 Z-0.4000",
        "G01 F300.00",
        "M03 S24000.0",
        "(Comment line)",
        NULL
    };

    gcode_command_t cmd;
    const char** line_ptr = test_lines;
    while (*line_ptr != NULL) {
        parse_gcode_line(*line_ptr, &cmd);
        printf("CMD: %s", cmd.cmd);
        if (cmd.has_x) printf(" X=%.4f", cmd.x);
        if (cmd.has_y) printf(" Y=%.4f", cmd.y);
        if (cmd.has_z) printf(" Z=%.4f", cmd.z);
        if (cmd.has_f) printf(" F=%.1f", cmd.f);
        if (cmd.has_s) printf(" S=%.1f", cmd.s);
        printf("\n");
        line_ptr++;
    }
}