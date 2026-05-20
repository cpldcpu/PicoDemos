#include "button.h"
#include "hardware/gpio.h"

static bool prev_pressed = false;

void button_init(void)
{
    gpio_init(DAWN_BUTTON_A_PIN);
    gpio_set_dir(DAWN_BUTTON_A_PIN, GPIO_IN);
    gpio_pull_up(DAWN_BUTTON_A_PIN);
    prev_pressed = false;
}

bool button_a_just_pressed(void)
{
    /* Active-low: pin reads 0 when pressed (pulled to GND). */
    const bool now_pressed = !gpio_get(DAWN_BUTTON_A_PIN);
    const bool edge = now_pressed && !prev_pressed;
    prev_pressed = now_pressed;
    return edge;
}
