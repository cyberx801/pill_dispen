#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/irq.h"

// Custom Libraries
#include "eeprom.h"
#include "lora.h"

// --- Hardware Pin Configurations ---
// Motor Pins
#define MOTOR_PIN_1     13
#define MOTOR_PIN_2     6
#define MOTOR_PIN_3     3
#define MOTOR_PIN_4     2

// Buttons & LED
#define BUTTON_CALIB    7   // SW2 (Calibration)
#define BUTTON_START    9   // SW0 (Start
#define STATUS_LED      20

// Sensors
#define SENSOR_PIEZO    27
#define SENSOR_OPTO     28

// I2C Pins (EEPROM)
#define I2C_PORT        i2c0
#define I2C_PIN_SDA     16
#define I2C_PIN_SCL     17

// --- Settings & Constants ---
#define MOTOR_SPEED_MS      2        // Delay between steps
#define DEBOUNCE_TIME       30       // Button debounce in ms
#define DISPENSE_PERIOD_US  30000000 // 30 Seconds in microseconds
#define PIEZO_WAIT_MS       200      // Wait time for pill drop detection

// --- EEPROM Memory Map ---
// Storing states at the end of memory to avoid log overwrite
#define MEM_ADDR_PILL_COUNT     0x7FFE
#define MEM_ADDR_DAY_COUNT      0x7FFC
#define MEM_ADDR_IS_BUSY        0x7FFA // 1 if motor is moving
#define MEM_ADDR_CALIB_VAL      0x7FF0 // Calibration steps

// --- Global Variables ---
volatile bool irq_pill_dropped = false; // Flag set by interrupt
static uint8_t current_step_idx = 0;    // Motor sequence index

// Half-Step Sequence
const uint8_t STEPPER_PINS[4] = {MOTOR_PIN_1, MOTOR_PIN_2, MOTOR_PIN_3, MOTOR_PIN_4};
const uint8_t SEQ_MAP[8][4] = {
    {1,0,0,0}, {1,1,0,0}, {0,1,0,0}, {0,1,1,0},
    {0,0,1,0}, {0,0,1,1}, {0,0,0,1}, {1,0,0,1}
};

// --- Function Prototypes ---
void system_init(void);
void led_signal(int count, int delay_ms);
void step_motor_one_tick(int step_index);
uint16_t find_sensor_edge(bool expected_state);
void adjust_wheel_position(int adjustment);
void calibrate_system(uint16_t *steps_per_rev);
void dispense_pill_routine(uint16_t steps_total, uint8_t *pills, uint8_t *days);
void emergency_recovery(uint8_t day_idx, uint16_t total_steps, int adj);
void piezo_handler(uint gpio, uint32_t events);

// ===========================================================================
// MAIN APPLICATION
// ===========================================================================
int main() {
    // 1. Hardware Initialization
    system_init();

    uint16_t calibration_steps = 0;
    bool dispensing_mode = false;

    // State Tracking Variables
    uint8_t day_counter = 0;
    uint8_t pill_counter = 0;
    uint8_t busy_flag = 0;

    // Buffers
    uint8_t mem_buf[5];
    uint8_t addr_ptr[2];

    // 2. Connect to LoRaWAN (Retry loop)
    while (!loraInit()) {
        printf("LoRa Join Failed, Retrying...\n");
        sleep_ms(1000);
    }

    // 3. Boot Log
    readEELog(); // Print old logs to terminal
    writeEELog("System Booted");
    loraMsg("System Booted", 13);

    // 4. Enable Piezo Interrupt
    gpio_set_irq_enabled_with_callback(SENSOR_PIEZO, GPIO_IRQ_EDGE_FALL, true, &piezo_handler);

    // 5. Check Power Loss / Recovery Status from EEPROM

    // Read Busy Flag
    addr_ptr[0] = (uint8_t)(MEM_ADDR_IS_BUSY >> 8); addr_ptr[1] = (uint8_t)MEM_ADDR_IS_BUSY;
    eepromRead(addr_ptr, &busy_flag, 1);

    // Read Day Count
    addr_ptr[0] = (uint8_t)(MEM_ADDR_DAY_COUNT >> 8); addr_ptr[1] = (uint8_t)MEM_ADDR_DAY_COUNT;
    eepromRead(addr_ptr, &day_counter, 1);

    // Read Pill Count
    addr_ptr[0] = (uint8_t)(MEM_ADDR_PILL_COUNT >> 8); addr_ptr[1] = (uint8_t)MEM_ADDR_PILL_COUNT;
    eepromRead(addr_ptr, &pill_counter, 1);

    // Read Calibration Value
    addr_ptr[0] = (uint8_t)(MEM_ADDR_CALIB_VAL >> 8); addr_ptr[1] = (uint8_t)MEM_ADDR_CALIB_VAL;
    eepromRead(addr_ptr, mem_buf, 3);
    calibration_steps = (mem_buf[0] << 8) | mem_buf[1];
    int saved_adj = mem_buf[2];

    // --- RECOVERY LOGIC ---
    // If device was busy OR day count is between 1-6, it means power was cut during operation
    if (busy_flag == 1 || (day_counter > 0 && day_counter < 7)) {
        printf("Power loss detected!\n");
        writeEELog("Power Fail Recovery");
        loraMsg("Power Fail Recovery", 19);

        if (busy_flag == 1) {
            // If motor was moving, realign it
            emergency_recovery(day_counter, calibration_steps, saved_adj);
        }

        sleep_ms(1000);

        // Resume remaining days
        int remaining = 7 - day_counter;
        for (int k = 0; k < remaining; k++) {
            uint64_t t_start = time_us_64();

            dispense_pill_routine(calibration_steps, &pill_counter, &day_counter);

            if (k < remaining - 1) {
                // Wait 30 seconds unless it is the last pill
                while ((time_us_64() - t_start) < DISPENSE_PERIOD_US);
            }
        }

        // End of recovery cycle
        writeEELog("Recovery Finished");
        loraMsg("Dispenser Empty", 15);
    }

    // 6. Main Infinite Loop
    while (true) {
        // Reset variables for fresh start
        day_counter = 0;
        pill_counter = 0;
        irq_pill_dropped = false;
        dispensing_mode = false;

        // A. Wait for Calibration Button (SW2)
        while (gpio_get(BUTTON_CALIB)) {
            // Blink LED while waiting
            gpio_put(STATUS_LED, 1); sleep_ms(100);
            gpio_put(STATUS_LED, 0); sleep_ms(100);
        }

        sleep_ms(DEBOUNCE_TIME);
        if (!gpio_get(BUTTON_CALIB)) {
            gpio_put(STATUS_LED, 1); // LED ON indicates Calibration

            calibrate_system(&calibration_steps);

            while (!gpio_get(BUTTON_CALIB)); // Wait for release

            // B. Wait for Start Button (SW0)
            while (!dispensing_mode) {
                if (!gpio_get(BUTTON_START)) {
                    sleep_ms(DEBOUNCE_TIME);
                    if (!gpio_get(BUTTON_START)) {
                        gpio_put(STATUS_LED, 0); // LED OFF
                        dispensing_mode = true;

                        // Save Initial State to EEPROM
                        mem_buf[0] = (uint8_t)(MEM_ADDR_DAY_COUNT >> 8);
                        mem_buf[1] = (uint8_t)MEM_ADDR_DAY_COUNT;
                        mem_buf[2] = day_counter;
                        mem_buf[3] = pill_counter;
                        eepromWrite(mem_buf, 4);

                        // C. 7-Day Dispensing Cycle
                        for (int i = 0; i < 7; i++) {
                            uint64_t t_start = time_us_64();

                            dispense_pill_routine(calibration_steps, &pill_counter, &day_counter);

                            if (i == 6) {
                                // Final pill
                                writeEELog("Dispenser Empty");
                                loraMsg("Dispenser Empty", 15);
                            } else {
                                // Wait 30 Seconds
                                while ((time_us_64() - t_start) < DISPENSE_PERIOD_US);
                            }
                        }
                        while (!gpio_get(BUTTON_START)); // Wait release
                    }
                }
            }
        }
    }
    return 0;
}

// ===========================================================================
// PERIPHERAL INIT
// ===========================================================================
void system_init(void) {
    stdio_init_all();

    // Initialize I2C
    i2c_init(I2C_PORT, 100 * 1000);
    gpio_set_function(I2C_PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_PIN_SDA);
    gpio_pull_up(I2C_PIN_SCL);

    // Initialize Motor Pins
    for(int i=0; i<4; i++) {
        gpio_init(STEPPER_PINS[i]);
        gpio_set_dir(STEPPER_PINS[i], GPIO_OUT);
    }

    // Initialize Inputs
    const uint inputs[] = {BUTTON_CALIB, BUTTON_START, SENSOR_OPTO, SENSOR_PIEZO};
    for(int i=0; i<4; i++) {
        gpio_init(inputs[i]);
        gpio_set_dir(inputs[i], GPIO_IN);
        gpio_pull_up(inputs[i]);
    }

    // Initialize LED
    gpio_init(STATUS_LED);
    gpio_set_dir(STATUS_LED, GPIO_OUT);
}

// ===========================================================================
// MOTOR FUNCTIONS
// ===========================================================================
void step_motor_one_tick(int step_index) {
    for (int j = 0; j < 4; j++) {
        gpio_put(STEPPER_PINS[j], SEQ_MAP[step_index][j]);
    }
}

void led_signal(int count, int delay_ms) {
    for(int i=0; i<count; i++) {
        gpio_put(STATUS_LED, 1); sleep_ms(delay_ms);
        gpio_put(STATUS_LED, 0); sleep_ms(delay_ms);
    }
}

// Moves motor until Opto sensor state changes
uint16_t find_sensor_edge(bool expected_state) {
    uint16_t steps = 0;
    while (gpio_get(SENSOR_OPTO) == expected_state) {
        for (int i = current_step_idx; i < 8; i++) {
            step_motor_one_tick(i);
            sleep_ms(MOTOR_SPEED_MS);
            steps++;

            if (gpio_get(SENSOR_OPTO) != expected_state) {
                current_step_idx = (i == 7) ? 0 : i + 1;
                return steps;
            }
        }
        current_step_idx = 0;
    }
    return steps;
}

void adjust_wheel_position(int adjustment) {
    printf("Aligning wheel (%d steps)...\n", adjustment);

    // Calculate reverse start index
    uint8_t rev_idx = (current_step_idx < 2) ? (6 + current_step_idx) : (current_step_idx - 2);

    while (adjustment > 0) {
        for (int i = rev_idx; i >= 0; i--) {
            step_motor_one_tick(i);
            sleep_ms(MOTOR_SPEED_MS);
            adjustment--;

            if (adjustment == 0) {
                current_step_idx = (i == 7) ? 0 : i + 1;
                break;
            }
            if (i == 0) rev_idx = 7;
        }
    }
    writeEELog("Calibration Done");
}

void calibrate_system(uint16_t *steps_per_rev) {
    writeEELog("Calibrating...");

    current_step_idx = 0;
    uint16_t total_count = 0;

    // 1. Find Sensor Edge
    bool state = gpio_get(SENSOR_OPTO);
    if (state) {
        find_sensor_edge(state);
        state = gpio_get(SENSOR_OPTO);
    }
    find_sensor_edge(state); // Align

    // 2. Measure full revolutions
    int segment;
    for (int r = 0; r < 2; r++) { // 2 Loops
        for (int k = 0; k < 2; k++) { // 2 changes per loop
            state = gpio_get(SENSOR_OPTO);
            segment = find_sensor_edge(state);
            total_count += segment;
        }
    }

    int align_val = segment / 2;
    *steps_per_rev = total_count / 2; // Average

    // Save to EEPROM
    uint8_t ee_pack[5];
    ee_pack[0] = (uint8_t)(MEM_ADDR_CALIB_VAL >> 8);
    ee_pack[1] = (uint8_t)MEM_ADDR_CALIB_VAL;
    ee_pack[2] = (uint8_t)(*steps_per_rev >> 8);
    ee_pack[3] = (uint8_t)(*steps_per_rev);
    ee_pack[4] = align_val;
    eepromWrite(ee_pack, 5);

    // Align Position
    adjust_wheel_position(align_val);
}

// ===========================================================================
// DISPENSING LOGIC
// ===========================================================================
void dispense_pill_routine(uint16_t steps_total, uint8_t *pills, uint8_t *days) {
    char msg_buf[64];
    uint8_t state_pack[4];
    int steps_needed = steps_total / 8; // 8 compartments per rev

    // Set Busy Flag in EEPROM
    uint8_t busy = 1;
    state_pack[0] = (uint8_t)(MEM_ADDR_IS_BUSY >> 8);
    state_pack[1] = (uint8_t)MEM_ADDR_IS_BUSY;
    state_pack[2] = busy;
    eepromWrite(state_pack, 3);

    printf("Dispensing... Moving %d steps\n", steps_needed);

    // Turn Motor
    while (steps_needed > 0) {
        for (int i = current_step_idx; i < 8; i++) {
            step_motor_one_tick(i);
            sleep_ms(MOTOR_SPEED_MS);
            steps_needed--;

            if (steps_needed == 0) {
                current_step_idx = (i == 7) ? 0 : i + 1;

                // Clear Busy Flag
                busy = 0;
                state_pack[0] = (uint8_t)(MEM_ADDR_IS_BUSY >> 8);
                state_pack[1] = (uint8_t)MEM_ADDR_IS_BUSY;
                state_pack[2] = busy;
                eepromWrite(state_pack, 3);

                // Increment Day
                (*days)++;

                // Wait for Pill Drop
                sleep_ms(PIEZO_WAIT_MS);

                if (irq_pill_dropped) {
                    (*pills)++;
                    irq_pill_dropped = false;
                }

                // Save Counters
                state_pack[0] = (uint8_t)(MEM_ADDR_DAY_COUNT >> 8);
                state_pack[1] = (uint8_t)MEM_ADDR_DAY_COUNT;
                state_pack[2] = *days;
                state_pack[3] = *pills;
                eepromWrite(state_pack, 4);

                // Report via LoRa
                if (irq_pill_dropped) {
                    snprintf(msg_buf, 64, "Day %d: Pill Dispensed", *days);
                } else {
                    snprintf(msg_buf, 64, "Day %d: ERROR No Pill", *days);
                    // Blink Error Code (5 times)
                    led_signal(5, 300);
                }

                writeEELog(msg_buf);
                loraMsg(msg_buf, strlen(msg_buf));
                break;
            }
        }
        if (current_step_idx > 7) current_step_idx = 0;
    }
}

void emergency_recovery(uint8_t day_idx, uint16_t total_steps, int adj) {
    printf("Recovery Mode Active...\n");

    // 1. Reverse to find known position (Opto)
    bool state = gpio_get(SENSOR_OPTO);
    if (state) {
        while (gpio_get(SENSOR_OPTO) == state) {
            // Reverse rotation
            for (int i = 7; i >= 0; i--) {
                step_motor_one_tick(i);
                sleep_ms(MOTOR_SPEED_MS);

                if (gpio_get(SENSOR_OPTO) != state) {
                    current_step_idx = (i == 7) ? 0 : i + 1;
                    break;
                }
            }
        }
        // 2. Re-Align
        adjust_wheel_position(adj);

        // 3. Fast Forward to last known day
        // Steps = (StepsPerRev / 8) * CurrentDay
        int restore_steps = (total_steps / 8) * day_idx;
        printf("Skipping %d steps to restore Day %d\n", restore_steps, day_idx);

        while (restore_steps > 0) {
             for (int i = current_step_idx; i < 8; i++) {
                 step_motor_one_tick(i);
                 sleep_ms(MOTOR_SPEED_MS);
                 restore_steps--;

                 if (restore_steps == 0) {
                     current_step_idx = (i == 7) ? 0 : i + 1;
                     sleep_ms(PIEZO_WAIT_MS);
                     break;
                 }
             }
             if (current_step_idx > 7) current_step_idx = 0;
        }
    }
}

// ===========================================================================
// INTERRUPT HANDLER
// ===========================================================================
void piezo_handler(uint gpio, uint32_t events) {
    if (gpio == SENSOR_PIEZO) {
        irq_pill_dropped = true;
    }
}