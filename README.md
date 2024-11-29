# BME280 Sensor Program

## Overview

This is a user-space program designed to interface with a kernel space driver for the BME280 sensor, which measures temperature, humidity, and pressure. The program uses the `ioctl` system call to read data from the sensor and displays the readings in a human-readable format or serves them as Prometheus metrics over HTTP.

The program also supports running as a daemon for background monitoring, logging, and serving metrics.

## Features

- **Reads temperature, humidity, and pressure** from the BME280 sensor through `ioctl` commands.
- **Continuously displays sensor readings** with a 10-second interval between each read cycle.
- **Prometheus metrics HTTP server** for exposing environmental data.
- **Daemon mode**: Run the program in the background with logs redirected to `/var/log/bme280_sensor.log`.
- **PID management**: Creates a PID file at `/var/run/bme280_sensor.pid` when running in daemon mode.

## Requirements

- A kernel driver for the BME280 sensor, exposing the device node (e.g., `/dev/bme280`).
- Linux system with appropriate permissions to access the device node.
- `libmicrohttpd` library for serving HTTP requests.

## Build Instructions

1. **Ensure you have the necessary dependencies installed**:
   ```bash
   sudo apt-get install gcc libmicrohttpd-dev
   ```

2. **Compile the program**:
   ```bash
   gcc -o bme280-sensor bme280-sensor.c -lmicrohttpd
   ```

3. **Run the program**:
   ```bash
   sudo ./bme280-sensor
   ```

## Running as a Daemon

The program supports running as a background daemon. To enable this mode, use the `-d` flag:

```bash
sudo ./bme280-sensor -d
```

### Daemon Mode Features

- **Logs**: Logs are written to `/var/log/bme280_sensor.log`.
- **PID File**: A PID file is created at `/var/run/bme280_sensor.pid`.

To stop the daemon, use the PID file to terminate the process:
```bash
sudo kill $(cat /var/run/bme280_sensor.pid)
```

## Program Explanation

### Code Breakdown

- **Device Path**: The program interacts with the sensor via the device node `/dev/bme280`.
- **IOCTL Commands**: Predefined commands are used to get sensor readings:
  ```c
  #define IOCTL_GET_TEMPERATURE _IOR('B', 1, int)
  #define IOCTL_GET_HUMIDITY _IOR('B', 2, int)
  #define IOCTL_GET_PRESSURE _IOR('B', 3, int)
  ```

### Functions

- **`read_sensor_data()`**:
    - Reads sensor data using `ioctl` system calls and updates global variables for temperature, humidity, and pressure.
    - Scales and formats sensor readings:
        - **Temperature**: `XX.XX °C` and `XX.XX °F`
        - **Humidity**: `XX.XX %`
        - **Pressure**: `XX.XX hPa`
- **`metrics_handler()`**:
    - Serves Prometheus metrics over HTTP, providing real-time environmental data in a scrapeable format.

### Main Loop

- Continuously reads sensor data every 10 seconds and serves Prometheus metrics if running in daemon mode.
- Properly handles shutdown signals (`SIGINT`, `SIGTERM`) and cleans up resources, including removing the PID file.

## Sample Output

### Console Output
```
Temperature: 25.34 °C (77.61 °F)
Humidity: 45.67 %
Pressure: 1013.25 hPa
```

### Prometheus Metrics
```
# HELP temperature_celsius Temperature in Celsius
# TYPE temperature_celsius gauge
temperature_celsius 25.34
# HELP temperature_fahrenheit Temperature in Fahrenheit
# TYPE temperature_fahrenheit gauge
temperature_fahrenheit 77.61
# HELP humidity_percentage Humidity in percentage
# TYPE humidity_percentage gauge
humidity_percentage 45.67
# HELP pressure_hpa Pressure in hPa
# TYPE pressure_hpa gauge
pressure_hpa 1013.25
```

## Error Handling

- **Failed Device Open**: Logs an error and exits if the device node cannot be opened.
- **IOCTL Failures**: Logs an error message for debugging purposes if any `ioctl` call fails.
- **Daemon Mode Issues**: Logs errors for file redirection, PID creation, or HTTP server startup.

## Customization

- **Reading Interval**: The sleep duration between readings can be adjusted by modifying the `sleep(10)` line in the `main()` function.
- **HTTP Port**: Change the `HTTP_PORT` definition to use a different port for Prometheus metrics.
- **Device Path**: Update `#define DEVICE_PATH "/dev/bme280"` to match your device node if it differs.

## License

This program is open-source and available under the GPL-2.0 License.

## Author

Developed by Carlos Alvarado Martinez.