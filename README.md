# BME280 Sensor Program

## Overview

This is a user-space program designed to interface with a kernel space driver for the BME280 sensor, which measures temperature, humidity, and pressure. The program uses the `ioctl` system call to read data from the sensor and displays the readings in a human-readable format.

## Features

- **Reads temperature, humidity, and pressure** from the BME280 sensor through `ioctl` commands.
- **Continuously displays sensor readings** with a 10-second interval between each read cycle.
- **Simple and efficient design** for real-time monitoring of environmental data.

## Requirements

- A kernel driver for the BME280 sensor, exposing the device node (e.g., `/dev/bme280`).
- Linux system with appropriate permissions to access the device node.

## Build Instructions

1. **Ensure you have `gcc` installed**:
   ```bash
   sudo apt-get install gcc
   ```

2. **Compile the program**:
   ```bash
   gcc -o bme280-sensor bme280-sensor.c
   ```

3. **Run the program**:
   ```bash
   sudo ./bme280-sensor
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
  - Takes the file descriptor (`fd`), an `ioctl` command (`cmd`), and a `label` for printing the result.
  - Calls `ioctl` to get the sensor data and prints the value in an appropriate format.
  - Temperature, humidity, and pressure values are scaled and displayed as:
    - **Temperature**: `XX.XX °C`
    - **Humidity**: `XX.XX %`
    - **Pressure**: `XX.XX hPa`

### Main Loop

- The program continuously reads and prints the temperature, humidity, and pressure data every 10 seconds.
- It ensures the file descriptor is properly closed when the program exits.

## Sample Output

```
Temperature: 25.34 °C
Humidity: 45.67 %
Pressure: 1013.25 hPa

(Repeats every 10 seconds)
```

## Error Handling

- **Failed Device Open**: The program prints an error and exits if the device node cannot be opened.
- **IOCTL Failures**: If the `ioctl` call fails, an error message is printed for debugging purposes.

## Customization

- **Reading Interval**: The sleep duration between readings can be adjusted by changing the `sleep(10)` line in the `main()` function.
- **Device Path**: Update `#define DEVICE_PATH "/dev/bme280"` to match your device node if it differs.

## License

This program is open-source and available under the GPL-2.0 License.

## Author

Developed by Carlos Alvarado Martinez.
