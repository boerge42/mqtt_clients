TARGET = sysinfo2mqtt
CFLAGS = -lmosquitto -ljson -Wall

all:: $(TARGET)

$(TARGET): $(TARGET).c
	gcc -o $@ $? $(CFLAGS)

clean::
	rm -f $(TARGET)

install:
	strip $(TARGET)
	cp $(TARGET) /usr/bin/
	chown root:root /usr/bin/$(TARGET)
	chmod 0755 /usr/bin/$(TARGET)	

.PHONY: clean


