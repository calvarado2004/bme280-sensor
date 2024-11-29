#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <getopt.h>
#include <microhttpd.h>
#include <time.h>

#define DEVICE_PATH "/dev/bme280"
#define IOCTL_GET_TEMPERATURE _IOR('B', 1, int)
#define IOCTL_GET_HUMIDITY _IOR('B', 2, int)
#define IOCTL_GET_PRESSURE _IOR('B', 3, int)
#define HTTP_PORT 8080
#define PID_FILE "/var/run/bme280_sensor.pid"
#define LOG_FILE "/var/log/bme280_sensor.log"

// Global variables
float temperature_celsius = 0.0, temperature_fahrenheit = 0.0, humidity = 0.0, pressure = 0.0;
pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile sig_atomic_t keep_running = 1;  // Flag to control program termination
struct MHD_Daemon *http_server;

// File pointer for logging
FILE *log_file = NULL;

// Signal handler
void handle_signal(int sig) {
    fprintf(log_file, "Caught signal %d. Shutting down...\n", sig);
    fflush(log_file);
    keep_running = 0;
}

// Function to write the PID file
void write_pid_file() {
    FILE *pid_file = fopen(PID_FILE, "w");
    if (pid_file) {
        fprintf(pid_file, "%d\n", getpid());
        fclose(pid_file);
    } else {
        fprintf(log_file, "Failed to write PID file: %s\n", PID_FILE);
        fflush(log_file);
    }
}

// Function to write logs with timestamps
void log_with_timestamp(const char *format, ...) {
    va_list args;
    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);

    fprintf(log_file, "[%04d-%02d-%02d %02d:%02d:%02d] ",
            local_time->tm_year + 1900, local_time->tm_mon + 1, local_time->tm_mday,
            local_time->tm_hour, local_time->tm_min, local_time->tm_sec);

    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    fflush(log_file);
}

// Function to read sensor data
void read_sensor_data(int fd) {
    int temp_val, hum_val, press_val;

    pthread_mutex_lock(&data_mutex);

    if (ioctl(fd, IOCTL_GET_TEMPERATURE, &temp_val) == 0) {
        temperature_celsius = temp_val / 100.0;
        temperature_fahrenheit = (temperature_celsius * 9.0 / 5.0) + 32.0;
    }

    if (ioctl(fd, IOCTL_GET_HUMIDITY, &hum_val) == 0) {
        humidity = hum_val / 1024.0;
    }

    if (ioctl(fd, IOCTL_GET_PRESSURE, &press_val) == 0) {
        pressure = press_val / 100.0;
    }

    pthread_mutex_unlock(&data_mutex);

    log_with_timestamp("Sensor readings - Temp: %.2f°C, %.2f°F, Humidity: %.2f%%, Pressure: %.2f hPa\n",
                       temperature_celsius, temperature_fahrenheit, humidity, pressure);
}

// Function to serve Prometheus metrics
enum MHD_Result metrics_handler(void *cls, struct MHD_Connection *connection,
                                const char *url, const char *method, const char *version,
                                const char *upload_data, size_t *upload_data_size, void **ptr) {
    if (strcmp(method, "GET") != 0) {
        return MHD_NO;
    }

    pthread_mutex_lock(&data_mutex);

    char metrics[1024];
    snprintf(metrics, sizeof(metrics),
             "# HELP temperature_celsius Temperature in Celsius\n"
             "# TYPE temperature_celsius gauge\n"
             "temperature_celsius %.2f\n"
             "# HELP temperature_fahrenheit Temperature in Fahrenheit\n"
             "# TYPE temperature_fahrenheit gauge\n"
             "temperature_fahrenheit %.2f\n"
             "# HELP humidity_percentage Humidity in percentage\n"
             "# TYPE humidity_percentage gauge\n"
             "humidity_percentage %.2f\n"
             "# HELP pressure_hpa Pressure in hPa\n"
             "# TYPE pressure_hpa gauge\n"
             "pressure_hpa %.2f\n",
             temperature_celsius, temperature_fahrenheit, humidity, pressure);

    pthread_mutex_unlock(&data_mutex);

    struct MHD_Response *response = MHD_create_response_from_buffer(strlen(metrics),
                                                                     (void *)metrics, MHD_RESPMEM_PERSISTENT);

    // Add the required Content-Type header
    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/plain; version=0.0.4; charset=utf-8");
    MHD_add_response_header(response, MHD_HTTP_HEADER_CACHE_CONTROL, "no-cache");

    int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);

    return ret == MHD_YES ? MHD_YES : MHD_NO;
}

// Thread for the HTTP server
void *http_server_thread(void *arg) {
    http_server = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, HTTP_PORT, NULL, NULL,
                                   &metrics_handler, NULL, MHD_OPTION_END);
    if (!http_server) {
        log_with_timestamp("Failed to start HTTP server\n");
        return NULL;
    }

    log_with_timestamp("HTTP server started on port %d\n", HTTP_PORT);

    while (keep_running) {
        sleep(1);
    }

    MHD_stop_daemon(http_server);
    return NULL;
}

int main(int argc, char *argv[]) {
    int daemonize = 0;
    int opt;

    // Parse command-line options
    while ((opt = getopt(argc, argv, "d")) != -1) {
        switch (opt) {
            case 'd':
                daemonize = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (daemonize) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("Failed to fork");
            exit(EXIT_FAILURE);
        }
        if (pid > 0) {
            exit(EXIT_SUCCESS);
        }

        umask(0);
        setsid();

        if (chdir("/") < 0) {
            perror("Failed to change directory to root");
            exit(EXIT_FAILURE);
        }

        log_file = fopen(LOG_FILE, "a");
        if (!log_file) {
            perror("Failed to open log file");
            exit(EXIT_FAILURE);
        }

        write_pid_file();
    } else {
        log_file = stdout;  // In foreground mode, log to stdout
    }

    openlog("bme280_sensor", LOG_PID | LOG_CONS, LOG_DAEMON);

    // Set up signal handling
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    int fd = open(DEVICE_PATH, O_RDONLY);
    if (fd < 0) {
        log_with_timestamp("Failed to open the device: %s\n", DEVICE_PATH);
        return EXIT_FAILURE;
    }

    pthread_t server_thread;
    if (pthread_create(&server_thread, NULL, http_server_thread, NULL) != 0) {
        log_with_timestamp("Failed to create HTTP server thread\n");
        close(fd);
        return EXIT_FAILURE;
    }

    log_with_timestamp("BME280 Sensor Program started successfully.\n");

    while (keep_running) {
        read_sensor_data(fd);
        sleep(10);
    }

    log_with_timestamp("Shutting down...\n");

    close(fd);
    pthread_join(server_thread, NULL);

    remove(PID_FILE);
    if (log_file && log_file != stdout) fclose(log_file);
    closelog();

    return EXIT_SUCCESS;
}