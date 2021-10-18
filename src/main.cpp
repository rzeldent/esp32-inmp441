#include <Arduino.h>

#include <.settings.h>

#include "soc/rtc_cntl_reg.h"
#include <WiFi.h>
#include <ESPmDNS.h>

#include <arduinoFFT.h>

#include <WebServer.h>

#include <audio_capture_mems.h>
#include <audio_capture_dac.h>

#include <ArduinoOTA.h>

#include <telnet_server.h>

// Web server
WebServer web_server;
// Telnet server
telnet_server telnet;

// Audio capture MEMS microphone pin configuration
const i2s_pin_config_t inmp441_pin_config = {
    .bck_io_num = INMP441_PIN_SCK,
    .ws_io_num = INMP441_PIN_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = INMP441_PIN_SD};

// Capture instance
//audio_capture_mems capture(I2S_NUM_PORT, inmp441_pin_config);
audio_capture_dac capture(I2S_NUM_PORT, ANALOG_ADC1_CHANNEL);

void handle_not_found()
{
  web_server.send(404, "text/plain", "Not found");
}

void handle_root()
{
  web_server.send(200, "text/html", "Connected");
}

void handle_audio()
{
  log_i("Handling audio request");
  // Send content
  web_server.sendContent("HTTP/1.1 200 OK\r\n"
                         "Content-Type: audio/x-wav\r\n\r\n");
  // Get raw client
  auto wifi_client = web_server.client();
  if (!wifi_client.connected())
    return;

  // Send WAV header
  auto wav_header = capture.wav_header(1024 * 1024 * 1024); // 1G samples
  if (wifi_client.write(wav_header.data(), wav_header.size()) != wav_header.size())
    return;

  while (wifi_client.connected())
  {
    // Get samples
    auto sample_buffer = capture.pop_samples();
    if (sample_buffer)
    {
      // Write raw PCM
      wifi_client.write((const char *)sample_buffer->samples.data(), sample_buffer->samples.size() * sizeof(mono_sample_t));

      std::vector<double> vReal(sample_buffer->samples.size());
      std::vector<double> vImag(sample_buffer->samples.size());
      std::transform(sample_buffer->samples.begin(), sample_buffer->samples.end(), vReal.begin(),
                     [](const mono_sample_t &s) -> float
                     { return s; });

      // Do some processing
      arduinoFFT fft(vReal.data(), vImag.data(), vReal.size(), capture.get_sample_rate());
      fft.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
      fft.Compute(FFT_FORWARD);
      auto peak = fft.MajorPeak();
      log_i("FFT peak at %f Hz", peak);
      telnet.printf("FFT peak at %f Hz\n", peak);
    }
    else
      log_e("Unable to pop samples");
  }
}

void setup()
{
  // Disable brownout
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  Serial.setDebugOutput(true);
  esp_log_level_set("*", ESP_LOG_VERBOSE);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, false);

  log_i("MAC: %s", WiFi.macAddress().c_str());
  log_i("CPU Freq: %d Mhz", getCpuFrequencyMhz());

  // Print chip information
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  log_i("%s chip with %d CPU cores, WiFi%s%s, ", CONFIG_IDF_TARGET, chip_info.cores, (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "", (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
  log_i("silicon revision: %d, ", chip_info.revision);
  log_i("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024), (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
  log_i("Free heap: %d\n", esp_get_free_heap_size());

  log_i("Starting...");

  capture.start();

  log_i("Connecting to accesspoint: %s", WIFI_SSID_NAME);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID_NAME, WIFI_SSID_PASSWORD);
  log_w("Waiting for WiFi connection");
  if (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    log_w("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  log_i("WiFi connected. IP: %s", WiFi.localIP().toString().c_str());

  // OTA Updates

  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  // ArduinoOTA.setHostname("myesp32");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
      .onStart([]()
               {
                 const char *type = ArduinoOTA.getCommand() == U_FLASH ? "application" : "filesystem";
                 // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
                 log_i("Start OTA update %s", type);
               })
      .onEnd([]()
             { log_i("OTA update finished"); })
      .onProgress([](unsigned int progress, unsigned int total)
                  { log_i("OTA update progress: %u%%", progress / (total / 100)); })
      .onError([](ota_error_t error)
               {
                 const char *error_text = "Unknown";
                 switch (error)
                 {
                 case OTA_AUTH_ERROR:
                   error_text = "Auth Failed";
                   break;
                 case OTA_BEGIN_ERROR:
                   error_text = "Begin Failed";
                   break;
                 case OTA_CONNECT_ERROR:
                   error_text = "Connect Failed";
                   break;
                 case OTA_RECEIVE_ERROR:
                   error_text = "Receive Failed";
                   break;
                 case OTA_END_ERROR:
                   error_text = "End Failed";
                   break;
                 }

                 log_e("Error[%u]: %s", error, error_text);
               });

  ArduinoOTA.begin();

  // Start MDNS
  if (!MDNS.begin("esp32"))
    log_e("Unable to start mDNS");

  // Add service to mDNS - rtsp
  MDNS.addService("http", "tcp", 80);

  web_server.on("/", handle_root);
  web_server.on("/audio", handle_audio);
  web_server.onNotFound(handle_not_found);

  web_server.begin();
}

void loop()
{
  // put your main code here, to run repeatedly:
  ArduinoOTA.handle();
  telnet.handleClient();
  web_server.handleClient();
}