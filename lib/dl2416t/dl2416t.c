#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <stdint.h>
#include "dl2416t.h"


void __gpio_setup_output(unsigned int *gpio_ptr) {
    if (gpio_ptr != NULL) {
        gpio_init(*gpio_ptr);
        gpio_set_dir(*gpio_ptr, GPIO_OUT);
    }
}


void setup_display(struct dl2416t *dl2416t) {
    __gpio_setup_output(&dl2416t->blank_n);
    __gpio_setup_output(&dl2416t->clear_n);
    __gpio_setup_output(&dl2416t->write_n);
    __gpio_setup_output(&dl2416t->cursor_en);
    __gpio_setup_output(&dl2416t->cursor_sel);
    for (int i = 0; i < 7; i++) {
        __gpio_setup_output(&dl2416t->data_pin[i]);
    }
    for (int i = 0; i < 2; i++) {
        __gpio_setup_output(&dl2416t->address_pin[i]);
    }

    gpio_put((uintptr_t) dl2416t->blank_n, true);
    gpio_put((uintptr_t) dl2416t->cursor_en, false);
    gpio_put((uintptr_t) dl2416t->cursor_sel, true);
    gpio_put((uintptr_t) dl2416t->clear_n, true);
    gpio_put((uintptr_t) dl2416t->write_n, true);
    sleep_us(2);  // Need to respect the hold time of the display
}


void display_char(struct dl2416t *dl2416t, char character, uint8_t index) {
    gpio_put((uintptr_t) dl2416t->write_n, true);
    sleep_us(1);  // hold time
    gpio_put_masked(
        0x1 << (uintptr_t) dl2416t->data_pin[0]    |
        0x1 << (uintptr_t) dl2416t->data_pin[1]    |
        0x1 << (uintptr_t) dl2416t->data_pin[2]    |
        0x1 << (uintptr_t) dl2416t->data_pin[3]    |
        0x1 << (uintptr_t) dl2416t->data_pin[4]    |
        0x1 << (uintptr_t) dl2416t->data_pin[5]    |
        0x1 << (uintptr_t) dl2416t->data_pin[6]    |
        0x1 << (uintptr_t) dl2416t->address_pin[0] |
        0x1 << (uintptr_t) dl2416t->address_pin[1],
        ((character & 0b0000001u) != 0) << (uintptr_t) dl2416t->data_pin[0]    |
        ((character & 0b0000010u) != 0) << (uintptr_t) dl2416t->data_pin[1]    |
        ((character & 0b0000100u) != 0) << (uintptr_t) dl2416t->data_pin[2]    |
        ((character & 0b0001000u) != 0) << (uintptr_t) dl2416t->data_pin[3]    |
        ((character & 0b0010000u) != 0) << (uintptr_t) dl2416t->data_pin[4]    |
        ((character & 0b0100000u) != 0) << (uintptr_t) dl2416t->data_pin[5]    |
        ((character & 0b1000000u) != 0) << (uintptr_t) dl2416t->data_pin[6]    |
        ((index     & 0b01      ) != 0) << (uintptr_t) dl2416t->address_pin[0] |
        ((index     & 0b10      ) != 0) << (uintptr_t) dl2416t->address_pin[1]
    );
    sleep_us(2);
    gpio_put((uintptr_t) dl2416t->write_n, false);
    sleep_us(1);
    gpio_put((uintptr_t) dl2416t->write_n, true);
}


void display_word(struct dl2416t *dl2416t, char string[5]) {
    for (int i = 0; i <= 3; i++) {
        display_char(dl2416t, string[i], 3-i);
    }
}


void display_chars_all(struct dl2416t *dl2416t, char char_0, char char_1, char char_2, char char_3) {
    display_char(dl2416t, char_0, 0u);
    display_char(dl2416t, char_1, 1u);
    display_char(dl2416t, char_2, 2u);
    display_char(dl2416t, char_3, 3u);
}


void display_blank(struct dl2416t *dl2416t, bool blank_enable) {
    gpio_put((uintptr_t) dl2416t->blank_n, !blank_enable);
}

