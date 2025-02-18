#include <vector>
#include <string>

typedef struct 
{
    std::string address;
    std::string name;
    bool enabled;
    bool connected;
    std::vector<int> gpioPins;
    float targetTemperature;
    float currentTemperature;
    float currentHumidity;
    unsigned long gpioOnTime;
} ClientData;

// WiFi Credentials Structure
typedef struct 
{
  std::string ssid;
  std::string password;
} WifiCredentials;

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
std::string toJson(const ClientData& data) {
    std::string json = "{";
    json += "\"address\":\"" + data.address + "\",";
    json += "\"name\":\"" + data.name + "\",";
    json += "\"enabled\":" + std::string(data.enabled ? "true" : "false") + ",";
    json += "\"connected\":" + std::string(data.connected ? "true" : "false") + ",";
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
    json += "\"currentHumidity\":" + std::to_string(data.currentHumidity) + ",";
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
ClientData fromJson(const std::string& json) {
    ClientData data;
    auto extractValue = [](const std::string& json, const std::string& key) {
        size_t start = json.find(key) + key.length() + 2;
        size_t end = json.find_first_of(",}", start);
        return json.substr(start, end - start - (json[start] == '\"' ? 1 : 0));
    };

    data.address = extractValue(json, "\"address\"");
    data.name = extractValue(json, "\"name\"");
    data.enabled = extractValue(json, "\"enabled\"") == "true";
    data.connected = extractValue(json, "\"connected\"") == "true";
    data.gpioPins = parseJsonGpioPins(json);
    data.targetTemperature = std::stof(extractValue(json, "\"targetTemperature\""));
    data.currentTemperature = std::stof(extractValue(json, "\"currentTemperature\""));
    data.currentHumidity = std::stof(extractValue(json, "\"currentHumidity\""));
    data.gpioOnTime = std::stoul(extractValue(json, "\"gpioOnTime\""));

    return data;
}

// // Чтение массива ClientData из SPIFFS
// std::vector<ClientData> readJsonArrayFromSPIFFS(const std::string& filename) {
//     std::vector<ClientData> clients;
//     if (SPIFFS.begin(true)) {
//         File file = SPIFFS.open(filename.c_str(), FILE_READ);
//         if (file) {
//             while (file.available()) {
//                 std::string json;
//                 while (file.available()) {
//                     char c = file.read();
//                     json += c;
//                     if (c == '}') break;
//                 }
//                 clients.push_back(fromJson(json));
//             }
//             file.close();
//         }
//     }
//     return clients;
// }

// // Запись массива ClientData в SPIFFS
// void writeJsonArrayToSPIFFS(const std::string& filename, const std::vector<ClientData>& clients) {
//     if (SPIFFS.begin(true)) {
//         File file = SPIFFS.open(filename.c_str(), FILE_WRITE);
//         if (file) {
//             file.print("[\n");
//             for (size_t i = 0; i < clients.size(); ++i) {
//                 file.print(toJson(clients[i]).c_str());
//                 if (i != clients.size() - 1) {
//                     file.print(",\n");
//                 }
//             }
//             file.print("\n]");
//             file.close();
//         }
//     }
// }