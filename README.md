# Micro-alarm clock software

This repo contains the software used to run the RP2354-based microclock (see https://github.com/davidb990/micro_clock and https://github.com/davidb990/uclock_pwr_rcode for the HW design).

The software uses the pico-sdk to control the RP2354, and brings in a custom library to control the display (https://github.com/davidb990/dl2416t).

The software is FSM-based, with each state's flow defined in a function that returns the next state.
