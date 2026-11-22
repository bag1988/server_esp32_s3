#include "task_monitor.h"
#include <ArduinoJson.h>
#include "variables_info.h"

void TaskMonitor::registerTask(TaskHandle_t handle, const char* name, uint32_t checkInIntervalMs,
                               bool critical, void (*restartFn)(void*), void* restartArg) {
    if (taskCount >= MAX_TASKS) {
        logAndSend("TaskMonitor: Max tasks reached, cannot register " + String(name));
        return;
    }
    
    tasks[taskCount] = {
        .handle = handle,
        .name = name,
        .lastCheckIn = millis(),
        .checkInIntervalMs = checkInIntervalMs,
        .restartCount = 0,
        .critical = critical,
        .restartFn = restartFn,
        .restartArg = restartArg
    };
    taskCount++;
    logAndSend("TaskMonitor: Registered task " + String(name) + " (interval: " + String(checkInIntervalMs) + "ms)");
}

void TaskMonitor::checkIn(const char* taskName) {
    for (int i = 0; i < taskCount; i++) {
        if (strcmp(tasks[i].name, taskName) == 0) {
            tasks[i].lastCheckIn = millis();
            return;
        }
    }
}

void TaskMonitor::begin(uint32_t monitorIntervalMs) {
    this->monitorIntervalMs = monitorIntervalMs;
    running = true;
    
    xTaskCreate(
        monitorTaskFn,
        "taskMonitor",
        4096,
        this,
        2,  // Higher priority
        &monitorTaskHandle
    );
    logAndSend("TaskMonitor: Started");
}

void TaskMonitor::monitorTaskFn(void* parameter) {
    TaskMonitor* self = static_cast<TaskMonitor*>(parameter);
    self->running = true;
    
    while (self->running) {
        self->checkTasks();
        vTaskDelay(pdMS_TO_TICKS(self->monitorIntervalMs));
    }
    vTaskDelete(nullptr);
}

void TaskMonitor::checkTasks() {
    uint32_t now = millis();
    
    for (int i = 0; i < taskCount; i++) {
        TaskHealthInfo& info = tasks[i];
        
        // Handle millis() rollover
        uint32_t elapsed = (now >= info.lastCheckIn) ? 
            (now - info.lastCheckIn) : (UINT32_MAX - info.lastCheckIn + now + 1);
        
        if (elapsed > info.checkInIntervalMs * 3) {  // 3x interval = dead
            logAndSend("TaskMonitor: Task " + String(info.name) + " appears dead (elapsed: " + String(elapsed) + "ms)");
            
            if (info.critical) {
                logAndSend("TaskMonitor: Critical task failed, restarting system...");
                vTaskDelay(pdMS_TO_TICKS(1000));
                esp_restart();
            } else if (info.restartFn) {
                restartTaskInternal(info);
            }
        }
    }
}

void TaskMonitor::restartTaskInternal(TaskHealthInfo& info) {
    logAndSend("TaskMonitor: Restarting task " + String(info.name) + " (attempt " + String(info.restartCount + 1) + ")");
    
    // Delete old task if handle is valid
    if (info.handle) {
        vTaskDelete(info.handle);
        info.handle = nullptr;
    }
    
    // Call restart function
    if (info.restartFn) {
        info.restartFn(info.restartArg);
    }
    
    info.restartCount++;
    info.lastCheckIn = millis();
}

void TaskMonitor::restartTask(const char* name) {
    for (int i = 0; i < taskCount; i++) {
        if (strcmp(tasks[i].name, name) == 0) {
            restartTaskInternal(tasks[i]);
            return;
        }
    }
}

void TaskMonitor::getHealthReport(JsonDocument& doc) {
    JsonArray tasksArray = doc.to<JsonArray>();
    uint32_t now = millis();
    
    for (int i = 0; i < taskCount; i++) {
        TaskHealthInfo& info = tasks[i];
        uint32_t elapsed = (now >= info.lastCheckIn) ? 
            (now - info.lastCheckIn) : (UINT32_MAX - info.lastCheckIn + now + 1);
        
        JsonObject taskObj = tasksArray.add<JsonObject>();
        taskObj["name"] = info.name;
        taskObj["last_check_in_ms_ago"] = elapsed;
        taskObj["check_in_interval_ms"] = info.checkInIntervalMs;
        taskObj["restart_count"] = info.restartCount;
        taskObj["critical"] = info.critical;
        taskObj["healthy"] = (elapsed <= info.checkInIntervalMs * 3);
        taskObj["handle_valid"] = (info.handle != nullptr);
    }
}