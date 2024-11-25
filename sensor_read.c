#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <microhttpd.h>

#define DEVICE_PATH "/dev/bme280"
#define IOCTL_GET_TEMPERATURE _IOR('B', 1, int)
#define IOCTL_GET_HUMIDITY _IOR('B', 2, int)
#define IOCTL_GET_PRESSURE _IOR('B', 3, int)
#define HTTP_PORT 8080

// Global variables
float temperature_celsius = 0.0, temperature_fahrenheit = 0.0, humidity = 0.0, pressure = 0.0;
pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile sig_atomic_t keep_running = 1;  // Flag to control program termination
struct MHD_Daemon *http_server;  // Renamed from daemon

// Signal handler
void handle_signal(int sig) {
    printf("\nCaught signal %d. Shutting down...\n", sig);
    keep_running = 0;
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
}

// Function to serve Prometheus metrics
enum MHD_Result metrics_handler(void *cls, struct MHD_Connection *connection,
                                 const char *url, const char *method, const char *version,
                                 const char *upload_data, size_t *upload_data_size, void **ptr) {
    (void)cls; (void)upload_data; (void)upload_data_size; (void)ptr;

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
    int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);

    return ret == MHD_YES ? MHD_YES : MHD_NO; // Ensure correct return type
}

// Thread for the HTTP server
void *http_server_thread(void *arg) {
    http_server = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, HTTP_PORT, NULL, NULL,
                                   &metrics_handler, NULL, MHD_OPTION_END);
    if (!http_server) {
        fprintf(stderr, "Failed to start HTTP server\n");
        return NULL;
    }

    printf("HTTP server started on port %d\n", HTTP_PORT);

    // Keep the thread running until the program is terminated
    while (keep_running) {
        sleep(1);
    }

    MHD_stop_daemon(http_server);
    return NULL;
}

int main() {
    // Set up signal handling
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);  // Handle CTRL+C
    sigaction(SIGTERM, &sa, NULL);  // Handle termination signal

    int fd = open(DEVICE_PATH, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open the device");
        return EXIT_FAILURE;
    }

    pthread_t server_thread;
    if (pthread_create(&server_thread, NULL, http_server_thread, NULL) != 0) {
        perror("Failed to create HTTP server thread");
        close(fd);
        return EXIT_FAILURE;
    }

    while (keep_running) {
        read_sensor_data(fd);
        sleep(10);  // Read sensor data every 10 seconds
    }

    printf("Shutting down...\n");

    close(fd);
    pthread_join(server_thread, NULL);

    return EXIT_SUCCESS;
}
