#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define DEVICE_PATH "/dev/bme280"
#define IOCTL_GET_TEMPERATURE _IOR('B', 1, int)
#define IOCTL_GET_HUMIDITY _IOR('B', 2, int)
#define IOCTL_GET_PRESSURE _IOR('B', 3, int)

void read_sensor_data(int fd, unsigned int cmd, const char *label) {
    int value;

    if (ioctl(fd, cmd, &value) < 0) {
        perror("ioctl failed");
        return;
    }

    // Print the value with appropriate formatting based on the command
    if (cmd == IOCTL_GET_TEMPERATURE) {
        printf("%s: %.2f °C\n", label, value / 100.0);
    } else if (cmd == IOCTL_GET_HUMIDITY) {
        printf("%s: %f %%\n", label, value / 100.0);
    } else if (cmd == IOCTL_GET_PRESSURE) {
        printf("%s: %.2f hPa\n", label, value / 100.0);
    }
}

int main() {
    int fd = open(DEVICE_PATH, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open the device");
        return EXIT_FAILURE;
    }

    while (1) {
        read_sensor_data(fd, IOCTL_GET_TEMPERATURE, "Temperature");
        read_sensor_data(fd, IOCTL_GET_HUMIDITY, "Humidity");
        read_sensor_data(fd, IOCTL_GET_PRESSURE, "Pressure");

        sleep(10); // Wait for 10 seconds before reading the data again
    }

    close(fd);
    return EXIT_SUCCESS;
}
