#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/powman.h"
#include "hardware/structs/powman.h"
#include "pico/aon_timer.h"
#include "hardware/pio.h"
#include "quadrature_encoder.pio.h"

#define TX    0
#define RX    1

#define CLR_N 6u
#define CUE   7u
#define CU_N  8u
#define WR_N  9u
#define BL_N  25u
#define A1    10u
#define A0    11u
#define D0    18u
#define D1    19u
#define D2    20u
#define D3    21u
#define D4    24u
#define D5    23u
#define D6    22u

#define BUZ   16u
#define LED   12u

#define ROTM  26u
#define ROTP  28u
#define ROTBN 27u

#define TCXO  14u

enum FSMstate {
    STARTUP = 0,
    DISPLAY_TIME = 1,
    SET_TIME = 2,
    DISABLE_ALARM = 3,
    SET_LED = 4,
    ALARM_ACTIVE = 5,
    ADJUST_HRS = 6,
    ADJUST_MINS = 7,
    ADJUST_HRS_MINS = 8,
    SET_MENU = 9
    
};

enum menu {
    MENU_ALARM = 0,
    MENU_TIME = 1,
    MENU_ALARM_OFF = 2
};

// timespec struct, initalise at 0
struct tm time_stat = {0, 0, 0, 0, 0, 0, 0, 0};
struct tm alarm =     {0, 0, 0, 0, 0, 0, 0, 0};

bool alarm_on = false;

PIO pio = pio0;
const uint sm = 0;


void setup_gpio() {
    gpio_init_mask(
        0x1 << CLR_N | 
        0x1 << CUE   | 
        0x1 << CU_N  | 
        0x1 << WR_N  | 
        0x1 << BL_N  | 
        0x1 << A1    | 
        0x1 << A0    | 
        0x1 << D0    | 
        0x1 << D1    | 
        0x1 << D2    | 
        0x1 << D3    | 
        0x1 << D4    | 
        0x1 << D5    | 
        0x1 << D6    |
        0x1 << BUZ   |
        0x1 << LED   |
        0x1 << ROTM  |
        0x1 << ROTP  |
        0x1 << ROTBN 
    );
    gpio_set_dir_out_masked(
        0x1 << CLR_N | 
        0x1 << CUE   | 
        0x1 << CU_N  | 
        0x1 << WR_N  | 
        0x1 << BL_N  | 
        0x1 << A1    | 
        0x1 << A0    | 
        0x1 << D0    | 
        0x1 << D1    | 
        0x1 << D2    | 
        0x1 << D3    | 
        0x1 << D4    | 
        0x1 << D5    | 
        0x1 << D6    |
        0x1 << BUZ   |
        0x1 << LED   
    );
}


void setup_clks() {
    gpio_init(TCXO);
    clock_configure_gpin(clk_ref, TCXO, 12*MHZ, 12*MHZ);
}


void setup_display() {
    gpio_put(BL_N, true);
    gpio_put(CUE, false);
    gpio_put(CU_N, true);
    gpio_put(CLR_N, true);
    gpio_put(WR_N, true);
    sleep_us(2);
}


void display_char(char character, uint8_t index) {
    gpio_put(WR_N, true);
    sleep_us(1);
    gpio_put_masked(
        0x1 << D0 |
        0x1 << D1 |
        0x1 << D2 |
        0x1 << D3 |
        0x1 << D4 |
        0x1 << D5 |
        0x1 << D6 |
        0x1 << A0 |
        0x1 << A1 ,
        ((character & 0b0000001u) != 0) << D0 |
        ((character & 0b0000010u) != 0) << D1 |
        ((character & 0b0000100u) != 0) << D2 |
        ((character & 0b0001000u) != 0) << D3 |
        ((character & 0b0010000u) != 0) << D4 |
        ((character & 0b0100000u) != 0) << D5 |
        ((character & 0b1000000u) != 0) << D6 |
        ((index     & 0b01      ) != 0) << A0 |
        ((index     & 0b10      ) != 0) << A1
    );
    sleep_us(2);
    gpio_put(WR_N, false);
    sleep_us(1);
    gpio_put(WR_N, true);
}


void display_word(char string[5]) {
    gpio_put(CLR_N, false);
    sleep_us(2);
    gpio_put(CLR_N, true);
    sleep_us(2);
    for (int i = 0; i <= 3; i++) {
        display_char(string[i], 3-i);
    }
}


int run_startup(void) {
    stdio_init_all();
    uart_init(uart0, 115200);

    setup_gpio();
    setup_clks();
    setup_display();

    // PIO setup
    pio_add_program(pio, &quadrature_encoder_program);
    quadrature_encoder_program_init(pio, sm, ROTP, ROTM, 1000);

    // initial time to aid with debug
    time_stat.tm_hour = 8;
    time_stat.tm_min  = 0;
    aon_timer_start_calendar(&time_stat);

    // initial alarm time
    alarm.tm_hour = 7;
    alarm.tm_min  = 30;

    display_word("PWR-");
    sleep_ms(1000);
    display_word("-ON!");
    sleep_ms(1000);
    return ADJUST_HRS;
}


int run_display_time(void) {
    // Check button and alarm
    bool curr_btn;
    bool last_btn = gpio_get(ROTBN);

    int curr_min, last_min;
    int curr_hr, last_hr;

    aon_timer_get_time_calendar(&time_stat);
    last_min = time_stat.tm_min;
    last_hr  = time_stat.tm_hour;

    display_char((uint32_t) last_min%10 + 0x30, 0);
    display_char((uint32_t) last_min/10 + 0x30, 1);
    display_char((uint32_t) last_hr%10  + 0x30, 2);
    display_char((uint32_t) last_hr/10  + 0x30, 3);

    if (alarm_on) gpio_put(LED, true);
    else          gpio_put(LED, false);

    gpio_put(BL_N, true);

    while(1) {
        aon_timer_get_time_calendar(&time_stat);
        curr_min = time_stat.tm_min;
        curr_hr  = time_stat.tm_hour;

        if (alarm_on && (curr_hr == alarm.tm_hour) && (curr_min == alarm.tm_min)) return ALARM_ACTIVE;

        curr_btn = gpio_get(ROTBN);
        if (!curr_btn & last_btn) {
            sleep_ms(50); // sleep for debounce
            // wait for button to be deasserted
            while (!gpio_get(ROTBN)) {
                tight_loop_contents();
            }
            sleep_ms(100); // more debounce
            return SET_MENU;
        }

        if (last_min != curr_min) {
            display_char((uint32_t) curr_min%10 + 0x30, 0);
            display_char((uint32_t) curr_min/10 + 0x30, 1);
            display_char((uint32_t) curr_hr%10  + 0x30, 2);
            display_char((uint32_t) curr_hr/10  + 0x30, 3);
        }

        last_min = curr_min;
        last_hr  = curr_hr;
        last_btn = curr_btn;        
    } 
}


int run_adjust_hrs(void) {
    int target_hr = time_stat.tm_hour;

    bool curr_rot[2];
    bool last_rot[2] = {true, true};

    bool curr_btn;
    bool last_btn = gpio_get(ROTBN);

    uint32_t gpio_poll;

    int new_value, delta, old_value = 0;
    int last_value = 0, last_delta = 0;

    while (1) {
        gpio_poll = gpio_get_all();
        curr_rot[0] = gpio_poll & (0x1 << ROTP) ? true : false;
        curr_rot[1] = gpio_poll & (0x1 << ROTM) ? true : false;

        display_char((uint32_t) 'H', 3);
        display_char((uint32_t) ':', 2);
        display_char((uint32_t) target_hr%10  + 0x30, 0);
        display_char((uint32_t) target_hr/10  + 0x30, 1);

        if (curr_rot[0] != last_rot[0]) {
            // if (!curr_rot[0] & curr_rot[1]) {
            //     target_hr = ((target_hr + 1) % 24);
            //     //sleep_ms(40);
            // } else if (curr_rot[0] & !curr_rot[1]) {
            //     target_hr = target_hr == 0x0 ? 23u : ((target_hr - 1) % 24);
            //     //sleep_ms(40);

            if (curr_rot[1] != curr_rot[0]) {
                target_hr = ((target_hr + 1) % 24);
                sleep_ms(80);
            } else {
                target_hr = target_hr == 0x0 ? 23u : ((target_hr - 1) % 24);
                sleep_ms(80);
            }
        }

        // new_value = quadrature_encoder_get_count(pio, sm);
        // delta = new_value - old_value;
        // old_value = new_value;

        // if (delta >= 1) {
        //     target_hr = ((target_hr + 1) % 24);
        //     last_value = new_value;
        //     last_delta = delta;
        // } else if (delta <= -1) {
        //     target_hr = ((target_hr - 1) % 24);
        //     last_value = new_value;
        //     last_delta = delta;
        // }
        // sleep_ms(200);

        curr_btn = gpio_get(ROTBN);
        if (!curr_btn & last_btn) {
            sleep_ms(50); // sleep for debounce
            // wait for button to be deasserted
            while (!gpio_get(ROTBN)) {
                tight_loop_contents();
            }
            time_stat.tm_hour = target_hr;
            sleep_ms(100); // more debounce
            return ADJUST_MINS;
        }

        last_btn = curr_btn;
        last_rot[0], last_rot[1] = curr_rot[0], curr_rot[1];
    }
}


int run_adjust_mins(void) {
    uint32_t target_min = time_stat.tm_min;

    bool curr_rot[2];
    bool last_rot[2] = {true, true};

    bool curr_btn;
    bool last_btn = gpio_get(ROTBN);

    uint32_t gpio_poll;

    while (1) {
        gpio_poll = gpio_get_all();
        curr_rot[0] = gpio_poll & (0x1 << ROTP) ? true : false;
        curr_rot[1] = gpio_poll & (0x1 << ROTM) ? true : false;

        display_char((uint32_t) 'M', 3);
        display_char((uint32_t) ':', 2);
        display_char((uint32_t) target_min%10  + 0x30, 0);
        display_char((uint32_t) target_min/10  + 0x30, 1);

        if (curr_rot != last_rot) {
            if (!curr_rot[0] & curr_rot[1]) {
                target_min = ((target_min + 1) % 60);
                sleep_ms(80);
            } else if (curr_rot[0] & !curr_rot[1]) {
                target_min = target_min == 0x0 ? 59u : ((target_min - 1) % 60);
                sleep_ms(80);
            }
        }

        curr_btn = gpio_get(ROTBN);
        if (!curr_btn & last_btn) {
            sleep_ms(50); // sleep for debounce
            // wait for button to be deasserted
            while (!gpio_get(ROTBN)) {
                tight_loop_contents();
            }
            sleep_ms(100); // more debounce
            time_stat.tm_min = target_min;
            time_stat.tm_sec = 0;
            aon_timer_set_time_calendar(&time_stat);
            return DISPLAY_TIME;
        }

        last_btn = curr_btn;
        last_rot[0], last_rot[1] = curr_rot[0], curr_rot[1];
    }
}


int run_set_menu() {
    uint32_t menu_sel;

    bool curr_rot[2];
    bool last_rot[2] = {true, true};

    bool curr_btn;
    bool last_btn = gpio_get(ROTBN);

    uint32_t gpio_poll;

    gpio_put(BL_N, true);

    while (1) {
        gpio_poll = gpio_get_all();
        curr_rot[0] = gpio_poll & (0x1 << ROTP) ? true : false;
        curr_rot[1] = gpio_poll & (0x1 << ROTM) ? true : false;

        curr_btn = gpio_get(ROTBN);
        if (!curr_btn & last_btn) {
            sleep_ms(50); // sleep for debounce
            // wait for button to be deasserted
            while (!gpio_get(ROTBN)) {
                tight_loop_contents();
            }
            sleep_ms(100); // more debounce
            if (menu_sel == MENU_ALARM    ) return ADJUST_HRS_MINS;
            if (menu_sel == MENU_TIME     ) return ADJUST_HRS;
            if (menu_sel == MENU_ALARM_OFF) return DISABLE_ALARM;
        }

        if (curr_rot != last_rot) {
            if (!curr_rot[0] & curr_rot[1]) {
                menu_sel = (menu_sel + 1) % 3;
                sleep_ms(100);
            } else if (curr_rot[0] & !curr_rot[1]) {
                menu_sel = (menu_sel + 2) % 3;
                sleep_ms(100);
            }
        }

        switch (menu_sel) {
            case MENU_TIME  : 
                display_word("TIME");
                sleep_ms(15);
                break;
            case MENU_ALARM : 
                display_word("ALRM");
                sleep_ms(15);
                break;
            case MENU_ALARM_OFF   : 
                display_word("AOFF");
                sleep_ms(15);
                break;
            default: // go back to time display is we get to unknown state
                return DISPLAY_TIME;
        }

        last_btn = curr_btn;
        last_rot[0], last_rot[1] = curr_rot[0], curr_rot[1];
    }
}


int run_adjust_hrs_mins(void) {
    // intended for alarm only
    bool curr_rot[2];
    bool last_rot[2] = {true, true};

    bool curr_btn;
    bool last_btn = gpio_get(ROTBN);

    uint32_t gpio_poll;

    alarm.tm_min = alarm.tm_min - (alarm.tm_min % 5);

    while (1) {
        gpio_poll = gpio_get_all();
        curr_rot[0] = gpio_poll & (0x1 << ROTP) ? true : false;
        curr_rot[1] = gpio_poll & (0x1 << ROTM) ? true : false;

        // toggle LED every 256 ms
        gpio_put(LED, !(powman_hw->read_time_lower & 0b110000000));

        display_char((uint32_t) alarm.tm_min%10  + 0x30, 0);
        display_char((uint32_t) alarm.tm_min/10  + 0x30, 1);
        display_char((uint32_t) alarm.tm_hour%10 + 0x30, 2);
        display_char((uint32_t) alarm.tm_hour/10 + 0x30, 3);

        if (curr_rot != last_rot) {
            if (!curr_rot[0] & curr_rot[1]) {
                if (alarm.tm_min == 55) {
                    alarm.tm_min = 0;
                    alarm.tm_hour = alarm.tm_hour == 23 ? 0 : alarm.tm_hour + 1;
                } else {
                    alarm.tm_min = alarm.tm_min + 5;
                }
                sleep_ms(80);
            } else if (curr_rot[0] & !curr_rot[1]) {
                if (alarm.tm_min == 0) {
                    alarm.tm_min = 55;
                    alarm.tm_hour = alarm.tm_hour == 0 ? 23 : alarm.tm_hour - 1;
                } else {
                    alarm.tm_min = alarm.tm_min - 5;
                }
                sleep_ms(80);
            }
        }

        curr_btn = gpio_get(ROTBN);
        if (!curr_btn & last_btn) {
            sleep_ms(50); // sleep for debounce
            // wait for button to be deasserted
            while (!gpio_get(ROTBN)) {
                tight_loop_contents();
            }
            alarm_on = true;
            sleep_ms(100); // more debounce
            return DISPLAY_TIME;
        }

        last_btn = curr_btn;
        last_rot[0], last_rot[1] = curr_rot[0], curr_rot[1];
    }
}


int run_alarm_active(void) {
    uint64_t time_zero;
    uint64_t timeout;

    bool curr_btn;
    bool last_btn = gpio_get(ROTBN);

    int curr_min, last_min;
    int curr_hr, last_hr;

    aon_timer_get_time_calendar(&time_stat);
    last_min = time_stat.tm_min;
    last_hr  = time_stat.tm_hour;

    display_char((uint32_t) last_min%10 + 0x30, 0);
    display_char((uint32_t) last_min/10 + 0x30, 1);
    display_char((uint32_t) last_hr%10  + 0x30, 2);
    display_char((uint32_t) last_hr/10  + 0x30, 3);

    timeout = powman_timer_get_ms() + (uint64_t) 1800000u;  // 20 mins timeout

    while(1) {
        if (timeout < powman_timer_get_ms()) return DISABLE_ALARM;

        aon_timer_get_time_calendar(&time_stat);
        curr_min = time_stat.tm_min;
        curr_hr  = time_stat.tm_hour;

        gpio_put(BL_N, powman_hw->read_time_lower & 0b10000000  );
        gpio_put(BUZ , !(powman_hw->read_time_lower & 0b1010000000));

        curr_btn = gpio_get(ROTBN);
        if (!curr_btn & last_btn) {
            time_zero = powman_timer_get_ms();
            gpio_put(BL_N, true);
            gpio_put(BUZ , false);
            sleep_ms(50); // sleep for debounce
            // wait for button to be deasserted
            while (!gpio_get(ROTBN)) {
                gpio_put(BL_N, powman_hw->read_time_lower & 0b10000000  );
                gpio_put(BUZ , !(powman_hw->read_time_lower & 0b1010000000));
                if (powman_timer_get_ms() > time_zero + 10000u) return DISABLE_ALARM;
            }
            sleep_ms(100); // more debounce
            
            if (time_stat.tm_min < 51) {
                alarm.tm_min = time_stat.tm_min + 9;
            } else {
                alarm.tm_min = (time_stat.tm_min + 9) % 60;
                alarm.tm_hour = time_stat.tm_hour;
            }
            gpio_put(BUZ, false);
            sleep_ms(100);
            gpio_put(BUZ, true);
            sleep_ms(100);
            gpio_put(BUZ, false);
            sleep_ms(100);
            gpio_put(BUZ, true);
            sleep_ms(100);
            gpio_put(BUZ, false);
            return DISPLAY_TIME;
        }
        
        if (last_min != curr_min) {
            display_char((uint32_t) curr_min%10 + 0x30, 0);
            display_char((uint32_t) curr_min/10 + 0x30, 1);
            display_char((uint32_t) curr_hr%10  + 0x30, 2);
            display_char((uint32_t) curr_hr/10  + 0x30, 3);
        }

        last_min = curr_min;
        last_hr  = curr_hr;
        last_btn = curr_btn;
    } 
}


int run_disable_alarm(void) {
    gpio_put(BL_N, true);
    gpio_put(LED, false);
    alarm_on = false;
    display_word("ALRM");
    gpio_put(BUZ, true);
    sleep_ms(100);
    gpio_put(BUZ, false);
    sleep_ms(100);
    gpio_put(BUZ, true);
    sleep_ms(100);
    gpio_put(BUZ, false);
    sleep_ms(400);
    display_word("OFF ");
    sleep_ms(600);
    gpio_put(BUZ, true);
    sleep_ms(600);
    gpio_put(BUZ, false);
    return DISPLAY_TIME;
}


int run_state(enum FSMstate state) {
    if      (state == STARTUP        ) return run_startup();
    else if (state == DISPLAY_TIME   ) return run_display_time();
    else if (state == SET_MENU       ) return run_set_menu();
    else if (state == ALARM_ACTIVE   ) return run_alarm_active();
    else if (state == ADJUST_HRS     ) return run_adjust_hrs();
    else if (state == ADJUST_MINS    ) return run_adjust_mins();
    else if (state == ADJUST_HRS_MINS) return run_adjust_hrs_mins();
    else if (state == DISABLE_ALARM  ) return run_disable_alarm();
    else                               return run_display_time();
}


int main() {
    enum FSMstate next_state = STARTUP;
    for (;;) {
        next_state = run_state(next_state);
    }
}
