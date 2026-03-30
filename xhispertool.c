/*
 * xhisper - Whisper for Linux
 * Combined daemon and client for text input via uinput
 *
 * Uses libxkbcommon to auto-detect the active keyboard layout at startup,
 * so punctuation is typed correctly regardless of layout (US, IT, DE, etc.).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <libgen.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <linux/uinput.h>
#include <xkbcommon/xkbcommon.h>

#define KEY_LEFTCTRL 29
#define KEY_RIGHTCTRL 97
#define KEY_LEFTALT 56
#define KEY_RIGHTALT 100
#define KEY_LEFTSHIFT 42
#define KEY_RIGHTSHIFT 54
#define KEY_LEFTMETA 125
#define KEY_V 47

#define MOD_SHIFT 0x01
#define MOD_ALTGR 0x02
#define UNMAPPED  0xFFFF

struct char_mapping {
    uint16_t keycode;
    uint8_t  modifiers;
};

static struct char_mapping char_map[128];
static int fd_uinput = -1;
static int fd_socket = -1;

void cleanup(void);
void emit(int type, int code, int val);
void do_paste(void);
void type_char(unsigned char c);
void do_backspace(void);
void do_key(int keycode);
int build_char_map(const char *layout, const char *variant);
int setup_uinput(void);
int setup_socket(void);
int run_daemon(const char *layout, const char *variant);
void show_usage(void);
int run_client(int argc, char *argv[]);

int build_char_map(const char *layout, const char *variant) {
    for (int i = 0; i < 128; i++) {
        char_map[i].keycode = UNMAPPED;
        char_map[i].modifiers = 0;
    }

    struct xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!ctx) {
        fprintf(stderr, "failed to create xkb context\n");
        return -1;
    }

    struct xkb_rule_names names = {0};
    if (layout) names.layout = layout;
    if (variant) names.variant = variant;

    struct xkb_keymap *keymap = xkb_keymap_new_from_names(
        ctx, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!keymap) {
        fprintf(stderr, "failed to create xkb keymap");
        if (layout) fprintf(stderr, " for layout '%s'", layout);
        fprintf(stderr, "\n");
        xkb_context_unref(ctx);
        return -1;
    }

    xkb_keycode_t min_kc = xkb_keymap_min_keycode(keymap);
    xkb_keycode_t max_kc = xkb_keymap_max_keycode(keymap);

    for (xkb_keycode_t kc = min_kc; kc <= max_kc; kc++) {
        if (xkb_keymap_num_layouts_for_key(keymap, kc) == 0)
            continue;

        int num_levels = xkb_keymap_num_levels_for_key(keymap, kc, 0);
        for (int level = 0; level < num_levels && level < 4; level++) {
            const xkb_keysym_t *syms;
            int num_syms = xkb_keymap_key_get_syms_by_level(
                keymap, kc, 0, level, &syms);

            for (int s = 0; s < num_syms; s++) {
                uint32_t cp = xkb_keysym_to_utf32(syms[s]);
                if (cp == 0 || cp >= 128) continue;
                if (char_map[cp].keycode != UNMAPPED) continue;

                uint8_t mods = 0;
                if (level == 1) mods = MOD_SHIFT;
                else if (level == 2) mods = MOD_ALTGR;
                else if (level == 3) mods = MOD_SHIFT | MOD_ALTGR;

                char_map[cp].keycode = kc - 8; // XKB keycode → evdev keycode
                char_map[cp].modifiers = mods;
            }
        }
    }

    printf("xhispertoold: keymap loaded");
    if (layout)  printf(" (layout: %s", layout);
    if (variant) printf(", variant: %s", variant);
    if (layout)  printf(")");
    if (!layout) printf(" (auto-detected from XKB_DEFAULT_LAYOUT or system default)");
    printf("\n");

    xkb_keymap_unref(keymap);
    xkb_context_unref(ctx);
    return 0;
}

void cleanup() {
    if (fd_uinput >= 0) {
        ioctl(fd_uinput, UI_DEV_DESTROY);
        close(fd_uinput);
    }
    if (fd_socket >= 0) {
        close(fd_socket);
    }
}

void emit(int type, int code, int val) {
    struct input_event ie = {
        .type = type,
        .code = code,
        .value = val
    };
    write(fd_uinput, &ie, sizeof(ie));
}

void do_paste() {
    emit(EV_KEY, KEY_LEFTCTRL, 1);
    emit(EV_SYN, SYN_REPORT, 0);
    usleep(8000);
    emit(EV_KEY, KEY_V, 1);
    emit(EV_SYN, SYN_REPORT, 0);
    usleep(8000);
    emit(EV_KEY, KEY_V, 0);
    emit(EV_SYN, SYN_REPORT, 0);
    usleep(2000);
    emit(EV_KEY, KEY_LEFTCTRL, 0);
    emit(EV_SYN, SYN_REPORT, 0);
}

void type_char(unsigned char c) {
    if (c >= 128) return;
    if (char_map[c].keycode == UNMAPPED) return;

    uint16_t keycode = char_map[c].keycode;
    uint8_t mods = char_map[c].modifiers;

    if (mods & MOD_SHIFT) {
        emit(EV_KEY, KEY_LEFTSHIFT, 1);
        emit(EV_SYN, SYN_REPORT, 0);
        usleep(2000);
    }
    if (mods & MOD_ALTGR) {
        emit(EV_KEY, KEY_RIGHTALT, 1);
        emit(EV_SYN, SYN_REPORT, 0);
        usleep(2000);
    }

    emit(EV_KEY, keycode, 1);
    emit(EV_SYN, SYN_REPORT, 0);
    usleep(8000);

    emit(EV_KEY, keycode, 0);
    emit(EV_SYN, SYN_REPORT, 0);
    usleep(2000);

    if (mods & MOD_ALTGR) {
        emit(EV_KEY, KEY_RIGHTALT, 0);
        emit(EV_SYN, SYN_REPORT, 0);
    }
    if (mods & MOD_SHIFT) {
        emit(EV_KEY, KEY_LEFTSHIFT, 0);
        emit(EV_SYN, SYN_REPORT, 0);
    }
}

void do_backspace() {
    emit(EV_KEY, KEY_BACKSPACE, 1);
    emit(EV_SYN, SYN_REPORT, 0);
    usleep(8000);
    emit(EV_KEY, KEY_BACKSPACE, 0);
    emit(EV_SYN, SYN_REPORT, 0);
}

void do_key(int keycode) {
    emit(EV_KEY, keycode, 1);
    emit(EV_SYN, SYN_REPORT, 0);
    usleep(8000);
    emit(EV_KEY, keycode, 0);
    emit(EV_SYN, SYN_REPORT, 0);
}

int setup_uinput() {
    fd_uinput = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd_uinput < 0) {
        perror("failed to open /dev/uinput");
        return -1;
    }

    ioctl(fd_uinput, UI_SET_EVBIT, EV_KEY);

    // Register all keycodes discovered in char_map
    for (int i = 0; i < 128; i++) {
        if (char_map[i].keycode != UNMAPPED)
            ioctl(fd_uinput, UI_SET_KEYBIT, char_map[i].keycode);
    }

    // Modifiers and special keys (needed for paste, backspace, wrap keys)
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_LEFTCTRL);
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_RIGHTCTRL);
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_LEFTALT);
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_RIGHTALT);
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_LEFTSHIFT);
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_RIGHTSHIFT);
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_LEFTMETA);
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_BACKSPACE);
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_V);

    struct uinput_setup setup = {0};
    setup.id.bustype = BUS_USB;
    setup.id.vendor = 0x1234;
    setup.id.product = 0x5678;
    snprintf(setup.name, UINPUT_MAX_NAME_SIZE, "xhisper");

    if (ioctl(fd_uinput, UI_DEV_SETUP, &setup) < 0) {
        perror("failed to setup uinput device");
        return -1;
    }
    if (ioctl(fd_uinput, UI_DEV_CREATE) < 0) {
        perror("failed to create uinput device");
        return -1;
    }

    usleep(100000);
    return 0;
}

int setup_socket() {
    fd_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd_socket < 0) {
        perror("failed to create socket");
        return -1;
    }

    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    addr.sun_path[0] = '\0';
    strncpy(addr.sun_path + 1, "xhisper_socket", sizeof(addr.sun_path) - 2);

    if (bind(fd_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        if (errno == EADDRINUSE) {
            fprintf(stderr, "xhispertoold is already running\n");
        } else {
            perror("failed to bind socket");
        }
        return -1;
    }

    return 0;
}

int run_daemon(const char *layout, const char *variant) {
    atexit(cleanup);

    if (build_char_map(layout, variant) < 0) {
        return 1;
    }

    if (setup_uinput() < 0) {
        return 1;
    }

    if (setup_socket() < 0) {
        return 1;
    }

    printf("xhispertoold: listening on @xhisper_socket\n");

    char buf[2];
    while (1) {
        ssize_t n = recv(fd_socket, buf, sizeof(buf), 0);
        if (n >= 1) {
            char cmd = buf[0];
            if (cmd == 'p') {
                do_paste();
            } else if (cmd == 't' && n == 2) {
                type_char((unsigned char)buf[1]);
            } else if (cmd == 'b') {
                do_backspace();
            } else if (cmd == 'r') {
                do_key(KEY_RIGHTALT);
            } else if (cmd == 'L') {
                do_key(KEY_LEFTALT);
            } else if (cmd == 'C') {
                do_key(KEY_LEFTCTRL);
            } else if (cmd == 'R') {
                do_key(KEY_RIGHTCTRL);
            } else if (cmd == 'S') {
                do_key(KEY_LEFTSHIFT);
            } else if (cmd == 'T') {
                do_key(KEY_RIGHTSHIFT);
            } else if (cmd == 'M') {
                do_key(KEY_LEFTMETA);
            }
        }
    }

    return 0;
}

void show_usage() {
    fprintf(stderr,
        "Usage:\n"
        "  xhispertool paste            - Paste from clipboard (Ctrl+V)\n"
        "  xhispertool type <char>      - Type a single ASCII character\n"
        "  xhispertool backspace        - Press backspace\n"
        "\n"
        "Input switching keys:\n"
        "  xhispertool leftalt          - Press left alt\n"
        "  xhispertool rightalt         - Press right alt\n"
        "  xhispertool leftctrl         - Press left ctrl\n"
        "  xhispertool rightctrl        - Press right ctrl\n"
        "  xhispertool leftshift        - Press left shift\n"
        "  xhispertool rightshift       - Press right shift\n"
        "  xhispertool super            - Press super (Windows key)\n"
        "\n"
        "Daemon:\n"
        "  xhispertoold [--layout <code>] [--variant <code>]\n"
        "                               - Run daemon with optional layout override\n"
        "                                 (auto-detects from XKB_DEFAULT_LAYOUT if omitted)\n"
    );
}

int run_client(int argc, char *argv[]) {
    if (argc < 2) {
        show_usage();
        return 1;
    }

    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("failed to create socket");
        return 1;
    }

    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    addr.sun_path[0] = '\0';
    strncpy(addr.sun_path + 1, "xhisper_socket", sizeof(addr.sun_path) - 2);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        int err = errno;
        fprintf(stderr, "failed to connect to xhispertoold: %s\n", strerror(err));

        switch (err) {
            case ENOENT:
            case ECONNREFUSED:
                fprintf(stderr, "Please check if xhispertoold is running.\n");
                fprintf(stderr, "Start it with: xhispertoold &\n");
                break;
            case EACCES:
            case EPERM:
                fprintf(stderr, "Permission denied. Check socket permissions.\n");
                break;
        }
        close(fd);
        return 2;
    }

    char buf[2];
    ssize_t len = 0;

    if (strcmp(argv[1], "paste") == 0) {
        buf[0] = 'p';
        len = 1;
    } else if (strcmp(argv[1], "backspace") == 0) {
        buf[0] = 'b';
        len = 1;
    } else if (strcmp(argv[1], "rightalt") == 0) {
        buf[0] = 'r';
        len = 1;
    } else if (strcmp(argv[1], "leftalt") == 0) {
        buf[0] = 'L';
        len = 1;
    } else if (strcmp(argv[1], "leftctrl") == 0) {
        buf[0] = 'C';
        len = 1;
    } else if (strcmp(argv[1], "rightctrl") == 0) {
        buf[0] = 'R';
        len = 1;
    } else if (strcmp(argv[1], "leftshift") == 0) {
        buf[0] = 'S';
        len = 1;
    } else if (strcmp(argv[1], "rightshift") == 0) {
        buf[0] = 'T';
        len = 1;
    } else if (strcmp(argv[1], "super") == 0) {
        buf[0] = 'M';
        len = 1;
    } else if (strcmp(argv[1], "type") == 0) {
        if (argc != 3 || strlen(argv[2]) != 1) {
            fprintf(stderr, "Error: 'type' requires exactly one character argument\n");
            show_usage();
            close(fd);
            return 1;
        }
        buf[0] = 't';
        buf[1] = argv[2][0];
        len = 2;
    } else {
        fprintf(stderr, "Error: Unknown command '%s'\n", argv[1]);
        show_usage();
        close(fd);
        return 1;
    }

    if (write(fd, buf, len) != len) {
        perror("failed to send command");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

int main(int argc, char *argv[]) {
    char *prog = basename(argv[0]);

    int is_daemon = (strcmp(prog, "xhispertoold") == 0);
    if (!is_daemon && argc > 1 && strcmp(argv[1], "--daemon") == 0)
        is_daemon = 1;

    if (is_daemon) {
        const char *layout = NULL;
        const char *variant = NULL;
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--layout") == 0 && i + 1 < argc)
                layout = argv[++i];
            else if (strcmp(argv[i], "--variant") == 0 && i + 1 < argc)
                variant = argv[++i];
        }
        return run_daemon(layout, variant);
    } else {
        return run_client(argc, argv);
    }
}
