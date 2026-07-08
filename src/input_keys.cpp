#include "input_keys.h"

#ifdef _WIN32

#include <conio.h>

KeyEvent poll_key_nonblocking() {
    if (!_kbhit()) return KeyEvent::None;
    int c = _getch();
    if (c == 0 || c == 0xE0) {
        int c2 = _getch();
        if (c2 == 72) return KeyEvent::Up;
        if (c2 == 80) return KeyEvent::Down;
        if (c2 == 75) return KeyEvent::Left;
        if (c2 == 77) return KeyEvent::Right;
        return KeyEvent::None;
    }
    if (c == 13) return KeyEvent::Enter;
    if (c == 27) return KeyEvent::Escape;
    return KeyEvent::None;
}

#else  // POSIX

#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <cstdio>

namespace {

struct RawModeGuard {
    termios original{};
    bool active = false;

    RawModeGuard() {
        if (tcgetattr(STDIN_FILENO, &original) != 0) return;
        termios raw = original;
        raw.c_lflag &= ~(ICANON | ECHO);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0) {
            active = true;
        }
    }

    ~RawModeGuard() {
        if (active) {
            tcsetattr(STDIN_FILENO, TCSANOW, &original);
        }
    }
};

bool inputReady() {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    timeval tv{0, 0};
    return select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0;
}

}  // namespace

KeyEvent poll_key_nonblocking() {
    static RawModeGuard guard;  // construit/restaure une seule fois pour tout le processus

    if (!inputReady()) return KeyEvent::None;
    unsigned char c = 0;
    if (read(STDIN_FILENO, &c, 1) != 1) return KeyEvent::None;

    if (c == '\r' || c == '\n') return KeyEvent::Enter;
    if (c != 27) return KeyEvent::None;  // pas une sequence d'echappement

    if (!inputReady()) return KeyEvent::Escape;  // ESC seul
    unsigned char c2 = 0;
    if (read(STDIN_FILENO, &c2, 1) != 1 || c2 != '[') return KeyEvent::Escape;

    if (!inputReady()) return KeyEvent::Escape;
    unsigned char c3 = 0;
    if (read(STDIN_FILENO, &c3, 1) != 1) return KeyEvent::Escape;
    if (c3 == 'A') return KeyEvent::Up;
    if (c3 == 'B') return KeyEvent::Down;
    if (c3 == 'C') return KeyEvent::Right;
    if (c3 == 'D') return KeyEvent::Left;
    return KeyEvent::None;
}

#endif
