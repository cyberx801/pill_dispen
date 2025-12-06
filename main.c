/*

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"


#define I2C_PORT i2c1
#define I2C_SDA 5
#define I2C_SCL 4


#define EEPROM_ADDR 0x50
#define EEPROM_SIZE 32768


#define LED0 20
#define LED1 21
#define LED2 22


#define SW0 9
#define SW1 8
#define SW2 7


typedef struct ledstate {
    uint8_t state;
    uint8_t not_state;
} ledstate;

uint64_t start_time;


void set_led_state(ledstate *ls, uint8_t value) {
    ls->state = value;
    ls->not_state = ~value;
}


bool led_state_is_valid(ledstate *ls) {
    return ls->state == (uint8_t)~ls->not_state;
}


void eeprom_write_byte(uint16_t addr, uint8_t data) {
    uint8_t buffer[3];
    int result;
    buffer[0] = addr >> 8;
    buffer[1] = addr & 0xFF;
    buffer[2] = data;

    result = i2c_write_blocking(I2C_PORT, EEPROM_ADDR, buffer, 3, false);
    sleep_ms(10);
    printf("  Write byte addr=%u, data=0x%02X, result=%d\n", addr, data, result);

}


uint8_t eeprom_read_byte(uint16_t addr) {
    uint8_t buffer[2];
         uint8_t data;
    int result;

         buffer[0] = addr >> 8;
    buffer[1] = addr & 0xFF;

         result= i2c_write_blocking(I2C_PORT, EEPROM_ADDR, buffer, 2, true);
   result = i2c_read_blocking(I2C_PORT, EEPROM_ADDR, &data, 1, false);

    return data;
}


void eeprom_write_bytes(uint16_t addr, uint8_t *data, int length) {
             int i;
    for (i = 0; i < length; i++) {
        eeprom_write_byte(addr + i, data[i]);
    }
}
    void eeprom_read_bytes(uint16_t addr, uint8_t *data, int length) {
    int i;
    for (i = 0; i < length; i++) {
        data[i] = eeprom_read_byte(addr + i);
    }
}

void save_state(uint8_t state) {
    ledstate ls;
             uint16_t addr;
    ledstate verify;

     set_led_state(&ls, state);


    addr = EEPROM_SIZE - sizeof(ledstate);
    printf("Saving state to EEPROM: state=0x%02X, not_state=0x%02X, addr=%u\n",
           ls.state, ls.not_state, addr);
        eeprom_write_bytes(addr, (uint8_t*)&ls, sizeof(ledstate));
    sleep_ms(10);
    eeprom_read_bytes(addr, (uint8_t*)&verify, sizeof(ledstate));

    printf("Verification read: state=0x%02X, not_state=0x%02X\n",
           verify.state, verify.not_state);

    if (verify.state == ls.state && verify.not_state == ls.not_state) {
        printf("State saved and verified successfully!\n");
    } else {
        printf("WARNING: Verification FAILED!\n");
    }}

uint8_t load_state() {
    ledstate ls;
    uint16_t addr;

        addr = EEPROM_SIZE - sizeof(ledstate);
        printf("Loading state from EEPROM, addr=%u\n", addr);

            eeprom_read_bytes(addr, (uint8_t*)&ls, sizeof(ledstate));
    printf("Read from EEPROM: state=0x%02X, not_state=0x%02X\n", ls.state, ls.not_state);

    if (led_state_is_valid(&ls)) {
        return ls.state;
    } else {

        return 0x02;
    }
}


void print_state(uint8_t state) {
    uint64_t current_time = time_us_64();
             double seconds = (current_time - start_time) / 1000000.0;

    printf("%.2f LED0=%d LED1=%d LED2=%d\n",
           seconds,
           (state & 0x01) ? 1 : 0,
           (state & 0x02) ? 1 : 0,
           (state & 0x04) ? 1 : 0);
}


void set_leds(uint8_t state) {
         gpio_put(LED0, state & 0x01);
    gpio_put(LED1, state & 0x02);
    gpio_put(LED2, state & 0x04);
}

int main() {
    uint8_t current_state;
    bool button0_was_pressed = false;
         bool button1_was_pressed = false;
    bool button2_was_pressed = false;
    bool button0_now, button1_now, button2_now;

    stdio_init_all();
             sleep_ms(2000);

    start_time = time_us_64();


    i2c_init(I2C_PORT, 100000);
         gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);


    gpio_init(LED0);
    gpio_set_dir(LED0, GPIO_OUT);
    gpio_init(LED1);
         gpio_set_dir(LED1, GPIO_OUT);
    gpio_init(LED2);
    gpio_set_dir(LED2, GPIO_OUT);


    gpio_init(SW0);
    gpio_set_dir(SW0, GPIO_IN);
    gpio_pull_up(SW0);
          gpio_init(SW1);
    gpio_set_dir(SW1, GPIO_IN);
    gpio_pull_up(SW1);
    gpio_init(SW2);
    gpio_set_dir(SW2, GPIO_IN);
    gpio_pull_up(SW2);


    current_state = load_state();


    set_leds(current_state);
    print_state(current_state);
    printf("\nProgram started. Press buttons to toggle LEDs.\n");
    printf("Current LED state will be saved to EEPROM.\n\n");

    while (1) {

        button0_now = !gpio_get(SW0);
        button1_now = !gpio_get(SW1);
        button2_now = !gpio_get(SW2);


        if (button0_now == true && button0_was_pressed == false) {
            current_state = current_state ^ 0x01;
            set_leds(current_state);
            print_state(current_state);
            save_state(current_state);
        }
        button0_was_pressed = button0_now;


        if (button1_now == true && button1_was_pressed == false) {
            current_state = current_state ^ 0x02;
            set_leds(current_state);
            print_state(current_state);
            save_state(current_state);
        }
        button1_was_pressed = button1_now;


        if (button2_now == true && button2_was_pressed == false) {
            current_state = current_state ^ 0x04;
            set_leds(current_state);
            print_state(current_state);
            save_state(current_state);
        }
        button2_was_pressed = button2_now;

        sleep_ms(10);
    }

    return 0;
}
*/
/*
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
/*void dispense_pill_routine(uint16_t steps_total, uint8_t *pills, uint8_t *days) {
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
}*/
// ===========================================================================
// DISPENSING LOGIC - DÜZELTILMIŞ VERSIYON
// ===========================================================================
/*void dispense_pill_routine(uint16_t steps_total, uint8_t *pills, uint8_t *days) {
    char msg_buf[64];
    uint8_t state_pack[4];
    int steps_needed = steps_total / 8; // 8 compartments per rev

    // Set Busy Flag in EEPROM
    uint8_t busy = 1;
    state_pack[0] = (uint8_t)(MEM_ADDR_IS_BUSY >> 8);
    state_pack[1] = (uint8_t)MEM_ADDR_IS_BUSY;
    state_pack[2] = busy;
    eepromWrite(state_pack, 3);

    // İnterrupt flag'i temizle (motor hareketi öncesi)
    irq_pill_dropped = false;

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

                // *** DÜZELTME 1: Hap düşmesi için daha uzun bekleme ***
                printf("Waiting for pill drop...\n");
                sleep_ms(500); // 200ms yerine 500ms - ayarlanabilir

                // *** DÜZELTME 2: Flag'i AYRI bir değişkende sakla ***
                bool pill_detected = irq_pill_dropped;
                irq_pill_dropped = false; // Sonraki dispense için temizle

                // Pill sayacını güncelle
                if (pill_detected) {
                    (*pills)++;
                    printf("✓ Pill detected!\n");
                } else {
                    printf("✗ No pill detected!\n");
                }

                // Save Counters
                state_pack[0] = (uint8_t)(MEM_ADDR_DAY_COUNT >> 8);
                state_pack[1] = (uint8_t)MEM_ADDR_DAY_COUNT;
                state_pack[2] = *days;
                state_pack[3] = *pills;
                eepromWrite(state_pack, 4);

                // *** DÜZELTME 3: Kaydedilmiş değişkeni kullan ***
                if (pill_detected) {
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

// ===========================================================================
// INTERRUPT HANDLER - EK KONTROLLER
// ===========================================================================
void piezo_handler(uint gpio, uint32_t events) {
    if (gpio == SENSOR_PIEZO) {
        // Opsiyonel: Debounce kontrolü eklenebilir
        irq_pill_dropped = true;
        printf("[IRQ] Pill drop detected!\n"); // Debug için
    }
}

// ===========================================================================
// EK ÖNERILER - Constants'ları ayarlayın:
// ===========================================================================
// Kodun başında bu değerleri değiştirin:

#define PIEZO_WAIT_MS 500    // 200'den 500'e çıkarın
// veya daha da uzun test edin:
// #define PIEZO_WAIT_MS 1000   // 1 saniye

// Eğer hala sorun olursa, piezo sensörünü test edin:
void test_piezo_sensor(void) {
    printf("Testing Piezo Sensor - Drop a pill now...\n");
    irq_pill_dropped = false;

    sleep_ms(3000); // 3 saniye bekle

    if (irq_pill_dropped) {
        printf("✓ Piezo sensor working!\n");
        led_signal(3, 200); // 3 kez yanıp sönsün
    } else {
        printf("✗ Piezo sensor NOT working!\n");
        led_signal(10, 100); // 10 kez hızlı yanıp sönsün
    }
}

// Ana döngüde kalibrasyondan sonra bu testi ekleyin:
// test_piezo_sensor();
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
/*void piezo_handler(uint gpio, uint32_t events) {
    if (gpio == SENSOR_PIEZO) {
        irq_pill_dropped = true;
    }*/
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/irq.h"

// Custom Libraries
#include "eeprom.h"
#include "lora.h"

// --- Hardware Pin Configurations ---
#define MOTOR_PIN_1     13
#define MOTOR_PIN_2     6
#define MOTOR_PIN_3     3
#define MOTOR_PIN_4     2

#define BUTTON_CALIB    7
#define BUTTON_START    9
#define STATUS_LED      20

#define SENSOR_PIEZO    27
#define SENSOR_OPTO     28

#define I2C_PORT        i2c0
#define I2C_PIN_SDA     16
#define I2C_PIN_SCL     17

// --- Settings & Constants ---
#define MOTOR_SPEED_MS      2
#define DEBOUNCE_TIME       30
#define DISPENSE_PERIOD_US  30000000
#define PIEZO_WAIT_MS       500
#define PIEZO_DEBOUNCE_MS   200

#define TOTAL_COMPARTMENTS  8
#define CALIB_COMPARTMENTS  1
#define PILL_COMPARTMENTS   7

// --- EEPROM Memory Map (2KB EEPROM) ---
#define MEM_ADDR_PILL_COUNT     1950
#define MEM_ADDR_DAY_COUNT      1951
#define MEM_ADDR_IS_BUSY        1952
#define MEM_ADDR_CALIB_STEPS    1953
#define MEM_ADDR_CALIB_ADJ      1955

// --- Global Variables ---
volatile bool irq_pill_dropped = false;
volatile uint64_t last_piezo_time = 0;
static uint8_t current_step_idx = 0;

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
void calibrate_system(uint16_t *steps_per_rev, uint8_t *adj_val);
void dispense_pill_routine(uint16_t steps_total, uint8_t *pills, uint8_t *days);
void emergency_recovery(uint8_t day_idx, uint16_t total_steps, int adj);
void piezo_handler(uint gpio, uint32_t events);

void save_state_byte(uint16_t addr, uint8_t value);
uint8_t read_state_byte(uint16_t addr);
void save_state_word(uint16_t addr, uint16_t value);
uint16_t read_state_word(uint16_t addr);

// ===========================================================================
// MAIN
// ===========================================================================
int main() {
    system_init();

    uint16_t calibration_steps = 0;
    uint8_t calibration_adj = 0;
    bool dispensing_mode = false;

    uint8_t day_counter = 0;
    uint8_t pill_counter = 0;
    uint8_t busy_flag = 0;

    printf("\n=== Starting Pill Dispenser ===\n");

   /* while (!loraInit()) {
        printf("LoRa Join Failed, Retrying...\n");
        sleep_ms(2000);
    }*/

    readEELog();

    char boot_msg[64];
    uint64_t boot_time = time_us_64() / 1000000;
    snprintf(boot_msg, 64, "[%llu] System Booted", boot_time);
    writeEELog(boot_msg);
    loraMsg(boot_msg, strlen(boot_msg));

    sleep_ms(2000);  // Wait 2 seconds before next LoRa message

    gpio_set_irq_enabled_with_callback(SENSOR_PIEZO, GPIO_IRQ_EDGE_FALL, true, &piezo_handler);

    // Read EEPROM
    busy_flag = read_state_byte(MEM_ADDR_IS_BUSY);
    day_counter = read_state_byte(MEM_ADDR_DAY_COUNT);
    pill_counter = read_state_byte(MEM_ADDR_PILL_COUNT);
    calibration_steps = read_state_word(MEM_ADDR_CALIB_STEPS);
    calibration_adj = read_state_byte(MEM_ADDR_CALIB_ADJ);

    if (busy_flag == 0xFF || day_counter == 0xFF || calibration_steps == 0xFFFF) {
        printf("EEPROM uninitialized, setting defaults...\n");
        busy_flag = 0;
        day_counter = 0;
        pill_counter = 0;
        calibration_steps = 0;
        calibration_adj = 0;
    }

    printf("Loaded from EEPROM: Day=%d, Pills=%d, Busy=%d, Steps=%d\n",
           day_counter, pill_counter, busy_flag, calibration_steps);

    // Recovery logic
    if (busy_flag == 1 || (day_counter > 0 && day_counter < PILL_COMPARTMENTS)) {
        printf("\n*** Power loss detected! ***\n");

        char recovery_msg[64];
        uint64_t recovery_time = time_us_64() / 1000000;
        snprintf(recovery_msg, 64, "[%llu] Power Loss Detected", recovery_time);
        writeEELog(recovery_msg);

        sleep_ms(2000);  // Wait before sending LoRa (rate limit)
        loraMsg(recovery_msg, strlen(recovery_msg));
        sleep_ms(2000);  // Wait after sending

        if (busy_flag == 1) {
            emergency_recovery(day_counter, calibration_steps, calibration_adj);
        }

        sleep_ms(1000);

        int remaining = PILL_COMPARTMENTS - day_counter;
        printf("Resuming %d remaining doses...\n", remaining);

        for (int k = 0; k < remaining; k++) {
            uint64_t t_start = time_us_64();
            dispense_pill_routine(calibration_steps, &pill_counter, &day_counter);
            if (k < remaining - 1) {
                while ((time_us_64() - t_start) < DISPENSE_PERIOD_US) {
                    sleep_ms(100);
                }
            }
        }

        char finish_msg[64];
        uint64_t finish_time = time_us_64() / 1000000;
        snprintf(finish_msg, 64, "[%llu] Recovery Complete", finish_time);
        writeEELog(finish_msg);

        sleep_ms(2000);  // Wait before sending
        loraMsg(finish_msg, strlen(finish_msg));
        sleep_ms(2000);  // Wait after sending

        char empty_msg[64];
        snprintf(empty_msg, 64, "[%llu] Dispenser Empty", finish_time);
        loraMsg(empty_msg, strlen(empty_msg));
        sleep_ms(2000);
    }

    // Main loop
    while (true) {
        day_counter = 0;
        pill_counter = 0;
        irq_pill_dropped = false;
        dispensing_mode = false;

        save_state_byte(MEM_ADDR_DAY_COUNT, 0);
        save_state_byte(MEM_ADDR_PILL_COUNT, 0);
        save_state_byte(MEM_ADDR_IS_BUSY, 0);

        printf("\n=== Waiting for Calibration ===\n");
        printf("Press SW2 to calibrate...\n");

        while (gpio_get(BUTTON_CALIB)) {
            gpio_put(STATUS_LED, 1); sleep_ms(100);
            gpio_put(STATUS_LED, 0); sleep_ms(100);
        }

        sleep_ms(DEBOUNCE_TIME);
        if (!gpio_get(BUTTON_CALIB)) {
            gpio_put(STATUS_LED, 1);
            calibrate_system(&calibration_steps, &calibration_adj);
            while (!gpio_get(BUTTON_CALIB));
            sleep_ms(DEBOUNCE_TIME);

            printf("\n=== Calibration Complete ===\n");
            printf("Press SW0 to start dispensing...\n");

            while (!dispensing_mode) {
                if (!gpio_get(BUTTON_START)) {
                    sleep_ms(DEBOUNCE_TIME);
                    if (!gpio_get(BUTTON_START)) {
                        gpio_put(STATUS_LED, 0);
                        dispensing_mode = true;

                        printf("\n=== Starting Dispense Cycle ===\n");

                        for (int i = 0; i < PILL_COMPARTMENTS; i++) {
                            uint64_t t_start = time_us_64();

                            printf("\n--- Dispensing Day %d ---\n", i + 1);
                            dispense_pill_routine(calibration_steps, &pill_counter, &day_counter);

                            if (i == (PILL_COMPARTMENTS - 1)) {
                                printf("\n=== All Pills Dispensed ===\n");

                                char empty_msg[64];
                                uint64_t empty_time = time_us_64() / 1000000;
                                snprintf(empty_msg, 64, "[%llu] Dispenser Empty", empty_time);
                                writeEELog(empty_msg);
                                loraMsg(empty_msg, strlen(empty_msg));
                            } else {
                                printf("Waiting 30 seconds...\n");
                                while ((time_us_64() - t_start) < DISPENSE_PERIOD_US) {
                                    sleep_ms(100);
                                }
                            }
                        }
                        while (!gpio_get(BUTTON_START));
                        sleep_ms(DEBOUNCE_TIME);
                    }
                }
            }
        }
    }
    return 0;
}

// ===========================================================================
// INIT
// ===========================================================================
void system_init(void) {
    stdio_init_all();
    sleep_ms(2000);

    i2c_init(I2C_PORT, 100 * 1000);
    gpio_set_function(I2C_PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_PIN_SDA);
    gpio_pull_up(I2C_PIN_SCL);

    for(int i=0; i<4; i++) {
        gpio_init(STEPPER_PINS[i]);
        gpio_set_dir(STEPPER_PINS[i], GPIO_OUT);
    }

    const uint inputs[] = {BUTTON_CALIB, BUTTON_START, SENSOR_OPTO, SENSOR_PIEZO};
    for(int i=0; i<4; i++) {
        gpio_init(inputs[i]);
        gpio_set_dir(inputs[i], GPIO_IN);
        gpio_pull_up(inputs[i]);
    }

    gpio_init(STATUS_LED);
    gpio_set_dir(STATUS_LED, GPIO_OUT);
}

// ===========================================================================
// EEPROM HELPERS
// ===========================================================================
void save_state_byte(uint16_t addr, uint8_t value) {
    uint8_t buf[3];
    buf[0] = (uint8_t)(addr >> 8);
    buf[1] = (uint8_t)(addr & 0xFF);
    buf[2] = value;
    eepromWrite(buf, 3);
}

uint8_t read_state_byte(uint16_t addr) {
    uint8_t addr_buf[2];
    uint8_t value = 0;
    addr_buf[0] = (uint8_t)(addr >> 8);
    addr_buf[1] = (uint8_t)(addr & 0xFF);
    eepromRead(addr_buf, &value, 1);
    return value;
}

void save_state_word(uint16_t addr, uint16_t value) {
    uint8_t buf[4];
    buf[0] = (uint8_t)(addr >> 8);
    buf[1] = (uint8_t)(addr & 0xFF);
    buf[2] = (uint8_t)(value >> 8);
    buf[3] = (uint8_t)(value & 0xFF);
    eepromWrite(buf, 4);
}

uint16_t read_state_word(uint16_t addr) {
    uint8_t addr_buf[2];
    uint8_t data[2];
    addr_buf[0] = (uint8_t)(addr >> 8);
    addr_buf[1] = (uint8_t)(addr & 0xFF);
    eepromRead(addr_buf, data, 2);
    return (data[0] << 8) | data[1];
}

// ===========================================================================
// MOTOR
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

void calibrate_system(uint16_t *steps_per_rev, uint8_t *adj_val) {
    printf("Starting calibration...\n");
    writeEELog("Calibrating...");

    current_step_idx = 0;
    uint16_t revolutions[3];
    uint16_t total_count = 0;

    // 1. Clear sensor - move until sensor is HIGH
    printf("Clearing sensor...\n");
    int safety_counter = 0;
    while (gpio_get(SENSOR_OPTO) == 0 && safety_counter < 10000) {
        for (int i = current_step_idx; i < 8; i++) {
            step_motor_one_tick(i);
            sleep_ms(MOTOR_SPEED_MS);
            if (gpio_get(SENSOR_OPTO) == 1) {
                current_step_idx = (i == 7) ? 0 : i + 1;
                break;
            }
        }
        current_step_idx = 0;
        safety_counter++;
    }

    // 2. Find falling edge - move until sensor goes LOW
    printf("Finding falling edge...\n");
    safety_counter = 0;
    while (gpio_get(SENSOR_OPTO) == 1 && safety_counter < 10000) {
        for (int i = current_step_idx; i < 8; i++) {
            step_motor_one_tick(i);
            sleep_ms(MOTOR_SPEED_MS);
            if (gpio_get(SENSOR_OPTO) == 0) {
                current_step_idx = (i == 7) ? 0 : i + 1;
                break;
            }
        }
        current_step_idx = 0;
        safety_counter++;
    }

    printf("Aligned at falling edge\n");

    // 3. Measure 3 complete revolutions (falling edge to falling edge)
    for (int rev = 0; rev < 3; rev++) {
        printf("Measuring revolution %d...\n", rev + 1);

        int count = 0;
        bool prev_state = gpio_get(SENSOR_OPTO);  // Should be 0 (LOW)

        // Move until we see falling edge again
        while (count < 10000) {
            for (int i = current_step_idx; i < 8; i++) {
                step_motor_one_tick(i);
                sleep_ms(MOTOR_SPEED_MS);
                count++;

                bool current_state = gpio_get(SENSOR_OPTO);

                // Detect falling edge (HIGH to LOW)
                if (prev_state == 1 && current_state == 0) {
                    current_step_idx = (i == 7) ? 0 : i + 1;
                    revolutions[rev] = count;
                    total_count += count;
                    printf("Revolution %d: %d steps\n", rev + 1, count);
                    goto next_revolution;  // Exit nested loops
                }

                prev_state = current_state;
            }
            current_step_idx = 0;
        }

        next_revolution:
        continue;
    }

    // 4. Calculate average
    *steps_per_rev = total_count / 3;

    // 5. Calculate alignment - measure the LOW segment (hole size)
    printf("Measuring hole size for alignment...\n");
    int hole_size = 0;
    bool prev_state = gpio_get(SENSOR_OPTO);  // Should be 0 (LOW)

    // Count steps while sensor is LOW
    while (hole_size < 1000) {
        for (int i = current_step_idx; i < 8; i++) {
            step_motor_one_tick(i);
            sleep_ms(MOTOR_SPEED_MS);
            hole_size++;

            bool current_state = gpio_get(SENSOR_OPTO);

            if (current_state == 1) {  // Sensor went HIGH = hole ended
                current_step_idx = (i == 7) ? 0 : i + 1;
                printf("Hole size: %d steps\n", hole_size);
                goto hole_measured;
            }
        }
        current_step_idx = 0;
    }

    hole_measured:

    // Move back to center of hole
    *adj_val = hole_size / 2;

    printf("Calibration complete: %d steps per revolution\n", *steps_per_rev);
    printf("Revolution 1: %d steps\n", revolutions[0]);
    printf("Revolution 2: %d steps\n", revolutions[1]);
    printf("Revolution 3: %d steps\n", revolutions[2]);
    printf("Hole size: %d steps\n", hole_size);
    printf("Alignment: %d steps (center of hole)\n", *adj_val);

    save_state_word(MEM_ADDR_CALIB_STEPS, *steps_per_rev);
    save_state_byte(MEM_ADDR_CALIB_ADJ, *adj_val);

    adjust_wheel_position(*adj_val);

    char calib_msg[64];
    uint64_t calib_time = time_us_64() / 1000000;
    snprintf(calib_msg, 64, "[%llu] Calibration OK", calib_time);
    loraMsg(calib_msg, strlen(calib_msg));
}

// ===========================================================================
// DISPENSE
// ===========================================================================
void dispense_pill_routine(uint16_t steps_total, uint8_t *pills, uint8_t *days) {
    char msg_buf[64];
    char timestamp[32];
    int steps_needed = steps_total / TOTAL_COMPARTMENTS;

    save_state_byte(MEM_ADDR_IS_BUSY, 1);
    irq_pill_dropped = false;

    printf("Moving %d steps...\n", steps_needed);

    // ESKİ MOTOR MANTIĞI - NESTED FOR LOOP
    while (steps_needed > 0) {
        for (int i = current_step_idx; i < 8; i++) {
            step_motor_one_tick(i);
            sleep_ms(MOTOR_SPEED_MS);
            steps_needed--;

            if (steps_needed == 0) {
                current_step_idx = (i == 7) ? 0 : i + 1;

                save_state_byte(MEM_ADDR_IS_BUSY, 0);
                (*days)++;

                printf("Waiting for pill...\n");
                sleep_ms(PIEZO_WAIT_MS);

                bool pill_detected = irq_pill_dropped;
                irq_pill_dropped = false;

                if (pill_detected) {
                    (*pills)++;
                    printf("✓ Pill detected!\n");
                } else {
                    printf("✗ No pill detected!\n");
                }

                save_state_byte(MEM_ADDR_DAY_COUNT, *days);
                save_state_byte(MEM_ADDR_PILL_COUNT, *pills);

                // Create timestamp
                uint64_t time_sec = time_us_64() / 1000000;
                snprintf(timestamp, 32, "[%llu] ", time_sec);

                if (pill_detected) {
                    snprintf(msg_buf, 64, "%sDay %d: Pill OK", timestamp, *days);
                } else {
                    snprintf(msg_buf, 64, "%sDay %d: No Pill", timestamp, *days);
                    led_signal(5, 300);
                }

                writeEELog(msg_buf);
                loraMsg(msg_buf, strlen(msg_buf));
                break;
            }
        }
        current_step_idx = 0;
    }
}

void emergency_recovery(uint8_t day_idx, uint16_t total_steps, int adj) {
    printf("Emergency recovery...\n");

    bool state = gpio_get(SENSOR_OPTO);
    if (state) {
        while (gpio_get(SENSOR_OPTO) == state) {
            for (int i = 7; i >= 0; i--) {
                step_motor_one_tick(i);
                sleep_ms(MOTOR_SPEED_MS);

                if (gpio_get(SENSOR_OPTO) != state) {
                    current_step_idx = (i == 7) ? 0 : i + 1;
                    break;
                }
            }
        }

        adjust_wheel_position(adj);

        int restore_steps = (total_steps / TOTAL_COMPARTMENTS) * day_idx;
        printf("Restoring to day %d (%d steps)\n", day_idx, restore_steps);

        while (restore_steps > 0) {
             for (int i = current_step_idx; i < 8; i++) {
                 step_motor_one_tick(i);
                 sleep_ms(MOTOR_SPEED_MS);
                 restore_steps--;

                 if (restore_steps == 0) {
                     current_step_idx = (i == 7) ? 0 : i + 1;
                     break;
                 }
             }
             current_step_idx = 0;
        }
    }
    save_state_byte(MEM_ADDR_IS_BUSY, 0);
}

// ===========================================================================
// INTERRUPT
// ===========================================================================
void piezo_handler(uint gpio, uint32_t events) {
    if (gpio == SENSOR_PIEZO) {
        uint64_t now = time_us_64();
        if ((now - last_piezo_time) > (PIEZO_DEBOUNCE_MS * 1000)) {
            irq_pill_dropped = true;
            last_piezo_time = now;
            printf("[IRQ] Pill detected!\n");
        }
    }
}