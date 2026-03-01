#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

#include "lvgl/lvgl.h"
#include "linux_nfc_api.h"
#include "MQTTAsync.h"

#define DISPLAY_WIDTH   720
#define DISPLAY_HEIGHT  720
#define FB_DEVICE       "/dev/fb0"
#define TOUCH_DEVICE    "/dev/input/event4"

#define MQTT_ADDRESS    "tcp://192.168.188.50:1883"
#define MQTT_CLIENT_ID  "nfc-terminal"
#define MQTT_USERNAME   "admin"
#define MQTT_PASSWORD   "admin"
#define MQTT_QOS        2
#define MQTT_TIMEOUT    3000L  /* 3 seconds */
#define MQTT_RECONNECT_MIN_DELAY  1   /* Minimum reconnect delay in seconds */
#define MQTT_RECONNECT_MAX_DELAY  60  /* Maximum reconnect delay in seconds */

/* Logging macro - printf with immediate flush */
#define LOG(fmt, ...) do { printf(fmt, ##__VA_ARGS__); fflush(stdout); } while(0)

/* Color themes */
typedef enum {
    THEME_HIGH_CONTRAST,
    THEME_DARK_MOCHA,
    THEME_LIGHT_LATTE
} color_theme_t;

typedef struct {
    lv_color_t bg;           /* Background */
    lv_color_t modal_bg;     /* Modal background (slightly darker) */
    lv_color_t header;       /* Header background */
    lv_color_t btn_default;  /* Button default */
    lv_color_t btn_pressed;  /* Button pressed */
    lv_color_t btn_selected; /* Button selected (yellow) */
    lv_color_t btn_checked;  /* Button checked in (green) */
    lv_color_t text;         /* Primary text */
    lv_color_t text_secondary; /* Secondary/muted text */
    lv_color_t btn_text;     /* Button label text */
    lv_color_t role_unselected_bg;   /* Role button unselected background */
    lv_color_t role_unselected_text; /* Role button unselected text */
} theme_colors_t;

/* High Contrast theme */
static const theme_colors_t g_theme_high_contrast = {
    .bg           = {.red = 0x1a, .green = 0x1a, .blue = 0x2e},  /* 0x1a1a2e */
    .modal_bg     = {.red = 0x14, .green = 0x14, .blue = 0x26},  /* Slightly darker */
    .header       = {.red = 0x12, .green = 0x12, .blue = 0x24},  /* 0x121224 */
    .btn_default  = {.red = 0x80, .green = 0x80, .blue = 0x80},  /* 0x808080 */
    .btn_pressed  = {.red = 0xC8, .green = 0xC8, .blue = 0xC8},  /* 0xC8C8C8 */
    .btn_selected = {.red = 0xFF, .green = 0xD7, .blue = 0x00},  /* 0xFFD700 */
    .btn_checked  = {.red = 0x32, .green = 0xCD, .blue = 0x32},  /* 0x32CD32 */
    .text         = {.red = 0xFF, .green = 0xFF, .blue = 0xFF},  /* 0xFFFFFF */
    .text_secondary = {.red = 0xA0, .green = 0xA0, .blue = 0xA0}, /* 0xA0A0A0 */
    .btn_text     = {.red = 0x1a, .green = 0x1a, .blue = 0x2e},  /* 0x1a1a2e (dark on button) */
    .role_unselected_bg   = {.red = 0xC8, .green = 0xC8, .blue = 0xC8},  /* Light grey */
    .role_unselected_text = {.red = 0x40, .green = 0x40, .blue = 0x40},  /* Dark grey */
};

/* Catppuccin Mocha (Dark) */
static const theme_colors_t g_theme_dark_mocha = {
    .bg           = {.red = 0x1e, .green = 0x1e, .blue = 0x2e},  /* Base: 0x1e1e2e */
    .modal_bg     = {.red = 0x18, .green = 0x18, .blue = 0x26},  /* Slightly darker */
    .header       = {.red = 0x18, .green = 0x18, .blue = 0x25},  /* Mantle: 0x181825 */
    .btn_default  = {.red = 0x31, .green = 0x32, .blue = 0x44},  /* Surface0: 0x313244 */
    .btn_pressed  = {.red = 0x45, .green = 0x47, .blue = 0x5a},  /* Surface1: 0x45475a */
    .btn_selected = {.red = 0xf9, .green = 0xe2, .blue = 0xaf},  /* Yellow: 0xf9e2af */
    .btn_checked  = {.red = 0xa6, .green = 0xe3, .blue = 0xa1},  /* Green: 0xa6e3a1 */
    .text         = {.red = 0xcd, .green = 0xd6, .blue = 0xf4},  /* Text: 0xcdd6f4 */
    .text_secondary = {.red = 0x6c, .green = 0x70, .blue = 0x86}, /* Overlay0: 0x6c7086 */
    .btn_text     = {.red = 0xcd, .green = 0xd6, .blue = 0xf4},  /* Text: light on dark button */
    .role_unselected_bg   = {.red = 0x93, .green = 0x9d, .blue = 0xb4},  /* Overlay2: lighter */
    .role_unselected_text = {.red = 0x1e, .green = 0x1e, .blue = 0x2e},  /* Base: dark */
};

/* Catppuccin Latte (Light) */
static const theme_colors_t g_theme_light_latte = {
    .bg           = {.red = 0xef, .green = 0xf1, .blue = 0xf5},  /* Base: 0xeff1f5 */
    .modal_bg     = {.red = 0xe0, .green = 0xe3, .blue = 0xea},  /* Slightly darker */
    .header       = {.red = 0xe6, .green = 0xe9, .blue = 0xef},  /* Mantle: 0xe6e9ef */
    .btn_default  = {.red = 0xac, .green = 0xb0, .blue = 0xbe},  /* Surface2: darker */
    .btn_pressed  = {.red = 0x9c, .green = 0xa0, .blue = 0xb0},  /* Overlay0: darker */
    .btn_selected = {.red = 0xdf, .green = 0x8e, .blue = 0x1d},  /* Yellow: 0xdf8e1d */
    .btn_checked  = {.red = 0x40, .green = 0xa0, .blue = 0x2b},  /* Green: 0x40a02b */
    .text         = {.red = 0x4c, .green = 0x4f, .blue = 0x69},  /* Text: 0x4c4f69 */
    .text_secondary = {.red = 0x7c, .green = 0x7f, .blue = 0x93}, /* Subtext0: darker */
    .btn_text     = {.red = 0x4c, .green = 0x4f, .blue = 0x69},  /* Text (dark on button) */
    .role_unselected_bg   = {.red = 0xbc, .green = 0xc0, .blue = 0xcc},  /* Surface1: darker */
    .role_unselected_text = {.red = 0x4c, .green = 0x4f, .blue = 0x69},  /* Text: darker */
};

static color_theme_t g_current_theme = THEME_HIGH_CONTRAST;
static const theme_colors_t *g_theme = &g_theme_high_contrast;
static bool g_theme_locked = false;  /* Theme locked after leaving landing page */

/* Convenience macros to access current theme colors */
#define THEME_BG           (g_theme->bg)
#define THEME_MODAL_BG     (g_theme->modal_bg)
#define THEME_HEADER       (g_theme->header)
#define THEME_BTN_DEFAULT  (g_theme->btn_default)
#define THEME_BTN_PRESSED  (g_theme->btn_pressed)
#define THEME_BTN_SELECTED (g_theme->btn_selected)
#define THEME_BTN_CHECKED  (g_theme->btn_checked)
#define THEME_TEXT         (g_theme->text)
#define THEME_TEXT_SECONDARY (g_theme->text_secondary)
#define THEME_BTN_TEXT     (g_theme->btn_text)
#define THEME_ROLE_UNSELECTED_BG   (g_theme->role_unselected_bg)
#define THEME_ROLE_UNSELECTED_TEXT (g_theme->role_unselected_text)

/* Backward-compatible color macros using theme */
#define COLOR_GREY       THEME_BTN_DEFAULT
#define COLOR_LIGHT_GREY THEME_TEXT_SECONDARY
#define COLOR_PRESSED    THEME_BTN_PRESSED
#define COLOR_YELLOW     THEME_BTN_SELECTED
#define COLOR_CHECKED    THEME_BTN_CHECKED
#define COLOR_BG         THEME_BG
#define COLOR_HEADER     THEME_HEADER
#define COLOR_TEXT       THEME_TEXT

/* App/page state*/
typedef enum {
    PAGE_LANDING,
    PAGE_SIMPLE_CHECKIN,
    PAGE_ROLES_BOOKING,
    PAGE_EV_CHARGING
} app_page_t;

static app_page_t g_current_page = PAGE_LANDING;

/*====================
   ROLE STATE MACHINE
 *====================*/
typedef enum {
    ROLE_STATE_UNSELECTED,    /* Grey button, not active */
    ROLE_STATE_SELECTED,      /* Yellow button, waiting for check-in */
    ROLE_STATE_CHECKED_IN     /* Green button, checked in with tag */
} role_state_t;

typedef struct {
    role_state_t state;
    char tag_id[64];          /* Tag ID that checked in */
    lv_obj_t *btn;
    lv_obj_t *label;
} role_t;

/*====================
   GLOBAL STATE
 *====================*/
#define NUM_ROLES 8
static const char *g_role_names[NUM_ROLES] = {"Role A", "Role B", "Role C", "Role D", "Role E", "Role F", "Role G", "Role H"};
static role_t g_roles[NUM_ROLES] = {0};
static lv_obj_t *g_status_label = NULL;
static lv_obj_t *g_header = NULL;
static lv_obj_t *g_touch_debug_label = NULL;  /* Debug: show touch coords */

/* Landing page UI */
static lv_obj_t *g_landing_container = NULL;
static lv_obj_t *g_landing_title = NULL;
static lv_obj_t *g_btn_simple_checkin = NULL;
static lv_obj_t *g_btn_roles_booking = NULL;
static lv_obj_t *g_btn_ev_charging = NULL;
static lv_obj_t *g_btn_settings = NULL;  /* Settings/hamburger button */
static lv_obj_t *g_btn_theme_contrast = NULL;
static lv_obj_t *g_btn_theme_dark = NULL;
static lv_obj_t *g_btn_theme_light = NULL;

/* Settings modal */
static lv_obj_t *g_settings_modal = NULL;
static lv_obj_t *g_settings_title = NULL;
static lv_obj_t *g_settings_lbl_mac = NULL;
static lv_obj_t *g_settings_lbl_ip = NULL;
static lv_obj_t *g_settings_lbl_mqtt = NULL;

/* Roles booking app */
static lv_obj_t *g_roles_container = NULL;
static lv_obj_t *g_btn_back = NULL;

static char g_last_tag_id[64] = "";
static pthread_mutex_t g_ui_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile int g_running = 1;
static volatile int g_nfc_ready = 0;
static volatile int g_ui_needs_refresh = 0;  /* Flag for main loop to refresh UI */

/* Framebuffer */
static int g_fb_fd = -1;
static uint16_t *g_fb_mem = NULL;  /* RGB565 framebuffer memory */
static struct fb_var_screeninfo g_vinfo;
static struct fb_fix_screeninfo g_finfo;
static lv_display_t *g_display = NULL;

/* Touch input */
static int g_touch_fd = -1;
static lv_indev_t *g_touch_indev = NULL;
static int g_touch_x = 0;
static int g_touch_y = 0;
static int g_touch_pressed = 0;

/* MQTT Client (Async) */
static MQTTAsync g_mqtt_client = NULL;
static volatile int g_mqtt_connected = 0;
static pthread_mutex_t g_mqtt_mutex = PTHREAD_MUTEX_INITIALIZER;
static char g_device_mac[18] = "";  /* MAC address in format "AA:BB:CC:DD:EE:FF" */
static char g_mqtt_topic[64] = "data/unknown/nfc";  /* Topic: data/<MAC>/nfc */
static char g_mqtt_state_topic[64] = "data/unknown/state";  /* Topic: data/<MAC>/state */

/* MQTT Message Queue - simple ring buffer for tag events to publish */
#define MQTT_QUEUE_SIZE 8
typedef struct {
    char tag_id[64];
    uint8_t protocol;  /* NFC protocol type: T1T, T2T, T3T, etc. */
} mqtt_queue_entry_t;
static mqtt_queue_entry_t g_mqtt_queue[MQTT_QUEUE_SIZE];
static volatile int g_mqtt_queue_head = 0;
static volatile int g_mqtt_queue_tail = 0;
static pthread_mutex_t g_mqtt_queue_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Forward declarations */
static void mqtt_publish_tag_event(const char *tag_id, uint8_t protocol);
static void mqtt_queue_tag(const char *tag_id, uint8_t protocol);
static const char* protocol_to_string(uint8_t protocol);
static void mqtt_publish_state(const char *state);

/* MQTT Async Callbacks (MQTT v5.0) */
static void mqtt_on_connect(void *context, MQTTAsync_successData5 *response);
static void mqtt_on_connect_failure(void *context, MQTTAsync_failureData5 *response);
static void mqtt_on_connected(void *context, char *cause);

/*====================
   TICK FUNCTION
 *====================*/
static uint32_t g_tick_start = 0;

uint32_t custom_tick_get(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint32_t tick = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
    if (g_tick_start == 0) g_tick_start = tick;
    return tick - g_tick_start;
}

/*====================
   CONSOLE CONTROL
 *====================*/
static void clear_console(void) {
    /* Clear the console and hide cursor */
    printf("\033[2J");      /* Clear screen */
    printf("\033[H");       /* Move cursor to home */
    printf("\033[?25l");    /* Hide cursor */
    fflush(stdout);
    fflush(stderr);
    
    /* Redirect stdout/stderr to log file for debugging */
    FILE *f = freopen("/tmp/nfc-lvgl-app.log", "w", stdout);
    if (!f) {
        /* Try to create a direct debug log */
        FILE *dbg = fopen("/tmp/freopen-failed.log", "w");
        if (dbg) { fprintf(dbg, "freopen stdout failed\n"); fclose(dbg); }
    }
    if (!freopen("/tmp/nfc-lvgl-app.log", "a", stderr)) {
        /* Ignore - stderr redirect is optional */
    }
}

static void restore_console(void) {
    printf("\033[?25h");    /* Show cursor */
    fflush(stdout);
}

/* Framebuffer display*/
static int g_flush_count = 0;

static void fb_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    g_flush_count++;
    
    /* Log every 100th flush or if area includes status label region (y < 300) */
    /*
    if (g_flush_count % 100 == 0 || (area->y1 < 300 && area->y2 > 150)) {
        LOG("FB: flush #%d area=(%d,%d)-(%d,%d)\n", 
               g_flush_count, area->x1, area->y1, area->x2, area->y2);
    }
    */

    if (g_fb_mem == NULL) {
        lv_display_flush_ready(disp);
        return;
    }

    int32_t w = area->x2 - area->x1 + 1;
    uint16_t *src = (uint16_t *)px_map;
    uint32_t stride = g_finfo.line_length / 2;  /* pixels per line */

    for (int32_t y = area->y1; y <= area->y2; y++) {
        if (y >= 0 && y < (int32_t)g_vinfo.yres) {
            uint16_t *dst = g_fb_mem + y * stride + area->x1;
            int32_t copy_w = w;
            int32_t start_x = area->x1;

            /* Clip to screen bounds */
            if (start_x < 0) {
                copy_w += start_x;
                src -= start_x;
                start_x = 0;
            }
            if (start_x + copy_w > (int32_t)g_vinfo.xres) {
                copy_w = g_vinfo.xres - start_x;
            }

            if (copy_w > 0) {
                memcpy(dst, src, copy_w * 2);
            }
        }
        src += w;
    }

    lv_display_flush_ready(disp);
}

static int fb_init(void) {
    g_fb_fd = open(FB_DEVICE, O_RDWR);
    if (g_fb_fd < 0) {
        perror("Cannot open framebuffer");
        return -1;
    }

    if (ioctl(g_fb_fd, FBIOGET_VSCREENINFO, &g_vinfo) < 0) {
        perror("FBIOGET_VSCREENINFO");
        close(g_fb_fd);
        return -1;
    }

    if (ioctl(g_fb_fd, FBIOGET_FSCREENINFO, &g_finfo) < 0) {
        perror("FBIOGET_FSCREENINFO");
        close(g_fb_fd);
        return -1;
    }

    if (g_vinfo.bits_per_pixel != 16) {
        fprintf(stderr, "Unsupported bpp: %d (need 16)\n", g_vinfo.bits_per_pixel);
        close(g_fb_fd);
        return -1;
    }

    size_t fb_size = g_vinfo.yres * g_finfo.line_length;
    g_fb_mem = mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, g_fb_fd, 0);
    if (g_fb_mem == MAP_FAILED) {
        perror("mmap framebuffer");
        g_fb_mem = NULL;
        close(g_fb_fd);
        return -1;
    }

    /* Clear framebuffer to black */
    memset(g_fb_mem, 0, fb_size);

    return 0;
}

static void fb_deinit(void) {
    if (g_fb_mem && g_fb_mem != MAP_FAILED) {
        size_t fb_size = g_vinfo.yres * g_finfo.line_length;
        munmap(g_fb_mem, fb_size);
        g_fb_mem = NULL;
    }
    if (g_fb_fd >= 0) {
        close(g_fb_fd);
        g_fb_fd = -1;
    }
}

/*====================
   TOUCH INPUT
 *====================*/
/* Touch coordinate ranges (set during init) */
static int g_touch_x_min = 0, g_touch_x_max = 720;
static int g_touch_y_min = 0, g_touch_y_max = 720;

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    (void)indev;
    struct input_event ev;

    /* Read all pending events */
    while (g_touch_fd >= 0) {
        ssize_t n = read(g_touch_fd, &ev, sizeof(ev));
        
        if (n < 0) {
            /* EAGAIN/EWOULDBLOCK means no more events available */
            break;
        }
        if (n != sizeof(ev)) {
            break;
        }

        if (ev.type == EV_ABS) {
            switch (ev.code) {
                case ABS_X:
                case ABS_MT_POSITION_X:
                    g_touch_x = ev.value;
                    break;
                case ABS_Y:
                case ABS_MT_POSITION_Y:
                    g_touch_y = ev.value;
                    break;
                case ABS_MT_TRACKING_ID:
                    /* MT tracking: >= 0 means touch down, -1 means touch up */
                    g_touch_pressed = (ev.value >= 0) ? 1 : 0;
                    break;
            }
        } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
            g_touch_pressed = ev.value;
        }
    }

    /* Scale touch coordinates to display */
    int scaled_x = (g_touch_x - g_touch_x_min) * 720 / (g_touch_x_max - g_touch_x_min + 1);
    int scaled_y = (g_touch_y - g_touch_y_min) * 720 / (g_touch_y_max - g_touch_y_min + 1);
    
    /* Clamp to display bounds */
    if (scaled_x < 0) scaled_x = 0;
    if (scaled_x >= 720) scaled_x = 719;
    if (scaled_y < 0) scaled_y = 0;
    if (scaled_y >= 720) scaled_y = 719;

    data->point.x = scaled_x;
    data->point.y = scaled_y;
    data->state = g_touch_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static int touch_init(void) {
    g_touch_fd = open(TOUCH_DEVICE, O_RDONLY | O_NONBLOCK);
    if (g_touch_fd < 0) {
        perror("Cannot open touch device");
        return -1;
    }

    /* Get touch device info for coordinate scaling */
    struct input_absinfo abs_x, abs_y;
    /* Try multitouch coords first, then regular ABS */
    if (ioctl(g_touch_fd, EVIOCGABS(ABS_MT_POSITION_X), &abs_x) == 0 &&
        ioctl(g_touch_fd, EVIOCGABS(ABS_MT_POSITION_Y), &abs_y) == 0) {
        g_touch_x_min = abs_x.minimum;
        g_touch_x_max = abs_x.maximum;
        g_touch_y_min = abs_y.minimum;
        g_touch_y_max = abs_y.maximum;
        LOG("Touch MT: X=%d-%d, Y=%d-%d\n", 
               g_touch_x_min, g_touch_x_max, g_touch_y_min, g_touch_y_max);
    } else if (ioctl(g_touch_fd, EVIOCGABS(ABS_X), &abs_x) == 0 &&
               ioctl(g_touch_fd, EVIOCGABS(ABS_Y), &abs_y) == 0) {
        g_touch_x_min = abs_x.minimum;
        g_touch_x_max = abs_x.maximum;
        g_touch_y_min = abs_y.minimum;
        g_touch_y_max = abs_y.maximum;
        LOG("Touch ABS: X=%d-%d, Y=%d-%d\n",
               g_touch_x_min, g_touch_x_max, g_touch_y_min, g_touch_y_max);
    }

    LOG("Touch initialized\n");
    return 0;
}

static void touch_deinit(void) {
    if (g_touch_fd >= 0) {
        close(g_touch_fd);
        g_touch_fd = -1;
    }
}

/*====================
   UI HELPERS
 *====================*/
static void update_button_color(role_t *role) {
    lv_color_t color;

    switch (role->state) {
        case ROLE_STATE_SELECTED:
            color = COLOR_YELLOW;
            break;
        case ROLE_STATE_CHECKED_IN:
            color = COLOR_CHECKED;
            break;
        case ROLE_STATE_UNSELECTED:
        default:
            color = THEME_ROLE_UNSELECTED_BG;
            break;
    }

    lv_obj_set_style_bg_color(role->btn, color, LV_PART_MAIN);
    lv_obj_invalidate(role->btn);  /* Force redraw of this button */
}

static void update_status_label(void) {
    char status[256] = "";
    const char *tag_str = (g_last_tag_id[0] != '\0') ? g_last_tag_id : "";

    LOG("UI: update_status_label called, g_nfc_ready=%d, tag=%s\n", g_nfc_ready, tag_str);

    /* Build status string */
    if (tag_str[0] == '\0') {
        if (g_nfc_ready) {
            snprintf(status, sizeof(status), "Waiting for NFC card...");
        } else {
            snprintf(status, sizeof(status), "Initializing NFC...");
        }
    } else {
        snprintf(status, sizeof(status), "Tag: %s", tag_str);
    }

    lv_label_set_text(g_status_label, status);
    LOG("UI: set label text to: '%s'\n", status);
}

/*====================
   BUTTON CALLBACKS
 *====================*/
static void role_btn_event_cb(lv_event_t *e) {
    role_t *role = (role_t *)lv_event_get_user_data(e);

    /* Note: No mutex needed here - we're called from lv_timer_handler() 
       which already holds the mutex via the main loop */

    /*
    LOG("UI: button clicked: state %d -> %d\n", role->state, 
        (role->state == ROLE_STATE_UNSELECTED) ? ROLE_STATE_SELECTED :
        (role->state == ROLE_STATE_SELECTED) ? ROLE_STATE_UNSELECTED : role->state);
    */

    if (role->state == ROLE_STATE_UNSELECTED) {
        role->state = ROLE_STATE_SELECTED;
    } else if (role->state == ROLE_STATE_SELECTED) {
        role->state = ROLE_STATE_UNSELECTED;
    }
    /* CHECKED_IN state: button click does nothing (need same tag to check out) */

    update_button_color(role);
    update_status_label();
    
    /* Force immediate display refresh */
    lv_refr_now(g_display);
}

/*====================
   NFC TAG HANDLING
 *====================*/
/* Convert NFC protocol value to human-readable string */
static const char* protocol_to_string(uint8_t protocol) {
    switch (protocol) {
        case 0x01: return "T1T";
        case 0x02: return "T2T";
        case 0x03: return "T3T";
        case 0x04: return "ISO-DEP";
        case 0x06: return "ISO15693";
        case 0x80: return "MIFARE";
        default:   return "UNKNOWN";
    }
}

static void format_tag_id(nfc_tag_info_t *tag, char *buf, size_t buflen) {
    size_t pos = 0;
    for (unsigned int i = 0; i < tag->uid_length && pos < buflen - 3; i++) {
        pos += snprintf(buf + pos, buflen - pos, "%02X",
                        (unsigned char)tag->uid[i]);
        if (i < tag->uid_length - 1 && pos < buflen - 1) {
            buf[pos++] = ':';
        }
    }
    buf[pos] = '\0';
}

static void process_tag_discovery(const char *tag_id, uint8_t protocol) {
    int same_tag = (strcmp(tag_id, g_last_tag_id) == 0);

    LOG("NFC: process_tag_discovery tag=%s, type=%s, same=%d\n", tag_id, protocol_to_string(protocol), same_tag);

    /* Process all roles */
    for (int i = 0; i < NUM_ROLES; i++) {
        role_t *role = &g_roles[i];
        if (role->state == ROLE_STATE_CHECKED_IN) {
            if (same_tag && strcmp(role->tag_id, tag_id) == 0) {
                /* Same tag -> check out */
                role->state = ROLE_STATE_UNSELECTED;
                role->tag_id[0] = '\0';
            }
        } else if (role->state == ROLE_STATE_SELECTED) {
            /* Selected -> check in */
            role->state = ROLE_STATE_CHECKED_IN;
            strncpy(role->tag_id, tag_id, sizeof(role->tag_id) - 1);
        }
    }

    /* Update last seen tag */
    strncpy(g_last_tag_id, tag_id, sizeof(g_last_tag_id) - 1);

    /* Queue tag event for MQTT publish from main thread */
    mqtt_queue_tag(tag_id, protocol);

    /* Signal main loop to refresh UI */
    g_ui_needs_refresh = 1;
}

/*====================
   NFC CALLBACKS
 *====================*/
static void on_tag_arrival(nfc_tag_info_t *tag_info) {
    LOG("NFC: on_tag_arrival called, tag_info=%p\n", (void*)tag_info);
    if (!tag_info) return;
    char tag_id[64];
    format_tag_id(tag_info, tag_id, sizeof(tag_id));
    LOG("NFC: Tag detected: %s (uid_len=%u, protocol=0x%02X)\n", tag_id, tag_info->uid_length, tag_info->protocol);
    process_tag_discovery(tag_id, tag_info->protocol);
}

static void on_tag_departure(void) {
    /* Nothing to do on tag removal */
}

static nfcTagCallback_t g_nfc_callbacks = {
    .onTagArrival = on_tag_arrival,
    .onTagDeparture = on_tag_departure
};

/*====================
   NFC THREAD
 *====================*/
static void *nfc_thread(void *arg) {
    (void)arg;
    int res;

    /* Give the UI time to render first frame */
    sleep(2);

    res = nfcManager_doInitialize();
    if (res != 0) {
        fprintf(stderr, "NFC init failed: %d\n", res);
        fflush(stderr);
        pthread_mutex_lock(&g_ui_mutex);
        lv_label_set_text(g_status_label, "NFC Init Failed!\nCheck hardware.");
        pthread_mutex_unlock(&g_ui_mutex);
        return NULL;
    } else{
        LOG("NFC: Initialized NFC\n");
    }

    /*LOG("NFC: Registering callbacks...\n");*/
    nfcManager_registerTagCallback(&g_nfc_callbacks);
    
    /*LOG("NFC: Enabling discovery (reader_only=0)...\n");*/
    nfcManager_enableDiscovery(DEFAULT_NFA_TECH_MASK, 0, 0, 0);
    
    LOG("NFC: Enabled NFC discovery\n");

    /* Mark NFC as ready - main loop will refresh UI */
    g_nfc_ready = 1;
    g_ui_needs_refresh = 1;

    /* Keep thread alive while polling */
    while (g_running) {
        sleep(1);
    }

    nfcManager_disableDiscovery();
    nfcManager_deregisterTagCallback();
    nfcManager_doDeinitialize();

    return NULL;
}

/*====================
   UI SETUP
 *====================*/

/* Forward declarations for theme functions */
static void apply_theme(void);

/* Apply current theme colors to all UI elements */
static void apply_theme(void) {
    lv_obj_t *scr = lv_screen_active();
    
    LOG("UI: Applying theme %d\n", g_current_theme);
    
    /* Screen background */
    lv_obj_set_style_bg_color(scr, THEME_BG, LV_PART_MAIN);
    
    /* Landing page */
    if (g_landing_container) {
        lv_obj_set_style_bg_color(g_landing_container, THEME_BG, LV_PART_MAIN);
    }
    if (g_landing_title) {
        lv_obj_set_style_text_color(g_landing_title, THEME_TEXT, LV_PART_MAIN);
    }
    if (g_btn_simple_checkin) {
        lv_obj_set_style_bg_color(g_btn_simple_checkin, THEME_BTN_DEFAULT, LV_PART_MAIN);
        lv_obj_t *lbl = lv_obj_get_child(g_btn_simple_checkin, 0);
        if (lbl) lv_obj_set_style_text_color(lbl, THEME_BTN_TEXT, LV_PART_MAIN);
    }
    if (g_btn_roles_booking) {
        lv_obj_set_style_bg_color(g_btn_roles_booking, THEME_BTN_DEFAULT, LV_PART_MAIN);
        lv_obj_t *lbl = lv_obj_get_child(g_btn_roles_booking, 0);
        if (lbl) lv_obj_set_style_text_color(lbl, THEME_BTN_TEXT, LV_PART_MAIN);
    }
    if (g_btn_ev_charging) {
        lv_obj_set_style_bg_color(g_btn_ev_charging, THEME_BTN_DEFAULT, LV_PART_MAIN);
        lv_obj_t *lbl = lv_obj_get_child(g_btn_ev_charging, 0);
        if (lbl) lv_obj_set_style_text_color(lbl, THEME_BTN_TEXT, LV_PART_MAIN);
    }
    if (g_btn_settings) {
        lv_obj_set_style_bg_color(g_btn_settings, THEME_BTN_DEFAULT, LV_PART_MAIN);
    }
    if (g_btn_back) {
        lv_obj_set_style_bg_color(g_btn_back, THEME_BTN_DEFAULT, LV_PART_MAIN);
    }
    
    /* Settings modal */
    if (g_settings_modal) {
        lv_obj_set_style_bg_color(g_settings_modal, THEME_MODAL_BG, LV_PART_MAIN);
        lv_obj_set_style_border_color(g_settings_modal, THEME_BTN_DEFAULT, LV_PART_MAIN);
    }
    if (g_settings_title) {
        lv_obj_set_style_text_color(g_settings_title, THEME_TEXT, LV_PART_MAIN);
    }
    if (g_settings_lbl_mac) {
        lv_obj_set_style_text_color(g_settings_lbl_mac, THEME_TEXT, LV_PART_MAIN);
    }
    if (g_settings_lbl_ip) {
        lv_obj_set_style_text_color(g_settings_lbl_ip, THEME_TEXT, LV_PART_MAIN);
    }
    if (g_settings_lbl_mqtt) {
        lv_obj_set_style_text_color(g_settings_lbl_mqtt, THEME_TEXT, LV_PART_MAIN);
    }
    
    /* Update theme toggle button styles (not the selected one) */
    if (g_btn_theme_contrast) {
        lv_obj_set_style_bg_color(g_btn_theme_contrast, 
            g_current_theme == THEME_HIGH_CONTRAST ? COLOR_YELLOW : THEME_BTN_DEFAULT, LV_PART_MAIN);
        lv_obj_t *lbl = lv_obj_get_child(g_btn_theme_contrast, 0);
        if (lbl) lv_obj_set_style_text_color(lbl, 
            g_current_theme == THEME_HIGH_CONTRAST ? lv_color_black() : THEME_BTN_TEXT, LV_PART_MAIN);
    }
    if (g_btn_theme_dark) {
        lv_obj_set_style_bg_color(g_btn_theme_dark, 
            g_current_theme == THEME_DARK_MOCHA ? COLOR_YELLOW : THEME_BTN_DEFAULT, LV_PART_MAIN);
        lv_obj_t *lbl = lv_obj_get_child(g_btn_theme_dark, 0);
        if (lbl) lv_obj_set_style_text_color(lbl, 
            g_current_theme == THEME_DARK_MOCHA ? lv_color_black() : THEME_BTN_TEXT, LV_PART_MAIN);
    }
    if (g_btn_theme_light) {
        lv_obj_set_style_bg_color(g_btn_theme_light, 
            g_current_theme == THEME_LIGHT_LATTE ? COLOR_YELLOW : THEME_BTN_DEFAULT, LV_PART_MAIN);
        lv_obj_t *lbl = lv_obj_get_child(g_btn_theme_light, 0);
        if (lbl) lv_obj_set_style_text_color(lbl, 
            g_current_theme == THEME_LIGHT_LATTE ? lv_color_black() : THEME_BTN_TEXT, LV_PART_MAIN);
    }
    
    /* Roles booking page */
    if (g_header) {
        lv_obj_set_style_bg_color(g_header, THEME_HEADER, LV_PART_MAIN);
    }
    if (g_status_label) {
        lv_obj_set_style_text_color(g_status_label, THEME_TEXT_SECONDARY, LV_PART_MAIN);
    }
    if (g_roles_container) {
        lv_obj_set_style_bg_color(g_roles_container, THEME_BG, LV_PART_MAIN);
    }
    
    /* Role buttons - only update unselected ones */
    for (int i = 0; i < NUM_ROLES; i++) {
        if (g_roles[i].btn) {
            if (g_roles[i].state == ROLE_STATE_UNSELECTED) {
                lv_obj_set_style_bg_color(g_roles[i].btn, THEME_ROLE_UNSELECTED_BG, LV_PART_MAIN);
                lv_obj_set_style_border_color(g_roles[i].btn, THEME_ROLE_UNSELECTED_BG, LV_PART_MAIN);
            }
            if (g_roles[i].label) {
                lv_obj_set_style_text_color(g_roles[i].label, THEME_ROLE_UNSELECTED_TEXT, LV_PART_MAIN);
            }
        }
    }
    
    lv_obj_invalidate(scr);
    lv_refr_now(g_display);
}

/* Helper to update theme button styles */
static void update_theme_button_styles(void) {
    if (!g_btn_theme_contrast || !g_btn_theme_dark || !g_btn_theme_light) return;
    
    /* Set all buttons to default (not selected) */
    lv_obj_set_style_bg_color(g_btn_theme_contrast, COLOR_GREY, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_btn_theme_dark, COLOR_GREY, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_btn_theme_light, COLOR_GREY, LV_PART_MAIN);
    
    /* Update label colors for non-selected buttons */
    lv_obj_t *lbl;
    lbl = lv_obj_get_child(g_btn_theme_contrast, 0);
    if (lbl) lv_obj_set_style_text_color(lbl, THEME_BTN_TEXT, LV_PART_MAIN);
    lbl = lv_obj_get_child(g_btn_theme_dark, 0);
    if (lbl) lv_obj_set_style_text_color(lbl, THEME_BTN_TEXT, LV_PART_MAIN);
    lbl = lv_obj_get_child(g_btn_theme_light, 0);
    if (lbl) lv_obj_set_style_text_color(lbl, THEME_BTN_TEXT, LV_PART_MAIN);
    
    /* Highlight the selected button */
    switch (g_current_theme) {
        case THEME_HIGH_CONTRAST:
            lv_obj_set_style_bg_color(g_btn_theme_contrast, COLOR_YELLOW, LV_PART_MAIN);
            lbl = lv_obj_get_child(g_btn_theme_contrast, 0);
            if (lbl) lv_obj_set_style_text_color(lbl, lv_color_black(), LV_PART_MAIN);
            break;
        case THEME_DARK_MOCHA:
            lv_obj_set_style_bg_color(g_btn_theme_dark, COLOR_YELLOW, LV_PART_MAIN);
            lbl = lv_obj_get_child(g_btn_theme_dark, 0);
            if (lbl) lv_obj_set_style_text_color(lbl, lv_color_black(), LV_PART_MAIN);
            break;
        case THEME_LIGHT_LATTE:
            lv_obj_set_style_bg_color(g_btn_theme_light, COLOR_YELLOW, LV_PART_MAIN);
            lbl = lv_obj_get_child(g_btn_theme_light, 0);
            if (lbl) lv_obj_set_style_text_color(lbl, lv_color_black(), LV_PART_MAIN);
            break;
    }
    
    lv_obj_invalidate(g_btn_theme_contrast);
    lv_obj_invalidate(g_btn_theme_dark);
    lv_obj_invalidate(g_btn_theme_light);
}

/* Theme button callback */
static void theme_btn_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code != LV_EVENT_CLICKED) {
        return;
    }
    
    if (g_theme_locked) {
        LOG("theme_btn_cb: Theme is locked, ignoring change\n");
        return;
    }
    
    lv_obj_t *btn = lv_event_get_target(e);
    
    if (btn == g_btn_theme_contrast) {
        g_current_theme = THEME_HIGH_CONTRAST;
        g_theme = &g_theme_high_contrast;
        LOG("UI: Selected High Contrast\n");
    } else if (btn == g_btn_theme_dark) {
        g_current_theme = THEME_DARK_MOCHA;
        g_theme = &g_theme_dark_mocha;
        LOG("UI: Selected Dark Mocha\n");
    } else if (btn == g_btn_theme_light) {
        g_current_theme = THEME_LIGHT_LATTE;
        g_theme = &g_theme_light_latte;
        LOG("UI: Selected Light Latte\n");
    }
    
    update_theme_button_styles();
    apply_theme();
    lv_refr_now(NULL);
}

/* Show/hide pages based on current page */
static void show_page(app_page_t page) {
    LOG("UI: Switching to page %d\n", page);
    
    /* Lock theme when leaving landing page */
    if (g_current_page == PAGE_LANDING && page != PAGE_LANDING) {
        g_theme_locked = true;
        LOG("UI: Theme locked to %d\n", g_current_theme);
    }
    
    g_current_page = page;
    
    /* Hide all pages first */
    if (g_landing_container) lv_obj_add_flag(g_landing_container, LV_OBJ_FLAG_HIDDEN);
    if (g_roles_container) lv_obj_add_flag(g_roles_container, LV_OBJ_FLAG_HIDDEN);
    if (g_header) lv_obj_add_flag(g_header, LV_OBJ_FLAG_HIDDEN);
    
    /* Show the selected page */
    switch (page) {
        case PAGE_LANDING:
            LOG("UI: Showing landing page\n");
            if (g_landing_container) lv_obj_remove_flag(g_landing_container, LV_OBJ_FLAG_HIDDEN);
            break;
        case PAGE_ROLES_BOOKING:
            LOG("UI: Showing roles booking page\n");
            if (g_header) lv_obj_remove_flag(g_header, LV_OBJ_FLAG_HIDDEN);
            if (g_roles_container) lv_obj_remove_flag(g_roles_container, LV_OBJ_FLAG_HIDDEN);
            break;
        case PAGE_SIMPLE_CHECKIN:
            LOG("UI: Showing simple check-in page (not implemented)\n");
            /* Not implemented yet - stay on landing */
            if (g_landing_container) lv_obj_remove_flag(g_landing_container, LV_OBJ_FLAG_HIDDEN);
            break;
        case PAGE_EV_CHARGING:
            LOG("UI: Showing EV charging page (not implemented)\n");
            /* Not implemented yet - stay on landing */
            if (g_landing_container) lv_obj_remove_flag(g_landing_container, LV_OBJ_FLAG_HIDDEN);
            break;
    }
    
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(g_display);
}

/* Generic button press effect callback */
static void btn_press_effect_cb(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        lv_obj_set_style_bg_color(btn, COLOR_PRESSED, LV_PART_MAIN);
        lv_obj_invalidate(btn);
        lv_refr_now(g_display);
        LOG("UI: Button PRESSED - color changed to PRESSED\n");
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        lv_obj_set_style_bg_color(btn, COLOR_GREY, LV_PART_MAIN);
        lv_obj_invalidate(btn);
        lv_refr_now(g_display);
        LOG("UI: Button RELEASED - color changed to GREY\n");
    }
}

/* Press effect callback for modal close button - no color change, just stays modal bg */
static void modal_close_btn_press_effect_cb(lv_event_t *e) {
    (void)e;
    /* No visual press effect for X button - keeps modal background color */
}

/* Landing page button callbacks */
static void landing_btn_simple_checkin_cb(lv_event_t *e) {
    (void)e;
    LOG("Landing: Check-in/Check-out selected (not implemented)\n");
    /* Not implemented yet */
}

static void landing_btn_roles_booking_cb(lv_event_t *e) {
    (void)e;
    LOG("UI: Landing page - Roles Booking selected\n");
    show_page(PAGE_ROLES_BOOKING);
}

static void landing_btn_ev_charging_cb(lv_event_t *e) {
    (void)e;
    LOG("UI: Landing page - EV Charging selected (not implemented)\n");
    /* Not implemented yet */
}

static void back_btn_cb(lv_event_t *e) {
    (void)e;
    LOG("UI: Back button pressed - returning to landing page\n");
    g_theme_locked = false;  /* Unlock theme when returning to landing */
    show_page(PAGE_LANDING);
}

/* Get IP address of network interface */
static void get_ip_address(char *buf, size_t buflen) {
    struct ifaddrs *ifaddr, *ifa;
    
    if (getifaddrs(&ifaddr) == -1) {
        snprintf(buf, buflen, "N/A");
        return;
    }
    
    /* Walk through linked list of interfaces */
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;
        
        /* Only interested in IPv4 addresses */
        if (ifa->ifa_addr->sa_family != AF_INET)
            continue;
        
        /* Skip loopback interface */
        if (strcmp(ifa->ifa_name, "lo") == 0)
            continue;
        
        struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
        inet_ntop(AF_INET, &addr->sin_addr, buf, buflen);
        freeifaddrs(ifaddr);
        return;
    }
    
    freeifaddrs(ifaddr);
    snprintf(buf, buflen, "N/A");
}

/* Update settings modal info labels */
static void update_settings_info(void) {
    if (!g_settings_modal) return;
    
    char buf[128];
    char ip[32];
    
    get_ip_address(ip, sizeof(ip));
    
    if (g_settings_lbl_mac) {
        snprintf(buf, sizeof(buf), "MAC: %s", g_device_mac[0] ? g_device_mac : "N/A");
        lv_label_set_text(g_settings_lbl_mac, buf);
    }
    if (g_settings_lbl_ip) {
        snprintf(buf, sizeof(buf), "IP: %s", ip);
        lv_label_set_text(g_settings_lbl_ip, buf);
    }
    if (g_settings_lbl_mqtt) {
        snprintf(buf, sizeof(buf), "MQTT: %s", MQTT_ADDRESS);
        lv_label_set_text(g_settings_lbl_mqtt, buf);
    }
}

/* Settings button callback - open settings modal */
static void settings_btn_cb(lv_event_t *e) {
    (void)e;
    LOG("UI: Settings button pressed\n");
    if (g_settings_modal) {
        update_settings_info();
        lv_obj_remove_flag(g_settings_modal, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(g_settings_modal);  /* Ensure modal is on top */
        lv_refr_now(g_display);
    }
}

/* Settings close button callback */
static void settings_close_btn_cb(lv_event_t *e) {
    (void)e;
    LOG("UI: Settings modal closed\n");
    if (g_settings_modal) {
        lv_obj_add_flag(g_settings_modal, LV_OBJ_FLAG_HIDDEN);
        /* Reset input device to clear any stale press state */
        if (g_touch_indev) {
            lv_indev_reset(g_touch_indev, NULL);
        }
        lv_refr_now(g_display);
    }
}

static void create_ui(void) {
    LOG("UI: Starting UI creation...\n");

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    LOG("UI: Creating landing page...\n");

    g_landing_container = lv_obj_create(scr);
    lv_obj_remove_style_all(g_landing_container);
    lv_obj_set_size(g_landing_container, 720, 720);
    lv_obj_set_pos(g_landing_container, 0, 0);
    lv_obj_set_style_bg_color(g_landing_container, COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_landing_container, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(g_landing_container, LV_OBJ_FLAG_SCROLLABLE);  /* Disable scrolling */

    /* Landing page title */
    g_landing_title = lv_label_create(g_landing_container);
    lv_label_set_text(g_landing_title, "NFC Terminal Demo");
    lv_obj_set_style_text_color(g_landing_title, COLOR_TEXT, LV_PART_MAIN);
    lv_obj_set_style_text_font(g_landing_title, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_align(g_landing_title, LV_ALIGN_TOP_MID, 0, 32);

    /* Settings button (hamburger menu) in top right corner */
    g_btn_settings = lv_button_create(g_landing_container);
    lv_obj_set_size(g_btn_settings, 88, 88);
    lv_obj_align(g_btn_settings, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_set_style_bg_color(g_btn_settings, COLOR_GREY, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_btn_settings, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(g_btn_settings, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_btn_settings, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(g_btn_settings, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(g_btn_settings, btn_press_effect_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(g_btn_settings, btn_press_effect_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(g_btn_settings, btn_press_effect_cb, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_add_event_cb(g_btn_settings, settings_btn_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *lbl_settings = lv_label_create(g_btn_settings);
    lv_label_set_text(lbl_settings, LV_SYMBOL_LIST);  /* Hamburger menu icon */
    lv_obj_set_style_text_color(lbl_settings, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_settings, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_center(lbl_settings);

    /* Landing page buttons - vertically centered */
    const int landing_btn_width = 400;
    const int landing_btn_height = 80;
    const int landing_btn_gap = 20;
    const int landing_total_height = 3 * landing_btn_height + 2 * landing_btn_gap;
    const int landing_start_y = (720 - landing_total_height) / 2 - 4;

    /* Button 1: Check-in/Check-out */
    g_btn_simple_checkin = lv_button_create(g_landing_container);
    lv_obj_set_size(g_btn_simple_checkin, landing_btn_width, landing_btn_height);
    lv_obj_set_pos(g_btn_simple_checkin, (720 - landing_btn_width) / 2, landing_start_y);
    lv_obj_set_style_bg_color(g_btn_simple_checkin, COLOR_GREY, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_btn_simple_checkin, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(g_btn_simple_checkin, 12, LV_PART_MAIN);
    lv_obj_add_event_cb(g_btn_simple_checkin, btn_press_effect_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(g_btn_simple_checkin, btn_press_effect_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(g_btn_simple_checkin, btn_press_effect_cb, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_add_event_cb(g_btn_simple_checkin, landing_btn_simple_checkin_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *lbl1 = lv_label_create(g_btn_simple_checkin);
    lv_label_set_text(lbl1, "Check-in/Check-out");
    lv_obj_set_style_text_font(lbl1, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl1, THEME_BTN_TEXT, LV_PART_MAIN);
    lv_obj_center(lbl1);

    /* Button 2: Roles Booking */
    g_btn_roles_booking = lv_button_create(g_landing_container);
    lv_obj_set_size(g_btn_roles_booking, landing_btn_width, landing_btn_height);
    lv_obj_set_pos(g_btn_roles_booking, (720 - landing_btn_width) / 2, landing_start_y + landing_btn_height + landing_btn_gap);
    lv_obj_set_style_bg_color(g_btn_roles_booking, COLOR_GREY, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_btn_roles_booking, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(g_btn_roles_booking, 12, LV_PART_MAIN);
    lv_obj_add_event_cb(g_btn_roles_booking, btn_press_effect_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(g_btn_roles_booking, btn_press_effect_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(g_btn_roles_booking, btn_press_effect_cb, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_add_event_cb(g_btn_roles_booking, landing_btn_roles_booking_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *lbl2 = lv_label_create(g_btn_roles_booking);
    lv_label_set_text(lbl2, "Roles Booking");
    lv_obj_set_style_text_font(lbl2, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl2, THEME_BTN_TEXT, LV_PART_MAIN);
    lv_obj_center(lbl2);

    /* Button 3: EV Charging */
    g_btn_ev_charging = lv_button_create(g_landing_container);
    lv_obj_set_size(g_btn_ev_charging, landing_btn_width, landing_btn_height);
    lv_obj_set_pos(g_btn_ev_charging, (720 - landing_btn_width) / 2, landing_start_y + 2 * (landing_btn_height + landing_btn_gap));
    lv_obj_set_style_bg_color(g_btn_ev_charging, COLOR_GREY, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_btn_ev_charging, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(g_btn_ev_charging, 12, LV_PART_MAIN);
    lv_obj_add_event_cb(g_btn_ev_charging, btn_press_effect_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(g_btn_ev_charging, btn_press_effect_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(g_btn_ev_charging, btn_press_effect_cb, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_add_event_cb(g_btn_ev_charging, landing_btn_ev_charging_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *lbl3 = lv_label_create(g_btn_ev_charging);
    lv_label_set_text(lbl3, "EV Charging");
    lv_obj_set_style_text_font(lbl3, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl3, THEME_BTN_TEXT, LV_PART_MAIN);
    lv_obj_center(lbl3);

    /*========================================
       ROLES BOOKING APP
     *========================================*/
    /* Header: 720x120 at top */
    g_header = lv_obj_create(scr);
    lv_obj_remove_style_all(g_header);
    lv_obj_set_size(g_header, 720, 120);
    lv_obj_set_pos(g_header, 0, 0);
    lv_obj_set_style_bg_color(g_header, COLOR_HEADER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_header, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_left(g_header, 16, LV_PART_MAIN);

    /* Status label in header, left-aligned, vertically centered */
    g_status_label = lv_label_create(g_header);
    lv_label_set_text(g_status_label, "Initializing NFC...");
    lv_obj_set_style_text_color(g_status_label, COLOR_LIGHT_GREY, LV_PART_MAIN);
    lv_obj_set_style_text_font(g_status_label, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_text_align(g_status_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_width(g_status_label, 600);
    lv_obj_align(g_status_label, LV_ALIGN_LEFT_MID, 0, 0);

    /* Back button in header, top-right corner */
    g_btn_back = lv_button_create(g_header);
    lv_obj_set_size(g_btn_back, 96, 96);
    lv_obj_align(g_btn_back, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_obj_set_style_bg_color(g_btn_back, COLOR_GREY, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_btn_back, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(g_btn_back, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_btn_back, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(g_btn_back, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(g_btn_back, btn_press_effect_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(g_btn_back, btn_press_effect_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(g_btn_back, btn_press_effect_cb, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_add_event_cb(g_btn_back, back_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_back = lv_label_create(g_btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT);  /* FA f08b placeholder */
    lv_obj_set_style_text_color(lbl_back, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_center(lbl_back);

    /* Roles container - holds all role buttons */
    g_roles_container = lv_obj_create(scr);
    lv_obj_remove_style_all(g_roles_container);
    lv_obj_set_size(g_roles_container, 720, 600);
    lv_obj_set_pos(g_roles_container, 0, 120);
    lv_obj_set_style_bg_color(g_roles_container, COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_roles_container, LV_OPA_COVER, LV_PART_MAIN);

    /* Button dimensions: 8px gaps, 4 buttons per row */
    const int btn_width = 170;
    const int btn_height = 90;
    const int gap = 8;
    const int row1_y = 8;  /* First row in container */
    const int row2_y = row1_y + btn_height + gap;

    /* Create 8 buttons in 2 rows */
    for (int i = 0; i < NUM_ROLES; i++) {
        int row = i / 4;
        int col = i % 4;
        int x = gap + col * (btn_width + gap);
        int y = (row == 0) ? row1_y : row2_y;

        g_roles[i].btn = lv_button_create(g_roles_container);
        lv_obj_set_size(g_roles[i].btn, btn_width, btn_height);
        lv_obj_set_pos(g_roles[i].btn, x, y);
        lv_obj_set_style_bg_color(g_roles[i].btn, THEME_ROLE_UNSELECTED_BG, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(g_roles[i].btn, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(g_roles[i].btn, 8, LV_PART_MAIN);
        lv_obj_set_style_border_color(g_roles[i].btn, THEME_ROLE_UNSELECTED_BG, LV_PART_MAIN);
        lv_obj_set_style_border_width(g_roles[i].btn, 4, LV_PART_MAIN);
        lv_obj_set_style_border_opa(g_roles[i].btn, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_add_event_cb(g_roles[i].btn, role_btn_event_cb, LV_EVENT_CLICKED, &g_roles[i]);

        g_roles[i].label = lv_label_create(g_roles[i].btn);
        lv_label_set_text(g_roles[i].label, g_role_names[i]);
        lv_obj_set_style_text_font(g_roles[i].label, &lv_font_montserrat_28, LV_PART_MAIN);
        lv_obj_set_style_text_color(g_roles[i].label, THEME_ROLE_UNSELECTED_TEXT, LV_PART_MAIN);
        lv_obj_center(g_roles[i].label);
    }

    /* Touch debug label at bottom (hidden by default, shows touch status) */
    g_touch_debug_label = lv_label_create(scr);
    lv_label_set_text(g_touch_debug_label, "");
    lv_obj_set_style_text_color(g_touch_debug_label, lv_color_hex(0x808080), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_touch_debug_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_align(g_touch_debug_label, LV_ALIGN_BOTTOM_MID, 0, -11);

    /*========================================
       SETTINGS MODAL
     *========================================*/
    g_settings_modal = lv_obj_create(scr);
    lv_obj_set_size(g_settings_modal, 700, 600);
    lv_obj_align(g_settings_modal, LV_ALIGN_CENTER, 0, 48);  /* 48px down from center */
    lv_obj_set_style_bg_color(g_settings_modal, THEME_MODAL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_settings_modal, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(g_settings_modal, 16, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_settings_modal, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_settings_modal, COLOR_GREY, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(g_settings_modal, 20, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(g_settings_modal, LV_OPA_50, LV_PART_MAIN);
    lv_obj_remove_flag(g_settings_modal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_settings_modal, LV_OBJ_FLAG_HIDDEN);  /* Start hidden */

    /* Settings title */
    g_settings_title = lv_label_create(g_settings_modal);
    lv_label_set_text(g_settings_title, "Settings");
    lv_obj_set_style_text_color(g_settings_title, COLOR_TEXT, LV_PART_MAIN);
    lv_obj_set_style_text_font(g_settings_title, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_align(g_settings_title, LV_ALIGN_TOP_MID, 0, 10);

    /* Close button (X) */
    lv_obj_t *btn_close = lv_button_create(g_settings_modal);
    lv_obj_set_size(btn_close, 84, 84);
    lv_obj_align(btn_close, LV_ALIGN_TOP_RIGHT, 16, -16);
    lv_obj_set_style_bg_color(btn_close, THEME_MODAL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn_close, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(btn_close, 8, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_close, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn_close, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(btn_close, modal_close_btn_press_effect_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(btn_close, modal_close_btn_press_effect_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(btn_close, modal_close_btn_press_effect_cb, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_add_event_cb(btn_close, settings_close_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_close = lv_label_create(btn_close);
    lv_label_set_text(lbl_close, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(lbl_close, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_close, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_center(lbl_close);

    /* Device info labels */
    const int info_start_y = 76;
    const int info_line_height = 35;

    g_settings_lbl_mac = lv_label_create(g_settings_modal);
    lv_label_set_text(g_settings_lbl_mac, "MAC: --");
    lv_obj_set_style_text_color(g_settings_lbl_mac, COLOR_TEXT, LV_PART_MAIN);
    lv_obj_set_style_text_font(g_settings_lbl_mac, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_align(g_settings_lbl_mac, LV_ALIGN_TOP_LEFT, 20, info_start_y);

    g_settings_lbl_ip = lv_label_create(g_settings_modal);
    lv_label_set_text(g_settings_lbl_ip, "IP: --");
    lv_obj_set_style_text_color(g_settings_lbl_ip, COLOR_TEXT, LV_PART_MAIN);
    lv_obj_set_style_text_font(g_settings_lbl_ip, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_align(g_settings_lbl_ip, LV_ALIGN_TOP_LEFT, 20, info_start_y + info_line_height);

    g_settings_lbl_mqtt = lv_label_create(g_settings_modal);
    lv_label_set_text(g_settings_lbl_mqtt, "MQTT: --");
    lv_obj_set_style_text_color(g_settings_lbl_mqtt, COLOR_TEXT, LV_PART_MAIN);
    lv_obj_set_style_text_font(g_settings_lbl_mqtt, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_align(g_settings_lbl_mqtt, LV_ALIGN_TOP_LEFT, 20, info_start_y + 2 * info_line_height);

    /* Theme selector buttons */
    const int theme_btn_size = 84;
    const int theme_btn_gap = 20;
    const int theme_btn_y = -30;

    /* Button 1: High Contrast */
    g_btn_theme_contrast = lv_button_create(g_settings_modal);
    lv_obj_set_size(g_btn_theme_contrast, theme_btn_size, theme_btn_size);
    lv_obj_align(g_btn_theme_contrast, LV_ALIGN_BOTTOM_MID, -(theme_btn_size + theme_btn_gap), theme_btn_y);
    lv_obj_set_style_bg_color(g_btn_theme_contrast, COLOR_YELLOW, LV_PART_MAIN);  /* Selected by default */
    lv_obj_set_style_bg_opa(g_btn_theme_contrast, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(g_btn_theme_contrast, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_btn_theme_contrast, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(g_btn_theme_contrast, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(g_btn_theme_contrast, btn_press_effect_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(g_btn_theme_contrast, btn_press_effect_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(g_btn_theme_contrast, btn_press_effect_cb, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_add_event_cb(g_btn_theme_contrast, theme_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_contrast = lv_label_create(g_btn_theme_contrast);
    lv_label_set_text(lbl_contrast, LV_SYMBOL_IMAGE);  /* Contrast-like icon */
    lv_obj_set_style_text_color(lbl_contrast, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_contrast, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_center(lbl_contrast);

    /* Button 2: Dark (Mocha) */
    g_btn_theme_dark = lv_button_create(g_settings_modal);
    lv_obj_set_size(g_btn_theme_dark, theme_btn_size, theme_btn_size);
    lv_obj_align(g_btn_theme_dark, LV_ALIGN_BOTTOM_MID, 0, theme_btn_y);
    lv_obj_set_style_bg_color(g_btn_theme_dark, COLOR_GREY, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_btn_theme_dark, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(g_btn_theme_dark, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_btn_theme_dark, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(g_btn_theme_dark, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(g_btn_theme_dark, btn_press_effect_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(g_btn_theme_dark, btn_press_effect_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(g_btn_theme_dark, btn_press_effect_cb, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_add_event_cb(g_btn_theme_dark, theme_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_dark = lv_label_create(g_btn_theme_dark);
    lv_label_set_text(lbl_dark, LV_SYMBOL_EYE_CLOSE);  /* Moon-like (closed eye) */
    lv_obj_set_style_text_color(lbl_dark, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_dark, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_center(lbl_dark);

    /* Button 3: Light (Latte) */
    g_btn_theme_light = lv_button_create(g_settings_modal);
    lv_obj_set_size(g_btn_theme_light, theme_btn_size, theme_btn_size);
    lv_obj_align(g_btn_theme_light, LV_ALIGN_BOTTOM_MID, theme_btn_size + theme_btn_gap, theme_btn_y);
    lv_obj_set_style_bg_color(g_btn_theme_light, COLOR_GREY, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_btn_theme_light, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(g_btn_theme_light, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_btn_theme_light, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(g_btn_theme_light, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(g_btn_theme_light, btn_press_effect_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(g_btn_theme_light, btn_press_effect_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(g_btn_theme_light, btn_press_effect_cb, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_add_event_cb(g_btn_theme_light, theme_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_light = lv_label_create(g_btn_theme_light);
    lv_label_set_text(lbl_light, LV_SYMBOL_EYE_OPEN);  /* Sun-like (open eye) */
    lv_obj_set_style_text_color(lbl_light, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_light, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_center(lbl_light);

    LOG("UI: Settings modal created\n");

    /* Start on landing page */
    LOG("UI: Calling show_page(PAGE_LANDING)...\n");
    show_page(PAGE_LANDING);
    LOG("UI: UI creation complete.\n");
}

/*====================
   MQTT CLIENT (Async with Auto-Reconnect)
 *====================*/

/* Callback: Successfully connected (MQTT v5.0) */
static void mqtt_on_connect(void *context, MQTTAsync_successData5 *response) {
    (void)context;
    (void)response;
    pthread_mutex_lock(&g_mqtt_mutex);
    g_mqtt_connected = 1;
    pthread_mutex_unlock(&g_mqtt_mutex);
    LOG("MQTT: Initial connection successful to %s (MQTT v5.0)\n", MQTT_ADDRESS);
}

/* Callback: Called when connected (including auto-reconnect) */
static void mqtt_on_connected(void *context, char *cause) {
    (void)context;
    (void)cause;
    pthread_mutex_lock(&g_mqtt_mutex);
    g_mqtt_connected = 1;
    pthread_mutex_unlock(&g_mqtt_mutex);
    LOG("MQTT: Connected (cause: %s), publishing state ON\n", cause ? cause : "initial");
    
    /* Publish online state - called on both initial connect and reconnect */
    if (!g_mqtt_client) {
        LOG("MQTT: Client is NULL, cannot publish state\n");
        return;
    }
    
    static char payload[32];
    snprintf(payload, sizeof(payload), "{\"state\":\"ON\"}");
    
    MQTTAsync_message msg = MQTTAsync_message_initializer;
    msg.payload = payload;
    msg.payloadlen = (int)strlen(payload);
    msg.qos = MQTT_QOS;
    msg.retained = 1;
    
    LOG("MQTT: Publishing state ON to %s\n", g_mqtt_state_topic);
    
    int rc = MQTTAsync_sendMessage(g_mqtt_client, g_mqtt_state_topic, &msg, NULL);
    if (rc == MQTTASYNC_SUCCESS) {
        LOG("MQTT: State ON queued for publish\n");
    } else {
        LOG("MQTT: Failed to queue state ON, rc=%d\n", rc);
    }
}

/* Callback: Connection failed (MQTT v5.0) */
static void mqtt_on_connect_failure(void *context, MQTTAsync_failureData5 *response) {
    (void)context;
    pthread_mutex_lock(&g_mqtt_mutex);
    g_mqtt_connected = 0;
    pthread_mutex_unlock(&g_mqtt_mutex);
    LOG("MQTT: Connect failed, rc=%d (will auto-retry)\n", response ? response->code : -1);
}

/* Publish state message (ON/OFF) to state topic - async, non-blocking */
static void mqtt_publish_state(const char *state) {
    pthread_mutex_lock(&g_mqtt_mutex);
    int connected = g_mqtt_connected;
    pthread_mutex_unlock(&g_mqtt_mutex);
    
    if (!g_mqtt_client || !connected) {
        LOG("MQTT: Not connected, skipping state publish\n");
        return;
    }
    
    static char payload[32];  /* Static to ensure it persists during async send */
    snprintf(payload, sizeof(payload), "{\"state\":\"%s\"}", state);
    
    MQTTAsync_message msg = MQTTAsync_message_initializer;
    msg.payload = payload;
    msg.payloadlen = (int)strlen(payload);
    msg.qos = MQTT_QOS;
    msg.retained = 1;  /* Retain state message */
    
    int rc = MQTTAsync_sendMessage(g_mqtt_client, g_mqtt_state_topic, &msg, NULL);
    if (rc == MQTTASYNC_SUCCESS) {
        LOG("MQTT: Published state %s to %s\n", state, g_mqtt_state_topic);
    } else {
        LOG("MQTT: Failed to publish state, rc=%d\n", rc);
    }
}

static int mqtt_init(void) {
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer5;
    MQTTAsync_willOptions will_opts = MQTTAsync_willOptions_initializer;
    MQTTAsync_createOptions create_opts = MQTTAsync_createOptions_initializer;
    int rc;

    LOG("MQTT: mqtt_init() starting, state_topic=%s\n", g_mqtt_state_topic);

    /* Create async client with MQTT v5.0 */
    create_opts.MQTTVersion = MQTTVERSION_5;
    rc = MQTTAsync_createWithOptions(&g_mqtt_client, MQTT_ADDRESS, MQTT_CLIENT_ID,
                          MQTTCLIENT_PERSISTENCE_NONE, NULL, &create_opts);
    if (rc != MQTTASYNC_SUCCESS) {
        LOG("MQTT: Failed to create client, rc=%d\n", rc);
        return -1;
    }
    LOG("MQTT: Created async client for %s (MQTT v5.0)\n", MQTT_ADDRESS);

    /* Set connected callback - called on connect AND auto-reconnect */
    LOG("MQTT: Setting connected callback...\n");
    rc = MQTTAsync_setConnected(g_mqtt_client, NULL, mqtt_on_connected);
    if (rc != MQTTASYNC_SUCCESS) {
        LOG("MQTT: Failed to set connected callback, rc=%d\n", rc);
        MQTTAsync_destroy(&g_mqtt_client);
        g_mqtt_client = NULL;
        return -1;
    }
    LOG("MQTT: Connected callback set successfully\n");

    /* Configure Last Will and Testament (LWT) */
    will_opts.topicName = g_mqtt_state_topic;
    will_opts.message = "{\"state\":\"OFF\"}";
    will_opts.qos = MQTT_QOS;
    will_opts.retained = 1;

    /* Broker detects disconnect in 1.5x keepAlive (~15s) and publishes LWT message */
    conn_opts.MQTTVersion = MQTTVERSION_5;
    conn_opts.keepAliveInterval = 10;
    conn_opts.cleanstart = 1;  /* MQTT v5.0 uses cleanstart instead of cleansession */
    conn_opts.username = MQTT_USERNAME;
    conn_opts.password = MQTT_PASSWORD;
    conn_opts.connectTimeout = MQTT_TIMEOUT / 1000;
    conn_opts.will = &will_opts;
    conn_opts.onSuccess5 = mqtt_on_connect;
    conn_opts.onFailure5 = mqtt_on_connect_failure;
    conn_opts.automaticReconnect = 1;
    conn_opts.minRetryInterval = MQTT_RECONNECT_MIN_DELAY;
    conn_opts.maxRetryInterval = MQTT_RECONNECT_MAX_DELAY;

    LOG("MQTT: LWT configured - topic=%s, message=%s, qos=%d, retained=%d\n",
           will_opts.topicName, will_opts.message, will_opts.qos, will_opts.retained);
    LOG("MQTT: keepAlive=%ds (LWT fires after ~%ds on ungraceful disconnect)\n",
           conn_opts.keepAliveInterval, (int)(conn_opts.keepAliveInterval * 1.5));

    /* Initiate async connection */
    LOG("MQTT: Initiating connection...\n");
    rc = MQTTAsync_connect(g_mqtt_client, &conn_opts);
    if (rc != MQTTASYNC_SUCCESS) {
        LOG("MQTT: Failed to start connect, rc=%d\n", rc);
        MQTTAsync_destroy(&g_mqtt_client);
        g_mqtt_client = NULL;
        return -1;
    }

    LOG("MQTT: Connection initiated (auto-reconnect: %d-%ds)\n", 
           MQTT_RECONNECT_MIN_DELAY, MQTT_RECONNECT_MAX_DELAY);

    return 0;
}

static void mqtt_deinit(void) {
    if (g_mqtt_client) {
        pthread_mutex_lock(&g_mqtt_mutex);
        int connected = g_mqtt_connected;
        pthread_mutex_unlock(&g_mqtt_mutex);
        
        if (connected) {
            /* Publish offline state before disconnect */
            mqtt_publish_state("OFF");
            LOG("MQTT: Disconnecting...\n");
            
            MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
            opts.timeout = MQTT_TIMEOUT;
            MQTTAsync_disconnect(g_mqtt_client, &opts);
            
            pthread_mutex_lock(&g_mqtt_mutex);
            g_mqtt_connected = 0;
            pthread_mutex_unlock(&g_mqtt_mutex);
        }
        MQTTAsync_destroy(&g_mqtt_client);
        g_mqtt_client = NULL;
        LOG("MQTT: Client destroyed\n");
    }
}

static void get_mac_address(void) {
    unsigned char mac[6] = {0, 0, 0, 0, 0, 0};
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock >= 0) {
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, "eth0", IFNAMSIZ - 1);

        if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
            memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
        }
        close(sock);
    }

    snprintf(g_device_mac, sizeof(g_device_mac), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    /* Build MQTT topics: data/<MAC_NO_COLONS>/nfc and data/<MAC_NO_COLONS>/state */
    snprintf(g_mqtt_topic, sizeof(g_mqtt_topic), "data/%02X%02X%02X%02X%02X%02X/nfc",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    snprintf(g_mqtt_state_topic, sizeof(g_mqtt_state_topic), "data/%02X%02X%02X%02X%02X%02X/state",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    LOG("MQTT: State topic: %s\n", g_mqtt_state_topic);
}

static void mqtt_publish_tag_event(const char *tag_id, uint8_t protocol) {
    pthread_mutex_lock(&g_mqtt_mutex);
    int connected = g_mqtt_connected;
    pthread_mutex_unlock(&g_mqtt_mutex);

    if (!g_mqtt_client) {
        LOG("MQTT: No client, skipping publish\n");
        return;
    }

    if (!connected) {
        LOG("MQTT: Not connected, message queued for when connection resumes\n");
        /* With auto-reconnect, we can still attempt to publish - 
           it will be queued internally if supported, or fail gracefully */
    }

    /* Build tag ID without colons: "A4:FB:3B:A0" -> "A4FB3BA0" */
    char tag_no_colons[64];
    int j = 0;
    for (int i = 0; tag_id[i] && j < (int)sizeof(tag_no_colons) - 1; i++) {
        if (tag_id[i] != ':') {
            tag_no_colons[j++] = tag_id[i];
        }
    }
    tag_no_colons[j] = '\0';

    /* Build JSON payload with tagId and type */
    static char payload[128];  /* Static to persist during async send */
    snprintf(payload, sizeof(payload), "{\"tagId\":\"%s\",\"type\":\"%s\"}", tag_no_colons, protocol_to_string(protocol));

    /* Async publish - non-blocking */
    MQTTAsync_message msg = MQTTAsync_message_initializer;
    msg.payload = payload;
    msg.payloadlen = (int)strlen(payload);
    msg.qos = MQTT_QOS;
    msg.retained = 0;

    int rc = MQTTAsync_sendMessage(g_mqtt_client, g_mqtt_topic, &msg, NULL);
    if (rc == MQTTASYNC_SUCCESS) {
        LOG("MQTT: Publish queued payload='%s' to topic='%s'\n", payload, g_mqtt_topic);
    } else {
        LOG("MQTT: Failed to queue publish, rc=%d\n", rc);
    }
}

/* Queue a tag ID for publishing from main thread */
static void mqtt_queue_tag(const char *tag_id, uint8_t protocol) {
    pthread_mutex_lock(&g_mqtt_queue_mutex);

    /* Deduplicate: don't queue if same as last queued tag */
    if (g_mqtt_queue_head != g_mqtt_queue_tail) {
        int last_idx = (g_mqtt_queue_head - 1 + MQTT_QUEUE_SIZE) % MQTT_QUEUE_SIZE;
        if (strcmp(g_mqtt_queue[last_idx].tag_id, tag_id) == 0) {
            pthread_mutex_unlock(&g_mqtt_queue_mutex);
            return;  /* Skip duplicate */
        }
    }

    int next_head = (g_mqtt_queue_head + 1) % MQTT_QUEUE_SIZE;
    if (next_head != g_mqtt_queue_tail) {
        strncpy(g_mqtt_queue[g_mqtt_queue_head].tag_id, tag_id, sizeof(g_mqtt_queue[0].tag_id) - 1);
        g_mqtt_queue[g_mqtt_queue_head].tag_id[sizeof(g_mqtt_queue[0].tag_id) - 1] = '\0';
        g_mqtt_queue[g_mqtt_queue_head].protocol = protocol;
        g_mqtt_queue_head = next_head;
    } else {
        LOG("MQTT: Queue full, dropping message\n");
    }
    pthread_mutex_unlock(&g_mqtt_queue_mutex);
}

/* Process queued MQTT messages - call from main thread only */
static void mqtt_process_queue(void) {
    char tag_id[64];
    uint8_t protocol;
    while (1) {
        pthread_mutex_lock(&g_mqtt_queue_mutex);
        if (g_mqtt_queue_head == g_mqtt_queue_tail) {
            pthread_mutex_unlock(&g_mqtt_queue_mutex);
            break;
        }
        strncpy(tag_id, g_mqtt_queue[g_mqtt_queue_tail].tag_id, sizeof(tag_id) - 1);
        tag_id[sizeof(tag_id) - 1] = '\0';
        protocol = g_mqtt_queue[g_mqtt_queue_tail].protocol;
        g_mqtt_queue_tail = (g_mqtt_queue_tail + 1) % MQTT_QUEUE_SIZE;
        pthread_mutex_unlock(&g_mqtt_queue_mutex);
        
        mqtt_publish_tag_event(tag_id, protocol);
    }
}

/*====================
   SIGNAL HANDLER
 *====================*/
static void signal_handler(int sig) {
    (void)sig;
    g_running = 0;
}

/*====================
   MAIN
 *====================*/
int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    pthread_t nfc_tid;

    /* Setup signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Clear console and hide cursor */
    clear_console();

    /* Initialize framebuffer */
    if (fb_init() != 0) {
        fprintf(stderr, "Failed to initialize framebuffer\n");
        restore_console();
        return 1;
    }

    lv_init();

    /* Create display */
    g_display = lv_display_create(g_vinfo.xres, g_vinfo.yres);
    if (!g_display) {
        fprintf(stderr, "Failed to create display\n");
        fb_deinit();
        restore_console();
        return 1;
    }

    /* Setup draw buffers - use FULL rendering for cleaner display */
    static uint8_t buf1[DISPLAY_WIDTH * 40 * 2];  /* 40 lines * 2 bytes/pixel (RGB565) */
    static uint8_t buf2[DISPLAY_WIDTH * 40 * 2];
    lv_display_set_buffers(g_display, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(g_display, fb_flush_cb);

    /* Initialize touch input */
    if (touch_init() == 0) {
        g_touch_indev = lv_indev_create();
        if (g_touch_indev) {
            lv_indev_set_type(g_touch_indev, LV_INDEV_TYPE_POINTER);
            lv_indev_set_read_cb(g_touch_indev, touch_read_cb);
            lv_indev_set_display(g_touch_indev, g_display);  /* Associate with display */
            LOG("Touch input initialized\n");
        }
    } else {
        LOG("Touch: touch_init() FAILED\n");
    }

    create_ui();

    /* Force full screen refresh */
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(g_display);

    get_mac_address();

    /* Initialize MQTT client */
    if (mqtt_init() != 0) {
        LOG("Warning: Failed to initialize MQTT client, continuing without MQTT\n");
    }

    /* Start NFC thread */
    if (pthread_create(&nfc_tid, NULL, nfc_thread, NULL) != 0) {
        fprintf(stderr, "Failed to start NFC thread\n");
        touch_deinit();
        fb_deinit();
        restore_console();
        return 1;
    }

    /* Main loop */
    while (g_running) {
        /* Process MQTT queue from main thread */
        mqtt_process_queue();

        pthread_mutex_lock(&g_ui_mutex);
        
        /* Check if NFC thread requested UI refresh */
        if (g_ui_needs_refresh) {
            g_ui_needs_refresh = 0;
            update_status_label();
            for (int i = 0; i < NUM_ROLES; i++) {
                update_button_color(&g_roles[i]);
            }
            lv_obj_invalidate(lv_screen_active());
            lv_refr_now(g_display);  /* Force immediate refresh */
            LOG("UI: Refreshed from main loop\n");
        }
        
        /* Explicitly poll touch input */
        if (g_touch_indev) {
            lv_indev_read(g_touch_indev);
        }
        
        lv_timer_handler();
        pthread_mutex_unlock(&g_ui_mutex);
        usleep(10000);  /* 10ms - ~100 FPS */
    }

    /* Cleanup */
    pthread_join(nfc_tid, NULL);
    mqtt_deinit();
    touch_deinit();
    fb_deinit();
    restore_console();

    return 0;
}
