/*
MIT License

This file is part of cupkee project

Copyright (c) 2017 Lixing Ding <ding.lixing@gmail.com>

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

#ifndef __HW_MOCK_INC__
#define __HW_MOCK_INC__

void hw_mock_init(size_t mem_size);
void hw_mock_deinit(void);

cupkee_device_t *mock_device_curr(void);
size_t           mock_device_curr_want(void);

int  hw_mock_device_curr_id(void);
size_t hw_mock_device_curr_want(void);

/* TIMER */
int hw_mock_timer_curr_id(void);
int hw_mock_timer_curr_state(void);
int hw_mock_timer_period(void);
void hw_mock_timer_duration_set(int us);

#endif /* __HW_MOCK_INC__ */

