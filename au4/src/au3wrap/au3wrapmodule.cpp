/*
* Audacity: A Digital Audio Editor
*/
#include "au3wrapmodule.h"
#include "effects/effectsettings.h"

#include <wx/log.h>

#include "libraries/lib-preferences/Prefs.h"
#include "libraries/lib-audio-io/AudioIO.h"
#include "libraries/lib-project-file-io/ProjectFileIO.h"
#include "libraries/lib-files/FileNames.h"
#include "libraries/lib-module-manager/PluginManager.h"

#include "mocks/qtBasicUI.h"

#include "modularity/ioc.h"

#include "internal/wxlogwrap.h"
#include "internal/au3project.h"
#include "internal/au3audiodevicesprovider.h"
#include "internal/au3commonsettings.h"

#include "log.h"

using namespace au::au3;
using namespace muse::modularity;

std::string Au3WrapModule::moduleName() const
{
    return "au3wrap";
}

void Au3WrapModule::registerExports()
{
    m_audioDevicesProvider = std::make_shared<Au3AudioDevicesProvider>();

    ioc()->registerExport<IAu3ProjectCreator>(moduleName(), new Au3ProjectCreator());
    ioc()->registerExport<playback::IAudioDevicesProvider>(moduleName(), m_audioDevicesProvider);
}

void Au3WrapModule::onInit(const muse::IApplication::RunMode&)
{
    m_wxLog = new WxLogWrap();
    wxLog::SetActiveTarget(m_wxLog);

    std::unique_ptr<Au3CommonSettings> auset = std::make_unique<Au3CommonSettings>();
    InitPreferences(std::move(auset));

    PluginManager::Get().Initialize([](const FilePath& localFileName) {
       return std::make_unique<EffectSettings>(localFileName.ToStdString());
    });

    AudioIO::Init();

    bool ok = ProjectFileIO::InitializeSQL();
    if (!ok) {
        LOGE() << "failed init sql";
    }

    m_audioDevicesProvider->init();

    static QtBasicUI uiServices;
    (void)BasicUI::Install(&uiServices);
}

void Au3WrapModule::onDeinit()
{
    wxLog::SetActiveTarget(nullptr);
    delete m_wxLog;
}
