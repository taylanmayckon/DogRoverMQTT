#ifndef LED_MATRIX_H
#define LED_MATRIX_H

#include "hardware/clocks.h"
#include "hardware/timer.h"
#include "ws2812.pio.h"

#define NUM_PIXELS 25

// Struct com a estrutura das cores
typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} Led_color;

// Struct para armazenar a estrutura dos frames
typedef struct {
    Led_color led[NUM_PIXELS];
} Led_frame;

// Declaração das funções utilizadas na lib led_matrix
static inline void put_pixel(uint32_t pixel_grb);

uint32_t urgb_u32(double r, double g, double b);

void set_leds(float intensidade);

// Função genérica para atualiza matriz
void matrix_update_leds(Led_frame *frame, float intensidade);

#endif