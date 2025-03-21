# 2048

An implementation of the popular game 2048 in C.

First commit runs under Windows Subsystem for Linux (WSL) with Ubuntu 16.04.4

Future implementation will be in STM32F072 Cortex M0 virtual serial port (CDC Class)

2025-03-20: Removed WSL compliance and reconfigured for linux console using termios

#To Do?
- Use cursor movement to repaint in place
- Tidy up 'Game Over' behavior
- fix cmake
- animate collisions go game is less 'surprising'
- maybe add in some timing delay between collision/merger/spawning to enhance UX


