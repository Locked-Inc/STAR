/**
 * @file motor-test.c
 * @brief Standalone motor/encoder/IMU test for BeagleBone Blue.
 *
 * Runs without the Pi5 gateway. Tests each motor channel, reads encoders,
 * and prints IMU data. Designed for bench testing with or without motors
 * physically connected.
 *
 * Build (on BBB):
 *   gcc -O2 -o motor-test motor-test.c -lrobotcontrol -lm -lpthread
 *
 * Run:
 *   sudo ./motor-test              # full test (motors + encoders + IMU)
 *   sudo ./motor-test --no-motors  # encoders + IMU only (no motor power)
 *   sudo ./motor-test --motor 1    # test only motor 1
 *
 * Requires: librobotcontrol (patched for 5.10-ti kernel)
 * Power: Motors need barrel jack (9-18V) or 2S LiPo. USB alone = no motors.
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>
#include <robotcontrol.h>

#define NUM_MOTORS    4
#define NUM_ENCODERS  3  /* eQEP 1-3 only on 5.10-ti kernel */
#define TEST_DUTY     0.3
#define RAMP_STEPS    10
#define RAMP_DELAY_US 100000  /* 100ms per step */
#define READ_DELAY_US 500000  /* 500ms encoder/IMU read */
#define TICKS_PER_REV 341.0   /* 341 PPR Hall encoders */

static volatile int g_running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

static void print_separator(void)
{
    printf("--------------------------------------------------\n");
}

static void print_header(const char *title)
{
    printf("\n");
    print_separator();
    printf("  %s\n", title);
    print_separator();
}

/* Read and print encoder ticks for channels 1-3 */
static void read_encoders(void)
{
    printf("  Encoders: ");
    for (int ch = 1; ch <= NUM_ENCODERS; ch++) {
        int ticks = rc_encoder_eqep_read(ch);
        printf("E%d=%6d  ", ch, ticks);
    }
    printf("\n");
}

/* Read and print IMU accel + gyro */
static void read_imu(rc_mpu_data_t *data)
{
    if (rc_mpu_read_accel(data) == 0 && rc_mpu_read_gyro(data) == 0) {
        printf("  IMU accel: X=%+6.2f  Y=%+6.2f  Z=%+6.2f m/s^2\n",
               data->accel[0], data->accel[1], data->accel[2]);
        printf("  IMU gyro:  X=%+6.2f  Y=%+6.2f  Z=%+6.2f deg/s\n",
               data->gyro[0], data->gyro[1], data->gyro[2]);
    } else {
        printf("  IMU: read failed\n");
    }
}

/* Test a single motor: ramp up, hold, read encoder, ramp down */
static void test_motor(int ch, double duty, rc_mpu_data_t *imu)
{
    printf("\n  Motor %d: ramping to %.0f%% duty...\n", ch, duty * 100);

    /* Zero encoder before test */
    if (ch <= NUM_ENCODERS) {
        rc_encoder_eqep_write(ch, 0);
    }

    /* Ramp up */
    for (int i = 1; i <= RAMP_STEPS && g_running; i++) {
        double d = duty * (double)i / RAMP_STEPS;
        rc_motor_set(ch, d);
        usleep(RAMP_DELAY_US);
    }

    if (!g_running) return;

    /* Hold and read encoder */
    printf("  Motor %d: holding at %.0f%% for 2s...\n", ch, duty * 100);
    int ticks_before = (ch <= NUM_ENCODERS) ? rc_encoder_eqep_read(ch) : 0;
    usleep(2000000);  /* 2 seconds */
    int ticks_after = (ch <= NUM_ENCODERS) ? rc_encoder_eqep_read(ch) : 0;

    int delta = ticks_after - ticks_before;
    double rpm = (delta / TICKS_PER_REV) * 30.0;  /* ticks/2s -> rev/min */

    if (ch <= NUM_ENCODERS) {
        printf("  Motor %d: encoder delta=%d ticks, ~%.1f RPM\n", ch, delta, rpm);
    } else {
        printf("  Motor %d: encoder ch4 (PRU) not available on 5.10\n", ch);
    }

    /* Read IMU during motor operation */
    read_imu(imu);

    /* Ramp down */
    printf("  Motor %d: ramping down...\n", ch);
    for (int i = RAMP_STEPS; i >= 0 && g_running; i--) {
        double d = duty * (double)i / RAMP_STEPS;
        rc_motor_set(ch, d);
        usleep(RAMP_DELAY_US);
    }

    rc_motor_set(ch, 0.0);

    if (delta == 0 && ch <= NUM_ENCODERS) {
        printf("  Motor %d: WARNING -- no encoder ticks (motor not connected?)\n", ch);
    } else if (ch <= NUM_ENCODERS) {
        printf("  Motor %d: PASS\n", ch);
    }
}

static void usage(void)
{
    printf("Usage: sudo ./motor-test [OPTIONS]\n\n");
    printf("  --no-motors   Skip motor tests (encoders + IMU only)\n");
    printf("  --motor N     Test only motor N (1-4)\n");
    printf("  --duty D      Set test duty cycle 0.0-1.0 (default 0.3)\n");
    printf("  --help        Show this help\n\n");
    printf("Power: Motors need barrel jack (9-18V) or 2S LiPo.\n");
    printf("       USB power alone cannot drive motors.\n");
}

int main(int argc, char *argv[])
{
    int no_motors = 0;
    int single_motor = 0;
    double duty = TEST_DUTY;

    /* Parse args */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-motors") == 0) {
            no_motors = 1;
        } else if (strcmp(argv[i], "--motor") == 0 && i + 1 < argc) {
            single_motor = atoi(argv[++i]);
            if (single_motor < 1 || single_motor > 4) {
                fprintf(stderr, "Motor must be 1-4\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--duty") == 0 && i + 1 < argc) {
            duty = atof(argv[++i]);
            if (duty < 0.0 || duty > 1.0) {
                fprintf(stderr, "Duty must be 0.0-1.0\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage();
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage();
            return 1;
        }
    }

    /* Signal handling */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (rc_kill_existing_process(2.0) < -2) {
        fprintf(stderr, "Failed to kill existing rc process\n");
        return 1;
    }

    rc_make_pid_file();

    print_header("STAR Motor Test -- BeagleBone Blue");
    printf("  Duty: %.0f%%  Motors: %s\n",
           duty * 100,
           no_motors ? "SKIP" : (single_motor ? "single" : "all"));

    /* Init hardware */
    print_header("Hardware Init");

    printf("  ADC: ");
    if (rc_adc_init() == 0) {
        printf("OK\n");
    } else {
        printf("FAIL (battery monitoring unavailable)\n");
    }

    printf("  Motors: ");
    if (rc_motor_init() == 0) {
        printf("OK\n");
    } else {
        printf("FAIL\n");
        if (!no_motors) {
            fprintf(stderr, "Motor init failed. Check PWM kernel patches.\n");
            return 1;
        }
    }

    printf("  Encoders (eQEP 1-3): ");
    if (rc_encoder_eqep_init() == 0) {
        printf("OK\n");
    } else {
        printf("FAIL\n");
        fprintf(stderr, "Encoder init failed. Check counter subsystem patches.\n");
    }

    rc_mpu_data_t imu_data;
    rc_mpu_config_t imu_cfg = rc_mpu_default_config();
    printf("  IMU (MPU-9250 poll): ");
    if (rc_mpu_initialize(&imu_data, imu_cfg) == 0) {
        printf("OK\n");
    } else {
        printf("FAIL (may need inv-mpu6050 unbind)\n");
    }

    printf("  Battery: ");
    double vbatt = rc_adc_batt();
    if (vbatt > 1.0) {  /* rc_adc_batt() deadzone: returns 0.0 below 1.0V */
        printf("%.2f V", vbatt);
        if (vbatt < 6.0) printf(" (LOW -- motors may not work on USB power)");
        printf("\n");
    } else {
        printf("N/A (ADC not available)\n");
    }

    /* Baseline readings */
    print_header("Baseline (motors off)");
    read_encoders();
    read_imu(&imu_data);

    /* Motor tests */
    if (!no_motors && g_running) {
        print_header("Motor Tests");

        if (single_motor) {
            test_motor(single_motor, duty, &imu_data);
            if (g_running) {
                printf("\n  Reverse test...\n");
                test_motor(single_motor, -duty, &imu_data);
            }
        } else {
            for (int ch = 1; ch <= NUM_MOTORS && g_running; ch++) {
                test_motor(ch, duty, &imu_data);
                if (g_running) usleep(READ_DELAY_US);
            }
            if (g_running) {
                print_header("Reverse Tests");
                for (int ch = 1; ch <= NUM_MOTORS && g_running; ch++) {
                    test_motor(ch, -duty, &imu_data);
                    if (g_running) usleep(READ_DELAY_US);
                }
            }
        }
    }

    /* Final readings */
    if (g_running) {
        print_header("Final Readings");
        read_encoders();
        read_imu(&imu_data);
    }

    /* Cleanup */
    print_header("Cleanup");
    rc_motor_set(0, 0.0);  /* all motors off */
    rc_mpu_power_off();
    rc_adc_cleanup();
    rc_encoder_eqep_cleanup();
    rc_motor_cleanup();
    rc_remove_pid_file();
    printf("  All motors stopped, hardware released.\n");
    printf("  Done.\n\n");

    return 0;
}
