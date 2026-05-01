#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/powman.h"
#include "hardware/structs/powman.h"
#include "pico/aon_timer.h"
#include "lib/dl2416t/dl2416t.h" 


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


struct dl2416t display = {
    {D0, D1, D2, D3, D4, D5, D6},
    {A0, A1},
    CLR_N,
    WR_N,
    BL_N,
    CUE,
    CU_N
};


void setup_gpio() {
    gpio_init_mask(
        0x1 << BUZ   |
        0x1 << LED   |
        0x1 << ROTM  |
        0x1 << ROTP  |
        0x1 << ROTBN 
    );
    gpio_set_dir_out_masked(
        0x1 << BUZ   |
        0x1 << LED   
    );
}


void setup_clks() {
    gpio_init(TCXO);
    clock_configure_gpin(clk_ref, TCXO, 12*MHZ, 12*MHZ);
}


int run_startup(void) {
    stdio_init_all();
    uart_init(uart0, 115200);

    setup_gpio();
    setup_clks();
    setup_display(&display);

    // initial time to aid with debug
    time_stat.tm_hour = 8;
    time_stat.tm_min  = 0;
    aon_timer_start_calendar(&time_stat);

    // initial alarm time
    alarm.tm_hour = 7;
    alarm.tm_min  = 30;

    display_word(&display, "PWR-");
    sleep_ms(1000);
    display_word(&display, "-ON!");
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

    display_chars_all(
        &display,
        last_min%10 + 0x30,
        last_min/10 + 0x30,
        last_hr%10  + 0x30,
        last_hr/10  + 0x30
    );

    if (alarm_on) gpio_put(LED, true);
    else          gpio_put(LED, false);

    display_blank(&display, false);

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
            display_chars_all(
                &display,
                curr_min%10 + 0x30,
                curr_min/10 + 0x30,
                curr_hr%10  + 0x30,
                curr_hr/10  + 0x30
            );
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

        display_chars_all(
            &display,
            target_hr%10  + 0x30,
            target_hr/10  + 0x30,
            ':',
            'H'
        );

        if (curr_rot[0] != last_rot[0]) {
            if (curr_rot[1] != curr_rot[0]) {
                target_hr = ((target_hr + 1) % 24);
                sleep_ms(80);
            } else {
                target_hr = target_hr == 0x0 ? 23u : ((target_hr - 1) % 24);
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

        display_chars_all(
            &display,
            target_min%10  + 0x30,
            target_min/10  + 0x30,
            ':',
            'M'
        );

        if (curr_rot[0] != last_rot[0]) {
            if (curr_rot[1] != curr_rot[0]) {
                target_min = ((target_min + 1) % 60);
                sleep_ms(80);
            } else {
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

        if (curr_rot[0] != last_rot[0]) {
            if (curr_rot[1] != curr_rot[0]) {
                menu_sel = (menu_sel + 1) % 3;
                sleep_ms(80);
            } else {
                menu_sel = (menu_sel + 2) % 3;
                sleep_ms(80);
            }
        }

        switch (menu_sel) {
            case MENU_TIME  : 
                display_word(&display, "TIME");
                sleep_ms(15);
                break;
            case MENU_ALARM : 
                display_word(&display, "ALRM");
                sleep_ms(15);
                break;
            case MENU_ALARM_OFF   : 
                display_word(&display, "AOFF");
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

        display_chars_all(
            &display,
            alarm.tm_min%10  + 0x30,
            alarm.tm_min/10  + 0x30,
            alarm.tm_hour%10  + 0x30,
            alarm.tm_hour/10 + 0x30
        );

        if (curr_rot[0] != last_rot[0]) {
            if (curr_rot[1] != curr_rot[0]) {
                if (alarm.tm_min == 55) {
                    alarm.tm_min = 0;
                    alarm.tm_hour = alarm.tm_hour == 23 ? 0 : alarm.tm_hour + 1;
                } else {
                    alarm.tm_min = alarm.tm_min + 5;
                }
                sleep_ms(80);
            } else {
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

    display_chars_all(
            &display,
            last_min%10 + 0x30,
            last_min/10 + 0x30,
            last_hr%10  + 0x30,
            last_hr/10  + 0x30
        );

    timeout = powman_timer_get_ms() + (uint64_t) 1800000u;  // 20 mins timeout

    while(1) {
        if (timeout < powman_timer_get_ms()) return DISABLE_ALARM;

        aon_timer_get_time_calendar(&time_stat);
        curr_min = time_stat.tm_min;
        curr_hr  = time_stat.tm_hour; 

        display_blank(&display, powman_hw->read_time_lower & 0b10000000 ? true : false);
        gpio_put(BUZ , !(powman_hw->read_time_lower & 0b1010000000));

        curr_btn = gpio_get(ROTBN);
        if (!curr_btn & last_btn) {
            time_zero = powman_timer_get_ms();
            display_blank(&display, false);
            gpio_put(BUZ , false);
            sleep_ms(50); // sleep for debounce
            // wait for button to be deasserted
            while (!gpio_get(ROTBN)) {
                display_blank(&display, powman_hw->read_time_lower & 0b10000000 ? true : false);
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
            display_chars_all(
                &display,
                curr_min%10 + 0x30,
                curr_min/10 + 0x30,
                curr_hr%10  + 0x30,
                curr_hr/10  + 0x30
            );
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
    display_word(&display, "ALRM");
    gpio_put(BUZ, true);
    sleep_ms(100);
    gpio_put(BUZ, false);
    sleep_ms(100);
    gpio_put(BUZ, true);
    sleep_ms(100);
    gpio_put(BUZ, false);
    sleep_ms(400);
    display_word(&display, "OFF ");
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
