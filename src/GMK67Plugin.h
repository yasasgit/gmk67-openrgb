#pragma once

#include <QObject>
#include <QLabel>
#include <QPointer>
#include <atomic>
#include "OpenRGBPluginInterface.h"
#include "GMK67Device.h"

class GMK67Plugin : public QObject, public OpenRGBPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenRGBPluginInterface_IID FILE "GMK67Plugin.json")
    Q_INTERFACES(OpenRGBPluginInterface)

public:
    ~GMK67Plugin() override;

    OpenRGBPluginInfo   GetPluginInfo() override;
    unsigned int        GetPluginAPIVersion() override;

    void                Load(OpenRGBPluginAPIInterface* plugin_api_ptr) override;
    QWidget*            GetWidget() override;
    QMenu*              GetTrayMenu() override;
    void                Unload() override;
    void                OnProfileAboutToLoad() override;
    void                OnProfileLoad(nlohmann::json profile_data) override;
    nlohmann::json      OnProfileSave() override;
    unsigned char*      OnSDKCommand(unsigned int pkt_id, unsigned char* pkt_data, unsigned int* pkt_size) override;

    void                ProfileManagerUpdated(unsigned int update_reason) override;
    void                ResourceManagerUpdated(unsigned int update_reason) override;
    void                SettingsManagerUpdated(unsigned int update_reason) override;

    void                UpdateLEDs();
    void                UpdateMode();
    void                SaveMode();

private:
    nlohmann::json              LoadSettings();
    std::vector<std::uint8_t>   BuildSaveArea();
    void                        OnConnectionChanged(bool connected);

    OpenRGBPluginAPIInterface*  api             = nullptr;
    RGBControllerInterface*     controller      = nullptr;
    GMK67Device*                device          = nullptr;
    QPointer<QLabel>            status_label;
    std::vector<std::uint8_t>   light_indices;
    std::atomic<int>            applied_mode    { -1 };
};
