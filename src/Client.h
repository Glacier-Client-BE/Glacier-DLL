#pragma once
#include <Windows.h>

class Client {
public:
    static Client& get();

    void init(HMODULE hModule);
    void shutdown();

    bool    isRunning() const { return m_running; }
    HMODULE getModule() const { return m_module;  }

private:
    Client() = default;
    ~Client() = default;
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    HMODULE m_module  = nullptr;
    bool    m_running = false;
};
