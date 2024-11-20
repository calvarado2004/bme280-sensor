CC ?= gcc
CFLAGS ?= -Wall
LDFLAGS ?=

all: bme_sensor

bme_sensor: sensor_read.c
	$(CC) $(CFLAGS) sensor_read.c -o bme_sensor $(LDFLAGS)

clean:
	rm -f bme_sensor
