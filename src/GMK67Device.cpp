#include "GMK67Device.h"

#include <chrono>
#include <cstring>
#include <string>
#include <setupapi.h>
#include <cfgmgr32.h>

extern "C"
{
#include <hidsdi.h>
}

#define GMK67_VID               0x05AC
#define GMK67_PID               0x024F
#define GMK67_REPORT_LEN        65
#define GMK67_AREA_LEN          576
#define GMK67_LIVE_LEN          512

static bool IsGMK67Path(std::wstring path)
{
    for(wchar_t& c : path)
    {
        c = towlower(c);
    }
    return(path.find(L"vid_05ac&pid_024f") != std::wstring::npos && path.find(L"mi_00") != std::wstring::npos);
}

static DWORD CALLBACK HotplugCallback(HCMNOTIFICATION, PVOID context, CM_NOTIFY_ACTION action, PCM_NOTIFY_EVENT_DATA data, DWORD)
{
    if((action == CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL || action == CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL)
    && data->FilterType == CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE
    && IsGMK67Path(data->u.DeviceInterface.SymbolicLink))
    {
        ((GMK67Device*)context)->SignalDeviceChanged();
    }
    return(ERROR_SUCCESS);
}

GMK67Device::GMK67Device(unsigned int frame_interval_ms, std::function<void(bool)> connection_callback)
{
    interval_ms         = frame_interval_ms;
    on_connection       = connection_callback;
    handle              = INVALID_HANDLE_VALUE;
    notification        = NULL;
    stop                = false;
    live_pending        = false;
    save_pending        = false;
    mode_pending        = false;
    device_changed      = false;
    connected           = Open();
    worker              = std::thread(&GMK67Device::WorkerThread, this);

    CM_NOTIFY_FILTER filter = {};
    filter.cbSize       = sizeof(filter);
    filter.FilterType   = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
    HidD_GetHidGuid(&filter.u.DeviceInterface.ClassGuid);
    CM_Register_Notification(&filter, this, HotplugCallback, &notification);
}

GMK67Device::~GMK67Device()
{
    if(notification)
    {
        CM_Unregister_Notification(notification);
    }
    {
        std::lock_guard<std::mutex> guard(lock);
        stop = true;
    }
    cv.notify_all();
    worker.join();
    Close();
}

bool GMK67Device::IsConnected()
{
    return(connected);
}

void GMK67Device::SignalDeviceChanged()
{
    {
        std::lock_guard<std::mutex> guard(lock);
        device_changed = true;
    }
    cv.notify_all();
}

bool GMK67Device::Open()
{
    GUID hid_guid;
    HidD_GetHidGuid(&hid_guid);

    HDEVINFO dev_info = SetupDiGetClassDevsW(&hid_guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if(dev_info == INVALID_HANDLE_VALUE)
    {
        return(false);
    }

    SP_DEVICE_INTERFACE_DATA if_data;
    if_data.cbSize = sizeof(if_data);

    for(DWORD idx = 0; SetupDiEnumDeviceInterfaces(dev_info, NULL, &hid_guid, idx, &if_data); idx++)
    {
        DWORD needed = 0;
        SetupDiGetDeviceInterfaceDetailW(dev_info, &if_data, NULL, 0, &needed, NULL);

        std::vector<std::uint8_t> buf(needed);
        PSP_DEVICE_INTERFACE_DETAIL_DATA_W detail = (PSP_DEVICE_INTERFACE_DETAIL_DATA_W)buf.data();
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        if(!SetupDiGetDeviceInterfaceDetailW(dev_info, &if_data, detail, needed, NULL, NULL))
        {
            continue;
        }

        if(!IsGMK67Path(detail->DevicePath))
        {
            continue;
        }

        HANDLE h = CreateFileW(detail->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if(h == INVALID_HANDLE_VALUE)
        {
            h = CreateFileW(detail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        }
        if(h == INVALID_HANDLE_VALUE)
        {
            continue;
        }

        HIDD_ATTRIBUTES attr;
        attr.Size = sizeof(attr);
        PHIDP_PREPARSED_DATA pp = NULL;
        HIDP_CAPS caps = {};

        if(HidD_GetAttributes(h, &attr) && HidD_GetPreparsedData(h, &pp))
        {
            HidP_GetCaps(pp, &caps);
            HidD_FreePreparsedData(pp);
        }

        if(attr.VendorID == GMK67_VID && attr.ProductID == GMK67_PID && caps.FeatureReportByteLength == GMK67_REPORT_LEN)
        {
            handle = h;
            SetupDiDestroyDeviceInfoList(dev_info);
            return(true);
        }

        CloseHandle(h);
    }

    SetupDiDestroyDeviceInfoList(dev_info);
    return(false);
}

void GMK67Device::Close()
{
    if(handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(handle);
        handle = INVALID_HANDLE_VALUE;
    }
}

bool GMK67Device::SendCommand(const std::uint8_t* data, std::size_t len, bool read_reply)
{
    std::uint8_t report[GMK67_REPORT_LEN] = {};
    std::memcpy(report + 1, data, len);

    if(!HidD_SetFeature(handle, report, GMK67_REPORT_LEN))
    {
        return(false);
    }

    if(read_reply)
    {
        std::uint8_t reply[GMK67_REPORT_LEN] = {};
        HidD_GetFeature(handle, reply, GMK67_REPORT_LEN);
    }
    return(true);
}

bool GMK67Device::WriteLive(const std::vector<std::uint8_t>& frame)
{
    const std::uint8_t sync[]   = { 0x04, 0x02 };
    const std::uint8_t live[]   = { 0x04, 0x20, 0, 0, 0, 0, 0, 0, 0x08 };

    if(!SendCommand(sync, sizeof(sync), true) || !SendCommand(live, sizeof(live), true))
    {
        return(false);
    }
    for(std::size_t page = 0; page < GMK67_LIVE_LEN / 64; page++)
    {
        if(!SendCommand(frame.data() + page * 64, 64, false))
        {
            return(false);
        }
    }
    return(true);
}

bool GMK67Device::WriteSave(const std::vector<std::uint8_t>& area)
{
    const std::uint8_t begin[]  = { 0x04, 0x18 };
    const std::uint8_t keys[]   = { 0x04, 0x23, 0, 0, 0, 0, 0, 0, 0x09 };
    const std::uint8_t end[]    = { 0x04, 0xF0 };

    if(!SendCommand(begin, sizeof(begin), true) || !SendCommand(keys, sizeof(keys), true))
    {
        return(false);
    }
    for(std::size_t page = 0; page < GMK67_AREA_LEN / 64; page++)
    {
        if(!SendCommand(area.data() + page * 64, 64, true))
        {
            return(false);
        }
    }
    return(SendCommand(end, sizeof(end), true));
}

bool GMK67Device::WriteModePage(const std::vector<std::uint8_t>& page)
{
    const std::uint8_t begin[]  = { 0x04, 0x18 };
    const std::uint8_t mode[]   = { 0x04, 0x13, 0, 0, 0, 0, 0, 0, 0x01 };
    const std::uint8_t end[]    = { 0x04, 0xF0 };

    return(SendCommand(begin, sizeof(begin), true) && SendCommand(mode, sizeof(mode), true)
        && SendCommand(page.data(), page.size(), true) && SendCommand(end, sizeof(end), true));
}

void GMK67Device::QueueLive(const std::vector<std::uint8_t>& frame)
{
    {
        std::lock_guard<std::mutex> guard(lock);
        if(frame == last_live && !live_pending)
        {
            return;
        }
        pending_live    = frame;
        live_pending    = true;
    }
    cv.notify_all();
}

void GMK67Device::QueueSave(const std::vector<std::uint8_t>& area)
{
    {
        std::lock_guard<std::mutex> guard(lock);
        pending_save    = area;
        save_pending    = true;
    }
    cv.notify_all();
}

void GMK67Device::QueueModePage(const std::vector<std::uint8_t>& page)
{
    {
        std::lock_guard<std::mutex> guard(lock);
        pending_mode    = page;
        mode_pending    = true;
        last_live.clear();
    }
    cv.notify_all();
}

void GMK67Device::WorkerThread()
{
    std::unique_lock<std::mutex> guard(lock);

    while(!stop)
    {
        cv.wait(guard, [this] { return(stop || live_pending || save_pending || mode_pending || device_changed); });
        if(stop)
        {
            break;
        }

        if(device_changed)
        {
            device_changed = false;
            guard.unlock();

            Close();
            bool now_connected = Open();
            bool changed       = (now_connected != connected);
            connected          = now_connected;
            if(changed && on_connection)
            {
                on_connection(now_connected);
            }

            guard.lock();
            continue;
        }

        bool                        do_mode         = mode_pending;
        std::vector<std::uint8_t>   mode_page       = pending_mode;
        bool                        do_save         = save_pending;
        std::vector<std::uint8_t>   save_area       = pending_save;
        bool                        do_live         = live_pending;
        std::vector<std::uint8_t>   live_frame      = pending_live;
        mode_pending        = false;
        save_pending        = false;
        live_pending        = false;
        guard.unlock();

        std::chrono::steady_clock::time_point next = std::chrono::steady_clock::now() + std::chrono::milliseconds(interval_ms);

        bool ok = connected && handle != INVALID_HANDLE_VALUE;
        if(ok && do_save)
        {
            ok = WriteSave(save_area);
        }
        if(ok && do_mode)
        {
            ok = WriteModePage(mode_page);
        }
        if(ok && do_live)
        {
            ok = WriteLive(live_frame);
        }
        if(!ok && connected)
        {
            Close();
            connected = false;
            if(on_connection)
            {
                on_connection(false);
            }
        }

        std::this_thread::sleep_until(next);
        guard.lock();

        if(ok && do_live)
        {
            last_live = live_frame;
        }
    }
}
