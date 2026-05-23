# ASCIIman - Infinite Arcade Engine
A terminal-based, AI-powered **Pacman** clone built entirely from scratch in C with zero external library dependencies (utilizing only standard libraries). This project brings the infinite loop and auto-restart mechanics of classic arcade cabinets to the terminal using modern system architecture principles.
## 🚀 Key Features
 * **Advanced AI & Pathfinding:** Ghosts utilize the **A* (A-Star) Pathfinding** algorithm paired with Manhattan Distance Heuristics to intelligently track down ASCIIman.
 * **Finite State Machine (FSM):** Ghosts dynamically transition between classic modes: Chase, Scatter, Frightened (Blue Mode), and Eaten (Eyes-Only Mode).
 * **Precise Turn Buffering:** If you input a direction (W, A, S, D) before reaching an intersection, the system buffers the input and executes the turn automatically at the first available corner for fluid, lossless grid movement.
 * **Input Buffer Flushing (Lag Prevention):** The terminal input buffer is flushed on every frame to prevent input accumulation and eliminate movement lag.
 * **Infinite Loop & Dynamic Difficulty:** When the player dies, the game does not close. Instead, it displays a "GAME OVER" screen and automatically restarts after a 3-second countdown. Each new level dynamically scales up the ghosts' movement speeds.
 * **High Performance:** Built without any dynamic memory allocation (malloc/free) using a static memory pool and Data-Oriented Design (DOD) principles to maximize CPU cache locality.
## 🕹️ Controls
The game operates with an asynchronous, non-blocking input scheme:
 * **W:** Move Up
 * **S:** Move Down
 * **A:** Move Left
 * **D:** Move Right
 * **Q:** Safe Exit (Gracefully restores original terminal state before exiting)
## 🛠️ Compilation and Execution
The code compiles directly on any POSIX-compliant terminal system (Linux, macOS, WSL) without needing extra dependencies.
### Compilation:
```bash
gcc -O3 pacman_infinite_loop.c -o asciiman

```
*(The -O3 flag is highly recommended for advanced compiler optimizations.)*
### Execution:
```bash
chmod +x ./asciiman
./asciiman

```
> **Note:** For the best visual experience, please set your terminal window size to at least 40x40 characters and ensure your terminal's font supports standard ANSI colors.
> 
## 📄 License
This project is licensed under the **GNU General Public License v3 (GPLv3)**. Please refer to the official license text for more details.
### Copyright & Author Information
**Copyright (c) 2026 Burak Yakub Güçer**
This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
This program is distributed in the hope that it will be useful, but **WITHOUT ANY WARRANTY**; without even the implied warranty of **MERCHANTABILITY** or **FITNESS FOR A PARTICULAR PURPOSE**. See the GNU General Public License for more details.
