#include "snake.h"
#include <terminal.h>
#include <memory.h>
#include <pit.h>
#include <keyboard.h>
#include <io.h>
#include <libc/stdint.h>

// Game Parameters
#define STATUS_ROW 0
#define BORDER_TOP 1
#define BORDER_BOT 24
#define BORDER_LEFT 0
#define BORDER_RIGHT 79
#define GAME_ROW_MIN 2
#define GAME_ROW_MAX 23
#define GAME_COL_MIN 1
#define GAME_COL_MAX 78
#define GAME_ROWS (GAME_ROW_MAX - GAME_ROW_MIN + 1) // 22
#define GAME_COLS (GAME_COL_MAX - GAME_COL_MIN + 1) // 78
#define MAX_SNAKE (GAME_ROWS * GAME_COLS) // 1716
#define GAME_SPEED_MS 150u

typedef struct
{
    uint8_t row;
    uint8_t col;
} Pos;

// State
static Pos *body;
static int head_idx; // index in body[] of the head segment
static int tail_idx; // index in body[] of the tail segment
static int length;
static int8_t dir_row; // direction applied this tick
static int8_t dir_col;
static int8_t next_row; // direction queued by keyboard, applied next tick
static int8_t next_col;
static int growing; // nonzero: grow instead of removing tail this tick
static uint8_t food_row;
static uint8_t food_col;
static uint32_t score;
static uint32_t rng_state;

// RNG: Distribution of food, simplified
static uint32_t rand_next(void)
{
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state;
}

// Drawing helpers
static void draw_str(int row, int col, const char *s, uint8_t colour)
{
    while (*s)
        terminal_set_cell(row, col++, *s++, colour);
}

// Draws val at (row, col) and returns the column after the last digit.
static int draw_uint(int row, int col, uint32_t val, uint8_t colour)
{
    char buf[12];
    int n = 0;
    if (val == 0)
    {
        terminal_set_cell(row, col++, '0', colour);
        return col;
    }
    while (val)
    {
        buf[n++] = (char)('0' + val % 10);
        val /= 10;
    }
    for (int i = n - 1; i >= 0; i--)
        terminal_set_cell(row, col++, buf[i], colour);
    return col;
}

static void draw_status(void)
{
    for (int c = 0; c < 80; c++)
        terminal_set_cell(STATUS_ROW, c, ' ', COLOR_STATUS_TEXT);

    int col = 1;
    draw_str(STATUS_ROW, col, "SNAKE", COLOR_STATUS_TEXT);
    col += 5;
    terminal_set_cell(STATUS_ROW, col++, ' ', COLOR_STATUS_TEXT);
    terminal_set_cell(STATUS_ROW, col++, '|', COLOR_BORDER);
    draw_str(STATUS_ROW, col, " Score: ", COLOR_STATUS_TEXT);
    col += 8;
    col = draw_uint(STATUS_ROW, col, score, COLOR_SCORE_VALUE);
    terminal_set_cell(STATUS_ROW, col++, ' ', COLOR_STATUS_TEXT);
    terminal_set_cell(STATUS_ROW, col++, '|', COLOR_BORDER);
    draw_str(STATUS_ROW, col, " W/A/S/D: Move", COLOR_STATUS_TEXT);
    col += 14;
    terminal_set_cell(STATUS_ROW, col++, ' ', COLOR_STATUS_TEXT);
    terminal_set_cell(STATUS_ROW, col++, '|', COLOR_BORDER);
    draw_str(STATUS_ROW, col, " Q: Quit", COLOR_STATUS_TEXT);
}

static void draw_border(void)
{
    terminal_set_cell(BORDER_TOP, BORDER_LEFT, '+', COLOR_BORDER);
    for (int c = BORDER_LEFT + 1; c < BORDER_RIGHT; c++)
        terminal_set_cell(BORDER_TOP, c, '-', COLOR_BORDER);
    terminal_set_cell(BORDER_TOP, BORDER_RIGHT, '+', COLOR_BORDER);

    terminal_set_cell(BORDER_BOT, BORDER_LEFT, '+', COLOR_BORDER);
    for (int c = BORDER_LEFT + 1; c < BORDER_RIGHT; c++)
        terminal_set_cell(BORDER_BOT, c, '-', COLOR_BORDER);
    terminal_set_cell(BORDER_BOT, BORDER_RIGHT, '+', COLOR_BORDER);

    for (int r = GAME_ROW_MIN; r <= GAME_ROW_MAX; r++)
    {
        terminal_set_cell(r, BORDER_LEFT, '|', COLOR_BORDER);
        terminal_set_cell(r, BORDER_RIGHT, '|', COLOR_BORDER);
    }
}

// Finds a random cell not occupied by the snake body, draws food there.
static void place_food(void)
{
    uint8_t r, c;
    int occupied;
    do
    {
        r = (uint8_t)((rand_next() % (uint32_t)GAME_ROWS) + GAME_ROW_MIN);
        c = (uint8_t)((rand_next() % (uint32_t)GAME_COLS) + GAME_COL_MIN);
        occupied = 0;
        int i = tail_idx;
        for (;;)
        {
            if (body[i].row == r && body[i].col == c)
            {
                occupied = 1;
                break;
            }
            if (i == head_idx)
                break;
            i = (i + 1) % MAX_SNAKE;
        }
    } while (occupied);
    food_row = r;
    food_col = c;
    terminal_set_cell(r, c, '*', COLOR_FOOD);
}

// Returns 1 if (r, c) is occupied by the snake body.
// Excludes the tail when it will vacate this tick (snake is not growing).
static int is_body(uint8_t r, uint8_t c)
{
    int i = growing ? tail_idx : (tail_idx + 1) % MAX_SNAKE;
    for (;;)
    {
        if (body[i].row == r && body[i].col == c)
            return 1;
        if (i == head_idx)
            break;
        i = (i + 1) % MAX_SNAKE;
    }
    return 0;
}

// Sound
// Programs PIT channel 2 and speaker to produce a tone, then silences it.
// Reads port 0x61 before and after so other bits are preserved.
static void play_tone(uint32_t freq, uint32_t ms)
{
    uint32_t div = 1193180u / freq;
    outb(0x43, 0xB6);
    outb(0x42, (uint8_t)(div & 0xFF));
    outb(0x42, (uint8_t)((div >> 8) & 0xFF));
    uint8_t ctrl = inb(0x61);
    outb(0x61, ctrl | 0x03);
    sleep_interrupt(ms);
    outb(0x61, ctrl & ~0x03);
}

static void beep_eat(void) { play_tone(880u, 50u); }

static void beep_death(void)
{
    play_tone(440u, 80u);
    play_tone(220u, 80u);
    play_tone(110u, 80u);
}

// Game over
// Overlays a game-over message on the frozen game area.
// Returns 1 if the player wants to restart, 0 if they quit.
static int game_over_screen(void)
{
    int mid = (GAME_ROW_MIN + GAME_ROW_MAX) / 2;

    draw_str(mid - 1, (80 - 9) / 2, "GAME OVER", COLOR_GAMEOVER);
    draw_str(mid, (80 - 7) / 2, "Score: ", COLOR_STATUS_TEXT);
    draw_uint(mid, (80 - 7) / 2 + 7, score, COLOR_SCORE_VALUE);
    draw_str(mid + 1, (80 - 27) / 2, "SPACE: Play again   Q: Quit", COLOR_STATUS_TEXT);

    for (;;)
    {
        asm volatile("sti; hlt");
        char c;
        while ((c = keyboard_getchar()) != 0)
        {
            if (c == ' ')
                return 1;
            if (c == 'q')
                return 0;
        }
    }
}

static void game_init(void)
{
    terminal_init();

    // Seed the RNG from the current tick so each game differs.
    rng_state = get_tick();

    body = (Pos *)malloc(MAX_SNAKE * sizeof(Pos));

    // Spawn a length-3 snake at the centre of the game area, heading right.
    int start_row = (GAME_ROW_MIN + GAME_ROW_MAX) / 2;
    int start_col = (GAME_COL_MIN + GAME_COL_MAX) / 2;
    tail_idx = 0;
    head_idx = 2;
    length = 3;
    body[0] = (Pos){(uint8_t)start_row, (uint8_t)(start_col - 2)};
    body[1] = (Pos){(uint8_t)start_row, (uint8_t)(start_col - 1)};
    body[2] = (Pos){(uint8_t)start_row, (uint8_t)start_col};

    dir_row = 0;
    dir_col = 1;
    next_row = 0;
    next_col = 1;
    growing = 0;
    score = 0;

    draw_border();
    terminal_set_cell(body[0].row, body[0].col, 'O', COLOR_SNAKE_BODY);
    terminal_set_cell(body[1].row, body[1].col, 'O', COLOR_SNAKE_BODY);
    terminal_set_cell(body[2].row, body[2].col, 'O', COLOR_SNAKE_HEAD);
    place_food();
    draw_status();
}

void snake_run(void)
{
    int restart;
    do
    {
        restart = 0;
        game_init();

        uint32_t last_tick = get_tick();
        int running = 1;
        int died = 0;

        while (running)
        {
            asm volatile("sti; hlt");

            char c;
            while ((c = keyboard_getchar()) != 0)
            {
                switch (c)
                {
                case 'w':
                    if (dir_row != 1)
                    {
                        next_row = -1;
                        next_col = 0;
                    }
                    break;
                case 's':
                    if (dir_row != -1)
                    {
                        next_row = 1;
                        next_col = 0;
                    }
                    break;
                case 'a':
                    if (dir_col != 1)
                    {
                        next_row = 0;
                        next_col = -1;
                    }
                    break;
                case 'd':
                    if (dir_col != -1)
                    {
                        next_row = 0;
                        next_col = 1;
                    }
                    break;
                case 'q':
                    running = 0;
                    break;
                default:
                    break;
                }
            }
            if (!running)
                break;

            if (get_tick() - last_tick < GAME_SPEED_MS)
                continue;
            last_tick = get_tick();

            dir_row = next_row;
            dir_col = next_col;

            int nr = (int)body[head_idx].row + dir_row;
            int nc = (int)body[head_idx].col + dir_col;

            if (nr < GAME_ROW_MIN || nr > GAME_ROW_MAX ||
                nc < GAME_COL_MIN || nc > GAME_COL_MAX ||
                is_body((uint8_t)nr, (uint8_t)nc))
            {
                died = 1;
                running = 0;
                break;
            }

            int old_head = head_idx;
            head_idx = (head_idx + 1) % MAX_SNAKE;
            body[head_idx] = (Pos){(uint8_t)nr, (uint8_t)nc};
            terminal_set_cell(body[old_head].row, body[old_head].col, 'O', COLOR_SNAKE_BODY);
            terminal_set_cell(nr, nc, 'O', COLOR_SNAKE_HEAD);

            if (growing)
            {
                growing = 0;
                length++;
            }
            else
            {
                terminal_set_cell(body[tail_idx].row, body[tail_idx].col, ' ', 0x00);
                tail_idx = (tail_idx + 1) % MAX_SNAKE;
            }

            if ((uint8_t)nr == food_row && (uint8_t)nc == food_col)
            {
                score += 10;
                growing = 1;
                beep_eat();
                place_food();
                draw_status();
            }
        }

        if (died)
        {
            beep_death();
            restart = game_over_screen();
        }

        free(body);
    } while (restart);

    terminal_init();
}
