/*
MIT License

This file is part of cupkee project.

Copyright (c) 2016-2017 Lixing Ding <ding.lixing@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "cupkee.h"
#include "cupkee.h"
#include "rbuff.h"

#define CONSOLE_BUF_SIZE 128

#define BELL    "\007"
#define CRLF    "\r\n"
#define RIGHT   "\033[C"
#define LEFT    "\010"
#define PROMPT  "> "

#define CONSOLE_IN       0
#define CONSOLE_OUT      1
#define CONSOLE_BUF_NUM  2

static void *console_tty = NULL;
static console_handle_t user_handle = NULL;

static uint32_t console_total_recv = 0;
static uint32_t console_total_send = 0;

static uint16_t console_cursor = 0;

static rbuff_t  console_buff[CONSOLE_BUF_NUM];
static char     console_buff_mem[CONSOLE_BUF_NUM][CONSOLE_BUF_SIZE];
static const char *console_prompt = PROMPT;

static int console_buf_write_byte(int x, char c)
{
    rbuff_t *rb = &console_buff[x];
    int pos = rbuff_push(rb);
    if (pos < 0) {
        return 0;
    }
    console_buff_mem[x][pos] = c;
    return 1;
}

static int console_buf_read_byte(int x, char *c)
{
    rbuff_t *rb = &console_buff[x];
    int pos = rbuff_shift(rb);
    if (pos < 0) {
        return 0;
    }
    *c = console_buff_mem[x][pos];
    return 1;
}

static int console_buf_unread_byte(int x, char c)
{
    rbuff_t *rb = &console_buff[x];
    int pos = rbuff_unshift(rb);
    if (pos < 0) {
        return 0;
    }
    console_buff_mem[x][pos] = c;
    return 1;
}

static int console_input_peek(int pos)
{
    rbuff_t *rb = &console_buff[CONSOLE_IN];
    char *ptr = console_buff_mem[CONSOLE_IN];

    if (pos < rb->cnt) {
        return ptr[rbuff_get(rb, pos)];
    } else {
        return 0;
    }
}

static void console_input_delete(void)
{
    rbuff_t *rb = &console_buff[CONSOLE_IN];
    char *ptr = console_buff_mem[CONSOLE_IN];

    if (console_cursor <= 0) {
        console_puts(BELL);
        return;
    }

    int pos = console_cursor--;
    int i, n = rbuff_end(rb) - pos;
    int dst = rbuff_get(rb, pos - 1);

    console_puts(LEFT);
    for (i = 0; i < n; i++) {
        int src = rbuff_get(rb, pos + i);

        ptr[dst] = ptr[src];
        dst = src;

        console_putc(ptr[src]);
    }
    console_putc(' ');
    while (n-- >= 0) {
        console_puts(LEFT);
    }

    rbuff_remove(rb, 1);
}

static void console_input_enter(void)
{
    console_cursor = 0;
    rbuff_reset(&console_buff[CONSOLE_IN]);
    console_puts(console_prompt);
}

static void console_input_seek(int n)
{
    int pos = console_cursor + n;

    if (pos < 0 || pos > rbuff_end(&console_buff[CONSOLE_IN])) {
        console_puts(BELL);
    } else {
        console_cursor = pos;
        if (n > 0) {
            while(n--)
                console_puts(RIGHT);
        } else {
            while(n++)
                console_puts(LEFT);
        }
    }
}

static int console_input_parse(char *input, int end, int *ppos, int *pch)
{
    int  type = CON_CTRL_IDLE;
    int  pos = *ppos;
    char key = input[pos++];

    if (key >= 32 && key < 127) {
        *pch = key;
        type = CON_CTRL_CHAR;
    } else {
        if (key == 8) {
            type = CON_CTRL_BACKSPACE;
        } else
        if (key == 9) {
            type = CON_CTRL_TABLE;
        } else
        if (key == 13) {
            type = CON_CTRL_ENTER;
        } else
        if (key == 127) {
            type = CON_CTRL_DELETE;
        } else
        if (key == 27) {
            if (pos == end) {
                type = CON_CTRL_ESCAPE;
            } else
            if (input[pos] == 91) {
                key = input[pos + 1];
                if (key == 65) {
                    type = CON_CTRL_UP;
                } else
                if (key == 66) {
                    type = CON_CTRL_DOWN;
                } else
                if (key == 67) {
                    type = CON_CTRL_RIGHT;
                } else
                if (key == 68) {
                    type = CON_CTRL_LEFT;
                }
            } else
            if (input[pos] == 79) {
                key = input[pos + 1];
                if (key == 80) {
                    type = CON_CTRL_F1;
                } else
                if (key == 81) {
                    type = CON_CTRL_F2;
                } else
                if (key == 82) {
                    type = CON_CTRL_F3;
                } else
                if (key == 83) {
                    type = CON_CTRL_F4;
                }
            }
            pos = end; // Skip all left input
        }
    }
    *ppos = pos;

    return type;
}

static void console_input_proc(int type, int c)
{
    if (type == CON_CTRL_ENTER) {
        console_puts(CRLF);
    }

    if (user_handle && user_handle(type, c)) {
        return;
    }

    switch (type) {
    case CON_CTRL_IDLE:         break;
    case CON_CTRL_CHAR:         console_input_char(c); break;
    case CON_CTRL_BACKSPACE:    console_input_delete(); break;
    case CON_CTRL_DELETE:       console_input_delete(); break;
    case CON_CTRL_TABLE:        console_puts(BELL); break;
    case CON_CTRL_ENTER:        console_input_enter(); break;
    case CON_CTRL_ESCAPE:       console_puts(BELL); break;
    case CON_CTRL_UP:           console_puts(BELL); break;
    case CON_CTRL_DOWN:         console_puts(BELL); break;
    case CON_CTRL_RIGHT:        console_input_seek(1);  break;
    case CON_CTRL_LEFT:         console_input_seek(-1); break;
    case CON_CTRL_F1:
    case CON_CTRL_F2:
    case CON_CTRL_F3:
    case CON_CTRL_F4:           console_puts(BELL); break;
    default: break;
    }
}

static void console_input_handle(int n, void *data)
{
    int pos = 0;
    int ch = '.'; // Give a initial value to make gcc happy

    console_total_recv += n;

    while (pos < n) {
        int type = console_input_parse(data, n, &pos, &ch);

        console_input_proc(type, ch);
    }
}

static void console_do_recv(void *tty)
{
    int n;
    char buf[4];

    while (0 < (n = cupkee_read(tty, 4, buf))) {
        console_input_handle(n, buf);
    }

}

static void console_do_send(void *tty)
{
    char c;

    while (console_buf_read_byte(CONSOLE_OUT, &c)) {
        if (!cupkee_write(tty, 1, &c)) {
            console_buf_unread_byte(CONSOLE_OUT, c);
            break;
        } else {
            console_total_send++;
        }
    }
}

static int console_device_handle(void *tty, int event, intptr_t param)
{
    (void) param;

    if (event == CUPKEE_EVENT_DATA) {
        console_do_recv(tty);
    } else
    if (event == CUPKEE_EVENT_DRAIN) {
        console_do_send(tty);
    }

    return 0;
}

int cupkee_console_init(void *tty, console_handle_t handle)
{
    console_cursor = 0;
    console_total_recv = 0;

    rbuff_init(&console_buff[CONSOLE_IN],  CONSOLE_BUF_SIZE);
    rbuff_init(&console_buff[CONSOLE_OUT], CONSOLE_BUF_SIZE);

    if (0 != cupkee_device_handle_set(tty, console_device_handle, 0)) {
        return -CUPKEE_EINVAL;
    }

    user_handle = handle;
    console_tty = tty;

    cupkee_listen(tty, CUPKEE_EVENT_DATA);
    cupkee_listen(tty, CUPKEE_EVENT_DRAIN);

    return 0;
}

void console_prompt_set(const char *prompt)
{
    if (prompt) {
        console_prompt = prompt;
    } else {
        console_prompt = PROMPT;
    }
}

int console_input_clean(void)
{
    rbuff_t *rb = &console_buff[CONSOLE_IN];
    int n = rbuff_end(rb);
    int i;

    if (n <= 0) {
        return 0;
    }

    rbuff_reset(rb);
    while (console_cursor) {
        console_putc(8);
        console_cursor--;
    }

    for (i = 0; i < n; i++) {
        console_putc(' ');
    }
    for (i = 0; i < n; i++) {
        console_putc(8);
    }

    return n;
}

int console_input_char(int ch)
{
    char c = ch;

    return console_input_insert(1, &c);
}

int console_input_token(int size, char *buf)
{
    if (console_cursor == 0) {
        return 0;
    }

    rbuff_t *rb = &console_buff[CONSOLE_IN];
    char *ptr = console_buff_mem[CONSOLE_IN];
    int start = console_cursor;
    int pos = 0;

    while ((pos = rbuff_get(rb, start - 1)) >= 0) {
        int c = ptr[pos];

        if (!isalnum(c) && c != '_') {
            break;
        }
        start--;
    }

    for (pos = 0; pos < size && start < console_cursor; pos++, start++) {
        buf[pos] = ptr[rbuff_get(rb, start)];
    }
    buf[pos] = 0;

    return pos;
}

int console_input_insert(int len, char *s)
{
    rbuff_t *rb = &console_buff[CONSOLE_IN];
    char *ptr = console_buff_mem[CONSOLE_IN];
    int pos, n, i;

    if (len == 0 || !s) {
        return -1;
    }

    if (!rbuff_has_space(rb, len)) {
        //Todo: post console error event
        return -1;
    }

    n = rbuff_end(rb) - console_cursor;
    pos = console_cursor;
    rbuff_append(rb, len);
    for (i = n - 1; i >= 0; i--)
        ptr[rbuff_get(rb, pos + i + len)] = ptr[rbuff_get(rb, pos + i)];
    for (i = 0; i < len; i++)
        ptr[rbuff_get(rb, console_cursor++)] = s[i];

    // echo back
    for (i = 0; i < n + len; i++) {
        console_putc(ptr[rbuff_get(rb, pos + i)]);
    }
    while (n--) {
        console_puts(LEFT);
    }

    return console_cursor;
}

int console_input_refresh(void)
{
    int i = 0, ch;

    console_puts(console_prompt);

    while ((ch = console_input_peek(i++)) > 0) {
        console_putc(ch);
    }

    return i;
}

int console_input_load(int size, char *buf)
{
    int n = console_buff[CONSOLE_IN].cnt;

    n = n > size ? size : n;
    memcpy(buf, console_buff_mem[CONSOLE_IN], n);

    return n;
}

int console_putc(int c)
{
    if (rbuff_is_empty(&console_buff[CONSOLE_OUT])) {
        char buf = c;
        if (cupkee_write(console_tty, 1, &buf)) {
            console_total_send++;
            return 1;
        }
    }

    return console_buf_write_byte(CONSOLE_OUT, c);
}

int console_puts(const char *s)
{
    const char *p = s;
    int len = strlen(s);

    if (rbuff_is_empty(&console_buff[CONSOLE_OUT])) {
        int n = cupkee_write(console_tty, len, s);
        if (n > 0) {
            console_total_send += n;
            p += n;
        }
    }

    while(*p && console_buf_write_byte(CONSOLE_OUT, *p))
        p++;

    return p - s;
}

int console_log(const char *fmt, ...)
{
    char buf[129];
    int  n;

    va_list va;
    va_start(va, fmt);

    n = vsnprintf(buf, 128, fmt, va);

    va_end(va);
    if (n >= 0) {
        buf[n] = 0;
        return console_puts(buf);
    } else {
        return 0;
    }
}

int console_putc_sync(int c)
{
    char buf = c;

    return cupkee_write_sync(console_tty, 1, &buf);
}

int console_puts_sync(const char *s)
{
    int len = strlen(s);
    int pos = 0;

    while (pos < len) {
        char ch = s[pos++];

        if (ch == '\n') {
            if (pos < 2 || s[pos - 2] != '\r') {
                char cr = '\r';
                cupkee_write_sync(console_tty, 1, &cr);
            }
        }

        cupkee_write_sync(console_tty, 1, &ch);
    }

    return pos;
}

int console_log_sync(const char *fmt, ...)
{
    char buf[256];
    int  n;

    va_list va;
    va_start(va, fmt);

    n = vsnprintf(buf, 255, fmt, va);

    va_end(va);
    buf[n] = 0;

    return console_puts_sync(buf);
}

