/**********************************************************************

  Audacity: A Digital Audio Editor

  AudioSetupToolBar.cpp

*******************************************************************//*!

\class AudioSetupToolBar
\brief A toolbar to allow easier changing of input and output devices .

*//*******************************************************************/

#include "AudioSetupToolBar.h"
#include "ToolManager.h"

#include <thread>

#include <wx/log.h>
#include <wx/sizer.h>
#include <wx/tooltip.h>

#include "../ActiveProject.h"

#include "AColor.h"
#include "AllThemeResources.h"
#include "AudioIOBase.h"
#include "DeviceToolBar.h"
#include "../KeyboardCapture.h"
#include "Project.h"
#include "../ProjectWindows.h"
#include "DeviceManager.h"
#include "../prefs/PrefsDialog.h"
#include "../prefs/DevicePrefs.h"
#include "../widgets/AButton.h"
#include "../widgets/BasicMenu.h"
#include "../widgets/wxWidgetsWindowPlacement.h"

namespace {
   class ViewDeviceSettingsDialog final : public PrefsDialog
   {
   public:
      ViewDeviceSettingsDialog(wxWindow* parent, AudacityProject& project,
         const TranslatableString& title, PrefsPanel::Factories& factories,
         int page)
         : PrefsDialog(parent, &project, title, factories)
         , mPage(page)
      {
      }

      long GetPreferredPage() override
      {
         return mPage;
      }

      void SavePreferredPage() override
      {
      }

   private:
      const int mPage;
   };
}

IMPLEMENT_CLASS(AudioSetupToolBar, ToolBar);

////////////////////////////////////////////////////////////
/// Methods for AudioSetupToolBar
////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(AudioSetupToolBar, ToolBar)
   EVT_BUTTON(ID_AUDIO_SETUP_BUTTON, AudioSetupToolBar::OnAudioSetup)
END_EVENT_TABLE()

//Standard constructor
AudioSetupToolBar::AudioSetupToolBar( AudacityProject &project )
: ToolBar( project, AudioSetupBarID, XO("Audio Setup"), wxT("Audio Setup") )
{
   mSubscription = DeviceManager::Instance()->Subscribe(
      *this, &AudioSetupToolBar::OnRescannedDevices );
}

AudioSetupToolBar::~AudioSetupToolBar()
{
}

AudioSetupToolBar &AudioSetupToolBar::Get( AudacityProject &project )
{
   auto &toolManager = ToolManager::Get( project );
   return *static_cast<AudioSetupToolBar*>( toolManager.GetToolBar(AudioSetupBarID) );
}

const AudioSetupToolBar &AudioSetupToolBar::Get( const AudacityProject &project )
{
   return Get( const_cast<AudacityProject&>( project )) ;
}

void AudioSetupToolBar::Create(wxWindow *parent)
{
   ToolBar::Create(parent);

   // Simulate a size event to set initial meter placement/size
   wxSizeEvent event(GetSize(), GetId());
   event.SetEventObject(this);
   GetEventHandler()->ProcessEvent(event);
}

void AudioSetupToolBar::DeinitChildren()
{
   mInput.reset();
   mOutput.reset();
   mInputChannels.Clear();
   mHost.Clear();
}

void AudioSetupToolBar::Populate()
{
   MakeButtonBackgroundsSmall();
   SetBackgroundColour( theTheme.Colour( clrMedium  ) );
   MakeAudioSetupButton();

   DeinitChildren();

#if wxUSE_TOOLTIPS
   RegenerateTooltips();
   wxToolTip::Enable(true);
   wxToolTip::SetDelay(1000);
#endif

   // Set default order and mode
   ArrangeButtons();

   RepopulateMenus();
}

void AudioSetupToolBar::Repaint(wxDC* dc)
{
#ifndef USE_AQUA_THEME
   wxSize s = mSizer->GetSize();
   wxPoint p = mSizer->GetPosition();

   wxRect bevelRect(p.x, p.y, s.GetWidth() - 1, s.GetHeight() - 1);
   AColor::Bevel(*dc, true, bevelRect);
#endif
}

void AudioSetupToolBar::MakeAudioSetupButton()
{
   mAudioSetup = safenew AButton(this, ID_AUDIO_SETUP_BUTTON);
   //i18n-hint: Audio setup button text, keep as short as possible
   mAudioSetup->SetLabel(XO("Audio Setup"));
   mAudioSetup->SetButtonType(AButton::FrameButton);
   mAudioSetup->SetImages(
      theTheme.Image(bmpRecoloredUpSmall),
      theTheme.Image(bmpRecoloredUpHiliteSmall),
      theTheme.Image(bmpRecoloredDownSmall),
      theTheme.Image(bmpRecoloredHiliteSmall),
      theTheme.Image(bmpRecoloredUpSmall));
   mAudioSetup->SetIcon(theTheme.Image(bmpSetup));
   mAudioSetup->SetForegroundColour(theTheme.Colour(clrTrackPanelText));
}

void AudioSetupToolBar::ArrangeButtons()
{
   int flags = wxALIGN_CENTER | wxRIGHT;

   // (Re)allocate the button sizer
   if (mSizer)
   {
      Detach(mSizer);
      std::unique_ptr < wxSizer > {mSizer}; // DELETE it
   }

   Add((mSizer = safenew wxBoxSizer(wxHORIZONTAL)), 1, wxEXPAND);
   mSizer->Add(mAudioSetup, 1, wxEXPAND);

   // Layout the sizer
   mSizer->Layout();

   // Layout the toolbar
   Layout();

   SetMinSize(GetSizer()->GetMinSize());
}

void AudioSetupToolBar::ReCreateButtons()
{
   bool isAudioSetupDown = false;

   // ToolBar::ReCreateButtons() will get rid of its sizer and
   // since we've attached our sizer to it, ours will get deleted too
   // so clean ours up first.
   if (mSizer)
   {
      isAudioSetupDown = mAudioSetup->IsDown();
      Detach(mSizer);

      std::unique_ptr < wxSizer > {mSizer}; // DELETE it
      mSizer = nullptr;
   }

   ToolBar::ReCreateButtons();

   if (isAudioSetupDown)
   {
      mAudioSetup->PushDown();
   }

   EnableDisableButtons();

   RegenerateTooltips();
}

void AudioSetupToolBar::OnFocus(wxFocusEvent &event)
{
   KeyboardCapture::OnFocus( *this, event );
}

void AudioSetupToolBar::OnAudioSetup(wxCommandEvent& WXUNUSED(evt))
{
   wxMenu menu;

   //i18n-hint: Audio setup menu
   mHost.AppendSubMenu(*this, menu, &AudioSetupToolBar::OnHost, _("&Host"));
   menu.AppendSeparator();

   //i18n-hint: Audio setup menu
   AppendSubMenu(menu, mOutput, _("&Playback Device"));
   menu.AppendSeparator();

   //i18n-hint: Audio setup menu
   AppendSubMenu(menu, mInput, _("&Recording Device"));
   menu.AppendSeparator();

   //i18n-hint: Audio setup menu
   mInputChannels.AppendSubMenu(*this,
      menu, &AudioSetupToolBar::OnChannels, _("Recording &Channels"));
   menu.AppendSeparator();
   menu.Append(kAudioSettings, _("&Audio Settings..."));

   menu.Bind(wxEVT_MENU_CLOSE, [this](auto&) { mAudioSetup->PopUp(); });
   // Bind two ID ranges and one single ID
   menu.Bind(wxEVT_MENU, &AudioSetupToolBar::OnInput, this,
      kInput, kOutput - 1);
   menu.Bind(wxEVT_MENU, &AudioSetupToolBar::OnOutput, this,
      kOutput, kAudioSettings - 1);
   menu.Bind(wxEVT_MENU, &AudioSetupToolBar::OnSettings, this, kAudioSettings);

   wxWindow* btn = FindWindow(ID_AUDIO_SETUP_BUTTON);
   wxRect r = btn->GetRect();
   BasicMenu::Handle{ &menu }.Popup(
      wxWidgetsWindowPlacement{ btn },
      { r.GetLeft(), r.GetBottom() }
   );
}

void AudioSetupToolBar::UpdatePrefs()
{
   wxString desc;
   const std::vector<DeviceSourceMap> &inMaps  = DeviceManager::Instance()->GetInputDeviceMaps();
   const std::vector<DeviceSourceMap> &outMaps = DeviceManager::Instance()->GetOutputDeviceMaps();

   auto selectedHost = mHost.Get();
   wxString oldHost = selectedHost ? *selectedHost : wxString{};

   auto hostName = AudioIOHost.Read();

   // if the prefs host name doesn't match the one displayed, it changed
   // in another project's AudioSetupToolBar, so we need to repopulate everything.
   if (oldHost != hostName)
      // updates mHost
      FillHostDevices();

   auto devName = AudioIORecordingDevice.Read();
   auto sourceName = AudioIORecordingSource.Read();
   if (sourceName.empty())
      desc = devName;
   else
      desc = devName + wxT(": ") + sourceName;

   auto selectedInput = GetSelectedRadioItemLabel(*mInput);
   if (selectedInput && *selectedInput != desc) {
      if (auto item = mInput->FindItem(desc); item != wxNOT_FOUND) {
         mInput->FindChildItem(item)->Check();
         // updates mInputChannels
         FillInputChannels();
      }
      else if (mInput->GetMenuItemCount()) {
         for (size_t i = 0; i < inMaps.size(); i++) {
            if (inMaps[i].hostString == hostName &&
               MakeDeviceSourceString(&inMaps[i]) == mInput->FindItem(kInput)->GetItemLabelText()) {
               // use the default.  It should exist but check just in case, falling back on the 0 index.
               DeviceSourceMap* defaultMap = DeviceManager::Instance()->GetDefaultInputDevice(inMaps[i].hostIndex);
               if (defaultMap) {
                  const auto menuId = mInput->FindItem(MakeDeviceSourceString(defaultMap));
                  auto item = mInput->FindChildItem(menuId);
                  if (item)
                     item->Check();

                  SetDevices(defaultMap, nullptr);
               }
               else {
                  //use the first item (0th index) if we have no familiar devices
                  auto item = mInput->FindChildItem(kInput);
                  if (item)
                     item->Check();

                  SetDevices(&inMaps[i], nullptr);
               }
               break;
            }
         }
      }
   } 

   devName = AudioIOPlaybackDevice.Read();
   sourceName = AudioIOPlaybackSource.Read();
   if (sourceName.empty())
      desc = devName;
   else
      desc = devName + wxT(": ") + sourceName;

   auto selectedOutput = GetSelectedRadioItemLabel(*mOutput);
   if (selectedOutput && *selectedOutput != desc) {
      if (auto item = mOutput->FindItem(desc); item != wxNOT_FOUND) {
         mOutput->FindChildItem(item)->Check();
      }
      else if (mOutput->GetMenuItemCount()) {
         for (size_t i = 0; i < outMaps.size(); i++) {
            if (outMaps[i].hostString == hostName &&
               MakeDeviceSourceString(&outMaps[i]) == mOutput->FindItem(kOutput)->GetItemLabelText()) {
               // use the default.  It should exist but check just in case, falling back on the 0 index.
               DeviceSourceMap* defaultMap = DeviceManager::Instance()->GetDefaultInputDevice(outMaps[i].hostIndex);
               if (defaultMap) {
                  const auto menuId = mOutput->FindItem(MakeDeviceSourceString(defaultMap));
                  auto item = mOutput->FindChildItem(menuId);
                  if (item)
                     item->Check();

                  SetDevices(nullptr, defaultMap);
               }
               else {
                  //use the first item (0th index) if we have no familiar devices
                  auto item = mOutput->FindChildItem(kOutput);
                  if (item)
                     item->Check();

                  SetDevices(nullptr, &outMaps[i]);
               }
               break;
            }
         }
      }
   }

   // 0 based choice id is one less than the number of channels
   long oldChannels = 1 + mInputChannels.GetSmallIntegerId();

   // Preferences store the actual number of channels
   auto newChannels = AudioIORecordChannels.ReadWithDefault(0);
   if (newChannels > 0 && oldChannels != newChannels)
      mInputChannels.Set(newChannels - 1);

   selectedHost = mHost.Get();
   if (!hostName.empty() && selectedHost && *selectedHost != hostName)
      mHost.Set(hostName);

   RegenerateTooltips();

   // Set label to pull in language change
   SetLabel(XO("Audio Setup"));

   // Give base class a chance
   ToolBar::UpdatePrefs();

   Layout();
   Refresh();
}

void AudioSetupToolBar::UpdateSelectedPrefs( int id )
{
   if (id == DeviceToolbarPrefsID())
      UpdatePrefs();
   ToolBar::UpdateSelectedPrefs( id );
}


void AudioSetupToolBar::EnableDisableButtons()
{
   auto gAudioIO = AudioIOBase::Get();
   if (gAudioIO) {
      // we allow changes when monitoring, but not when recording
      bool audioStreamActive = gAudioIO->IsStreamActive() && !gAudioIO->IsMonitoring();

      if (audioStreamActive) {
         mAudioSetup->Disable();
      }
      else {
         mAudioSetup->Enable();
      }
   }
}

void AudioSetupToolBar::RegenerateTooltips()
{
#if wxUSE_TOOLTIPS
   for (long iWinID = ID_AUDIO_SETUP_BUTTON; iWinID < BUTTON_COUNT; iWinID++)
   {
      auto pCtrl = static_cast<AButton*>(this->FindWindow(iWinID));
      CommandID name;
      switch (iWinID)
      {
      case ID_AUDIO_SETUP_BUTTON:
         name = wxT("Open Audio Setup");
         break;
      }
      std::vector<ComponentInterfaceSymbol> commands(
         1u, { name, Verbatim(pCtrl->GetLabel()) });

      // Some have a second
      switch (iWinID)
      {
      case ID_AUDIO_SETUP_BUTTON:
         break;
      }
      ToolBar::SetButtonToolTip(
         mProject, *pCtrl, commands.data(), commands.size());
   }
#endif
}

void AudioSetupToolBar::RepopulateMenus()
{
   FillHosts();
   FillHostDevices();
   FillInputChannels();
   // make the device display selection reflect the prefs if they exist
   UpdatePrefs();
}

void AudioSetupToolBar::FillHosts()
{
   const std::vector<DeviceSourceMap> &inMaps = DeviceManager::Instance()->GetInputDeviceMaps();
   const std::vector<DeviceSourceMap> &outMaps = DeviceManager::Instance()->GetOutputDeviceMaps();

   wxArrayString hosts;

   // go over our lists add the host to the list if it isn't there yet

   for (auto & device : inMaps) {
      if (!make_iterator_range(hosts).contains(device.hostString)) {
         hosts.push_back(device.hostString);
      }
   }

   for (auto & device : outMaps) {
      if (!make_iterator_range(hosts).contains(device.hostString)) {
         hosts.push_back(device.hostString);
      }
   }

   mHost.Set(std::move(hosts));
}

void AudioSetupToolBar::FillHostDevices()
{
   const std::vector<DeviceSourceMap> &inMaps = DeviceManager::Instance()->GetInputDeviceMaps();
   const std::vector<DeviceSourceMap> &outMaps = DeviceManager::Instance()->GetOutputDeviceMaps();

   //read what is in the prefs
   auto host = AudioIOHost.Read();
   int foundHostIndex = -1;

   // if the host is not in the hosts combo then we rescanned.
   // set it to blank so we search for another host.
   if (mHost.Find(host) < 0)
      host = wxT("");
 
   // Try to find a hostIndex, among either inputs or outputs, assumed to be
   // unique among the union of the set of input and output devices
   for (auto & device : outMaps) {
      if (device.hostString == host) {
         foundHostIndex = device.hostIndex;
         break;
      }
   }

   if (foundHostIndex == -1) {
      for (auto & device : inMaps) {
         if (device.hostString == host) {
            foundHostIndex = device.hostIndex;
            break;
         }
      }
   }

   // If no host was found based on the prefs device host, load the first available one
   if (foundHostIndex == -1) {
      if (outMaps.size()) {
         foundHostIndex = outMaps[0].hostIndex;
      }
      else if (inMaps.size()) {
         foundHostIndex = inMaps[0].hostIndex;
      }
   }

   // Make sure in/out are clear in case no host was found
   mInput = std::make_unique<wxMenu>();
   mOutput = std::make_unique<wxMenu>();

   // If we still have no host it means no devices, in which case do nothing.
   if (foundHostIndex == -1) {
      return;
   }

   // Repopulate the Input/Output device list available to the user
   for (int nextMenuId = kInput, i = 0; i < inMaps.size(); ++i) {
      auto& device = inMaps[i];

      if (foundHostIndex == device.hostIndex) {
         mInput->AppendRadioItem(nextMenuId, MakeDeviceSourceString(&device));
         nextMenuId++;

         if (host.empty()) {
            host = device.hostString;
            AudioIOHost.Write(host);
            mHost.Set(host);
         }
      }
   }

   for (int nextMenuId = kOutput, i = 0; i < outMaps.size(); ++i) {
      auto& device = outMaps[i];

      if (foundHostIndex == device.hostIndex) {
         mOutput->AppendRadioItem(nextMenuId, MakeDeviceSourceString(&device));
         nextMenuId++;

         if (host.empty()) {
            host = device.hostString;
            AudioIOHost.Write(host);
            mHost.Set(host);
         }
      }
   }

   gPrefs->Flush();

   // The setting of the Device is left up to menu handlers
}

void AudioSetupToolBar::FillInputChannels()
{
   const std::vector<DeviceSourceMap> &inMaps = DeviceManager::Instance()->GetInputDeviceMaps();
   auto host = AudioIOHost.Read();
   auto device = AudioIORecordingDevice.Read();
   auto source = AudioIORecordingSource.Read();
   long newChannels = 0;

   auto oldChannels = AudioIORecordChannels.Read();
   mInputChannels.Clear();

   wxArrayStringEx names;
   for (auto & dev: inMaps) {
      if (source == dev.sourceString &&
          device == dev.deviceString &&
          host   == dev.hostString) {

         // add one selection for each channel of this source
         for (size_t j = 0; j < (unsigned int)dev.numChannels; j++) {
            wxString name;

            if (j == 0) {
               name = _("1 (Mono) Recording Channel");
            }
            else if (j == 1) {
               name = _("2 (Stereo) Recording Channels");
            }
            else {
               name = wxString::Format(wxT("%d"), (int)j + 1);
            }
            names.push_back(name);
         }
         newChannels = dev.numChannels;
         if (oldChannels <= newChannels && oldChannels >= 1) {
            newChannels = oldChannels;
         }
         AudioIORecordChannels.Write(newChannels);
         break;
      }
   }
   mInputChannels.Set(std::move(names));
   if (newChannels >= 1)
      // Correct to 0-based index in choice
      mInputChannels.Set(newChannels - 1);
}

void AudioSetupToolBar::AppendSubMenu(wxMenu& menu, const std::unique_ptr<wxMenu>& submenu, const wxString& title)
{
   auto clonedMenu = std::make_unique<wxMenu>();

   for (const auto& item : submenu->GetMenuItems()) {
      auto cloneMenuItem = clonedMenu->AppendRadioItem(item->GetId(), item->GetItemLabelText());

      if (item->IsChecked())
         cloneMenuItem->Check();
   }

   auto menuItem = menu.AppendSubMenu(clonedMenu.release(), title);

   const auto selected = GetSelectedRadioItemLabel(*submenu);
   if (!selected) {
      menuItem->Enable(false);
   }
}

void AudioSetupToolBar::AppendSubMenu( AudioSetupToolBar &toolbar,
   wxMenu& menu, const wxArrayString &labels, int checkedItem,
   Callback callback, const wxString& title)
{
   auto subMenu = std::make_unique<wxMenu>();
   int ii = 0;
   for (const auto &label : labels) {
      // Assign fresh ID with wxID_ANY
      auto subMenuItem = subMenu->AppendRadioItem(wxID_ANY, label);
      if (ii == checkedItem)
         subMenuItem->Check();
      subMenu->Bind(wxEVT_MENU,
         [&toolbar, callback, ii](wxCommandEvent &){ (toolbar.*callback)(ii); },
         subMenuItem->GetId());
      ++ii;
   }
   auto menuItem = menu.AppendSubMenu(subMenu.release(), title);
   if (checkedItem < 0)
      menuItem->Enable(false);
}

void AudioSetupToolBar::Choice::AppendSubMenu(
   AudioSetupToolBar &toolBar, wxMenu &menu, const wxString &title)
{
   toolBar.AppendSubMenu(menu, mMenu, title);
}

void AudioSetupToolBar::Choices::AppendSubMenu(AudioSetupToolBar &toolBar,
   wxMenu &menu, Callback callback, const wxString &title)
{
   AudioSetupToolBar::
   AppendSubMenu(toolBar, menu, mStrings, mIndex, callback, title);
}

std::optional<wxString>
AudioSetupToolBar::GetSelectedRadioItemLabel(const wxMenu& menu)
{
   const auto& items = menu.GetMenuItems();

   for (const auto& item : items) {
      if (item->IsChecked())
         return item->GetItemLabelText();
   }

   return std::nullopt;
}

void AudioSetupToolBar::OnRescannedDevices(DeviceChangeMessage m)
{
   // Hosts may have disappeared or appeared so a complete repopulate is needed.
   if (m == DeviceChangeMessage::Rescan)
      RepopulateMenus();
}

//return true if host changed, false otherwise.
bool AudioSetupToolBar::ChangeHost(int hostId)
{
   // Update cache with selected host
   if (!mHost.Set(hostId))
      return false;
   auto name = mHost.Get();
   assert(name); // should not be nullopt if Set succeeded

   auto oldHost = AudioIOHost.Read();
   const auto newHost = *name;

   if (oldHost == newHost)
      return false;

   //change the host and switch to correct devices.
   AudioIOHost.Write(newHost);
   gPrefs->Flush();

   // populate the devices and reassign mHost
   FillHostDevices();

   return true;
}

void AudioSetupToolBar::SetDevices(const DeviceSourceMap *in, const DeviceSourceMap *out)
{
   if (in) {
      AudioIORecordingDevice.Write(in->deviceString);
      AudioIORecordingSourceIndex.Write(in->sourceIndex);
      if (in->totalSources >= 1)
         AudioIORecordingSource.Write(in->sourceString);
      else
         AudioIORecordingSource.Reset();
      gPrefs->Flush();

      // updates mInputChannels
      FillInputChannels();
   }

   if (out) {
      AudioIOPlaybackDevice.Write(out->deviceString);
      if (out->totalSources >= 1) {
         AudioIOPlaybackSource.Write(out->sourceString);
      } else {
         AudioIOPlaybackSource.Reset();
      }
      gPrefs->Flush();
   }
}

void AudioSetupToolBar::ChangeDevice(int deviceId, bool isInput)
{
   int newIndex = -1;
   auto& device = isInput ? mInput : mOutput;

   auto host = AudioIOHost.Read();
   const std::vector<DeviceSourceMap>& maps = isInput ? DeviceManager::Instance()->GetInputDeviceMaps()
      : DeviceManager::Instance()->GetOutputDeviceMaps();

   auto item = device->FindChildItem(deviceId);
   if (item) {
      // Update cache with the chosen device
      item->Check();
      wxString newDevice = item->GetItemLabelText();

      for (size_t i = 0; i < maps.size(); ++i) {
         wxString name = MakeDeviceSourceString(&maps[i]);
         if (name == newDevice && maps[i].hostString == host) {
            newIndex = i;
         }
      }
   }

   if (newIndex < 0) {
      wxLogDebug(wxT("AudioSetupToolBar::ChangeDevice(): couldn't find device indices"));
      return;
   }

   SetDevices(isInput ? &maps[newIndex] : nullptr,
              isInput ? nullptr : &maps[newIndex]);
}

void AudioSetupToolBar::OnHost(int id)
{
   ChangeHost(id);
   CommonMenuItemSteps(false);
}

void AudioSetupToolBar::OnChannels(int id)
{
   mInputChannels.Set(id);
   // Remember 1-based value in preferences
   AudioIORecordChannels.Write(id + 1);
   CommonMenuItemSteps(false);
}

void AudioSetupToolBar::OnInput(wxCommandEvent& event)
{
   int id = event.GetId();
   ChangeDevice(id, true);
   CommonMenuItemSteps(false);
}

void AudioSetupToolBar::OnOutput(wxCommandEvent& event)
{
   int id = event.GetId();
   ChangeDevice(id, false);
   CommonMenuItemSteps(false);
}

void AudioSetupToolBar::OnSettings(wxCommandEvent& event)
{
   CommonMenuItemSteps(true);
}

void AudioSetupToolBar::CommonMenuItemSteps(bool audioSettingsChosen)
{
   auto gAudioIO = AudioIOBase::Get();
   if (gAudioIO) {
      // We cannot have gotten here if gAudioIO->IsAudioTokenActive(),
      // per the setting of AudioIONotBusyFlag and AudioIOBusyFlag in
      // AudacityProject::GetUpdateFlags().
      // However, we can have an invalid audio token (so IsAudioTokenActive()
      // is false), but be monitoring.
      // If monitoring, have to stop the stream, so HandleDeviceChange() can work.
      // We could disable the Preferences command while monitoring, i.e.,
      // set AudioIONotBusyFlag/AudioIOBusyFlag according to monitoring, as well.
      // Instead allow it because unlike recording, for example, monitoring
      // is not clearly something that should prohibit changing device.
      // TODO: We *could* be smarter in this method and call HandleDeviceChange()
      // only when the device choices actually changed. True of lots of prefs!
      // As is, we always stop monitoring before handling the device change.
      if (gAudioIO->IsMonitoring())
      {
         gAudioIO->StopStream();
         while (gAudioIO->IsBusy()) {
            using namespace std::chrono;
            std::this_thread::sleep_for(100ms);
         }
      }

      if (audioSettingsChosen) {
         PrefsPanel::Factories factories;
         factories.push_back(PrefsPanel::PrefsNode(DevicePrefsFactory));

         ViewDeviceSettingsDialog dialog(&GetProjectFrame(mProject), mProject, XO("Audio Settings:"), factories, 0);
         dialog.SetSize(600, 420);
         dialog.Center();

         if (0 != dialog.ShowModal()) {
            PrefsListener::Broadcast(DeviceToolbarPrefsID());
         }
      }
      else {
         gAudioIO->HandleDeviceChange();
         PrefsListener::Broadcast(DeviceToolbarPrefsID());
      }
   }
}

static RegisteredToolbarFactory factory{ AudioSetupBarID,
   []( AudacityProject &project ){
      return ToolBar::Holder{ safenew AudioSetupToolBar{ project } };
   }
};

namespace {
AttachedToolBarMenuItem sAttachment{
   /* i18n-hint: Clicking this menu item shows the toolbar
      that manages the audio devices */
   AudioSetupBarID, wxT("ShowAudioSetupTB"), XXO("&Audio Setup Toolbar")
};
}

