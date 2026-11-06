#pragma once

#include <windows.h>
#include <cfgmgr32.h>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

/*---------------------------------------------------------*\
| Zuoya GMK67 (05AC:024F) lighting over stock firmware.     |
| MI_00 feature report, 64 bytes, no report ID.             |
|   Live frames:  04 02, 04 20 + 8 pages (vendor Music mode) |
|   Saved layout: 04 18, 04 23 + 9 pages, 04 f0              |
|   Effect page:  04 18, 04 13 + 1 page, 04 f0               |
\*---------------------------------------------------------*/
class GMK67Device
{
public:
    GMK67Device(unsigned int frame_interval_ms, std::function<void(bool)> connection_callback);
    ~GMK67Device();

    void            QueueLive(const std::vector<std::uint8_t>& frame);
    void            QueueSave(const std::vector<std::uint8_t>& area);
    void            QueueModePage(const std::vector<std::uint8_t>& page);
    bool            IsConnected();
    void            SignalDeviceChanged();

private:
    bool            Open();
    void            Close();
    bool            SendCommand(const std::uint8_t* data, std::size_t len, bool read_reply);
    bool            WriteLive(const std::vector<std::uint8_t>& frame);
    bool            WriteSave(const std::vector<std::uint8_t>& area);
    bool            WriteModePage(const std::vector<std::uint8_t>& page);
    void            WorkerThread();

    unsigned int                interval_ms;
    std::function<void(bool)>   on_connection;
    HCMNOTIFICATION             notification;
    HANDLE                      handle;
    std::thread                 worker;
    std::mutex                  lock;
    std::condition_variable     cv;
    bool                        stop;
    bool                        live_pending;
    bool                        save_pending;
    bool                        mode_pending;
    bool                        device_changed;
    std::vector<std::uint8_t>   pending_live;
    std::vector<std::uint8_t>   last_live;
    std::vector<std::uint8_t>   pending_save;
    std::vector<std::uint8_t>   pending_mode;
    std::atomic<bool>           connected;
};
