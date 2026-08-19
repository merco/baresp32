/*
 * FastLED_min - Minimal WS2812B Library for ESP32
 * Memory efficient alternative to FastLED
 * Uses ESP32 RMT peripheral for precise timing
 */

#ifndef FASTLED_MIN_H
#define FASTLED_MIN_H

#include <Arduino.h>
#include <driver/rmt.h>

// WS2812B timing constants (80MHz RMT clock = 12.5ns per tick)
#define T0H_TICKS 32   // 0 code high time (400ns)
#define T1H_TICKS 64   // 1 code high time (800ns)  
#define TL_TICKS  52   // Both low times (650ns average)

class CRGB {
public:
  union {
    struct {
      uint8_t r;
      uint8_t g;
      uint8_t b;
    };
    uint8_t raw[3];
  };
  
  CRGB() : r(0), g(0), b(0) {}
  CRGB(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}
  
  static const CRGB Black;
  static const CRGB Red;
  static const CRGB Green;
  static const CRGB Blue;
  static const CRGB Yellow;
  static const CRGB Cyan;
  static const CRGB Magenta;
  static const CRGB White;
  static const CRGB Orange;
  static const CRGB Purple;
  static const CRGB Pink;
  
  CRGB& operator=(const CRGB& rhs) {
    r = rhs.r; g = rhs.g; b = rhs.b;
    return *this;
  }
  
  uint8_t& operator[](uint8_t index) {
    return raw[index];
  }
};

inline const CRGB CRGB::Black = CRGB(0, 0, 0);
inline const CRGB CRGB::Red = CRGB(255, 0, 0);
inline const CRGB CRGB::Green = CRGB(0, 255, 0);
inline const CRGB CRGB::Blue = CRGB(0, 0, 255);
inline const CRGB CRGB::Yellow = CRGB(255, 255, 0);
inline const CRGB CRGB::Cyan = CRGB(0, 255, 255);
inline const CRGB CRGB::Magenta = CRGB(255, 0, 255);
inline const CRGB CRGB::White = CRGB(255, 255, 255);
inline const CRGB CRGB::Orange = CRGB(255, 165, 0);
inline const CRGB CRGB::Purple = CRGB(128, 0, 128);
inline const CRGB CRGB::Pink = CRGB(255, 192, 203);

template<uint8_t DATA_PIN>
class CFastLED_min {
private:
  CRGB* leds;
  uint16_t numLeds;
  uint8_t brightness;
  rmt_channel_t rmtChannel;
  bool initialized;
  
public:
  CFastLED_min() : leds(nullptr), numLeds(0), brightness(255), 
                   rmtChannel(RMT_CHANNEL_0), initialized(false) {}
  
  void addLeds(CRGB* ledArray, uint16_t count) {
    leds = ledArray;
    numLeds = count;
    
    rmt_config_t config = {};
    config.rmt_mode = RMT_MODE_TX;
    config.channel = rmtChannel;
    config.gpio_num = (gpio_num_t)DATA_PIN;
    config.clk_div = 1;  // 80MHz base clock
    config.mem_block_num = 1;
    config.tx_config.loop_en = false;
    config.tx_config.carrier_en = false;
    config.tx_config.idle_output_en = true;
    config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
    
    rmt_config(&config);
    rmt_driver_install(config.channel, 0, 0);
    
    initialized = true;
  }
  
  void setBrightness(uint8_t b) {
    brightness = b;
  }
  
  void show() {
    if (!initialized || leds == nullptr || numLeds == 0) return;
    
    uint32_t total_items = numLeds * 24;
    // Allocazione dinamica del buffer per tutti i led insieme
    rmt_item32_t* rmt_items = (rmt_item32_t*)malloc(total_items * sizeof(rmt_item32_t));
    if (rmt_items == nullptr) return; // Protezione out-of-memory
    
    uint32_t item_idx = 0;
    
    for (uint16_t i = 0; i < numLeds; i++) {
      // Scala la luminosità usando un bitshift veloce
      uint8_t r = (leds[i].r * brightness) >> 8;
      uint8_t g = (leds[i].g * brightness) >> 8;
      uint8_t b = (leds[i].b * brightness) >> 8;
      
      // Formato standard dei WS2812B: ordinamento GRB
      uint32_t color = (g << 16) | (r << 8) | b;
      
      for (int bit = 0; bit < 24; bit++) {
        bool isHigh = color & (1 << (23 - bit));
        rmt_items[item_idx].level0 = 1;
        rmt_items[item_idx].duration0 = isHigh ? T1H_TICKS : T0H_TICKS;
        rmt_items[item_idx].level1 = 0;
        rmt_items[item_idx].duration1 = TL_TICKS;
        item_idx++;
      }
    }
    
    // Invia tutto il pacchetto dati senza interruzioni software
    rmt_write_items(rmtChannel, rmt_items, total_items, true);
    
    // Libera la memoria allocata
    free(rmt_items);
    
    // Segnale di Reset hardware necessario per i WS2812B (> 50 microsecondi a LOW)
    delayMicroseconds(60);
  }
  
  void clear() {
    if (leds == nullptr) return;
    for (uint16_t i = 0; i < numLeds; i++) {
      leds[i] = CRGB::Black;
    }
  }
  
  CRGB& operator[](uint16_t index) {
    return leds[index];
  }
};

template<uint8_t DATA_PIN>
CFastLED_min<DATA_PIN> FastLED_min;

#define FASTLED_MIN_SETUP(PIN, LEDS, COUNT) \
  FastLED_min<PIN>.addLeds(LEDS, COUNT)

#endif // FASTLED_MIN_H