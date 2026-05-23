/*
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
gcc -O3 asciiman.c -o asciiman

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
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <ctype.h>


#define MAP_WIDTH 28
#define MAP_HEIGHT 31
#define MAX_GHOSTS 4
#define MAX_NODES (MAP_WIDTH * MAP_HEIGHT)
#define C_RST "\x1B[0m"
#define C_YEL "\x1B[33m"
#define C_BLU "\x1B[34m"
#define C_RED "\x1B[31m"
#define C_MAG "\x1B[35m"
#define C_CYA "\x1B[36m"
#define C_ORG "\x1B[38;5;208m"
#define C_WHT "\x1B[37m"


typedef enum {
    MODE_CHASE,
    MODE_SCATTER,
    MODE_FRIGHTENED,
    MODE_EATEN
} GhostMode;

typedef struct {
    int x;
    int y;
} Vec2;

typedef struct {
    Vec2 pos;
    Vec2 dir;
    Vec2 next_dir; 
    Vec2 start_pos;
    bool active;
} Transform;

typedef struct {
    GhostMode mode;
    Vec2 target;
    uint32_t mode_timer;
    uint8_t color_id;
    int move_delay; 
    int tick_counter;
} AIController;

typedef struct {
    uint32_t score;
    uint32_t level;
    uint16_t pellets_remaining;
    bool game_over;
    uint32_t global_tick;
    uint32_t frightened_timer;
} GameState;

typedef struct {
    Vec2 pos;
    uint16_t g_cost;
    uint16_t h_cost;
    uint16_t f_cost;
    Vec2 parent;
} PathNode;

typedef struct {
    PathNode nodes[MAX_NODES];
    uint16_t size;
} PriorityQueue;

Transform player;
Transform ghosts[MAX_GHOSTS];
AIController ghost_ai[MAX_GHOSTS];
GameState game;
char current_map[MAP_HEIGHT][MAP_WIDTH + 1];
struct termios orig_termios;

const Vec2 DIRS[4] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}}; 

const char* ROM_MAP[MAP_HEIGHT] = {
    "############################",
    "#............##............#",
    "#.####.#####.##.#####.####.#",
    "#O####.#####.##.#####.####O#",
    "#.####.#####.##.#####.####.#",
    "#..........................#",
    "#.####.##.########.##.####.#",
    "#.####.##.########.##.####.#",
    "#......##....##....##......#",
    "######.##### ## #####.######",
    "      .##### ## #####.      ",
    "      .##          ##.      ",
    "      .## ###--### ##.      ",
    "######.## #      # ##.######",
    "      .   #      #   .      ",
    "######.## #      # ##.######",
    "      .## ######## ##.      ",
    "      .##          ##.      ",
    "      .## ######## ##.      ",
    "######.## ######## ##.######",
    "#............##............#",
    "#.####.#####.##.#####.####.#",
    "#.####.#####.##.#####.####.#",
    "#O..##.......  .......##..O#",
    "###.##.##.########.##.##.###",
    "###.##.##.########.##.##.###",
    "#......##....##....##......#",
    "#.##########.##.##########.#",
    "#.##########.##.##########.#",
    "#..........................#",
    "############################"
};


void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    printf("\e[?25h");
}

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    printf("\e[?25l");
}

int kbhit() {
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
}

void load_level() {
    game.pellets_remaining = 0;
    for (int y = 0; y < MAP_HEIGHT; ++y) {
        strcpy(current_map[y], ROM_MAP[y]);
        for (int x = 0; x < MAP_WIDTH; ++x) {
            if (current_map[y][x] == '.' || current_map[y][x] == 'O') {
                game.pellets_remaining++;
            }
        }
    }
    
    
    player.pos.x = 14;
    player.pos.y = 23;
    player.dir.x = -1;
    player.dir.y = 0;
    player.next_dir = player.dir;
    
    
    Vec2 spawns[4] = {{13, 11}, {14, 11}, {12, 14}, {15, 14}};
    for (int i = 0; i < MAX_GHOSTS; ++i) {
        ghosts[i].pos = spawns[i];
        ghosts[i].start_pos = spawns[i];
        ghosts[i].active = true;
        ghost_ai[i].mode = MODE_SCATTER;
        ghost_ai[i].color_id = (uint8_t)i;
        ghost_ai[i].tick_counter = 0;
        
      
      
        int base_delay = 8 - (int)game.level;
        if (base_delay < 1) base_delay = 1;
        ghost_ai[i].move_delay = base_delay; 
    }
}


uint16_t heuristic(Vec2 a, Vec2 b) {
    int dx = a.x - b.x;
    int dy = a.y - b.y;
    return (uint16_t)((dx > 0 ? dx : -dx) + (dy > 0 ? dy : -dy));
}


void pq_push(PriorityQueue* pq, PathNode node) {
    pq->nodes[pq->size] = node;
    uint16_t curr = pq->size++;
    while (curr > 0) {
        uint16_t parent = (uint16_t)((curr - 1) / 2);
        if (pq->nodes[curr].f_cost >= pq->nodes[parent].f_cost) break;
        PathNode temp = pq->nodes[curr];
        pq->nodes[curr] = pq->nodes[parent];
        pq->nodes[parent] = temp;
        curr = parent;
    }
}


PathNode pq_pop(PriorityQueue* pq) {
    PathNode top = pq->nodes[0];
    pq->nodes[0] = pq->nodes[--pq->size];
    uint16_t curr = 0;
    while (true) {
        uint16_t left = (uint16_t)(2 * curr + 1);
        uint16_t right = (uint16_t)(2 * curr + 2);
        uint16_t smallest = curr;
        if (left < pq->size && pq->nodes[left].f_cost < pq->nodes[smallest].f_cost) smallest = left;
        if (right < pq->size && pq->nodes[right].f_cost < pq->nodes[smallest].f_cost) smallest = right;
        if (smallest == curr) break;
        PathNode temp = pq->nodes[curr];
        pq->nodes[curr] = pq->nodes[smallest];
        pq->nodes[smallest] = temp;
        curr = smallest;
    }
    return top;
}

Vec2 get_next_move(Vec2 start, Vec2 target) {
    if (start.x == target.x && start.y == target.y) return start;

    static bool closed[MAP_HEIGHT][MAP_WIDTH];
    memset(closed, 0, sizeof(closed));
    static Vec2 came_from[MAP_HEIGHT][MAP_WIDTH];
    memset(came_from, -1, sizeof(came_from));

    PriorityQueue pq;
    memset(&pq, 0, sizeof(PriorityQueue));

    PathNode s_node;
    s_node.pos = start;
    s_node.g_cost = 0;
    s_node.h_cost = heuristic(start, target);
    s_node.f_cost = (uint16_t)(s_node.g_cost + s_node.h_cost);
    s_node.parent.x = -1;
    s_node.parent.y = -1;
    
    pq_push(&pq, s_node);

    while (pq.size > 0) {
        PathNode curr = pq_pop(&pq);

        if (curr.pos.x == target.x && curr.pos.y == target.y) {
            Vec2 trace = curr.pos;
            while (came_from[trace.y][trace.x].x != start.x || came_from[trace.y][trace.x].y != start.y) {
                if (came_from[trace.y][trace.x].x == -1) return start;
                trace = came_from[trace.y][trace.x];
            }
            return trace;
        }

        closed[curr.pos.y][curr.pos.x] = true;

        for (int i = 0; i < 4; ++i) {
            Vec2 n;
            n.x = curr.pos.x + DIRS[i].x;
            n.y = curr.pos.y + DIRS[i].y;

            if (n.x < 0 || n.x >= MAP_WIDTH || n.y < 0 || n.y >= MAP_HEIGHT) continue;
            if (current_map[n.y][n.x] == '#' || current_map[n.y][n.x] == '-') continue;
            if (closed[n.y][n.x]) continue;

            PathNode n_node;
            n_node.pos = n;
            n_node.g_cost = (uint16_t)(curr.g_cost + 1);
            n_node.h_cost = heuristic(n, target);
            n_node.f_cost = (uint16_t)(n_node.g_cost + n_node.h_cost);
            n_node.parent = curr.pos;
            
            came_from[n.y][n.x] = curr.pos;
            pq_push(&pq, n_node);
        }
    }
    
  
    for (int i = 0; i < 4; ++i) {
        Vec2 r;
        r.x = start.x + DIRS[i].x;
        r.y = start.y + DIRS[i].y;
        if (current_map[r.y][r.x] != '#' && current_map[r.y][r.x] != '-') return r;
    }
    return start;
}


void update_player() {
    Vec2 test_next;
    test_next.x = player.pos.x + player.next_dir.x;
    test_next.y = player.pos.y + player.next_dir.y;
    
    if (test_next.x < 0) test_next.x = MAP_WIDTH - 1;
    if (test_next.x >= MAP_WIDTH) test_next.x = 0;

    if (current_map[test_next.y][test_next.x] != '#' && current_map[test_next.y][test_next.x] != '-') {
        player.dir = player.next_dir;
    }

    Vec2 next;
    next.x = player.pos.x + player.dir.x;
    next.y = player.pos.y + player.dir.y;
    
    if (next.x < 0) next.x = MAP_WIDTH - 1;
    if (next.x >= MAP_WIDTH) next.x = 0;

    if (current_map[next.y][next.x] != '#' && current_map[next.y][next.x] != '-') {
        player.pos = next;
        
        if (current_map[player.pos.y][player.pos.x] == '.') {
            current_map[player.pos.y][player.pos.x] = ' ';
            game.score += 10;
            game.pellets_remaining--;
        } else if (current_map[player.pos.y][player.pos.x] == 'O') {
            current_map[player.pos.y][player.pos.x] = ' ';
            game.score += 50;
            game.pellets_remaining--;
            game.frightened_timer = 50; 
            for (int i = 0; i < MAX_GHOSTS; ++i) {
                if (ghost_ai[i].mode != MODE_EATEN) ghost_ai[i].mode = MODE_FRIGHTENED;
            }
        }
    }
}

void update_ai() {
    for (int i = 0; i < MAX_GHOSTS; ++i) {
        if (!ghosts[i].active) continue;
        
        AIController* ai = &ghost_ai[i];
        
        ai->tick_counter++;
        if (ai->tick_counter < ai->move_delay) continue;
        ai->tick_counter = 0;

        if (ai->mode != MODE_FRIGHTENED && ai->mode != MODE_EATEN) {
            if (game.global_tick % 200 == 0) {
                ai->mode = (ai->mode == MODE_CHASE) ? MODE_SCATTER : MODE_CHASE;
            }
        }

        if (ai->mode == MODE_EATEN) {
            ai->target = ghosts[i].start_pos;
            if (ghosts[i].pos.x == ai->target.x && ghosts[i].pos.y == ai->target.y) {
                ai->mode = MODE_CHASE;
            }
        } else if (ai->mode == MODE_FRIGHTENED) {
            ai->target.x = rand() % MAP_WIDTH;
            ai->target.y = rand() % MAP_HEIGHT;
        } else if (ai->mode == MODE_SCATTER) {
            Vec2 corners[4] = {{MAP_WIDTH-2, 1}, {1, 1}, {MAP_WIDTH-2, MAP_HEIGHT-2}, {1, MAP_HEIGHT-2}};
            ai->target = corners[i];
        } else { 
            if (i == 0) {
                ai->target = player.pos; 
            } else if (i == 1) {
                ai->target.x = player.pos.x + player.dir.x * 4; 
                ai->target.y = player.pos.y + player.dir.y * 4;
            } else {
                ai->target = player.pos;
            }
        }

        ghosts[i].pos = get_next_move(ghosts[i].pos, ai->target);
    }
    
    if (game.frightened_timer > 0) {
        game.frightened_timer--;
        if (game.frightened_timer == 0) {
            for (int i = 0; i < MAX_GHOSTS; ++i) {
                if (ghost_ai[i].mode == MODE_FRIGHTENED) ghost_ai[i].mode = MODE_CHASE;
            }
        }
    }
}

void check_collisions() {
    for (int i = 0; i < MAX_GHOSTS; ++i) {
        if (ghosts[i].pos.x == player.pos.x && ghosts[i].pos.y == player.pos.y) {
            if (ghost_ai[i].mode == MODE_FRIGHTENED) {
                ghost_ai[i].mode = MODE_EATEN;
                game.score += 200;
            } else if (ghost_ai[i].mode != MODE_EATEN) {
                game.game_over = true;
            }
        }
    }
}

void render_frame() {
    printf("\033[H"); 
    
    printf("%sscore %06d%s    |    %slevel: %02d%s\n", C_WHT, game.score, C_RST, C_WHT, game.level, C_RST);
    printf("--------------------------------\n");

    char buffer[MAP_HEIGHT][MAP_WIDTH * 15];
    
    for (int y = 0; y < MAP_HEIGHT; ++y) {
        buffer[y][0] = '\0';
        for (int x = 0; x < MAP_WIDTH; ++x) {
            bool entity_here = false;
            
            if (player.pos.x == x && player.pos.y == y) {
                strcat(buffer[y], C_YEL "C" C_RST);
                entity_here = true;
            } else {
                for (int i = 0; i < MAX_GHOSTS; ++i) {
                    if (ghosts[i].pos.x == x && ghosts[i].pos.y == y) {
                        if (ghost_ai[i].mode == MODE_FRIGHTENED) strcat(buffer[y], C_BLU "M" C_RST);
                        else if (ghost_ai[i].mode == MODE_EATEN) strcat(buffer[y], C_WHT "\"" C_RST);
                        else {
                            if (i == 0) strcat(buffer[y], C_RED "M" C_RST);
                            else if (i == 1) strcat(buffer[y], C_MAG "M" C_RST);
                            else if (i == 2) strcat(buffer[y], C_CYA "M" C_RST);
                            else strcat(buffer[y], C_ORG "M" C_RST);
                        }
                        entity_here = true;
                        break;
                    }
                }
            }

            if (!entity_here) {
                char ch = current_map[y][x];
                if (ch == '#') strcat(buffer[y], C_BLU "█" C_RST);
                else if (ch == '.') strcat(buffer[y], C_WHT "·" C_RST);
                else if (ch == 'O') strcat(buffer[y], C_WHT "o" C_RST);
                else strcat(buffer[y], " ");
            }
        }
    }

    for (int y = 0; y < MAP_HEIGHT; ++y) {
        printf("%s\n", buffer[y]);
    }
}


void show_game_over_screen() {
    printf("\033[2J\033[H");
    printf("\n\n\n");
    printf("   %s###########################################%s\n", C_RED, C_RST);
    printf("   %s#                                         #%s\n", C_RED, C_RST);
    printf("   %s#               game over!               #%s\n", C_RED, C_RST);
    printf("   %s#                                         #%s\n", C_RED, C_RST);
    printf("   %s###########################################%s\n", C_RED, C_RST);
    printf("\n");
    printf("         %sLevel:%s %s%d%s\n", C_WHT, C_RST, C_YEL, game.level, C_RST);
    printf("         %s Score: %s     %s%d%s\n", C_WHT, C_RST, C_YEL, game.score, C_RST);
    printf("\n\n");
    
  
    for (int i = 3; i > 0; i--) {
        printf("\r         %sNew game %s%d%s ...%s", C_WHT, C_YEL, i, C_WHT, C_RST);
        fflush(stdout);
        sleep(1);
    }
    
   
    while (kbhit()) {
        getchar();
    }
}


int main() {
    srand((unsigned int)time(NULL));
    enable_raw_mode();
    printf("\033[2J"); 



    while (1) {
        game.score = 0;
        game.level = 1;
        game.game_over = false;
        game.global_tick = 0;
        game.frightened_timer = 0;
        
        load_level();

        
        while (!game.game_over) {
            
            while (kbhit()) {
                char c = (char)getchar();
                c = (char)tolower((unsigned char)c);
                if (c == 'q') { 
                    disable_raw_mode();
                    printf("\033[2J\033[H");
                    printf("quited\n");
                    return 0; 
                }
                if (c == 'w') { player.next_dir.x = 0; player.next_dir.y = -1; }
                if (c == 's') { player.next_dir.x = 0; player.next_dir.y = 1; }
                if (c == 'a') { player.next_dir.x = -1; player.next_dir.y = 0; }
                if (c == 'd') { player.next_dir.x = 1; player.next_dir.y = 0; }
            }

    
            update_player();
            update_ai();
            check_collisions();
            

            if (game.pellets_remaining == 0) {
                game.level++;
                load_level();
                usleep(1000000); 
            }

            render_frame();
            
            game.global_tick++;
            usleep(60000); 
        }

        show_game_over_screen();
    }

    return 0;
}