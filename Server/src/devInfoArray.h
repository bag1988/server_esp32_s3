
// #include <string.h>

typedef struct
{
    String ble_address;
    String name;
    bool enabled;
    bool isConnected;
    uint8_t gpioToEnable[10];
    bool pump;
    bool boiler;
    float targetTemp;
    float temp;
    float humidity;
    int tempReductionTime;
} DevInfo;

void addToList(DevInfo *list, DevInfo item)
{    
    int size = sizeof(list) / sizeof(DevInfo);
    list = (DevInfo *)realloc(list, sizeof(list) + sizeof(DevInfo));
    list[size++] = item;
}

DevInfo *findInList(DevInfo *list, String ble_address)
{
    for (size_t i = 0; i < sizeof(list) / sizeof(DevInfo); ++i)
    {
        if (list[i].ble_address == ble_address)
        {
            return &list[i];
        }
    }
    return NULL;
}

void removeAllFromList(DevInfo *list, String ble_address)
{
    DevInfo *result = (DevInfo *)malloc(sizeof(list) - sizeof(DevInfo));
    size_t i, j = 0;
    for (i = 0; i < sizeof(list) / sizeof(DevInfo); ++i)
    {
        if (list[i].ble_address != ble_address)
        {
            result[j++] = list[i];
        }
    }
    free(list);
    list = result;
}

bool anyGpioValue(uint8_t *gpioArray, uint8_t value)
{
    for (size_t i = 0; i < sizeof(gpioArray) / sizeof(uint8_t); ++i)
    {
        if (gpioArray[i] == value)
        {
            return true;
        }
    }
    return false;
}

DevInfo *filterList(DevInfo *list, int increment)
{
    DevInfo *result = (DevInfo *)malloc(sizeof(list));
    for (size_t i = 0; i < sizeof(list) / sizeof(DevInfo); ++i)
    {
        if (list[i].isConnected && list[i].enabled && (list[i].temp + 2) < list[i].targetTemp)
        {
            list[i].tempReductionTime += increment;
            addToList(result, list[i]);
        }
    }
    return result;
}

uint8_t *getGpioArray(DevInfo *list)
{
    static uint8_t gpioArray[100];
    size_t index = 0;
    for (size_t i = 0; i < sizeof(list) / sizeof(DevInfo); ++i)
    {
        for (size_t j = 0; j < 10; ++j)
        {
            bool unique = true;
            for (size_t k = 0; k < index; ++k)
            {
                if (gpioArray[k] == list[i].gpioToEnable[j])
                {
                    unique = false;
                    break;
                }
            }
            if (unique)
            {
                gpioArray[index++] = list[i].gpioToEnable[j];
            }
        }
    }
    return gpioArray;
}

bool anyPump(DevInfo *list)
{
    for (size_t i = 0; i < sizeof(list) / sizeof(DevInfo); ++i)
    {
        if (list[i].pump)
        {
            return true;
        }
    }
    return false;
}

bool anyBoiler(DevInfo *list)
{
    for (size_t i = 0; i < sizeof(list) / sizeof(DevInfo); ++i)
    {
        if (list[i].boiler)
        {
            return true;
        }
    }
    return false;
}
