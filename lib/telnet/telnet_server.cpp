#include <esp32-hal-log.h>

#include <stdio.h>

#include <vector>
#include <functional>

#include <telnet_server.h>

telnet_server::telnet_server(ushort port /*=23*/)
    : port_(port), is_listening_(false)
{
}

telnet_server::~telnet_server()
{
    if (is_listening_)
        server_.stop();
}

void telnet_server::handleClient()
{
    // Check if WiFi is connected
    if (WiFi.isConnected() && !is_listening_)
    {
        log_i("Start listening for connections");
        server_.begin(port_);
        server_.setNoDelay(true);
        is_listening_ = true;
        return;
    }

    // Check if WiFi is disconnected
    if (!WiFi.isConnected() && is_listening_)
    {
        log_i("Tearing down connections");
        for (auto client : clients_)
            client.stop();

        server_.stop();
        clients_.clear();
        is_listening_ = false;
        return;
    }

    auto newClient = server_.available();
    if (newClient)
    {
        log_i("Client connected: %s", newClient.remoteIP().toString().c_str());
        clients_.push_back(newClient);
    }

    clients_.remove_if(
        [](WiFiClient &c)
        {
            if (c.connected())
                return false;

            log_i("Client disconnected: %s", c.remoteIP().toString().c_str());
            return true;
        });
}

bool telnet_server::is_listening() const
{
    return is_listening_;
}

int telnet_server::printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    std::vector<char> buf(1 + vsnprintf(nullptr, 0, format, args));
    auto size = vsnprintf(buf.data(), buf.size(), format, args);
    va_end(args);

    for (auto client : clients_)
        client.write(buf.data());

    return size;
}
