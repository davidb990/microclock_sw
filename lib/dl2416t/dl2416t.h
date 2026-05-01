#include "hardware/gpio.h"

struct dl2416t {
    unsigned int data_pin[7];
    unsigned int address_pin[2];
    unsigned int clear_n;
    unsigned int write_n;
    unsigned int blank_n;
    unsigned int cursor_en;
    unsigned int cursor_sel;
};

void setup_display(struct dl2416t *display);

void display_char(struct dl2416t *dl2416t, char character, uint8_t index);

void display_word(struct dl2416t *dl2416t, char string[5]);

void display_chars_all(struct dl2416t *dl2416t, char char_0, char char_1, char char_2, char char_3);

void display_blank(struct dl2416t *dl2416t, bool blank_enable);
