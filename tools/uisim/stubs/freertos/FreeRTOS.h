#pragma once
/* Desktop stand-in (tools/uisim): single-threaded. Tasks are recorded and run
 * on demand by uisim_run_tasks() (see sim_rtos.c); mutexes are no-ops. */
#include <stdint.h>
#include <stdbool.h>
typedef int BaseType_t;
typedef unsigned UBaseType_t;
typedef uint32_t TickType_t;
typedef void *TaskHandle_t;
typedef void *SemaphoreHandle_t;
typedef void *QueueHandle_t;
typedef void (*TaskFunction_t)(void *);
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define pdFAIL 0
#define portMAX_DELAY 0xffffffffu
#define portTICK_PERIOD_MS 1
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define configTICK_RATE_HZ 1000
void vTaskDelay(TickType_t t);
TickType_t xTaskGetTickCount(void);
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(m) do { (void)(m); } while (0)
#define portEXIT_CRITICAL(m) do { (void)(m); } while (0)
#define taskENTER_CRITICAL(m) do { (void)(m); } while (0)
#define taskEXIT_CRITICAL(m) do { (void)(m); } while (0)
