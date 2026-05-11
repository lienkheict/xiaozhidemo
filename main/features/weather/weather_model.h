#ifndef WEATHER_MODEL_H
#define WEATHER_MODEL_H

#include <string>

// Weather information structure
struct WeatherInfo {
    std::string city;
    std::string description;
    std::string icon_code;
    float temp = 0.0f;
    int humidity = 0;
    float feels_like = 0.0f;
    int pressure = 0;
    float wind_speed = 0.0f;
    bool valid = false;
};

// Idle card display information
struct IdleCardInfo {
    std::string city;
    std::string time_text; // Thời gian
    std::string date_text; // Ngày
    std::string day_text; // Thứ
    std::string temperature_text; // NHiệt độ
    std::string humidity_text; // Độ ẩm
    std::string description_text; // Mô tả
    std::string feels_like_text; // Cảm như nhiệt độ
    std::string wind_text; // Tốc độ gió 
    std::string pressure_text; // Áp suất
    const char* icon = nullptr;
};

#endif // WEATHER_MODEL_H
