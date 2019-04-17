GCC := arm-linux-gnueabihf-gcc
OUTPUT := thread_gpio
SOURCES := $(wildcard *.c)
CCFLAGS := -pthread -Wall

all: 
	$(GCC) -o $(OUTPUT) $(CCFLAGS) $(SOURCES)

clean:
	rm $(OUTPUT)
	
.PHONY: all
 
