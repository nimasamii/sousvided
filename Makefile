BUILD_ARCH := $(shell uname -m)

CFLAGS = -g -Wall -Werror -I/home/nima/src/bcm2835-1.38/src -pthread
LDFLAGS = -L/home/nima/src/bcm2835-1.38/src
LDLIBS = -lbcm2835 -lm -lrt -lpthread

# Add CFLAGS to optimize build when compiling on RaspberryPi
ifeq ($(BUILD_ARCH),armv6l)
	CFLAGS += -O2 -march=armv6zk -mcpu=arm1176jzf-s -mtune=arm1176jzf-s
	CFLAGS += -mfpu=vfp -mfloat-abi=hard
endif

.PHONY: all clean

all: sousvided
clean:
	rm -rf *.o sousvided

buttons.o: buttons.c buttons.h
max31865.o: max31865.c max31865.h rtd_table.h
motor.o: motor.c motor.h
pid.o: pid.c pid.h
rtd_table.o: rtd_table.c rtd_table.h
sousvide.o: sousvide.c max31865.h rtd_table.h motor.h

sousvided: sousvided.o rtd_table.o max31865.o motor.o pid.o buttons.o
