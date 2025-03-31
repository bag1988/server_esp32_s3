#ifndef CONVERT_TO_JSON_H
#define CONVERT_TO_JSON_H

#include <convert_to_json.h>
#include <variables_info.h>

// Преобразование структуры WifiCredentials в JSON строку
std::string toJsonWifi(const WifiCredentials& data) {
    std::string json = "{";
    json += "\"ssid\":\"" + data.ssid + "\",";
    json += "\"password\":\"" + data.password + "\"";    
    json += "}";
    return json;
}

// Преобразование JSON строки в структуру ClientData
WifiCredentials fromJsonWifi(const std::string& json) {
    WifiCredentials data;
    auto extractValue = [](const std::string& json, const std::string& key) {
        size_t start = json.find(key) + key.length() + 2;
        size_t end = json.find_first_of(",}", start);
        return json.substr(start, end - start - (json[start] == '\"' ? 1 : 0));
    };

    data.ssid = extractValue(json, "\"ssid\"");
    data.password = extractValue(json, "\"password\"");
    return data;
}

// Преобразование структуры ClientData в JSON строку
std::string toJson(const DeviceData& data) {
    std::string json = "{";
    json += "\"macAddress\":\"" + data.macAddress + "\",";
    json += "\"name\":\"" + data.name + "\",";
    json += "\"enabled\":" + std::string(data.enabled ? "true" : "false") + ",";
    json += "\"isOnline\":" + std::string(data.isOnline ? "true" : "false") + ",";
    json += "\"gpioPins\":[";
    for (size_t i = 0; i < data.gpioPins.size(); ++i) {
        json += std::to_string(data.gpioPins[i]);
        if (i != data.gpioPins.size() - 1) {
            json += ",";
        }
    }
    json += "],";
    json += "\"targetTemperature\":" + std::to_string(data.targetTemperature) + ",";
    json += "\"currentTemperature\":" + std::to_string(data.currentTemperature) + ",";
    json += "\"humidity\":" + std::to_string(data.humidity) + ",";
    json += "\"gpioOnTime\":" + std::to_string(data.gpioOnTime);
    json += "}";
    return json;
}

std::vector<int> parseGpioPins(const std::string& gpioPinsStr) {
    std::string gpioPinsStrs = gpioPinsStr;
    std::vector<int> gpioPins;        
    size_t pos = 0;
    while ((pos = gpioPinsStrs.find(',')) != std::string::npos) {
        gpioPins.push_back(std::stoi(gpioPinsStrs.substr(0, pos)));
        gpioPinsStrs.erase(0, pos + 1);
    }
    if (!gpioPinsStrs.empty()) {
        gpioPins.push_back(std::stoi(gpioPinsStrs));
    }

    return gpioPins;
}

// Функция парсинга GPIO Pins из JSON
std::vector<int> parseJsonGpioPins(const std::string& json) {
    std::vector<int> gpioPins;
    size_t start = json.find("\"gpioPins\":[") + 12;
    size_t end = json.find("]", start);
    std::string gpioPinsStr = json.substr(start, end - start);
    return parseGpioPins(gpioPinsStr);
}


// Преобразование JSON строки в структуру ClientData
DeviceData fromJson(const std::string& json) {
    DeviceData data;
    auto extractValue = [](const std::string& json, const std::string& key) {
        size_t start = json.find(key) + key.length() + 2;
        size_t end = json.find_first_of(",}", start);
        return json.substr(start, end - start - (json[start] == '\"' ? 1 : 0));
    };

    data.macAddress = extractValue(json, "\"address\"");
    data.name = extractValue(json, "\"name\"");
    data.enabled = extractValue(json, "\"enabled\"") == "true";
    data.isOnline = extractValue(json, "\"connected\"") == "true";
    data.gpioPins = parseJsonGpioPins(json);
    data.targetTemperature = std::stof(extractValue(json, "\"targetTemperature\""));
    data.currentTemperature = std::stof(extractValue(json, "\"currentTemperature\""));
    data.humidity = std::stof(extractValue(json, "\"currentHumidity\""));
    data.gpioOnTime = std::stoul(extractValue(json, "\"gpioOnTime\""));

    return data;
}

#endif