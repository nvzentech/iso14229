SRCS += iso14229.c main.c
HDRS += iso14229.h 
TARGET = server
P2_MS ?= 1

CFLAGS = -DUDS_TP_ISOTP_SOCK=1 -DUDS_SERVER_DEFAULT_P2_MS=$(P2_MS) -g
LDFLAGS = -lcjson

all: $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)