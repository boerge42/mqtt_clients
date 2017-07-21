TARGET = sysinfo2mqtt
CFLAGS = -lmosquitto -Wall

all:: $(TARGET)

$(TARGET): $(TARGET).c
	gcc -o $@ $? $(CFLAGS)

clean::
	rm -f $(TARGET)

.PHONY: clean


