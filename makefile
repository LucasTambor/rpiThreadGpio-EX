GCC := arm-linux-gnueabihf-gcc
OUTPUT := thread_gpio_mod
SOURCES := $(wildcard *.c)
CCFLAGS := -pthread -Wall

all: $(OUTPUT)

$(OUTPUT):
	$(GCC) -o $(OUTPUT) $(CCFLAGS) $(SOURCES)

clean:
	rm $(OUTPUT)
	
.PHONY: all
 
