# Makefile for nfc-lvgl-app
# Builds LVGL + application and links against libnfc-nci

CC ?= gcc
CFLAGS ?= -O2 -g
LDFLAGS ?=
STAGING_DIR ?= /usr

# Directories
LVGL_DIR = $(CURDIR)/lvgl

# Include paths - define LV_CONF_INCLUDE_SIMPLE so LVGL finds lv_conf.h
INCLUDES = -I$(CURDIR) \
           -I$(LVGL_DIR) \
           -I$(STAGING_DIR)/usr/include \
           -DLV_CONF_INCLUDE_SIMPLE

# Libraries - use paho-mqtt3a for async API
LIBS = -lnfc_nci_linux -lpaho-mqtt3a -lpthread -lm

# Find ALL LVGL source files recursively
LVGL_SRC = $(shell find $(LVGL_DIR)/src -name '*.c' 2>/dev/null)

# Application source
APP_SRC = main.c \
         fonts/fa_solid_48.c \
         fonts/fa_regular_48.c \
         fonts/fa_brands_nfc_48.c \
         fonts/fa_brands_nfc_360.c \
         fonts/lv_font_montserrat_52.c \
         fonts/lv_font_montserrat_56.c \
         logo_small.c

# Object files
LVGL_OBJ = $(LVGL_SRC:.c=.o)
APP_OBJ = $(APP_SRC:.c=.o)

# Target
TARGET = nfc-lvgl-app

.PHONY: all clean

all: $(TARGET)

# Compile LVGL objects first
$(LVGL_OBJ): %.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# Compile app objects
$(APP_OBJ): %.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(TARGET): $(LVGL_OBJ) $(APP_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f $(TARGET) $(APP_OBJ)
	find $(LVGL_DIR) -name '*.o' -delete 2>/dev/null || true
