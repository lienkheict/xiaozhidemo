#ifndef LUNAR_CALENDAR_H
#define LUNAR_CALENDAR_H

#include <stdint.h>
#include <time.h>

// Cấu trúc lưu ngày tháng
struct LunarDate {
    int day;
    int month;
    int year;
    bool is_leap_month;  // Tháng nhuận
};

struct SolarDate {
    int day;
    int month;
    int year;
};

// Khai báo các hàm public
SolarDate lunar_to_solar(LunarDate lunar);
LunarDate solar_to_lunar(SolarDate solar);
LunarDate get_current_lunar_date();

// Hàm tiện ích
int get_lunar_month_days(int lunar_year, int lunar_month, bool is_leap);
int get_leap_month(int lunar_year);
SolarDate get_tet_date(int lunar_year);

#endif // LUNAR_CALENDAR_H