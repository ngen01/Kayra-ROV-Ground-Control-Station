#include <stdio.h>
#include <unistd.h>
#include "pca9685.h"
#include "config.h"

int main() {
    pca9685_t pwm;
    printf("--- KAYRA ROV Motor Test Program ---\n");
    printf("Initializing PCA9685 at bus=%d addr=0x%02X freq=%dHz...\n",
           PCA9685_I2C_BUS, PCA9685_I2C_ADDR, PCA9685_FREQ_HZ);
           
    if (pca9685_init(&pwm, PCA9685_I2C_BUS, PCA9685_I2C_ADDR, PCA9685_FREQ_HZ) < 0) {
        fprintf(stderr, "PCA9685 init failed! Please check your connections.\n");
        return 1;
    }
    
    printf("Arming ESCs... Sending 1500us neutral pulse for 3 seconds.\n");
    printf("You should hear arming beeps from your ESCs.\n");
    for (int t = 0; t < 30; t++) {
        pca9685_set_all_us(&pwm, 1500);
        usleep(100000);
    }
    
    // We will test the last 6 channels: CH10, CH11, CH12, CH13, CH14, CH15
    int test_channels[6] = {10, 11, 12, 13, 14, 15};
    const char *names[6] = {
        "CH10 (Front-Right / FR)",
        "CH11 (Front-Left / FL)",
        "CH12 (Back-Right / BR)",
        "CH13 (Back-Left / BL)",
        "CH14 (Vertical-Left / VL)",
        "CH15 (Vertical-Right / VR)"
    };
    
    for (int i = 0; i < 6; i++) {
        int ch = test_channels[i];
        printf("\n>>> Testing %s... Spinning motor for 2.5 seconds (1600us)\n", names[i]);
        for (int t = 0; t < 25; t++) {
            pca9685_set_pulse_us(&pwm, ch, 1600);
            usleep(100000);
        }
        // Neutralize channel
        pca9685_set_pulse_us(&pwm, ch, 1500);
        printf(">>> Stopped %s.\n", names[i]);
        usleep(500000); // 0.5s pause
    }
    
    printf("\nTest finished successfully. Neutralizing all channels and closing...\n");
    pca9685_set_all_us(&pwm, 1500);
    usleep(100000);
    pca9685_close(&pwm);
    return 0;
}
