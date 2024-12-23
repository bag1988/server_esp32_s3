// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <stdbool.h>
// #include <unistd.h>
// #include <pthread.h>
// #include "dev_manager.h"

// #define DELAY 1000

// typedef struct {
//     int gpio;
//     char name[20];
// } GpioAccess;

// GpioAccess gpioAccess[] = {
//     {18, "насос"},
//     {19, "котел"},
//     {20, "ванна"},
//     {21, "кухня"}
// };

// void manageDevicesAndControlGPIO() {
//     while (true) {
//         DevInfo* findList[MAX_DEVICES];
//         int findCount = 0;
//         filterDevicesByTemp(findList, &findCount);

//         if (findCount > 0) {
//             for (int i = 0; i < findCount; i++) {
//                 findList[i]->totalTimeActive += DELAY / 1000;
//             }

//             int gpioList[256] = {0};

//             for (int i = 0; i < findCount; i++) {
//                 for (int j = 0; j < MAX_GPIO; j++) {
//                     if (findList[i]->gpioToEnable[j] > 0) {
//                         gpioList[findList[i]->gpioToEnable[j]] = 1;
//                     }
//                 }
//             }

//             for (int i = 0; i < sizeof(gpioAccess) / sizeof(GpioAccess); i++) {
//                 if (gpioList[gpioAccess[i].gpio]) {
//                     printf("gpio %d:%s enabled\n", gpioAccess[i].gpio, gpioAccess[i].name);
//                 } else {
//                     printf("gpio %d:%s disabled\n", gpioAccess[i].gpio, gpioAccess[i].name);
//                 }
//             }
//         }

//         DevInfo* f = findDevice("aa:aa:aa:aa:aa:aa");
//         if (f != NULL) {
//             int enabledGpio[] = {18, 19, 21};
//             updateGpioToEnable(f, enabledGpio, 3);

//             for (int i = 0; i < MAX_GPIO; i++) {
//                 if (f->gpioToEnable[i] == 18) {
//                     for (int j = i; j < MAX_GPIO - 1; j++) {
//                         f->gpioToEnable[j] = f->gpioToEnable[j + 1];
//                     }
//                     f->gpioToEnable[MAX_GPIO - 1] = 0;
//                     break;
//                 }
//             }
//         }

//         removeDevice("cc:bb:bb:bb:bb:bb");

//         usleep(DELAY * 1000);
//     }
// }

// void runAddTemp() {
//     while (true) {
//         for (int i = 0; i < devCount; i++) {
//             if (strcmp(devices_ble[i].ble_address, "cc:cc:cc:cc:cc:cc") == 0) {
//                 devices_ble[i].temp += 1;
//             }
//         }
//         usleep(10000 * 1000);
//     }
// }

// int main() {
//     DevInfo buffer[] = {
//         {"aa:aa:aa:aa:aa:aa", "zal", false, true, {0}, 25.0, 20.0, 0.0, 0},
//         {"bb:bb:bb:bb:bb:bb", "vanna", false, true, {18, 19, 20}, 25.0, 20.0, 0.0, 0}
//     };
//     int bufferSize = sizeof(buffer) / sizeof(DevInfo);
//     initDevicesFromBuffer(buffer, bufferSize);

//     DevInfo* f = findDevice("aa:aa:aa:aa:aa:aa");
//     if (f != NULL) {
//         int gpioToEnable[] = {19};
//         updateGpioToEnable(f, gpioToEnable, 1);
//     }

//     pthread_t manageDevicesThread, runAddTempThread;
//     pthread_create(&manageDevicesThread, NULL, (void*)manageDevicesAndControlGPIO, NULL);
//     pthread_create(&runAddTempThread, NULL, (void*)runAddTemp, NULL);

//     pthread_join(manageDevicesThread, NULL);
//     pthread_join(runAddTempThread, NULL);

//     return 0;
// }
