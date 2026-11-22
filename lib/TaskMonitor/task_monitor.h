#ifndef TASK_MONITOR_H
#define TASK_MONITOR_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ArduinoJson.h>

struct TaskHealthInfo {
    TaskHandle_t handle;
    const char* name;
    uint32_t lastCheckIn;
    uint32_t checkInIntervalMs;
    uint32_t restartCount;
    bool critical;  // If true, system restart on failure
    void (*restartFn)(void*);  // Function to restart the task
    void* restartArg;
};

class TaskMonitor {
public:
    static TaskMonitor& instance() {
        static TaskMonitor monitor;
        return monitor;
    }
    
    // Register a task for monitoring
    void registerTask(TaskHandle_t handle, const char* name, uint32_t checkInIntervalMs, 
                      bool critical = false, void (*restartFn)(void*) = nullptr, void* restartArg = nullptr);
    
    // Task calls this periodically to indicate it's alive
    void checkIn(const char* taskName);
    
    // Start monitoring task
    void begin(uint32_t monitorIntervalMs = 5000);
    
    // Get health info for all tasks
    void getHealthReport(JsonDocument& doc);
    
    // Force restart a specific task
    void restartTask(const char* name);
    
private:
    TaskMonitor() : monitorTaskHandle(nullptr), running(false) {}
    TaskMonitor(const TaskMonitor&) = delete;
    TaskMonitor& operator=(const TaskMonitor&) = delete;
    
    static void monitorTaskFn(void* parameter);
    void checkTasks();
    void restartTaskInternal(TaskHealthInfo& info);
    
    static const int MAX_TASKS = 10;
    TaskHealthInfo tasks[MAX_TASKS];
    int taskCount = 0;
    TaskHandle_t monitorTaskHandle;
    bool running;
    uint32_t monitorIntervalMs;
};

#define TASK_MONITOR TaskMonitor::instance()

// Macro for tasks to easily check in
#define TASK_CHECK_IN() TASK_MONITOR.checkIn(pcTaskGetName(nullptr))

#endif // TASK_MONITOR_H