#pragma once

// Capture clavier non-bloquante, isolee du reste du code DSP portable.
// Implementation scindee en interne par plateforme (_WIN32 / POSIX).

enum class KeyEvent { None, Left, Right, Up, Down, Enter, Escape };

KeyEvent poll_key_nonblocking();
