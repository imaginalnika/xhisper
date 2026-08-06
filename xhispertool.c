/*
 * xhisper - Whisper for Linux
 * Combined daemon and client for text input via uinput
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
#include "keymap.h"
#define KEY_LEFTCTRL 29
#define KEY_RIGHTCTRL 97
#define KEY_LEFTALT 56
#define KEY_RIGHTALT 100
#define KEY_LEFTSHIFT 42
#define KEY_RIGHTSHIFT 54
#define KEY_LEFTMETA 125
#define KEY_V 47

// Function prototypes
void cleanup(void);
void emit(int type, int code, int val);
void do_paste(void);
void type_char(unsigned char c, const char *layout);
void do_backspace(void);
void do_key(int keycode);
int setup_uinput(void);
int setup_socket(void);
int run_daemon(void);
void show_usage(void);
int run_client(int argc, char *argv[]);

static int fd_uinput = -1;
static int fd_socket = -1;

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

void type_char(unsigned char c, const char *layout) {
    int32_t kdef = keymap_lookup(layout, c);
    if (kdef == -1) return;

    uint16_t keycode = kdef & 0xffff;
    int shift = (kdef & FLAG_UPPERCASE) != 0;
    int altgr = (kdef & FLAG_ALTGR) != 0;
    int dead = (kdef & FLAG_DEADKEY) != 0;

    if (shift) {
        emit(EV_KEY, KEY_LEFTSHIFT, 1);
        emit(EV_SYN, SYN_REPORT, 0);
        usleep(2000);
    }
    if (altgr) {
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

    if (altgr) {
        emit(EV_KEY, KEY_RIGHTALT, 0);
        emit(EV_SYN, SYN_REPORT, 0);
    }
    if (shift) {
        emit(EV_KEY, KEY_LEFTSHIFT, 0);
        emit(EV_SYN, SYN_REPORT, 0);
    }

    // Dead keys only produce a character when followed by a key;
    // Space resolves them to the standalone character.
    if (dead) {
        emit(EV_KEY, KEY_SPACE, 1);
        emit(EV_SYN, SYN_REPORT, 0);
        usleep(2000);
        emit(EV_KEY, KEY_SPACE, 0);
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

    // Register letters
    for (int i = KEY_Q; i <= KEY_P; i++) ioctl(fd_uinput, UI_SET_KEYBIT, i);
    for (int i = KEY_A; i <= KEY_L; i++) ioctl(fd_uinput, UI_SET_KEYBIT, i);
    for (int i = KEY_Z; i <= KEY_M; i++) ioctl(fd_uinput, UI_SET_KEYBIT, i);

    // Register numbers
    for (int i = KEY_1; i <= KEY_0; i++) ioctl(fd_uinput, UI_SET_KEYBIT, i);

    // Register special keys
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_SPACE);
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_MINUS);
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_EQUAL);
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_LEFTBRACE);
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_RIGHTBRACE);
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_SEMICOLON);
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_APOSTROPHE);
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_GRAVE);
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_BACKSLASH);
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_COMMA);
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_DOT);
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_SLASH);
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_102ND);
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_TAB);
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_ENTER);
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_BACKSPACE);

    // Register modifiers
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_LEFTCTRL);
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_RIGHTCTRL);
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_LEFTALT);
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_RIGHTALT);
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_LEFTSHIFT);
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_RIGHTSHIFT);
    ioctl(fd_uinput, UI_SET_KEYBIT, KEY_LEFTMETA);

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

    // Use abstract namespace socket (Linux-specific)
    // First byte is null, no filesystem entry, kernel manages lifecycle
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

// Daemon mode
int run_daemon() {
    atexit(cleanup);

    if (setup_uinput() < 0) {
        return 1;
    }

    if (setup_socket() < 0) {
        return 1;
    }

    const char *layout = getenv("XHISPER_LAYOUT");
    if (!layout || !*layout) {
        layout = "us";
    }

    // Persist the layout so xhisper.sh can detect and restart a stale daemon.
    FILE *layout_file = fopen("/tmp/xhispertoold.layout", "w");
    if (layout_file) {
        fprintf(layout_file, "%s\n", layout);
        fclose(layout_file);
    }

    printf("xhispertoold: listening on @xhisper_socket (layout: %s)\n", layout);

    char buf[2];
    while (1) {
        ssize_t n = recv(fd_socket, buf, sizeof(buf), 0);
        if (n >= 1) {
            char cmd = buf[0];
            if (cmd == 'p') {
                do_paste();
            } else if (cmd == 't' && n == 2) {
                type_char((unsigned char)buf[1], layout);
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

// Client mode
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
        "  xhispertoold                 - Run daemon (or xhispertool --daemon)\n"
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

    // Use abstract namespace socket (same as daemon)
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
    // Detect mode: daemon or client
    char *prog = basename(argv[0]);

    if (strcmp(prog, "xhispertoold") == 0 ||
        (argc > 1 && strcmp(argv[1], "--daemon") == 0)) {
        return run_daemon();
    } else {
        return run_client(argc, argv);
    }
}
