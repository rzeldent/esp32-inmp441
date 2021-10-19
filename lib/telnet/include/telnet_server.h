#pragma once

#if defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#elif defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#endif

#include <stdarg.h>
#include <list>

class telnet_server
{
private:
  ushort port_;
  bool is_listening_;
  WiFiServer server_;
  std::list<WiFiClient> clients_;

public:
  telnet_server(ushort port = 23);
  ~telnet_server();

  bool is_listening() const;

  void handleClient();

  int printf(const char *format, ...);
};
