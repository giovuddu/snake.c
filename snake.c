#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <termios.h>
#include <fcntl.h>

#define GRID_WIDTH 16
#define GRID_CELLS (GRID_WIDTH*GRID_WIDTH)

#define TICK_MS 200

/* ------------------------- */

typedef enum snake_direction {
    SNAKE_DIR_UP,
    SNAKE_DIR_DOWN,
    SNAKE_DIR_LEFT,
    SNAKE_DIR_RIGHT,
} snake_direction_t;

typedef struct snake_tile {
    int x;
    int y;
    struct snake_tile *next;
    struct snake_tile *prev;
} snake_tile_t;

typedef struct snake {
    snake_tile_t *head;
    snake_tile_t *tail;
} snake_t;

typedef enum grid_cell {
    CELL_EMPTY,
    CELL_SNAKE,
    CELL_FOOD,
} grid_cell_t;

typedef grid_cell_t grid_t[GRID_WIDTH][GRID_WIDTH];

typedef enum outcome {
    LOST,
    CONTINUE,
    WON,
} outcome_t;

struct termios original;

/* ------------------------- */

int wrap (int v) {
    return ((v % GRID_WIDTH) + GRID_WIDTH) % GRID_WIDTH;
}

grid_cell_t get_cell(grid_t grid, int x, int y) {
    return grid[wrap(x)][wrap(y)];
}

void set_cell(grid_t grid, int x, int y, grid_cell_t state) {
    grid[wrap(x)][wrap(y)] = state;
}

void init_grid(grid_t grid) {
    for (int y = 0; y < GRID_WIDTH; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            set_cell(grid, x, y, CELL_EMPTY);
        }
    }
}

snake_tile_t *create_snake_tile(){
    snake_tile_t *t = malloc(sizeof(snake_tile_t));
    if (!t) { printf("wtf bro? you're out of ram! "); exit(1); }
    return t;
}

void free_snake_tile(snake_tile_t *snake_tile) {
    free(snake_tile);
}

void init_snake(snake_t *snake) {
    snake_tile_t *core_tile = create_snake_tile();
    core_tile->x = GRID_WIDTH / 2;
    core_tile->y = GRID_WIDTH / 2;
    core_tile->prev = NULL;
    core_tile->next = NULL;

    snake->head = core_tile;
    snake->tail = core_tile;
}

void fill_snake_in_grid_of(grid_t grid, snake_tile_t *snake_head, grid_cell_t kind) {
    set_cell(grid, snake_head->x, snake_head->y, kind);

    if (!(snake_head->prev)) return;
    fill_snake_in_grid_of(grid, snake_head->prev, kind);
}

bool gen_food_in_grid(grid_t grid) {
    int empty[GRID_CELLS];

    int count = 0;
    for (int y = 0; y < GRID_WIDTH; y++)
        for (int x = 0; x < GRID_WIDTH; x++)
            if (get_cell(grid, x, y) == CELL_EMPTY) {
                empty[count] = y * GRID_WIDTH + x;
                count++;
            }
    if (!count) return true; // you won?

    int pick = empty[rand() % count];

    set_cell(grid, pick % GRID_WIDTH, pick / GRID_WIDTH, CELL_FOOD);
    return false;
}

void print_grid(grid_t grid) {
    printf("\033[2J\033[H");
    for (int y = GRID_WIDTH - 1; y >= 0; y--) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            grid_cell_t cur = get_cell(grid, x, y);

            switch (cur) {
                case CELL_EMPTY: printf("░░"); break;
                case CELL_SNAKE: printf("██"); break;
                case CELL_FOOD: printf("\033[31m██\033[0m"); break; // red
            }
        }
        printf("\n");
    }
}

void snake_next_position(snake_t *snake, snake_direction_t direction, int *out_x, int *out_y) {
    int dx[] = {0, 0, -1, 1};
    int dy[] = {1, -1, 0, 0};

    *out_x = wrap(snake->head->x + dx[direction]);
    *out_y = wrap(snake->head->y + dy[direction]);
}

void snake_move(snake_t *snake, int new_x, int new_y, bool eat) {
    snake_tile_t *new_head = create_snake_tile();
    new_head->x = new_x;
    new_head->y = new_y;
    new_head->next = NULL;
    new_head->prev = snake->head;
    snake->head->next = new_head;
    snake->head = new_head;

    if (!eat) {
        snake_tile_t *new_tail = snake->tail->next;
        free_snake_tile(snake->tail);
        new_tail->prev = NULL;
        snake->tail = new_tail;
    }
}

outcome_t compute_next_frame(grid_t grid, snake_t *snake, snake_direction_t direction) {
    int new_x, new_y;
    snake_next_position(snake, direction, &new_x, &new_y);


    grid_cell_t head_tile = get_cell(grid, new_x, new_y);
    bool eat = false;

    switch (head_tile) {
        case CELL_FOOD: eat = true; break;
        case CELL_SNAKE: return LOST;
        case CELL_EMPTY: break;
    }
    if (eat) {
        if (gen_food_in_grid(grid)) return WON;
    }
    fill_snake_in_grid_of(grid, snake->head, CELL_EMPTY);
    snake_move(snake, new_x, new_y, eat);
    fill_snake_in_grid_of(grid, snake->head, CELL_SNAKE);

    return CONTINUE;
}

/* ------ copypasted input stuff ------ */

void set_raw_mode() {
    tcgetattr(STDIN_FILENO, &original);

    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag &= ~(ICANON | ECHO);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

void restore_terminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &original);
}

// i'm a noob
snake_direction_t opposite(snake_direction_t dir) {
    switch (dir) {
        case SNAKE_DIR_UP:    return SNAKE_DIR_DOWN;
        case SNAKE_DIR_DOWN:  return SNAKE_DIR_UP;
        case SNAKE_DIR_LEFT:  return SNAKE_DIR_RIGHT;
        case SNAKE_DIR_RIGHT: return SNAKE_DIR_LEFT;
    }
    return dir;
}

snake_direction_t read_input(snake_direction_t old_dir) {
    char c;
    snake_direction_t dir = old_dir;
    while (read(STDIN_FILENO, &c, 1) > 0) {
        switch (c) {
            case 'w': dir = SNAKE_DIR_UP;    break;
            case 's': dir = SNAKE_DIR_DOWN;  break;
            case 'a': dir = SNAKE_DIR_LEFT;  break;
            case 'd': dir = SNAKE_DIR_RIGHT; break;
        }
    }
    return dir == opposite(old_dir) ? old_dir : dir;
}

/* -------------------------------- */

long now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int main() {
    srand(time(NULL));
    set_raw_mode();
    atexit(restore_terminal);

    grid_t grid;
    init_grid(grid);
    gen_food_in_grid(grid);
    snake_t snake;
    init_snake(&snake);

    snake_direction_t direction = SNAKE_DIR_RIGHT;
    while (1) {
        long start = now_ms();
        direction = read_input(direction);

        outcome_t outcome = compute_next_frame(grid, &snake, direction);
        if (outcome == LOST) {
            printf("you lost, you bad\n");
            break;
        } else if (outcome == WON) {
            printf("you... won?\n");
            break;
        }
        print_grid(grid);

        long wait = TICK_MS - (now_ms() - start);
        if (wait > 0) usleep(wait * 1000);
    }
    restore_terminal();
    return 0;
}
