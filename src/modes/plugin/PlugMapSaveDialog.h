/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * Dialog for saving (and deleting) FX parameter maps, modeled on the
 * ChannelStrip file dialogs:
 *
 *   - the name entry sits on TOP, pre-filled with the plugin name (the
 *     part after ':' up to '(' of TrackFX_GetFXName, trimmed)
 *   - one ListBox each for the existing user maps and the installed maps
 *   - clicking a list entry pre-fills the name field
 *   - a right-aligned Delete / Save / Cancel button row at the bottom;
 *     Delete is greyed out while no list entry is selected and asks for
 *     confirmation before deleting (same pattern as ChannelStripSaveDialog)
 *
 * The dialog is still shown through the SaveDialogWindow/runModalLoop
 * mechanism (kept on purpose) and closes via exitModalState(SAVEMAP_OK /
 * SAVEMAP_CANCEL).
 */
#ifndef __PLUGMAPSAVEDIALOG__
#define __PLUGMAPSAVEDIALOG__

#include "JuceHeader.h"
#include "PlugMapManager.h"
#include "../../ui/KlinkeLookAndFeel.h"

#define SAVEMAP_OK 11
#define SAVEMAP_CANCEL 12

class PlugMapManager;
class PlugMapSaveDialog;

class SaveDialogWindow : public DialogWindow {
public:
  SaveDialogWindow(const String &name, const Colour &backgroundColour,
                   const bool escapeKeyTriggersCloseButton,
                   const bool addToDesktop)
      : DialogWindow(name, backgroundColour, escapeKeyTriggersCloseButton,
                     addToDesktop) {
    // same window chrome as the ChannelStrip file dialogs
    // (LaunchOptions::useNativeTitleBar = true): without a native title bar
    // JUCE paints its own, which shows as a black bar under SWELL/X11
    setUsingNativeTitleBar(true);
  }
  void closeButtonPressed() { setVisible(false); }
};

// One ListBox model per list (a single model cannot drive two ListBoxes
// with different row sets). Reports row clicks to the owning dialog.
class PlugMapListModel : public ListBoxModel {
public:
  explicit PlugMapListModel(PlugMapSaveDialog *owner) : m_owner(owner) {}

  void setFiles(StringArray files) { m_files = std::move(files); }
  String fileName(int row) const { return m_files[row]; }
  String displayName(int row) const {
    return File(m_files[row]).getFileNameWithoutExtension();
  }

  // ListBoxModel
  int getNumRows() override { return m_files.size(); }
  void paintListBoxItem(int row, Graphics &g, int w, int h, bool selected) override;
  void listBoxItemClicked(int row, const MouseEvent &event) override;

private:
  StringArray m_files;
  PlugMapSaveDialog *m_owner;
};

class PlugMapSaveDialog : public Component, public Button::Listener {
public:
  explicit PlugMapSaveDialog(PlugMapManager *pPMM);
  ~PlugMapSaveDialog() override { deleteAllChildren(); }

  // called by PlugMapListModel when a row is clicked
  void listEntryClicked(PlugMapListModel *which, int row);

  void paint(Graphics &g) override;
  void resized() override;

  // Button::Listener
  void buttonClicked(Button *buttonThatWasClicked) override;

private:
  // (prevent copy constructor and operator= being generated..)
  PlugMapSaveDialog(const PlugMapSaveDialog &);
  const PlugMapSaveDialog &operator=(const PlugMapSaveDialog &);

  void refreshLists();
  void refreshDeleteState();

  PlugMapManager *m_pPMM;
  Label *m_labelName;
  TextEditor *m_mapName;
  Label *m_labelUser;
  ListBox *m_mapList;
  PlugMapListModel *m_userModel;
  Label *m_labelInstalled;
  ListBox *m_mapInstalled;
  PlugMapListModel *m_installedModel;
  Label *m_status;
  TextButton *m_delete;
  TextButton *m_okButton;
  TextButton *m_cancelButton;

  // which list has the current selection (for Delete)
  PlugMapListModel *m_selectedModel;
  int m_selectedRow;
};

#endif // __PLUGMAPSAVEDIALOG__
