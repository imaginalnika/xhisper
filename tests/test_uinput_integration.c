/*
 * test.c - Test program for xhisper
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/ioctl.h>
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

// ASCII to Linux keycode mapping
static const int32_t ascii2keycode_map[128] = {
	-1,-1,-1,-1,-1,-1,-1,-1,
	-1,KEY_TAB,KEY_ENTER,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,
	KEY_SPACE,KEY_1|FLAG_UPPERCASE,KEY_APOSTROPHE|FLAG_UPPERCASE,KEY_3|FLAG_UPPERCASE,KEY_4|FLAG_UPPERCASE,KEY_5|FLAG_UPPERCASE,KEY_7|FLAG_UPPERCASE,KEY_APOSTROPHE,
	KEY_9|FLAG_UPPERCASE,KEY_0|FLAG_UPPERCASE,KEY_8|FLAG_UPPERCASE,KEY_EQUAL|FLAG_UPPERCASE,KEY_COMMA,KEY_MINUS,KEY_DOT,KEY_SLASH,
	KEY_0,KEY_1,KEY_2,KEY_3,KEY_4,KEY_5,KEY_6,KEY_7,
	KEY_8,KEY_9,KEY_SEMICOLON|FLAG_UPPERCASE,KEY_SEMICOLON,KEY_COMMA|FLAG_UPPERCASE,KEY_EQUAL,KEY_DOT|FLAG_UPPERCASE,KEY_SLASH|FLAG_UPPERCASE,
	KEY_2|FLAG_UPPERCASE,KEY_A|FLAG_UPPERCASE,KEY_B|FLAG_UPPERCASE,KEY_C|FLAG_UPPERCASE,KEY_D|FLAG_UPPERCASE,KEY_E|FLAG_UPPERCASE,KEY_F|FLAG_UPPERCASE,KEY_G|FLAG_UPPERCASE,
	KEY_H|FLAG_UPPERCASE,KEY_I|FLAG_UPPERCASE,KEY_J|FLAG_UPPERCASE,KEY_K|FLAG_UPPERCASE,KEY_L|FLAG_UPPERCASE,KEY_M|FLAG_UPPERCASE,KEY_N|FLAG_UPPERCASE,KEY_O|FLAG_UPPERCASE,
	KEY_P|FLAG_UPPERCASE,KEY_Q|FLAG_UPPERCASE,KEY_R|FLAG_UPPERCASE,KEY_S|FLAG_UPPERCASE,KEY_T|FLAG_UPPERCASE,KEY_U|FLAG_UPPERCASE,KEY_V|FLAG_UPPERCASE,KEY_W|FLAG_UPPERCASE,
	KEY_X|FLAG_UPPERCASE,KEY_Y|FLAG_UPPERCASE,KEY_Z|FLAG_UPPERCASE,KEY_LEFTBRACE,KEY_BACKSLASH,KEY_RIGHTBRACE,KEY_6|FLAG_UPPERCASE,KEY_MINUS|FLAG_UPPERCASE,
	KEY_GRAVE,KEY_A,KEY_B,KEY_C,KEY_D,KEY_E,KEY_F,KEY_G,
	KEY_H,KEY_I,KEY_J,KEY_K,KEY_L,KEY_M,KEY_N,KEY_O,
	KEY_P,KEY_Q,KEY_R,KEY_S,KEY_T,KEY_U,KEY_V,KEY_W,
	KEY_X,KEY_Y,KEY_Z,KEY_LEFTBRACE|FLAG_UPPERCASE,KEY_BACKSLASH|FLAG_UPPERCASE,KEY_RIGHTBRACE|FLAG_UPPERCASE,KEY_GRAVE|FLAG_UPPERCASE,-1
};

static int fd_uinput = -1;

void cleanup() {
    if (fd_uinput >= 0) {
        ioctl(fd_uinput, UI_DEV_DESTROY);
        close(fd_uinput);
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

void press_key(int keycode) {
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
        fprintf(stderr, "Make sure you're in the 'input' group\n");
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
    snprintf(setup.name, UINPUT_MAX_NAME_SIZE, "xhisper-test");

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

void run_clipboard_command(const char *text) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "command -v wl-copy >/dev/null 2>&1 && echo -n '%s' | wl-copy", text);
    if (system(cmd) != 0) {
        snprintf(cmd, sizeof(cmd), "command -v xclip >/dev/null 2>&1 && echo -n '%s' | xclip -selection clipboard", text);
        system(cmd);
    }
}

void test_typer() {
    printf("\n--- Testing ASCII typing ---\n");

    const char *lower = "abcdefghijklmnopqrstuvwxyz";
    printf("Typing: %s\n", lower);
    for (int i = 0; lower[i]; i++) {
        type_char(lower[i]);
    }

    usleep(200000);
    type_char(' ');

    const char *upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    printf("Typing: %s\n", upper);
    for (int i = 0; upper[i]; i++) {
        type_char(upper[i]);
    }

    usleep(200000);
    type_char(' ');

    const char *numbers = "0123456789";
    printf("Typing: %s\n", numbers);
    for (int i = 0; numbers[i]; i++) {
        type_char(numbers[i]);
    }

    usleep(200000);
    type_char(' ');

    const char *symbols = "!@#$%^&*()_+-=[]{}\\|;:'\"<>,.?/`~";
    printf("Typing: %s\n", symbols);
    for (int i = 0; symbols[i]; i++) {
        type_char(symbols[i]);
    }

    usleep(500000);

    printf("Typing 'test' then deleting with backspace\n");
    type_char('t');
    type_char('e');
    type_char('s');
    type_char('t');
    usleep(300000);
    do_backspace();
    do_backspace();
    do_backspace();
    do_backspace();

    usleep(500000);

    const char *sentence = "Hello, World! Testing 123.";
    printf("Typing: %s\n", sentence);
    for (int i = 0; sentence[i]; i++) {
        type_char(sentence[i]);
    }
}

void test_paster() {
    printf("\n--- Testing Unicode/clipboard pasting ---\n");

    printf("Pasting: éàèùçäöüß你好世界مرحباМосква\n");

    const char *chars[] = {"é", "à", "è", "ù", "ç", "ä", "ö", "ü", "ß",
                           "你", "好", "世", "界", "م", "ر", "ح", "ب", "ا",
                           "М", "о", "с", "к", "в", "а"};

    for (int i = 0; i < 24; i++) {
        run_clipboard_command(chars[i]);
        usleep(50000);
        do_paste();
        usleep(100000);
    }
}

int main(int argc, char *argv[]) {
    int wrap_keycode = 0;
    const char *wrap_name = NULL;

    // Parse command-line arguments for input switching key
    if (argc > 1) {
        if (strcmp(argv[1], "--leftalt") == 0) {
            wrap_keycode = KEY_LEFTALT;
            wrap_name = "leftalt";
        } else if (strcmp(argv[1], "--rightalt") == 0) {
            wrap_keycode = KEY_RIGHTALT;
            wrap_name = "rightalt";
        } else if (strcmp(argv[1], "--leftctrl") == 0) {
            wrap_keycode = KEY_LEFTCTRL;
            wrap_name = "leftctrl";
        } else if (strcmp(argv[1], "--rightctrl") == 0) {
            wrap_keycode = KEY_RIGHTCTRL;
            wrap_name = "rightctrl";
        } else if (strcmp(argv[1], "--leftshift") == 0) {
            wrap_keycode = KEY_LEFTSHIFT;
            wrap_name = "leftshift";
        } else if (strcmp(argv[1], "--rightshift") == 0) {
            wrap_keycode = KEY_RIGHTSHIFT;
            wrap_name = "rightshift";
        } else if (strcmp(argv[1], "--super") == 0) {
            wrap_keycode = KEY_LEFTMETA;
            wrap_name = "super";
        } else {
            fprintf(stderr, "Usage: %s [--leftalt|--rightalt|--leftctrl|--rightctrl|--leftshift|--rightshift|--super]\n", argv[0]);
            return 1;
        }
    }

    printf("=== xhisper test ===\n");
    if (wrap_name) {
        printf("Wrapping with key: %s\n", wrap_name);
    }
    printf("\nMake sure you're in the 'input' group:\n");
    printf("  sudo usermod -aG input $USER\n");
    printf("  (then log out and back in)\n");
    printf("\nFocus a text editor and press Enter to start in 3 seconds...\n");
    getchar();
    sleep(3);

    atexit(cleanup);

    if (setup_uinput() < 0) {
        return 1;
    }

    // Press wrapping key before tests
    if (wrap_keycode) {
        printf("Pressing %s...\n", wrap_name);
        press_key(wrap_keycode);
        usleep(100000);
    }

    test_typer();
    sleep(1);
    test_paster();

    // Press wrapping key after tests
    if (wrap_keycode) {
        usleep(100000);
        printf("Pressing %s...\n", wrap_name);
        press_key(wrap_keycode);
    }

    printf("\n\n=== Test complete ===\n");
    printf("Expected output:\n");
    printf("abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789 !@#$%%^&*()_+-=[]{}\\|;:'\"<>,.?/`~ Hello, World! Testing 123.éàèùçäöüß你好世界مرحباМосква\n");

    return 0;
}
