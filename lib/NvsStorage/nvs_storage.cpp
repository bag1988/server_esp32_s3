#include "nvs_storage.h"
#include <ArduinoJson.h>
#include <esp_heap_caps.h>

namespace NvsStorage {

// CRC32 helper - implemented inline to avoid external dependency
static uint32_t calculateCrc32(const uint8_t* data, size_t len) {
    static const uint32_t crc_table[256] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
        0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
        0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
        0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
        0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bd150f,
        0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
        0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
        0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262404e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
        0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
        0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
        0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
        0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29e9c996, 0xb0e0982c, 0xc7e7a8ba, 0x54b34d83, 0x23b45d15, 0xbaad6c8d, 0xcd2b9c15,
        0x92d28e8b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdffb21, 0x9ad67a7c, 0xed614aec, 0x74683b56, 0x03610bcc,
        0x9ffc07f6, 0xe8fb3760, 0x71f266da, 0x06f5564c, 0x9bbcc5d2, 0xecb5f544, 0x75bca4de, 0x02bff44a,
        0x8db88b5d, 0xfaba9bc5, 0x63b3eb7f, 0x14b4db09, 0x8ad54065, 0xfdd270fd, 0x64db2045, 0x13d810d1,
        0x8e996f8d, 0xf99e5f1b, 0x60972ea1, 0x17901e37, 0x84d6ce99, 0xf3d1fe0f, 0x6ad88eb5, 0x1ddfbebd,
        0x35d5fb41, 0x42d2cbdb, 0xbbdb9a65, 0xccdcaaff, 0x38d07bfa, 0x4fd34b6c, 0xd6d4da16, 0xa1d3ea80,
        0x3e77c4d3, 0x4970f445, 0xd07785ff, 0xa770b569, 0x31d50ebc, 0x46d23e2a, 0xbdad8d90, 0xcaa1bd06,
        0x3a39ed1b, 0x4d3e9d8d, 0xd4370c37, 0xa3303caf, 0x33724e34, 0x44755ea2, 0xbd7c2f18, 0xca7b1fa0,
        0x2374101c, 0x5473208a, 0xcd7a5130, 0xba7d41a6, 0x25d1f6b1, 0x52d6c627, 0xcbdf179d, 0xbcd8270b,
        0x2a65625e, 0x5d6252c8, 0xc46b2372, 0xb36c13e4, 0x29b48c9f, 0x5eb79c0b, 0xc7b0edb1, 0xb0b7dd27,
        0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01, 0x7b07c9a2, 0x0c00f934, 0x9509a88e, 0xe20e9818,
        0x7ecfff1b, 0x09c8cf8d, 0x90c15e37, 0xe7c66ea1, 0x7a81f1dc, 0x0d86c14a, 0x948f50f0, 0xe3886066
    };
    
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc_table[(crc ^ data[i]) & 0xFF];
    }
    return ~crc;
}

static bool verifyCrc32(const uint8_t* data, size_t len, uint32_t expectedCrc) {
    return calculateCrc32(data, len) == expectedCrc;
}

esp_err_t init() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

// Save string to NVS with CRC
static esp_err_t saveString(nvs_handle_t handle, const char* key, const std::string& value) {
    uint32_t crc = calculateCrc32((const uint8_t*)value.c_str(), value.length());
    
    esp_err_t err = nvs_set_str(handle, key, value.c_str());
    if (err != ESP_OK) return err;
    
    char crcKey[32];
    snprintf(crcKey, sizeof(crcKey), "%s_crc", key);
    err = nvs_set_u32(handle, crcKey, crc);
    if (err != ESP_OK) return err;
    
    return nvs_commit(handle);
}

// Load string from NVS with CRC verification
static esp_err_t loadString(nvs_handle_t handle, const char* key, std::string& value) {
    size_t requiredSize = 0;
    esp_err_t err = nvs_get_str(handle, key, nullptr, &requiredSize);
    if (err != ESP_OK) return err;
    
    char* buffer = new char[requiredSize];
    err = nvs_get_str(handle, key, buffer, &requiredSize);
    if (err != ESP_OK) {
        delete[] buffer;
        return err;
    }
    
    // Verify CRC
    char crcKey[32];
    snprintf(crcKey, sizeof(crcKey), "%s_crc", key);
    uint32_t storedCrc = 0;
    err = nvs_get_u32(handle, crcKey, &storedCrc);
    if (err == ESP_OK) {
        uint32_t computedCrc = calculateCrc32((const uint8_t*)buffer, strlen(buffer));
        if (computedCrc != storedCrc) {
            delete[] buffer;
            return ESP_ERR_NVS_CRC_MISMATCH;
        }
    }
    
    value = buffer;
    delete[] buffer;
    return ESP_OK;
}

esp_err_t saveWifiCredentials(const std::string& ssid, const std::string& password) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("wifi", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    
    err = saveString(handle, "ssid", ssid);
    if (err != ESP_OK) { nvs_close(handle); return err; }
    
    err = saveString(handle, "password", password);
    if (err != ESP_OK) { nvs_close(handle); return err; }
    
    nvs_close(handle);
    return ESP_OK;
}

esp_err_t loadWifiCredentials(std::string& ssid, std::string& password) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("wifi", NVS_READONLY, &handle);
    if (err != ESP_OK) return err;
    
    err = loadString(handle, "ssid", ssid);
    if (err != ESP_OK) { nvs_close(handle); return err; }
    
    err = loadString(handle, "password", password);
    if (err != ESP_OK) { nvs_close(handle); return err; }
    
    nvs_close(handle);
    return ESP_OK;
}

// Save blob with CRC32
static esp_err_t saveBlob(nvs_handle_t handle, const char* key, const uint8_t* data, size_t len) {
    uint32_t crc = calculateCrc32(data, len);
    
    esp_err_t err = nvs_set_blob(handle, key, data, len);
    if (err != ESP_OK) return err;
    
    char crcKey[32];
    snprintf(crcKey, sizeof(crcKey), "%s_crc", key);
    err = nvs_set_u32(handle, crcKey, crc);
    if (err != ESP_OK) return err;
    
    return nvs_commit(handle);
}

// Load blob with CRC32 verification
static esp_err_t loadBlob(nvs_handle_t handle, const char* key, uint8_t* data, size_t* len) {
    esp_err_t err = nvs_get_blob(handle, key, data, len);
    if (err != ESP_OK) return err;
    
    char crcKey[32];
    snprintf(crcKey, sizeof(crcKey), "%s_crc", key);
    uint32_t storedCrc = 0;
    err = nvs_get_u32(handle, crcKey, &storedCrc);
    if (err == ESP_OK) {
        uint32_t computedCrc = calculateCrc32(data, *len);
        if (computedCrc != storedCrc) {
            return ESP_ERR_NVS_CRC_MISMATCH;
        }
    }
    
    return ESP_OK;
}

esp_err_t saveDevices(const std::vector<DeviceData>& devices) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("devices", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    
    JsonDocument doc;
    JsonArray devicesArray = doc.to<JsonArray>();
    
    for (const auto& device : devices) {
        JsonObject deviceObj = devicesArray.add<JsonObject>();
        deviceObj["name"] = device.name;
        deviceObj["mac"] = device.macAddress;
        deviceObj["target_temp"] = device.targetTemperature;
        deviceObj["enabled"] = device.enabled;
        deviceObj["total_heating"] = device.totalHeatingTime;
        deviceObj["humidity"] = device.humidity;
        deviceObj["battery"] = device.battery;
        deviceObj["batteryV"] = device.batteryV;
        
        for (uint8_t pin : device.gpioPins) {
            deviceObj["gpio_pins"].add(pin);
        }
    }
    
    size_t bufferSize = measureJson(doc);
    if (bufferSize == 0) {
        nvs_close(handle);
        return ESP_ERR_NVS_NOT_ENOUGH_SPACE;
    }
    
    uint8_t* buffer = (uint8_t*)heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buffer) {
        buffer = new uint8_t[bufferSize];
    }
    
    size_t serializedSize = serializeJson(doc, buffer, bufferSize);
    err = saveBlob(handle, "devices_blob", buffer, serializedSize);
    
    if (buffer) {
        heap_caps_free(buffer);
    }
    
    nvs_close(handle);
    return err;
}

esp_err_t loadDevices(std::vector<DeviceData>& devices) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("devices", NVS_READONLY, &handle);
    if (err != ESP_OK) return err;
    
    size_t blobSize = 0;
    err = nvs_get_blob(handle, "devices_blob", nullptr, &blobSize);
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }
    
    uint8_t* buffer = (uint8_t*)heap_caps_malloc(blobSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buffer) {
        buffer = new uint8_t[blobSize];
    }
    
    err = loadBlob(handle, "devices_blob", buffer, &blobSize);
    if (err == ESP_OK) {
        JsonDocument doc;
        DeserializationError jsonErr = deserializeJson(doc, buffer, blobSize);
        if (!jsonErr) {
            devices.clear();
            JsonArray devicesArray = doc.as<JsonArray>();
            
            for (JsonObject deviceObj : devicesArray) {
                DeviceData device;
                device.name = deviceObj["name"].as<const char*>();
                device.macAddress = deviceObj["mac"].as<const char*>();
                device.targetTemperature = deviceObj["target_temp"].as<float>();
                device.enabled = deviceObj["enabled"].as<bool>();
                
                if (!deviceObj["gpio_pins"].isNull()) {
                    JsonArray pinsArray = deviceObj["gpio_pins"].as<JsonArray>();
                    for (uint8_t pin : pinsArray) {
                        device.gpioPins.push_back(pin);
                    }
                }
                
                device.heatingStartTime = 0;
                device.totalHeatingTime = deviceObj["total_heating"].as<long>();
                device.isOnline = false;
                device.currentTemperature = device.targetTemperature;
                device.heatingActive = false;
                device.humidity = deviceObj["humidity"].as<float>();
                device.battery = deviceObj["battery"].as<uint8_t>();
                device.batteryV = deviceObj["batteryV"].as<uint16_t>();
                
                devices.push_back(device);
            }
        } else {
            err = ESP_ERR_NVS_CRC_MISMATCH;
        }
    }
    
    if (buffer) {
        heap_caps_free(buffer);
    }
    
    nvs_close(handle);
    return err;
}

esp_err_t saveGpioConfig(const std::vector<GpioPin>& gpioPins) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("gpio", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    
    JsonDocument doc;
    JsonArray gpioArray = doc.to<JsonArray>();
    
    for (const auto& gpio : gpioPins) {
        JsonObject gpioObj = gpioArray.add<JsonObject>();
        gpioObj["pin"] = gpio.pin;
        gpioObj["state"] = gpio.state;
        gpioObj["name"] = gpio.name;
        gpioObj["total_heating"] = gpio.totalHeatingTime;
    }
    
    size_t bufferSize = measureJson(doc);
    if (bufferSize == 0) {
        nvs_close(handle);
        return ESP_ERR_NVS_NOT_ENOUGH_SPACE;
    }
    
    uint8_t* buffer = (uint8_t*)heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buffer) {
        buffer = new uint8_t[bufferSize];
    }
    
    size_t serializedSize = serializeJson(doc, buffer, bufferSize);
    err = saveBlob(handle, "gpio_blob", buffer, serializedSize);
    
    if (buffer) {
        heap_caps_free(buffer);
    }
    
    nvs_close(handle);
    return err;
}

esp_err_t loadGpioConfig(std::vector<GpioPin>& gpioPins) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("gpio", NVS_READONLY, &handle);
    if (err != ESP_OK) return err;
    
    size_t blobSize = 0;
    err = nvs_get_blob(handle, "gpio_blob", nullptr, &blobSize);
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }
    
    uint8_t* buffer = (uint8_t*)heap_caps_malloc(blobSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buffer) {
        buffer = new uint8_t[blobSize];
    }
    
    err = loadBlob(handle, "gpio_blob", buffer, &blobSize);
    if (err == ESP_OK) {
        JsonDocument doc;
        DeserializationError jsonErr = deserializeJson(doc, buffer, blobSize);
        if (!jsonErr) {
            gpioPins.clear();
            JsonArray gpioArray = doc.as<JsonArray>();
            
            for (JsonObject gpioObj : gpioArray) {
                GpioPin gpio;
                gpio.pin = gpioObj["pin"].as<uint8_t>();
                gpio.state = gpioObj["state"].as<uint8_t>();
                gpio.name = gpioObj["name"].as<const char*>();
                gpio.totalHeatingTime = gpioObj["total_heating"].as<long>();
                gpioPins.push_back(gpio);
            }
        } else {
            err = ESP_ERR_NVS_CRC_MISMATCH;
        }
    }
    
    if (buffer) {
        heap_caps_free(buffer);
    }
    
    nvs_close(handle);
    return err;
}

esp_err_t saveServerSettings(unsigned long serverWorkTime, float hysteresisTemp) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("server", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    
    err = nvs_set_u64(handle, "server_time", serverWorkTime);
    if (err != ESP_OK) { nvs_close(handle); return err; }
    
    err = nvs_set_blob(handle, "hysteresis", &hysteresisTemp, sizeof(float));
    if (err != ESP_OK) { nvs_close(handle); return err; }
    
    // CRC for hysteresis
    uint32_t crc = calculateCrc32((const uint8_t*)&hysteresisTemp, sizeof(float));
    err = nvs_set_u32(handle, "hysteresis_crc", crc);
    if (err != ESP_OK) { nvs_close(handle); return err; }
    
    err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

esp_err_t loadServerSettings(unsigned long& serverWorkTime, float& hysteresisTemp) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("server", NVS_READONLY, &handle);
    if (err != ESP_OK) return err;
    
    uint64_t serverTime64 = 0;
    err = nvs_get_u64(handle, "server_time", &serverTime64);
    if (err != ESP_OK) { nvs_close(handle); return err; }
    serverWorkTime = (unsigned long)serverTime64;
    
    size_t size = sizeof(float);
    err = nvs_get_blob(handle, "hysteresis", &hysteresisTemp, &size);
    if (err != ESP_OK) { nvs_close(handle); return err; }
    
    // Verify CRC
    uint32_t storedCrc = 0;
    err = nvs_get_u32(handle, "hysteresis_crc", &storedCrc);
    if (err == ESP_OK) {
        uint32_t computedCrc = calculateCrc32((const uint8_t*)&hysteresisTemp, sizeof(float));
        if (computedCrc != storedCrc) {
            nvs_close(handle);
            return ESP_ERR_NVS_CRC_MISMATCH;
        }
    }
    
    nvs_close(handle);
    return ESP_OK;
}

} // namespace NvsStorage