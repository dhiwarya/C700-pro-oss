/* Desktop stand-in for FreeRTOS and esp_timer (tools/uisim).
 *
 * Everything runs on one thread. Time is virtual (uisim_advance_ms). Tasks
 * are recorded by xTaskCreate and run by uisim_run_tasks() while they have
 * notifications pending: a task blocks in ulTaskNotifyTake() by jumping back
 * to uisim_run_tasks(). That is enough for mapview.c's render task, which
 * waits for a notification, renders once and waits again. */
#include <setjmp.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "sim.h"

static int64_t s_us = 1000000;   /* not 0: code treats 0 as "never" */

int64_t esp_timer_get_time(void) { return s_us; }
void uisim_advance_ms(uint32_t ms) { s_us += (int64_t)ms * 1000; }
uint32_t uisim_ms(void) { return (uint32_t)(s_us / 1000); }
TickType_t xTaskGetTickCount(void) { return (TickType_t)(s_us / 1000); }
void vTaskDelay(TickType_t t) { s_us += (int64_t)t * 1000; }

#define MAX_TASKS 8
typedef struct { TaskFunction_t fn; void *arg; uint32_t pending; } task_t;
static task_t s_tasks[MAX_TASKS];
static int s_ntasks, s_cur = -1;
static jmp_buf s_jmp;

BaseType_t xTaskCreate(TaskFunction_t fn, const char *name, uint32_t stack, void *arg, UBaseType_t prio, TaskHandle_t *out)
{
    (void)name; (void)stack; (void)prio;
    if (s_ntasks == MAX_TASKS) return pdFAIL;
    task_t *t = &s_tasks[s_ntasks++];
    t->fn = fn;
    t->arg = arg;
    t->pending = 0;
    if (out) *out = t;
    return pdPASS;
}

BaseType_t xTaskCreatePinnedToCore(TaskFunction_t fn, const char *name, uint32_t stack, void *arg, UBaseType_t prio, TaskHandle_t *out, BaseType_t core)
{
    (void)core;
    return xTaskCreate(fn, name, stack, arg, prio, out);
}

BaseType_t xTaskNotifyGive(TaskHandle_t t)
{
    if (t) ((task_t *)t)->pending++;
    return pdPASS;
}

uint32_t ulTaskNotifyTake(BaseType_t clear, TickType_t wait)
{
    (void)wait;
    if (s_cur < 0) return 0;
    task_t *t = &s_tasks[s_cur];
    if (!t->pending) longjmp(s_jmp, 1);   /* "blocks": back to uisim_run_tasks() */
    uint32_t n = t->pending;
    t->pending = clear ? 0 : n - 1;
    return n;
}

void uisim_run_tasks(void)
{
    for (int round = 0; round < 8; round++) {
        bool ran = false;
        for (int i = 0; i < s_ntasks; i++) {
            if (!s_tasks[i].pending) continue;
            ran = true;
            s_cur = i;
            if (!setjmp(s_jmp)) s_tasks[i].fn(s_tasks[i].arg);
            s_cur = -1;
        }
        if (!ran) return;
    }
}

static int s_mutex_token;
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return &s_mutex_token; }
SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void) { return &s_mutex_token; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t w) { (void)s; (void)w; return pdTRUE; }
BaseType_t xSemaphoreGive(SemaphoreHandle_t s) { (void)s; return pdTRUE; }
BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t s, TickType_t w) { (void)s; (void)w; return pdTRUE; }
BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t s) { (void)s; return pdTRUE; }
