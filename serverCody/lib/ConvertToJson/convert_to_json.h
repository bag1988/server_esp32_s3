#ifndef CONVERT_TO_JSON_H
#define CONVERT_TO_JSON_H

#include <vector>
#include <string>
#include <variables_info.h>

std::string toJsonWifi(const WifiCredentials& data);
WifiCredentials fromJsonWifi(const std::string& json);
std::string toJson(const ClientData& data);
std::vector<int> parseGpioPins(const std::string& gpioPinsStr);
std::vector<int> parseJsonGpioPins(const std::string& json);
ClientData fromJson(const std::string& json);

#endif
