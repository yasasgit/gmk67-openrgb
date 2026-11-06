#include "GMK67Plugin.h"

#include <algorithm>
#include <fstream>
#include <QApplication>
#include <QImage>
#include <QPainter>

#define NA  0xFFFFFFFF

struct GMK67Key
{
    const char*     name;
    unsigned int    light_index;
};

/*---------------------------------------------------------*\
| 5 x 15 grid, light_index from vendor KeyboardLayout.xml   |
| (0 = empty cell)                                          |
\*---------------------------------------------------------*/
static const GMK67Key gmk67_grid[5][15] =
{
    { {"Key: Escape", 1}, {"Key: 1", 20}, {"Key: 2", 21}, {"Key: 3", 22}, {"Key: 4", 23}, {"Key: 5", 24}, {"Key: 6", 25}, {"Key: 7", 26},
      {"Key: 8", 27}, {"Key: 9", 28}, {"Key: 0", 29}, {"Key: -", 30}, {"Key: =", 31}, {"Key: Backspace", 103}, {nullptr, 0} },
    { {"Key: Tab", 37}, {"Key: Q", 38}, {"Key: W", 39}, {"Key: E", 40}, {"Key: R", 41}, {"Key: T", 42}, {"Key: Y", 43}, {"Key: U", 44},
      {"Key: I", 45}, {"Key: O", 46}, {"Key: P", 47}, {"Key: [", 48}, {"Key: ]", 49}, {"Key: \\ (ANSI)", 67}, {"Key: Delete", 119} },
    { {"Key: Caps Lock", 55}, {"Key: A", 56}, {"Key: S", 57}, {"Key: D", 58}, {"Key: F", 59}, {"Key: G", 60}, {"Key: H", 61}, {"Key: J", 62},
      {"Key: K", 63}, {"Key: L", 64}, {"Key: ;", 65}, {"Key: '", 66}, {"Key: Enter", 85}, {nullptr, 0}, {"Key: Page Up", 118} },
    { {"Key: Left Shift", 73}, {"Key: Z", 74}, {"Key: X", 75}, {"Key: C", 76}, {"Key: V", 77}, {"Key: B", 78}, {"Key: N", 79}, {"Key: M", 80},
      {"Key: ,", 81}, {"Key: .", 82}, {"Key: /", 83}, {"Key: Right Shift", 84}, {nullptr, 0}, {"Key: Up Arrow", 101}, {"Key: Page Down", 121} },
    { {"Key: Left Control", 91}, {"Key: Left Windows", 92}, {"Key: Left Alt", 93}, {nullptr, 0}, {nullptr, 0}, {nullptr, 0}, {"Key: Space", 94}, {nullptr, 0},
      {nullptr, 0}, {"Key: Right Alt", 95}, {"Key: Right Fn", 96}, {nullptr, 0}, {"Key: Left Arrow", 99}, {"Key: Down Arrow", 100}, {"Key: Right Arrow", 102} },
};

/*---------------------------------------------------------*\
| Light mode effects (light_modes.pcapng + language ids     |
| 200-219). fixed_speed: speed byte the vendor app sends    |
| for effects without a speed setting (0 = user speed)      |
\*---------------------------------------------------------*/
struct GMK67Effect
{
    const char*     name;
    std::uint8_t    value;
    bool            has_random;
    std::uint8_t    fixed_speed;
    bool            has_direction;
};

static const GMK67Effect gmk67_effects[] =
{
    { "Static",     0x01, false, 0x0B, false },
    { "SingleOn",   0x02, false, 0x06, false },
    { "SingleOff",  0x03, false, 0x06, false },
    { "Glittering", 0x04, true,  0,    false },
    { "Falling",    0x05, true,  0,    false },
    { "Colourful",  0x06, true,  0x10, false },
    { "Breath",     0x07, true,  0,    false },
    { "Spectrum",   0x08, true,  0x01, false },
    { "Outward",    0x09, true,  0,    false },
    { "Scrolling",  0x0A, true,  0x03, true  },
    { "Rolling",    0x0B, true,  0,    false },
    { "Rotating",   0x0C, true,  0,    false },
    { "Explode",    0x0D, true,  0,    false },
    { "Launch",     0x0E, true,  0x01, false },
    { "Ripples",    0x0F, true,  0x01, false },
    { "Flowing",    0x10, true,  0,    false },
    { "Pulsating",  0x11, true,  0,    false },
    { "Tilt",       0x12, true,  0,    false },
    { "Shuttle",    0x13, true,  0,    false },
};

#define GMK67_MODE_DIRECT   0x80
#define GMK67_MODE_OFF      0x00

static void CallbackUpdateLEDs(void* plugin)
{
    ((GMK67Plugin*)plugin)->UpdateLEDs();
}

static void CallbackUpdateZoneLEDs(void* plugin, int)
{
    ((GMK67Plugin*)plugin)->UpdateLEDs();
}

static void CallbackSaveMode(void* plugin)
{
    ((GMK67Plugin*)plugin)->SaveMode();
}

static void CallbackUpdateMode(void* plugin)
{
    ((GMK67Plugin*)plugin)->UpdateMode();
}

static void CallbackUpdateZoneMode(void* plugin, int)
{
    ((GMK67Plugin*)plugin)->UpdateMode();
}

static QImage SquareIcon(int size)
{
    QImage icon(size, size, QImage::Format_ARGB32_Premultiplied);
    icon.fill(Qt::transparent);

    QImage source = QImage(":/gmk67/gmk67.png").scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QPainter painter(&icon);
    painter.drawImage((size - source.width()) / 2, (size - source.height()) / 2, source);
    return(icon);
}

GMK67Plugin::~GMK67Plugin()
{
}

OpenRGBPluginInfo GMK67Plugin::GetPluginInfo()
{
    OpenRGBPluginInfo info;

    info.Name           = "Zuoya GMK67";
    info.Description    = "Per-key lighting for Zuoya GMK67 (05AC:024F) on stock firmware";
    info.Version        = "0.5";
    info.Commit         = "";
    info.URL            = "";
    info.Icon           = SquareIcon(64);
    info.Location       = OPENRGB_PLUGIN_LOCATION_INFORMATION;
    info.Label          = "GMK67";
    info.TabIcon        = SquareIcon(16);
    info.ProtocolVersion = 0;

    return(info);
}

unsigned int GMK67Plugin::GetPluginAPIVersion()
{
    return(OPENRGB_PLUGIN_API_VERSION);
}

void GMK67Plugin::Load(OpenRGBPluginAPIInterface* plugin_api_ptr)
{
    api     = plugin_api_ptr;

    nlohmann::json settings = LoadSettings();
    unsigned int interval = std::clamp<unsigned int>(settings["frame_interval_ms"].get<unsigned int>(), 20, 1000);

    device  = new GMK67Device(interval, [this](bool connected) { OnConnectionChanged(connected); });

    RGBController_Setup setup{};

    setup.name          = "Zuoya GMK67";
    setup.vendor        = "Zuoya";
    setup.description   = "Zuoya GMK67 (stock firmware, wired USB)";
    setup.location      = "HID: 05AC:024F MI_00";
    setup.serial        = "";
    setup.version       = "";
    setup.type          = DEVICE_TYPE_KEYBOARD;
    setup.flags         = CONTROLLER_FLAG_VIRTUAL;
    setup.active_mode   = 0;

    mode direct;
    direct.name             = "Direct";
    direct.value            = GMK67_MODE_DIRECT;
    direct.flags            = MODE_FLAG_HAS_PER_LED_COLOR | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_MANUAL_SAVE;
    direct.color_mode       = MODE_COLORS_PER_LED;
    direct.brightness_min   = 0;
    direct.brightness_max   = 15;
    direct.brightness       = 15;
    setup.modes.push_back(direct);

    for(const GMK67Effect& effect : gmk67_effects)
    {
        mode m;
        m.name              = effect.name;
        m.value             = effect.value;
        m.flags             = MODE_FLAG_HAS_MODE_SPECIFIC_COLOR | MODE_FLAG_HAS_BRIGHTNESS;
        m.color_mode        = MODE_COLORS_MODE_SPECIFIC;
        m.colors_min        = 1;
        m.colors_max        = 1;
        m.colors.push_back(ToRGBColor(255, 0, 0));
        m.brightness_min    = 0;
        m.brightness_max    = 15;
        m.brightness        = 15;

        if(effect.has_random)
        {
            m.flags        |= MODE_FLAG_HAS_RANDOM_COLOR;
            m.color_mode    = MODE_COLORS_RANDOM;
        }
        if(effect.fixed_speed == 0)
        {
            m.flags        |= MODE_FLAG_HAS_SPEED;
            m.speed_min     = 0;
            m.speed_max     = 15;
            m.speed         = 10;
        }
        if(effect.has_direction)
        {
            m.flags        |= MODE_FLAG_HAS_DIRECTION_LR | MODE_FLAG_HAS_DIRECTION_UD;
            m.direction     = MODE_DIRECTION_UP;
        }
        setup.modes.push_back(m);
    }

    mode off;
    off.name                = "Off";
    off.value               = GMK67_MODE_OFF;
    off.flags               = 0;
    off.color_mode          = MODE_COLORS_NONE;
    setup.modes.push_back(off);

    unsigned int map[5 * 15];
    unsigned int led_idx = 0;

    for(unsigned int row = 0; row < 5; row++)
    {
        for(unsigned int col = 0; col < 15; col++)
        {
            const GMK67Key& key = gmk67_grid[row][col];

            if(key.name == nullptr)
            {
                map[row * 15 + col] = NA;
                continue;
            }

            led new_led;
            new_led.name    = key.name;
            new_led.value   = key.light_index;
            setup.leds.push_back(new_led);
            light_indices.push_back((std::uint8_t)key.light_index);

            map[row * 15 + col] = led_idx++;
        }
    }

    zone keyboard;
    keyboard.name       = "Keyboard";
    keyboard.type       = ZONE_TYPE_MATRIX;
    keyboard.leds_count = led_idx;
    keyboard.leds_min   = led_idx;
    keyboard.leds_max   = led_idx;
    keyboard.matrix_map.Set(5, 15, map);
    setup.zones.push_back(keyboard);

    setup.object_ptr            = this;
    setup.DeviceUpdateLEDs      = CallbackUpdateLEDs;
    setup.DeviceUpdateZoneLEDs  = CallbackUpdateZoneLEDs;
    setup.DeviceUpdateSingleLED = CallbackUpdateZoneLEDs;
    setup.DeviceUpdateMode      = CallbackUpdateMode;
    setup.DeviceUpdateZoneMode  = CallbackUpdateZoneMode;
    setup.DeviceSaveMode        = CallbackSaveMode;

    controller = api->CreateVirtualRGBController(&setup);
    api->RegisterVirtualRGBControllerInThread(controller);
}

nlohmann::json GMK67Plugin::LoadSettings()
{
    nlohmann::json      defaults    = { { "frame_interval_ms", 50 } };
    nlohmann::json      settings    = defaults;
    filesystem::path    dir         = api->GetConfigurationDirectory() / "plugins" / "settings";
    filesystem::path    file        = dir / "ZuoyaGMK67.json";

    std::ifstream in(file);
    if(in)
    {
        nlohmann::json loaded = nlohmann::json::parse(in, nullptr, false);
        if(loaded.is_object())
        {
            settings.update(loaded);
        }
    }
    in.close();

    if(!settings["frame_interval_ms"].is_number_unsigned())
    {
        settings["frame_interval_ms"] = defaults["frame_interval_ms"];
    }

    std::error_code ec;
    filesystem::create_directories(dir, ec);
    std::ofstream out(file);
    if(out)
    {
        out << settings.dump(4);
    }

    return(settings);
}

static QString StatusText(bool connected)
{
    return(connected
        ? "Zuoya GMK67: Connected (wired USB).\nKeep the stock GMK67 app closed while using OpenRGB."
        : "Zuoya GMK67: Disconnected.\nConnect the keyboard by USB cable. The 2.4 GHz dongle and Bluetooth are not supported.");
}

QWidget* GMK67Plugin::GetWidget()
{
    QLabel* label = new QLabel();
    label->setText(StatusText(device && device->IsConnected()));
    label->setMargin(12);
    status_label = label;
    return(label);
}

void GMK67Plugin::OnConnectionChanged(bool connected)
{
    QPointer<QLabel> label = status_label;
    QMetaObject::invokeMethod(qApp, [label, connected]()
    {
        if(label)
        {
            label->setText(StatusText(connected));
        }
    }, Qt::QueuedConnection);

    if(connected)
    {
        applied_mode = -1;
        UpdateMode();
    }
}

QMenu* GMK67Plugin::GetTrayMenu()
{
    return(nullptr);
}

void GMK67Plugin::Unload()
{
    if(controller)
    {
        api->UnregisterVirtualRGBController(controller);
    }

    delete device;
    device = nullptr;

    if(controller)
    {
        api->DeleteVirtualRGBController(controller);
        controller = nullptr;
    }
}

void GMK67Plugin::UpdateLEDs()
{
    if(!controller || !device)
    {
        return;
    }

    int active = controller->GetActiveMode();
    if(active < 0 || controller->GetModeColorMode(active) != MODE_COLORS_PER_LED)
    {
        return;
    }

    std::vector<std::uint8_t> frame(512, 0);

    for(std::size_t led_idx = 0; led_idx < light_indices.size(); led_idx++)
    {
        RGBColor color = controller->GetColor((unsigned int)led_idx);

        frame[led_idx * 4 + 0] = light_indices[led_idx];
        frame[led_idx * 4 + 1] = (std::uint8_t)std::min<unsigned int>(RGBGetRValue(color), 0xFE);
        frame[led_idx * 4 + 2] = (std::uint8_t)std::min<unsigned int>(RGBGetGValue(color), 0xFE);
        frame[led_idx * 4 + 3] = (std::uint8_t)std::min<unsigned int>(RGBGetBValue(color), 0xFE);
    }

    device->QueueLive(frame);
}

void GMK67Plugin::SaveMode()
{
    if(!controller || !device)
    {
        return;
    }

    int active = controller->GetActiveMode();
    if(active < 0 || controller->GetModeColorMode(active) != MODE_COLORS_PER_LED)
    {
        return;
    }

    device->QueueSave(BuildSaveArea());
}

std::vector<std::uint8_t> GMK67Plugin::BuildSaveArea()
{
    std::vector<std::uint8_t> area(576, 0);

    for(std::size_t led_idx = 0; led_idx < light_indices.size(); led_idx++)
    {
        unsigned int    light_index = light_indices[led_idx];
        RGBColor        color       = controller->GetColor((unsigned int)led_idx);

        area[light_index * 4 + 0] = (std::uint8_t)light_index;
        area[light_index * 4 + 1] = (std::uint8_t)RGBGetRValue(color);
        area[light_index * 4 + 2] = (std::uint8_t)RGBGetGValue(color);
        area[light_index * 4 + 3] = (std::uint8_t)RGBGetBValue(color);
    }

    area[574] = 0xAA;
    area[575] = 0x55;

    return(area);
}

void GMK67Plugin::UpdateMode()
{
    if(!controller || !device)
    {
        return;
    }

    int active = controller->GetActiveMode();
    if(active < 0)
    {
        return;
    }

    bool entering_direct = (applied_mode.exchange(active) != 0);

    std::vector<std::uint8_t> page(16, 0);
    page[14] = 0xAA;
    page[15] = 0x55;

    if(active == 0)
    {
        page[0]  = GMK67_MODE_DIRECT;
        page[8]  = 0x01;
        page[9]  = (std::uint8_t)(controller->GetModeBrightness(active) + 1);
        page[10] = 0x0F;
        if(entering_direct)
        {
            device->QueueSave(BuildSaveArea());
        }
        device->QueueModePage(page);
        UpdateLEDs();
        return;
    }

    if((unsigned int)active > sizeof(gmk67_effects) / sizeof(gmk67_effects[0]))
    {
        page[0]  = GMK67_MODE_OFF;
        page[9]  = 0x01;
        page[10] = 0x01;
        device->QueueModePage(page);
        return;
    }

    const GMK67Effect&  effect  = gmk67_effects[active - 1];
    RGBColor            color   = controller->GetModeColor(active, 0);

    page[0]  = effect.value;
    page[1]  = (std::uint8_t)RGBGetRValue(color);
    page[2]  = (std::uint8_t)RGBGetGValue(color);
    page[3]  = (std::uint8_t)RGBGetBValue(color);
    page[8]  = (effect.has_random && controller->GetModeColorMode(active) == MODE_COLORS_RANDOM) ? 1 : 0;
    page[9]  = (std::uint8_t)(controller->GetModeBrightness(active) + 1);
    page[10] = effect.fixed_speed ? effect.fixed_speed : (std::uint8_t)(controller->GetModeSpeed(active) + 1);
    page[11] = effect.has_direction ? (std::uint8_t)controller->GetModeDirection(active) : 0;

    device->QueueModePage(page);
}

void GMK67Plugin::OnProfileAboutToLoad()
{
}

void GMK67Plugin::OnProfileLoad(nlohmann::json)
{
}

nlohmann::json GMK67Plugin::OnProfileSave()
{
    return(nlohmann::json());
}

unsigned char* GMK67Plugin::OnSDKCommand(unsigned int, unsigned char*, unsigned int*)
{
    return(nullptr);
}

void GMK67Plugin::ProfileManagerUpdated(unsigned int)
{
}

void GMK67Plugin::ResourceManagerUpdated(unsigned int)
{
}

void GMK67Plugin::SettingsManagerUpdated(unsigned int)
{
}
