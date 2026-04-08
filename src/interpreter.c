#include "interpreter.h"
#include "funge_stack.h"
#include "reporter.h"
#include "stack.h"
#include "string_builder.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define FILENAME "interpreter.c"

#define VERSION_NUMBER 1
#define HANDPRINT 0x534249 /* SBI */

/* TODO - change these if/when implemented */
#define T_IMPLEMENTED 0
#define I_IMPLEMENTED 0
#define O_IMPLEMENTED 0
#define EQ_IMPLEMENTED 0
#define UNBUFFERED_IO 1

typedef struct {
    int32_t x;
    int32_t y;
} vector_t;

struct Interpreter {
    StringBuilder *contents;

    /* ip location can be negative */
    vector_t ip;

    vector_t top_left;
    vector_t bottom_right;

    Stack *stack_stack; /* Stack<FungeStack> */

    /* not included in `stack_stack` but technically its head */
    FungeStack *stack;

    /* contents idx of start of row ip.y */
    size_t ip_y_contents_idx;
    vector_t momentum;

    vector_t storage_offset;
};

static void execute_instruction(Interpreter *self, char instr);

static void update_ip_y_contents_idx(Interpreter *self, int32_t y_diff) {
    while (y_diff > 0) {
        while (string_builder_get_char(self->contents, self->ip_y_contents_idx)
               != '\n') {
            self->ip_y_contents_idx++;
        }
        self->ip_y_contents_idx++;
        y_diff--;
    }

    while (y_diff < 0) {
        self->ip_y_contents_idx--;
        while (self->ip_y_contents_idx != 0
               && string_builder_get_char(self->contents,
                                          self->ip_y_contents_idx - 1)
                      != '\n') {
            self->ip_y_contents_idx--;
        }
        y_diff++;
    }
}

static void follow_momentum(Interpreter *self) {
    const int32_t old_y = self->ip.y;

    /* follow momentum */
    self->ip.x += self->momentum.x;
    self->ip.y += self->momentum.y;

    if (self->ip.x > self->bottom_right.x) {
        self->ip.x = self->top_left.x + (self->ip.x - self->bottom_right.x - 1);
    } else if (self->ip.x < self->top_left.x) {
        self->ip.x = self->bottom_right.x - (self->top_left.x - self->ip.x - 1);
    }

    if (self->ip.y > self->bottom_right.y) {
        self->ip.y = self->top_left.y + (self->ip.y - self->bottom_right.y - 1);
    } else if (self->ip.y < self->top_left.y) {
        self->ip.y = self->bottom_right.y - (self->top_left.y - self->ip.y - 1);
    }

    update_ip_y_contents_idx(self, self->ip.y - old_y);
}

static char next_instruction(Interpreter *self) {
    char instr = ' ';
    int32_t col;
    size_t i;
    follow_momentum(self);

    col = self->top_left.x - 1;
    i = self->ip_y_contents_idx;
    while (col != self->ip.x && instr != '\n'
           && i < string_builder_len(self->contents)) {
        col++;
        instr = string_builder_get_char(self->contents, i);
        i++;
    }

    if (col != self->ip.x || instr == '\n' || instr == '\r') {
        instr = ' ';
    }

    return instr;
}

static void reflect(Interpreter *self) {
    vector_t reflection;
    reflection.x = -self->momentum.x;
    reflection.y = -self->momentum.y;
    self->momentum = reflection;
}

static void duplicate_top(Interpreter *self) {
    funge_cell_t top = funge_stack_pop(self->stack);
    funge_stack_push(self->stack, top);
    funge_stack_push(self->stack, top);
}

static void update_momentum(Interpreter *self, int32_t new_x, int32_t new_y) {
    self->momentum.x = new_x;
    self->momentum.y = new_y;
}

static void logical_not(Interpreter *self) {
    funge_stack_push(self->stack, !funge_stack_pop(self->stack));
}

static void add(Interpreter *self) {
    funge_cell_t b = funge_stack_pop(self->stack);
    funge_cell_t a = funge_stack_pop(self->stack);
    funge_stack_push(self->stack, a + b);
}

static void subtract(Interpreter *self) {
    funge_cell_t b = funge_stack_pop(self->stack);
    funge_cell_t a = funge_stack_pop(self->stack);
    funge_stack_push(self->stack, a - b);
}

static void multiply(Interpreter *self) {
    funge_cell_t b = funge_stack_pop(self->stack);
    funge_cell_t a = funge_stack_pop(self->stack);
    funge_stack_push(self->stack, a * b);
}

static void divide(Interpreter *self) {
    funge_cell_t b = funge_stack_pop(self->stack);
    funge_cell_t a = funge_stack_pop(self->stack);
    if (b == 0) {
        /* TODO - technically this should prompt user */
        funge_stack_push(self->stack, 0);
    } else {
        funge_stack_push(self->stack, a / b);
    }
}

static void remainder(Interpreter *self) {
    funge_cell_t b = funge_stack_pop(self->stack);
    funge_cell_t a = funge_stack_pop(self->stack);
    if (b == 0) {
        /* TODO - technically this should prompt user */
        funge_stack_push(self->stack, 0);
    } else {
        funge_stack_push(self->stack, a % b);
    }
}

static void swap(Interpreter *self) {
    funge_cell_t b = funge_stack_pop(self->stack);
    funge_cell_t a = funge_stack_pop(self->stack);
    funge_stack_push(self->stack, b);
    funge_stack_push(self->stack, a);
}

static void greater(Interpreter *self) {
    funge_cell_t b = funge_stack_pop(self->stack);
    funge_cell_t a = funge_stack_pop(self->stack);
    funge_stack_push(self->stack, a > b);
}

static void string_mode(Interpreter *self) {
    char instr;
    bool last_was_space = false;
    while (true) {
        instr = next_instruction(self);
        if (instr == '"') {
            break;
        }
        if (instr != ' ' || !last_was_space) {
            funge_stack_push(self->stack, instr);
        }
        /* NOTE - if `t` gets implemented, several of these should be 1 tick */
        last_was_space = instr == ' ';
    }
}

static void fetch_character(Interpreter *self) {
    char instr = next_instruction(self);
    funge_stack_push(self->stack, instr);
}

static void comment(Interpreter *self) {
    char instr = ' ';
    while (instr != ';') {
        instr = next_instruction(self);
    }
}

static void turn_left(Interpreter *self) {
    vector_t new_momentum;
    new_momentum.x = self->momentum.y;
    new_momentum.y = -self->momentum.x;
    self->momentum = new_momentum;
}

static void turn_right(Interpreter *self) {
    vector_t new_momentum;
    new_momentum.x = -self->momentum.y;
    new_momentum.y = self->momentum.x;
    self->momentum = new_momentum;
}

static void east_west_if(Interpreter *self) {
    funge_cell_t cell = funge_stack_pop(self->stack);
    if (cell == 0) {
        update_momentum(self, 1, 0);
    } else {
        update_momentum(self, -1, 0);
    }
}

static void north_south_if(Interpreter *self) {
    funge_cell_t cell = funge_stack_pop(self->stack);
    if (cell == 0) {
        update_momentum(self, 0, 1);
    } else {
        update_momentum(self, 0, -1);
    }
}

static void compare(Interpreter *self) {
    funge_cell_t b = funge_stack_pop(self->stack);
    funge_cell_t a = funge_stack_pop(self->stack);
    if (a > b) {
        turn_right(self);
    } else if (a < b) {
        turn_left(self);
    }
}

static void absolute_delta(Interpreter *self) {
    funge_cell_t y = funge_stack_pop(self->stack);
    funge_cell_t x = funge_stack_pop(self->stack);
    self->momentum.x = x;
    self->momentum.y = y;
}

static void output_character(Interpreter *self) {
    putchar(funge_stack_pop(self->stack));
}

static void output_integer(Interpreter *self) {
    printf("%i ", funge_stack_pop(self->stack));
}

static void input_character(Interpreter *self) {
    funge_stack_push(self->stack, getchar());
}

static void input_integer(Interpreter *self) {
    int i;
    scanf("%i", &i);
    funge_stack_push(self->stack, i);
}

static void go_away(Interpreter *self) {
    uint8_t val = (rand() >> 4);

    self->momentum.x = val & 2 ? 0 : (val & 1 ? 1 : -1);
    self->momentum.y = val & 2 ? (val & 1 ? 1 : -1) : 0;
}

static void begin_block(Interpreter *self) {
    FungeStack *temp_stack = funge_stack_create();
    funge_cell_t n = funge_stack_pop(self->stack);
    funge_cell_t i;

    size_t preserved_ip_y_contents_idx = self->ip_y_contents_idx;
    vector_t preserved_ip = self->ip;

    for (i = 0; i < n; i++) {
        funge_stack_push(temp_stack, funge_stack_pop(self->stack));
    }
    for (i = 0; i > n; i--) {
        funge_stack_push(self->stack, '\0');
    }
    funge_stack_push(self->stack, self->storage_offset.x);
    funge_stack_push(self->stack, self->storage_offset.y);
    stack_push(self->stack_stack, self->stack);
    self->stack = funge_stack_create();
    while (funge_stack_size(temp_stack) != 0) {
        funge_stack_push(self->stack, funge_stack_pop(temp_stack));
    }
    funge_stack_destroy(temp_stack);

    follow_momentum(self);

    self->storage_offset.x = self->ip.x;
    self->storage_offset.y = self->ip.y;

    self->ip = preserved_ip;
    self->ip_y_contents_idx = preserved_ip_y_contents_idx;
}

static void end_block(Interpreter *self) {
    FungeStack *temp_stack = funge_stack_create();
    funge_cell_t n = funge_stack_pop(self->stack);
    funge_cell_t i;

    if (stack_len(self->stack_stack) != 0) {

        self->storage_offset.y = funge_stack_pop(stack_peek(self->stack_stack));
        self->storage_offset.x = funge_stack_pop(stack_peek(self->stack_stack));

        for (i = 0; i < n; i++) {
            funge_stack_push(temp_stack, funge_stack_pop(self->stack));
        }
        for (i = 0; i > n; i--) {
            funge_stack_pop(stack_peek(self->stack_stack));
        }
        funge_stack_destroy(self->stack);
        self->stack = stack_pop(self->stack_stack);
        while (funge_stack_size(temp_stack) != 0) {
            funge_stack_push(self->stack, funge_stack_pop(temp_stack));
        }
        funge_stack_destroy(temp_stack);

    } else {
        reflect(self);
    }
}

static void clear_stack(Interpreter *self) {
    while (funge_stack_size(self->stack) != 0) {
        funge_stack_pop(self->stack);
    }
}

static void add_column_left(Interpreter *self) {
    size_t contents_idx = 0;
    size_t contents_len = string_builder_len(self->contents) + 1;
    char contents_char;
    self->ip_y_contents_idx += self->ip.y - self->top_left.y;
    string_builder_insert_char(self->contents, 0, ' ');
    while (contents_idx < contents_len) {
        contents_char = string_builder_get_char(self->contents, contents_idx);
        if (contents_char == '\n') {
            string_builder_insert_char(self->contents, contents_idx + 1, ' ');
            contents_idx++;
            contents_len++;
        }
        contents_idx++;
    }
}

/**
 * Get contents index. Adds additional 0's and newlines as needed to reach the
 * index.
 *
 * Prereq: (x, y) is within the boundary of self->top_left and
 * self->bottom_right.
 */
static size_t get_contents_index(Interpreter *self, int32_t x, int32_t y) {
    int32_t row = self->top_left.y;
    int32_t col;
    char c;
    size_t i = 0;

    for (row = self->top_left.y; row < y; row++) {
        c = ' ';
        while (c != '\n') {
            if (i == string_builder_len(self->contents)) {
                string_builder_append_char(self->contents, '\n');
                c = '\n';
            } else {
                c = string_builder_get_char(self->contents, i);
            }
            i++;
        }
    }

    for (col = self->top_left.x; col < x; col++) {
        if (i == string_builder_len(self->contents)) {
            string_builder_append_char(self->contents, '\n');
            c = '\n';
        } else {
            c = string_builder_get_char(self->contents, i);
        }
        if (c != '\n') {
            i++;
        } else {
            string_builder_insert_char(self->contents, i, ' ');
            if (i < self->ip_y_contents_idx) {
                self->ip_y_contents_idx++;
            }
        }
    }
    return i;
}

static void put(Interpreter *self) {
    funge_cell_t y = funge_stack_pop(self->stack) + self->storage_offset.y;
    funge_cell_t x = funge_stack_pop(self->stack) + self->storage_offset.x;
    funge_cell_t n = funge_stack_pop(self->stack);
    size_t contents_idx;

    if (y > self->bottom_right.y) {
        self->bottom_right.y = y;
    } else {
        while (y < self->top_left.y) {
            self->top_left.y--;
            string_builder_insert_char(self->contents, 0, '\n');
            self->ip_y_contents_idx++;
        }
    }

    if (x > self->bottom_right.x) {
        self->bottom_right.x = x;
    } else {
        while (x < self->top_left.x) {
            self->top_left.x--;
            add_column_left(self);
        }
    }

    contents_idx = get_contents_index(self, x, y);
    string_builder_set_char(self->contents, contents_idx, n);
}

static void get(Interpreter *self) {
    funge_cell_t y = funge_stack_pop(self->stack) + self->storage_offset.y;
    funge_cell_t x = funge_stack_pop(self->stack) + self->storage_offset.x;
    size_t contents_idx;
    char c = '\0';

    if (y <= self->bottom_right.y && y >= self->top_left.y
        && x <= self->bottom_right.x && x >= self->top_left.x) {
        contents_idx = get_contents_index(self, x, y);
        c = string_builder_get_char(self->contents, contents_idx);
    }

    funge_stack_push(self->stack, c);
}

static void store(Interpreter *self) {
    funge_cell_t y;
    funge_cell_t x;
    funge_cell_t n = funge_stack_pop(self->stack);
    size_t contents_idx;

    follow_momentum(self);
    y = self->ip.y;
    x = self->ip.x;

    if (y > self->bottom_right.y) {
        self->bottom_right.y = y;
    } else {
        while (y < self->top_left.y) {
            self->top_left.y--;
            add_column_left(self);
        }
    }

    if (x > self->bottom_right.x) {
        self->bottom_right.x = x;
    } else {
        while (x < self->top_left.x) {
            self->top_left.x--;
            string_builder_insert_char(self->contents, 0, '\n');
            self->ip_y_contents_idx++;
        }
    }

    contents_idx = get_contents_index(self, x, y);
    string_builder_set_char(self->contents, contents_idx, n);
}

static void iterate(Interpreter *self) {
    funge_cell_t n = funge_stack_pop(self->stack);
    funge_cell_t i;
    size_t preserved_ip_y_contents_idx = self->ip_y_contents_idx;
    vector_t preserved_ip = self->ip;
    char instr;

    do {
        instr = next_instruction(self);
    } while (instr == ' ' || instr == ';');

    if (n > 0) {
        self->ip = preserved_ip;
        self->ip_y_contents_idx = preserved_ip_y_contents_idx;
        for (i = 0; i < n; i++) {
            execute_instruction(self, instr);
        }
    }
}

static void jump(Interpreter *self) {
    funge_cell_t n = funge_stack_pop(self->stack);
    funge_cell_t i;

    if (n >= 0) {
        for (i = 0; i < n; i++) {
            follow_momentum(self);
        }
    } else {
        reflect(self);
        for (i = 0; i > n; i--) {
            follow_momentum(self);
        }
        reflect(self);
    }
}

static void stack_under_stack(Interpreter *self) {
    funge_cell_t n = funge_stack_pop(self->stack);
    if (stack_len(self->stack_stack) == 0) {
        reflect(self);
    } else {
        while (n > 0) {
            funge_stack_push(self->stack,
                             funge_stack_pop(stack_peek(self->stack_stack)));
            n--;
        }
        while (n < 0) {
            funge_stack_push(stack_peek(self->stack_stack),
                             funge_stack_pop(self->stack));
            n++;
        }
    }
}

static void sys_info(Interpreter *self) {
    funge_cell_t n = funge_stack_pop(self->stack);
    time_t now_time = time(NULL);
    struct tm now = *localtime(&now_time);
    size_t i;
    size_t original_stack_size = funge_stack_size(self->stack);
    funge_cell_t cell;

    /* TODO - environment variables */
    funge_stack_push(self->stack, '\0');

    /* TODO - command-line arguments */
    funge_stack_push(self->stack, '\0');
    funge_stack_push(self->stack, '\0');

    /* size of stack-stack cells for size of each stack from TOSS to BOSS */
    for (i = stack_len(self->stack_stack); i > 0; i--) {
        funge_stack_push(self->stack,
                         stack_len(stack_get(self->stack_stack, i - 1)));
    }
    funge_stack_push(self->stack, original_stack_size);

    /* number of stacks currently in use */
    funge_stack_push(self->stack, 1 + stack_len(self->stack_stack));

    /* current (hour * 256 * 256) + (minute * 256) + (second) */
    funge_stack_push(self->stack,
                     now.tm_hour * 0x10000 + now.tm_min * 0x100 + now.tm_sec);

    /* current ((year - 1900) * 256 * 256) + (month * 256) + (day of month)
     */
    funge_stack_push(self->stack,
                     now.tm_year * 0x10000 + now.tm_mon * 0x100 + now.tm_mday);

    /* "greatest point that contains non-space" */
    funge_stack_push(self->stack, self->bottom_right.x);

    funge_stack_push(self->stack, self->bottom_right.y);

    /* "least point that contains non-space" */
    /* TODO - interpret this. What if (0, 1) and (1, 0) have non-space, but
     * (0, 0) does? */
    funge_stack_push(self->stack, self->top_left.x);

    funge_stack_push(self->stack, self->top_left.y);

    /* storage offset */
    funge_stack_push(self->stack, self->storage_offset.x);
    funge_stack_push(self->stack, self->storage_offset.y);

    /* IP delta */
    funge_stack_push(self->stack, self->momentum.x);
    funge_stack_push(self->stack, self->momentum.y);

    /* IP position */
    funge_stack_push(self->stack, self->ip.x);
    funge_stack_push(self->stack, self->ip.y);

    /* current IP team number */
    /* TODO - change NetFunge, BeGlad, etc is implemented */
    funge_stack_push(self->stack, 0);

    /* current IP unique ID */
    /* TODO - change if concurrent support added */
    funge_stack_push(self->stack, 0);

    /* number of scalars per vector */
    /* 2 for Befunge */
    funge_stack_push(self->stack, 2);

    /* path separator */
#if defined(_WIN32) || defined(_WIN64)
    funge_stack_push(self->stack, '\\');
#else
    funge_stack_push(self->stack, '/');
#endif

    /* operating paradigm */
    /* 1 represents `system()` behavior in C */
    funge_stack_push(self->stack, EQ_IMPLEMENTED ? 1 : 0);

    /* version number */
    funge_stack_push(self->stack, VERSION_NUMBER);

    /* handprint */
    funge_stack_push(self->stack, HANDPRINT);

    /* funge cell size in bytes */
    funge_stack_push(self->stack, sizeof(funge_cell_t));

    /* flags */
    funge_stack_push(self->stack, UNBUFFERED_IO << 4 | EQ_IMPLEMENTED << 3
                                      | O_IMPLEMENTED << 2 | I_IMPLEMENTED << 1
                                      | T_IMPLEMENTED);
    if (n > 0) {
        cell = funge_stack_get(self->stack, (size_t)(n - 1));
        while (funge_stack_size(self->stack) != original_stack_size) {
            funge_stack_pop(self->stack);
        }
        funge_stack_push(self->stack, cell);
    }
}

static void load_semantics(Interpreter *self) {
    funge_cell_t n = funge_stack_pop(self->stack);
    funge_cell_t value = 0;
    while (n > 0) {
        value *= 0x100;
        value += funge_stack_pop(self->stack);
        n--;
    }
    /* TODO - actually load semantic */
    (void)value;
    reflect(self);
}

static void unload_semantics(Interpreter *self) {
    funge_stack_pop(self->stack);
    /* TODO - actually unload semantic */
    reflect(self);
}

static void quit(Interpreter *self) {
    /* TODO - exit cleanly */
    exit(funge_stack_pop(self->stack));
}

static void execute_instruction(Interpreter *self, char instr) {
    /** ---- TODO ----
     * = - execute
     * i - input file
     * o - output file
     * t - split IP
     *
     * Change #defines at start of this file if above are implemented
     */

    if (isdigit(instr)) {
        funge_stack_push(self->stack, instr - '0');
    } else if ('a' <= instr && instr <= 'f') {
        funge_stack_push(self->stack, 10 + instr - 'a');
    } else {
        switch (instr) {
        case ' ':
        case 12: /* form feed */
        case 'z': break;
        case 'n': clear_stack(self); break;
        case '>': update_momentum(self, 1, 0); break;
        case 'v': update_momentum(self, 0, 1); break;
        case '<': update_momentum(self, -1, 0); break;
        case '^': update_momentum(self, 0, -1); break;
        case '[': turn_left(self); break;
        case ']': turn_right(self); break;
        case '?': go_away(self); break;
        case '#': follow_momentum(self); break;
        case 'j': jump(self); break;
        case 'k': iterate(self); break;
        case '"': string_mode(self); break;
        case '\'': fetch_character(self); break;
        case ';': comment(self); break;
        case '$': funge_stack_pop(self->stack); break;
        case '!': logical_not(self); break;
        case '+': add(self); break;
        case '-': subtract(self); break;
        case '*': multiply(self); break;
        case '/': divide(self); break;
        case '%': remainder(self); break;
        case '`': greater(self); break;
        case ':': duplicate_top(self); break;
        case '_': east_west_if(self); break;
        case '|': north_south_if(self); break;
        case 'g': get(self); break;
        case 'p': put(self); break;
        case 's': store(self); break;
        case '\\': swap(self); break;
        case 'w': compare(self); break;
        case 'x': absolute_delta(self); break;
        case ',': output_character(self); break;
        case '.': output_integer(self); break;
        case '~': input_character(self); break;
        case '&': input_integer(self); break;
        case '{': begin_block(self); break;
        case '}': end_block(self); break;
        case 'u': stack_under_stack(self); break;
        case 'y': sys_info(self); break;
        case '(': load_semantics(self); break;
        case ')': unload_semantics(self); break;
        case 'q': quit(self); break;
        case 'r':
        default: reflect(self); break;
        }
    }
}

void interpreter_run(Interpreter *self) {
    char instr = string_builder_get_char(self->contents, 0);
    while (instr != '@') {
        if (self->ip_y_contents_idx != 0 && string_builder_get_char(self->contents, self->ip_y_contents_idx - 1) != '\n') {
            report_logic_error("MISALIGNED");
        }
        execute_instruction(self, instr);
        instr = next_instruction(self);
    };
}

#define CHUNK_SIZE 128

/**
 * Initialize contents. Return `false` iff successful.
 */
static bool init_contents(Interpreter *self, const char *fname) {
    FILE *file = fopen(fname, "r");
    char chunk[CHUNK_SIZE];
    char *ptr;
    size_t n;

    if (file) {
        do {
            n = 0;
            ptr = chunk;
            while (n < CHUNK_SIZE && fread(ptr, 1, 1, file) == 1) {
                if (*ptr != 12) {
                    /* 12 is form feed, illegal */
                    ptr++;
                    n++;
                }
            }
            string_builder_append_bytes(self->contents, chunk, n);
        } while (n == CHUNK_SIZE);
        fclose(file);
    }
    return !!file;
}

static void init_corners(Interpreter *self) {
    int32_t col = 0;
    int32_t i;
    char c;
    self->top_left.x = 0;
    self->top_left.y = 0;
    self->bottom_right.x = 0;
    self->bottom_right.y = 0;
    for (i = 0; (size_t)i < string_builder_len(self->contents); i++) {
        c = string_builder_get_char(self->contents, i);
        if (c == '\n') {
            self->bottom_right.y++;
            col = 0;
        } else {
            if (col > self->bottom_right.x) {
                self->bottom_right.x = col;
            }
            col++;
        }
    }
}

Interpreter *interpreter_create(const char *fname) {
    Interpreter *self = calloc(1, sizeof(Interpreter));
    if (!self) {
        report_system_error(FILENAME ": memory allocation failure");
        goto interpreter_create_fail;
    }

    self->contents = string_builder_create();
    if (!self->contents) goto interpreter_create_fail;

    if (!init_contents(self, fname)) {
        report_error(FILENAME ": file not found or unreadable");
        goto interpreter_create_fail;
    }

    if (string_builder_len(self->contents) == 0) {
        report_error(FILENAME ": empty file");
        goto interpreter_create_fail;
    }

    self->ip.x = 0;
    self->ip.y = 0;
    self->ip_y_contents_idx = 0;
    self->momentum.x = 1;
    self->momentum.y = 0;

    self->stack = funge_stack_create();
    self->stack_stack = stack_create((void (*)(void *))funge_stack_destroy);

    self->storage_offset.x = 0;
    self->storage_offset.y = 0;

    init_corners(self);

    return self;
interpreter_create_fail:
    interpreter_destroy(self);
    return NULL;
}

void interpreter_destroy(Interpreter *self) {
    if (self) {
        string_builder_destroy(self->contents);
        funge_stack_destroy(self->stack);
        stack_destroy(self->stack_stack);
        free(self);
    }
}
