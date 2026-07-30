#include "audition_input.h"

#ifdef _WIN32

#include <conio.h>

namespace nathan::hrtf {

// Rien a garder entre deux appels : _kbhit/_getch ne demandent aucune
// configuration prealable sous Windows.
struct KeyboardAuditionInput::Impl {};

KeyboardAuditionInput::KeyboardAuditionInput() : impl_(std::make_unique<Impl>()) {}
KeyboardAuditionInput::~KeyboardAuditionInput() = default;

AuditionCommand KeyboardAuditionInput::poll() {
    if (!_kbhit()) return AuditionCommand::None;
    int c = _getch();
    if (c == 0 || c == 0xE0) {
        int c2 = _getch();
        if (c2 == 75) return AuditionCommand::Previous;  // fleche gauche
        if (c2 == 77) return AuditionCommand::Next;       // fleche droite
        return AuditionCommand::None;
    }
    if (c == 13) return AuditionCommand::Confirm;
    if (c == 27) return AuditionCommand::Cancel;
    if (c == 'q' || c == 'Q') return AuditionCommand::Cancel;
    return AuditionCommand::None;
}

}  // namespace nathan::hrtf

#else  // POSIX

#include <termios.h>
#include <unistd.h>
#include <sys/select.h>

namespace nathan::hrtf {

struct KeyboardAuditionInput::Impl {
    termios original{};
    bool active = false;

    Impl() {
        if (tcgetattr(STDIN_FILENO, &original) != 0) return;
        termios raw = original;
        raw.c_lflag &= ~(ICANON | ECHO);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0) {
            active = true;
        }
    }

    ~Impl() {
        if (active) {
            tcsetattr(STDIN_FILENO, TCSANOW, &original);
        }
    }

    static bool inputReady() {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        timeval tv{0, 0};
        return select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0;
    }
};

KeyboardAuditionInput::KeyboardAuditionInput() : impl_(std::make_unique<Impl>()) {}
KeyboardAuditionInput::~KeyboardAuditionInput() = default;

AuditionCommand KeyboardAuditionInput::poll() {
    if (!Impl::inputReady()) return AuditionCommand::None;
    unsigned char c = 0;
    if (read(STDIN_FILENO, &c, 1) != 1) return AuditionCommand::None;

    if (c == '\r' || c == '\n') return AuditionCommand::Confirm;
    if (c == 'q' || c == 'Q') return AuditionCommand::Cancel;
    if (c != 27) return AuditionCommand::None;  // pas une sequence d'echappement

    if (!Impl::inputReady()) return AuditionCommand::Cancel;  // ESC seul
    unsigned char c2 = 0;
    if (read(STDIN_FILENO, &c2, 1) != 1 || c2 != '[') return AuditionCommand::Cancel;

    if (!Impl::inputReady()) return AuditionCommand::Cancel;
    unsigned char c3 = 0;
    if (read(STDIN_FILENO, &c3, 1) != 1) return AuditionCommand::Cancel;
    if (c3 == 'C') return AuditionCommand::Next;      // fleche droite
    if (c3 == 'D') return AuditionCommand::Previous;  // fleche gauche
    return AuditionCommand::None;
}

}  // namespace nathan::hrtf

#endif
