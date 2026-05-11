#include "weather_ui.h"
#include <esp_log.h>
#include <time.h>
#include <cmath>
#include <font_awesome.h>
#include "lunar_calendar.h" 
#include "alarm_manager.h" 
#include <lvgl.h>

#ifndef FONT_AWESOME_BOLT
#define FONT_AWESOME_BOLT "\uf0e7"
#endif

// Khai báo font hệ thống từ bên ngoài
extern const lv_font_t samsung_48; 
extern const lv_font_t samsung_22; 

#define TAG "WeatherUI"
#define BORDER_THICKNESS 4
#define SCREEN_RES 240
#define HALF_RES 120

// External font declarations
LV_FONT_DECLARE(font_awesome_30_4);
LV_FONT_DECLARE(lv_font_montserrat_20); 
LV_FONT_DECLARE(lv_font_montserrat_28);

// Biến tĩnh quản lý 5 phân đoạn của hình vuông (Cạnh trên chia đôi để xuất phát từ giữa)
static lv_obj_t * square_borders[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};

/**
 * Hàm Timer: Cập nhật viền chạy từ vị trí 12 giờ theo chiều kim đồng hồ
 */
static void second_timer_cb(lv_timer_t * t) {
    if (!square_borders[0]) return;

    time_t now = time(nullptr);
    struct tm tm_info;
    localtime_r(&now, &tm_info);

    int seconds = tm_info.tm_sec;
    
    // Bảng màu Neon 12 phân khu
    static const uint32_t neon_palette[] = {
        0x00FFFF, 0x0080FF, 0x00FF80, 0x39FF14, 0xCCFF00, 0xFFFF00,
        0xFFCC00, 0xFF8000, 0xFF4500, 0xFF0033, 0xFF007F, 0xBF00FF
    };
    uint32_t current_color = neon_palette[seconds / 5];

    // Reset kích thước khi sang phút mới
    if (seconds == 0) {
        for(int i=0; i<5; i++) {
            if (i == 0 || i == 2 || i == 4) lv_obj_set_width(square_borders[i], 0);
            else lv_obj_set_height(square_borders[i], 0);
        }
    }

    // Tổng chu vi là 960px cho 60 giây -> 1 giây = 16px
    int total_px = seconds * 16;

    // 1. Phân đoạn 0: TOP-RIGHT (12h -> 1h30). Dài 120px.
    int w0 = (total_px >= 120) ? HALF_RES : total_px;
    lv_obj_set_width(square_borders[0], w0);
    lv_obj_set_style_bg_color(square_borders[0], lv_color_hex(current_color), 0);

    // 2. Phân đoạn 1: RIGHT (1h30 -> 4h30). Dài 240px.
    int h1 = (total_px < 120) ? 0 : (total_px >= 360 ? SCREEN_RES : (total_px - 120));
    lv_obj_set_height(square_borders[1], h1);
    lv_obj_set_style_bg_color(square_borders[1], lv_color_hex(current_color), 0);

    // 3. Phân đoạn 2: BOTTOM (4h30 -> 7h30). Dài 240px.
    int w2 = (total_px < 360) ? 0 : (total_px >= 600 ? SCREEN_RES : (total_px - 360));
    lv_obj_set_width(square_borders[2], w2);
    lv_obj_set_x(square_borders[2], SCREEN_RES - w2); // Chạy từ phải sang trái
    lv_obj_set_style_bg_color(square_borders[2], lv_color_hex(current_color), 0);

    // 4. Phân đoạn 3: LEFT (7h30 -> 10h30). Dài 240px.
    int h3 = (total_px < 600) ? 0 : (total_px >= 840 ? SCREEN_RES : (total_px - 600));
    lv_obj_set_height(square_borders[3], h3);
    lv_obj_set_y(square_borders[3], SCREEN_RES - h3); // Chạy từ dưới lên trên
    lv_obj_set_style_bg_color(square_borders[3], lv_color_hex(current_color), 0);

    // 5. Phân đoạn 4: TOP-LEFT (10h30 -> 12h). Dài 120px.
    int w4 = (total_px < 840) ? 0 : (total_px >= 960 ? HALF_RES : (total_px - 840));
    lv_obj_set_width(square_borders[4], w4);
    lv_obj_set_x(square_borders[4], HALF_RES - w4); // Chạy từ trái về giữa (120px)
    lv_obj_set_style_bg_color(square_borders[4], lv_color_hex(current_color), 0);
}

WeatherUI::WeatherUI() 
    : idle_panel_(nullptr), idle_time_label_(nullptr), idle_date_label_(nullptr)
    , idle_temp_label_(nullptr), idle_icon_label_(nullptr), idle_city_label_(nullptr)
    , idle_detail_label_(nullptr), screen_width_(0), screen_height_(0) {}

WeatherUI::~WeatherUI() { HideIdleCard(); }

const char* WeatherUI::GetWeatherIcon(const std::string& code) {
    if (code.size() < 2) return FONT_AWESOME_CLOUD;
    std::string prefix = code.substr(0, 2);
    if (prefix == "01") return FONT_AWESOME_SUN;
    if (prefix == "09" || prefix == "10") return FONT_AWESOME_CLOUD_RAIN;
    if (prefix == "11") return FONT_AWESOME_BOLT;
    return FONT_AWESOME_CLOUD;
}

void WeatherUI::SetupIdleUI(lv_obj_t* parent, int screen_width, int screen_height) {
    if (!parent) return;

    idle_panel_ = lv_obj_create(parent);
    lv_obj_set_size(idle_panel_, SCREEN_RES, SCREEN_RES);
    lv_obj_center(idle_panel_);
    lv_obj_set_style_bg_color(idle_panel_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(idle_panel_, 0, 0);
    lv_obj_set_style_radius(idle_panel_, 0, 0);
    lv_obj_set_style_pad_all(idle_panel_, 0, 0); 
    lv_obj_remove_flag(idle_panel_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(idle_panel_, LV_OBJ_FLAG_HIDDEN);

    // 2. TẠO 5 PHÂN ĐOẠN VIỀN
    
    // Cạnh 0: Top-Right (Bắt đầu từ x=120, y=0)
    square_borders[0] = lv_obj_create(idle_panel_);
    lv_obj_set_size(square_borders[0], 0, BORDER_THICKNESS);
    lv_obj_set_pos(square_borders[0], HALF_RES, 0);

    // Cạnh 1: Right (Bắt đầu từ x=236, y=0)
    square_borders[1] = lv_obj_create(idle_panel_);
    lv_obj_set_size(square_borders[1], BORDER_THICKNESS, 0);
    lv_obj_set_pos(square_borders[1], SCREEN_RES - BORDER_THICKNESS, 0);

    // Cạnh 2: Bottom (Bắt đầu từ y=236, chạy ngược x)
    square_borders[2] = lv_obj_create(idle_panel_);
    lv_obj_set_size(square_borders[2], 0, BORDER_THICKNESS);
    lv_obj_set_pos(square_borders[2], 0, SCREEN_RES - BORDER_THICKNESS);

    // Cạnh 3: Left (Bắt đầu từ x=0, chạy ngược y)
    square_borders[3] = lv_obj_create(idle_panel_);
    lv_obj_set_size(square_borders[3], BORDER_THICKNESS, 0);
    lv_obj_set_pos(square_borders[3], 0, 0);

    // Cạnh 4: Top-Left (Bắt đầu từ x=0, chạy tới x=120)
    square_borders[4] = lv_obj_create(idle_panel_);
    lv_obj_set_size(square_borders[4], 0, BORDER_THICKNESS);
    lv_obj_set_pos(square_borders[4], 0, 0);

    for(int i = 0; i < 5; i++) {
        lv_obj_set_style_bg_opa(square_borders[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(square_borders[i], 0, 0);
        lv_obj_set_style_radius(square_borders[i], 0, 0);
        lv_obj_remove_flag(square_borders[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    lv_timer_create(second_timer_cb, 200, NULL);

    // 3. Container nội dung
    lv_obj_t * text_cont = lv_obj_create(idle_panel_);
    lv_obj_set_size(text_cont, 220, 220);
    lv_obj_center(text_cont);
    lv_obj_set_style_bg_opa(text_cont, 0, 0); 
    lv_obj_set_style_border_width(text_cont, 0, 0);
    lv_obj_set_style_pad_all(text_cont, 0, 0);
    lv_obj_set_flex_flow(text_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(text_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    idle_city_label_ = lv_label_create(text_cont);
    lv_obj_set_style_text_font(idle_city_label_, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(idle_city_label_, lv_color_hex(0xFF007F), 0); 
    lv_obj_set_style_margin_top(idle_city_label_, -10, 0);
    
    idle_date_label_ = lv_label_create(text_cont);
    lv_obj_set_style_text_font(idle_date_label_, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(idle_date_label_, lv_color_hex(0x00D7FF), 0);

    idle_time_label_ = lv_label_create(text_cont);
    lv_obj_set_style_text_font(idle_time_label_, &samsung_48, 0);
    lv_obj_set_style_text_color(idle_time_label_, lv_color_hex(0xFFFFFF), 0);
	
    lv_obj_t* row = lv_obj_create(text_cont); 
    lv_obj_set_size(row, LV_SIZE_CONTENT, 40);
    lv_obj_set_style_bg_opa(row, 0, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(row, 10, 0);

    idle_icon_label_ = lv_label_create(row);
    lv_obj_set_style_text_font(idle_icon_label_, &font_awesome_30_4, 0);
    lv_obj_set_style_text_color(idle_icon_label_, lv_color_hex(0xFFFF00), 0);

    idle_temp_label_ = lv_label_create(row);
    lv_obj_set_style_text_font(idle_temp_label_, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(idle_temp_label_, lv_color_hex(0x39FF14), 0);

    idle_detail_label_ = lv_label_create(text_cont);
    lv_obj_set_width(idle_detail_label_, 180);
    lv_obj_set_style_text_align(idle_detail_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(idle_detail_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_font(idle_detail_label_, &samsung_22, 0);
    lv_obj_set_style_text_color(idle_detail_label_, lv_color_hex(0xFF00FF), 0);

    lv_obj_move_foreground(text_cont);
}

void WeatherUI::ShowIdleCard(const IdleCardInfo& info) {
    if (!idle_panel_) return;
    LunarDate lunar = get_current_lunar_date();
    std::string lunar_str = std::to_string((int)lunar.day) + "/" + std::to_string((int)lunar.month) + " (AL)";
    if (idle_time_label_) lv_label_set_text(idle_time_label_, info.time_text.c_str());
    if (idle_date_label_) {
        std::string full_date = info.date_text + "  -  " + lunar_str;
        lv_label_set_text(idle_date_label_, full_date.c_str());
    }
    if (idle_city_label_) {
        const char* days[] = {"C.N", "T.2", "T.3", "T.4", "T.5", "T.6", "T.7"};
        time_t now = time(nullptr);
        struct tm tm_i;
        localtime_r(&now, &tm_i);
        lv_label_set_text(idle_city_label_, days[tm_i.tm_wday]);
    }
    if (idle_icon_label_ && info.icon) lv_label_set_text(idle_icon_label_, info.icon);
    if (idle_temp_label_) lv_label_set_text(idle_temp_label_, info.temperature_text.c_str());
    if (idle_detail_label_) {
        std::string detail = "Độ ẩm: " + info.humidity_text + " | " + info.pressure_text;
        lv_label_set_text(idle_detail_label_, detail.c_str());
    }
    lv_obj_remove_flag(idle_panel_, LV_OBJ_FLAG_HIDDEN);
}

void WeatherUI::UpdateIdleDisplay(const WeatherInfo& weather_info) {
    if (!idle_panel_ || lv_obj_has_flag(idle_panel_, LV_OBJ_FLAG_HIDDEN)) return;
    time_t now = time(nullptr);
    struct tm tm_info;
    localtime_r(&now, &tm_info);
    static int last_sec = -1;
    if (tm_info.tm_sec != last_sec) {
        IdleCardInfo card;
        char buf[16];
        strftime(buf, sizeof(buf), "%H:%M", &tm_info);
        card.time_text = buf;
        strftime(buf, sizeof(buf), "%d/%m", &tm_info);
        card.date_text = buf;
        if (weather_info.valid) {
            card.temperature_text = std::to_string((int)round(weather_info.temp)) + "°C";
            card.icon = GetWeatherIcon(weather_info.icon_code);
            card.humidity_text = std::to_string(weather_info.humidity) + "%";
            card.pressure_text = std::to_string(weather_info.pressure) + " hPa";
        }
        ShowIdleCard(card);
        last_sec = tm_info.tm_sec;
    }
}

void WeatherUI::HideIdleCard() {
    if (idle_panel_) lv_obj_add_flag(idle_panel_, LV_OBJ_FLAG_HIDDEN);
}