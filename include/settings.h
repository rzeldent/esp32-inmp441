#pragma once

// The SSID and password of the accesspoint to connect to
#define WIFI_SSID_NAME "wifi ssid name"
#define WIFI_SSID_PASSWORD "wifi ssid password"

// I2S Port to use
#define I2S_NUM_PORT I2S_NUM_0

// ESP32 conenctions/pins for MEMS microphone INMP441

//  SCK >> GPIO2
//  SD >> GPIO4
//  WS >> GPIO15
//  L/R >> N.C.
//  GND >> GND
//  VDD >> VDD3.3v

#define INMP441_PIN_WS 15
#define INMP441_PIN_SD 4
#define INMP441_PIN_SCK 2

// const i2s_pin_config_t inmp441_pin_config = {
//    .bck_io_num =  INMP441_PIN_SCK,
//    .ws_io_num =  INMP441_PIN_WS,
//    .data_out_num = I2S_PIN_NO_CHANGE,
//    .data_in_num =  INMP441_PIN_SD};

// ESP32 conenctions/pins for DAC

#define ANALOG_ADC1_CHANNEL ADC1_GPIO36_CHANNEL