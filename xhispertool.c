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
#include <sys/stat.h>
#include <linux/uinput.h>
#define KEY_LEFTCTRL 29
#define KEY_RIGHTCTRL 97
#define KEY_LEFTALT 56
#define KEY_RIGHTALT 100
#define KEY_LEFTSHIFT 42
#define KEY_RIGHTSHIFT 54
#define KEY_LEFTMETA 125
#define KEY_V 47
#define FLAG_UPPERCASE 0x80000000
#define XHISPER_SOCKET_PATH "/tmp/xhisper_socket"

// Function prototypes
void cleanup(void);
void emit(int type, int code, int val);
void do_paste(void);
void type_char(unsigned char c);
void do_backspace(void);
void do_key(int keycode);
int setup_uinput(void);
int setup_socket(void);
int run_daemon(void);
void show_usage(void);
int run_client(int argc, char *argv[]);

// ASCII to Linux keycode mapping for US QWERTY layout
// Independently derived from:
// - ASCII character set specification (characters 0-127)
// - Linux input-event-codes.h KEY_* constants
// - US QWERTY keyboard physical layout
// Each entry maps an ASCII code to either:
//   -1 (unmapped/unsupported)
//   KEY_* constant for unshifted characters
//   KEY_* | FLAG_UPPERCASE for shifted characters
static const int32_t ascii2keycode_map[128] = {
	// Control characters (0x00-0x1f): mostly unmapped except tab and enter
	-1,-1,-1,-1,-1,-1,-1,-1,  // 0x00-0x07
	-1,KEY_TAB,KEY_ENTER,-1,-1,-1,-1,-1,  // 0x08-0x0f (tab=0x09, enter=0x0a)
	-1,-1,-1,-1,-1,-1,-1,-1,  // 0x10-0x17
	-1,-1,-1,-1,-1,-1,-1,-1,  // 0x18-0x1f

	// Printable characters (0x20-0x7e)
	// Space and symbols (0x20-0x2f)
	KEY_SPACE,                      // 0x20 ' '
	KEY_1|FLAG_UPPERCASE,           // 0x21 '!' (shift+1)
	KEY_APOSTROPHE|FLAG_UPPERCASE,  // 0x22 '"' (shift+')
	KEY_3|FLAG_UPPERCASE,           // 0x23 '#' (shift+3)
	KEY_4|FLAG_UPPERCASE,           // 0x24 '$' (shift+4)
	KEY_5|FLAG_UPPERCASE,           // 0x25 '%' (shift+5)
	KEY_7|FLAG_UPPERCASE,           // 0x26 '&' (shift+7)
	KEY_APOSTROPHE,                 // 0x27 '''
	KEY_9|FLAG_UPPERCASE,           // 0x28 '(' (shift+9)
	KEY_0|FLAG_UPPERCASE,           // 0x29 ')' (shift+0)
	KEY_8|FLAG_UPPERCASE,           // 0x2a '*' (shift+8)
	KEY_EQUAL|FLAG_UPPERCASE,       // 0x2b '+' (shift+=)
	KEY_COMMA,                      // 0x2c ','
	KEY_MINUS,                      // 0x2d '-'
	KEY_DOT,                        // 0x2e '.'
	KEY_SLASH,                      // 0x2f '/'

	// Digits (0x30-0x39)
	KEY_0,KEY_1,KEY_2,KEY_3,KEY_4,KEY_5,KEY_6,KEY_7,KEY_8,KEY_9,

	// More symbols (0x3a-0x40)
	KEY_SEMICOLON|FLAG_UPPERCASE,   // 0x3a ':' (shift+;)
	KEY_SEMICOLON,                  // 0x3b ';'
	KEY_COMMA|FLAG_UPPERCASE,       // 0x3c '<' (shift+,)
	KEY_EQUAL,                      // 0x3d '='
	KEY_DOT|FLAG_UPPERCASE,         // 0x3e '>' (shift+.)
	KEY_SLASH|FLAG_UPPERCASE,       // 0x3f '?' (shift+/)
	KEY_2|FLAG_UPPERCASE,           // 0x40 '@' (shift+2)

	// Uppercase letters (0x41-0x5a): A-Z
	KEY_A|FLAG_UPPERCASE,KEY_B|FLAG_UPPERCASE,KEY_C|FLAG_UPPERCASE,KEY_D|FLAG_UPPERCASE,
	KEY_E|FLAG_UPPERCASE,KEY_F|FLAG_UPPERCASE,KEY_G|FLAG_UPPERCASE,KEY_H|FLAG_UPPERCASE,
	KEY_I|FLAG_UPPERCASE,KEY_J|FLAG_UPPERCASE,KEY_K|FLAG_UPPERCASE,KEY_L|FLAG_UPPERCASE,
	KEY_M|FLAG_UPPERCASE,KEY_N|FLAG_UPPERCASE,KEY_O|FLAG_UPPERCASE,KEY_P|FLAG_UPPERCASE,
	KEY_Q|FLAG_UPPERCASE,KEY_R|FLAG_UPPERCASE,KEY_S|FLAG_UPPERCASE,KEY_T|FLAG_UPPERCASE,
	KEY_U|FLAG_UPPERCASE,KEY_V|FLAG_UPPERCASE,KEY_W|FLAG_UPPERCASE,KEY_X|FLAG_UPPERCASE,
	KEY_Y|FLAG_UPPERCASE,KEY_Z|FLAG_UPPERCASE,

	// Brackets and symbols (0x5b-0x60)
	KEY_LEFTBRACE,                  // 0x5b '['
	KEY_BACKSLASH,                  // 0x5c '\'
	KEY_RIGHTBRACE,                 // 0x5d ']'
	KEY_6|FLAG_UPPERCASE,           // 0x5e '^' (shift+6)
	KEY_MINUS|FLAG_UPPERCASE,       // 0x5f '_' (shift+-)
	KEY_GRAVE,                      // 0x60 '`'

	// Lowercase letters (0x61-0x7a): a-z
	KEY_A,KEY_B,KEY_C,KEY_D,KEY_E,KEY_F,KEY_G,KEY_H,
	KEY_I,KEY_J,KEY_K,KEY_L,KEY_M,KEY_N,KEY_O,KEY_P,
	KEY_Q,KEY_R,KEY_S,KEY_T,KEY_U,KEY_V,KEY_W,KEY_X,
	KEY_Y,KEY_Z,

	// Final symbols (0x7b-0x7e)
	KEY_LEFTBRACE|FLAG_UPPERCASE,   // 0x7b '{' (shift+[)
	KEY_BACKSLASH|FLAG_UPPERCASE,   // 0x7c '|' (shift+\)
	KEY_RIGHTBRACE|FLAG_UPPERCASE,  // 0x7d '}' (shift+])
	KEY_GRAVE|FLAG_UPPERCASE,       // 0x7e '~' (shift+`)

	-1  // 0x7f DEL (unmapped)
};

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

void type_char(unsigned char c) {
    if (c >= 128) return;

    int32_t kdef = ascii2keycode_map[c];
    if (kdef == -1) return;

    uint16_t keycode = kdef & 0xffff;

    if (kdef & FLAG_UPPERCASE) {
        emit(EV_KEY, KEY_LEFTSHIFT, 1);
        emit(EV_SYN, SYN_REPORT, 0);
        usleep(2000);
    }

    emit(EV_KEY, keycode, 1);
    emit(EV_SYN, SYN_REPORT, 0);
    usleep(8000);

    emit(EV_KEY, keycode, 0);
    emit(EV_SYN, SYN_REPORT, 0);
    usleep(2000);

    if (kdef & FLAG_UPPERCASE) {
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

    // Use filesystem socket so root daemon can accept user connections
    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    strncpy(addr.sun_path, XHISPER_SOCKET_PATH, sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';
    unlink(XHISPER_SOCKET_PATH);

    if (bind(fd_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        if (errno == EADDRINUSE) {
            fprintf(stderr, "xhispertoold is already running\n");
        } else {
            perror("failed to bind socket");
        }
        return -1;
    }

    if (chmod(XHISPER_SOCKET_PATH, 0666) < 0) {
        perror("failed to chmod socket");
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

    printf("xhispertoold: listening on %s\n", XHISPER_SOCKET_PATH);

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

    // Use filesystem socket (same as daemon)
    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    strncpy(addr.sun_path, XHISPER_SOCKET_PATH, sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

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
