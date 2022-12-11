/**********************************************************************

  Audacity: A Digital Audio Editor

  AudioSetupToolBar.h

  K. Soze

  **********************************************************************/

#ifndef __AUDACITY_AUDIO_SETUP_TOOLBAR__
#define __AUDACITY_AUDIO_SETUP_TOOLBAR__

#include <optional>
#include <vector>
#include <wx/menu.h>
#include "ToolBar.h"
#include "Observer.h"
#include "MemoryX.h"

enum class DeviceChangeMessage : char;

class wxMenu;
class wxString;
struct DeviceSourceMap;

class AudioSetupToolBar final : public ToolBar {
   static constexpr int kInput = 15200;
   static constexpr int kOutput = 15600;
   static constexpr int kAudioSettings = 15800;

 public:

   explicit AudioSetupToolBar( AudacityProject &project );
   virtual ~AudioSetupToolBar();

   static AudioSetupToolBar &Get( AudacityProject &project );
   static const AudioSetupToolBar &Get( const AudacityProject &project );

   void Create(wxWindow * parent) override;

   void UpdatePrefs() override;
   void UpdateSelectedPrefs( int ) override;

   void DeinitChildren();
   void Populate() override;
   void Repaint(wxDC* dc) override;
   void EnableDisableButtons() override;
   void ReCreateButtons() override;
   void OnFocus(wxFocusEvent &event);
   void OnAudioSetup(wxCommandEvent &event);

 private:
   void OnRescannedDevices(DeviceChangeMessage);
   void OnHost(int id);
   void OnInput(wxCommandEvent& event);
   void OnChannels(int id);
   void OnOutput(wxCommandEvent& event);
   void OnSettings(wxCommandEvent& event);
   void CommonMenuItemSteps(bool audioSettingsChosen);

   bool ChangeHost(int hostId);
   void ChangeDevice(int deviceId, bool isInput);
   void RepopulateMenus();
   void FillHosts();
   void FillHostDevices();
   void FillInputChannels();
   void SetDevices(const DeviceSourceMap *in, const DeviceSourceMap *out);
   void RegenerateTooltips() override;

   void MakeAudioSetupButton();
   void ArrangeButtons();

   void AppendSubMenu(wxMenu& menu, const std::unique_ptr<wxMenu>& submenu, const wxString& title);

   using Callback = void (AudioSetupToolBar::*)(int id);
   // Append submenu with one radio item group
   // Bind menu items to lambdas that invoke callback,
   // with successive ids from 0
   // Check the item with given index, or disable the submenu when that is < 0
   static void AppendSubMenu(AudioSetupToolBar &toolbar, wxMenu& menu,
      const wxArrayString &labels, int checkedItem,
      Callback callback, const wxString& title);

   static std::optional<wxString> GetSelectedRadioItemLabel(const wxMenu& menu);

   enum {
      ID_AUDIO_SETUP_BUTTON = 12000,
      BUTTON_COUNT,
   };

   AButton *mAudioSetup{};
   wxBoxSizer *mSizer{};

   class Choice {
   public:
      // id0 is the first in the sequence assigned to submenu items
      explicit Choice(int id0) : mId0{ id0 } {}
      void Clear() { mMenu = std::make_unique<wxMenu>(); }
      [[nodiscard]] bool Empty() const {
         return mMenu->GetMenuItemCount() == 0;
      }
      std::optional<wxString> Get() const {
         return GetSelectedRadioItemLabel(*mMenu);
      }
      wxString GetFirst() const {
         if (!Empty())
            return mMenu->FindItem(mId0)->GetItemLabelText();
         return {};
      }
      int GetSmallIntegerId() const {
         for (const auto & item : mMenu->GetMenuItems())
            if (item->IsChecked())
               return item->GetId() - mId0;
         return -1;
      }
      int Find(const wxString &name) const {
         return mMenu->FindItem(name);
      }
      bool Set(const wxString &name) {
         const auto id = mMenu->FindItem(name);
         if (id != wxNOT_FOUND) {
            mMenu->FindChildItem(id)->Check();
            return true;
         }
         return false;
      }
      void Set(wxArrayString &&names) {
         Clear();
         for (int i = 0; i < names.size(); ++i)
            mMenu->AppendRadioItem(mId0 + i, names[i]);
      }
      bool Set(int id) {
         auto item = mMenu->FindChildItem(id);
         if (!item)
            return false;
         item->Check();
         return true;
      }
      void AppendSubMenu(
         AudioSetupToolBar &toolBar, wxMenu &menu, const wxString &title);

   private:
      std::unique_ptr<wxMenu> mMenu{ std::make_unique<wxMenu>() };
      const int mId0;
   };

   class Choices {
   public:
      void Clear() { mStrings.Clear(); mIndex = -1; }
      [[nodiscard]] bool Empty() const { return mStrings.empty(); }
      std::optional<wxString> Get() const {
         if (mIndex < 0 || mIndex >= mStrings.size())
            return {};
         return { mStrings[mIndex] };
      }
      wxString GetFirst() const {
         if (!Empty())
            return mStrings[0];
         return {};
      }
      int GetSmallIntegerId() const {
         return mIndex;
      }
      int Find(const wxString &name) const {
         return make_iterator_range(mStrings).index(name);
      }
      bool Set(const wxString &name) {
         auto index = make_iterator_range(mStrings).index(name);
         if (index != -1) {
            mIndex = index;
            return true;
         }
         // else no state change
         return false;
      }
      void Set(wxArrayString &&names) {
         mStrings.swap(names);
         mIndex = mStrings.empty() ? -1 : 0;
      }
      // id is just a small-integer index into the string array
      bool Set(int id) {
         if (id < 0 || id >= mStrings.size())
            return false; // no change of state then
         mIndex = id;
         return true;
      }
      void AppendSubMenu(AudioSetupToolBar &toolBar,
         wxMenu &menu, Callback callback, const wxString &title);

   private:
      wxArrayStringEx mStrings;
      int mIndex{ -1 };
   };

   std::unique_ptr<wxMenu> mInput;
   std::unique_ptr<wxMenu> mOutput;
   Choices mInputChannels;
   Choices mHost;

   Observer::Subscription mSubscription;

 public:

   DECLARE_CLASS(AudioSetupToolBar)
   DECLARE_EVENT_TABLE()
};

#endif

