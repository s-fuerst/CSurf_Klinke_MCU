/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "JuceHeader.h"
#include "PlugMapSaveDialog.h"

// Shared button styling, copied from ChannelStripFileDialogs.cpp:
// the default LookAndFeel_V4 keyboard-focus emphasis only boosts the
// saturation of the button colour - invisible on the near-grey Save/Cancel
// buttons (which is why tab focus was hard to see). Render keyboard focus
// exactly like the mouse-hover state instead (the darker highlight).
class FocusTextButton : public TextButton {
public:
  explicit FocusTextButton(const String &text) : TextButton(text) {}
  void paintButton(Graphics &g, bool highlighted, bool down) override {
    TextButton::paintButton(g, highlighted || hasKeyboardFocus(true), down);
  }
};

static void styleButton(TextButton *b) {
  b->setColour(TextButton::buttonColourId, Colour(0xffe8e8e8));
  b->setColour(TextButton::buttonOnColourId, Colour(0xffc8c8c8));
  b->setColour(TextButton::textColourOffId, Colours::black);
}

static TextButton *makeDeleteButton() {
  auto *b = new FocusTextButton("Delete");
  b->setColour(TextButton::buttonColourId, Colour(0xfff4dada));
  b->setColour(TextButton::buttonOnColourId, Colour(0xffe4b8b8));
  b->setColour(TextButton::textColourOffId, Colours::black);
  return b;
}

// ===== PlugMapListModel =====

void PlugMapListModel::paintListBoxItem(int row, Graphics &g, int w, int h,
                                        bool selected) {
  if (row < 0 || row >= m_files.size())
    return;
  g.fillAll(selected ? Colours::lightblue : Colours::white);
  g.setColour(Colours::black);
  g.setFont(Font(FontOptions(Font::getDefaultSansSerifFontName(), 13.0f,
                             Font::plain)));
  // display name without the .xml extension (the stored name keeps it)
  g.drawText(displayName(row), 4, 1, w - 8, h, Justification::centredLeft,
             true);
}

void PlugMapListModel::listBoxItemClicked(int row, const MouseEvent &event) {
  // default model behavior = select the row, then hand over to the dialog
  ListBoxModel::listBoxItemClicked(row, event);
  if (m_owner && row >= 0 && row < m_files.size())
    m_owner->listEntryClicked(this, row);
}

// ===== PlugMapSaveDialog =====

static String defaultMapNameFromPlugName(const String &plugName) {
  // TrackFX_GetFXName returns e.g. "VST: FabFilter Pro-L 3 (1, 2)"
  // -> "FabFilter Pro-L 3" (after ':', up to '(', trimmed)
  String name = plugName.fromFirstOccurrenceOf(":", false, false);
  const int paren = name.indexOf("(");
  if (paren >= 0)
    name = name.substring(0, paren);
  return name.trim();
}

PlugMapSaveDialog::PlugMapSaveDialog(PlugMapManager *pPMM)
    : Component(String("PlugMapSaveDialog")),
      m_pPMM(pPMM), m_labelName(NULL), m_mapName(NULL), m_labelUser(NULL),
      m_mapList(NULL), m_userModel(NULL), m_labelInstalled(NULL),
      m_mapInstalled(NULL), m_installedModel(NULL), m_status(NULL),
      m_delete(NULL), m_okButton(NULL), m_cancelButton(NULL),
      m_selectedModel(NULL), m_selectedRow(-1) {
  addAndMakeVisible(m_labelName =
                        new Label(String("Name"), String("Name of new Map:\n")));
  m_labelName->setFont(Font(FontOptions(15.0f, Font::plain)));
  m_labelName->setJustificationType(Justification::centredLeft);
  m_labelName->setEditable(false, false, false);
  m_labelName->setColour(Label::textColourId, Colours::black);
  m_labelName->setColour(Label::backgroundColourId, Colours::white);

  addAndMakeVisible(m_mapName = new TextEditor(String("Map Name")));
  m_mapName->setTooltip(String("Name of the map to save. It should be part of "
                               "the plugin name so the map is found "
                               "automatically when the plugin is selected."));
  // JUCE 8 gotcha: the text colour is captured into the editor's text model
  // at setText() time - set the explicit colours BEFORE the first setText()
  m_mapName->setColour(TextEditor::textColourId, Colours::black);
  m_mapName->setColour(TextEditor::backgroundColourId, Colours::white);
  m_mapName->setColour(TextEditor::outlineColourId, Colours::darkgrey);
  m_mapName->setColour(TextEditor::highlightedTextColourId, Colours::white);
  m_mapName->setColour(TextEditor::highlightColourId, Colour(0xff4f6f9f));
  m_mapName->setFont(Font(FontOptions(Font::getDefaultSansSerifFontName(),
                                      13.0f, Font::plain)));
  // pre-fill with the plugin name (after ':', up to '(', trimmed)
  m_mapName->setText(defaultMapNameFromPlugName(m_pPMM->getActivePlugName()),
                     dontSendNotification);
  // select the whole default so the user can just start typing
  m_mapName->selectAll();

  addAndMakeVisible(m_labelUser = new Label(
      String("Existing"), String("Existing User-Maps:\n")));
  m_labelUser->setFont(Font(FontOptions(15.0f, Font::plain)));
  m_labelUser->setJustificationType(Justification::centredLeft);
  m_labelUser->setEditable(false, false, false);
  m_labelUser->setColour(Label::textColourId, Colours::black);
  m_labelUser->setColour(Label::backgroundColourId, Colours::white);

  m_userModel = new PlugMapListModel(this);
  addAndMakeVisible(m_mapList = new ListBox(String("Map List")));
  m_mapList->setModel(m_userModel);
  m_mapList->setRowHeight(20);
  m_mapList->setColour(ListBox::outlineColourId, Colours::grey);
  m_mapList->setOutlineThickness(1);
  m_mapList->setColour(ListBox::backgroundColourId, Colours::white);
  // derive the real path from getUserMapsLocation() so the tooltip always
  // matches the platform (My Documents on Windows, ~/.config/REAPER on
  // Linux/macOS)
  m_mapList->setTooltip(String("User maps are stored in the '") +
                        m_pPMM->getUserMapsLocation().getFullPathName() +
                        String("' directory. Click an entry to use its "
                               "name."));

  addAndMakeVisible(m_labelInstalled =
                        new Label(String("Installed"),
                                  String("Installed Maps:\n")));
  m_labelInstalled->setFont(Font(FontOptions(15.0f, Font::plain)));
  m_labelInstalled->setJustificationType(Justification::centredLeft);
  m_labelInstalled->setEditable(false, false, false);
  m_labelInstalled->setColour(Label::textColourId, Colours::black);
  m_labelInstalled->setColour(Label::backgroundColourId, Colours::white);

  m_installedModel = new PlugMapListModel(this);
  addAndMakeVisible(m_mapInstalled = new ListBox(String("Installed Map List")));
  m_mapInstalled->setModel(m_installedModel);
  m_mapInstalled->setRowHeight(20);
  m_mapInstalled->setColour(ListBox::outlineColourId, Colours::grey);
  m_mapInstalled->setOutlineThickness(1);
  m_mapInstalled->setColour(ListBox::backgroundColourId, Colours::white);
  m_mapInstalled->setTooltip(
      String("Maps installed with the plugin, found in the "
             "UserPlugins/MCU/PlugMaps/ directory."));

  addAndMakeVisible(m_status = new Label(String(), String()));
  m_status->setFont(Font(FontOptions(Font::getDefaultSansSerifFontName(),
                                     12.0f, Font::plain)));
  m_status->setJustificationType(Justification::centredLeft);
  m_status->setEditable(false, false, false);
  m_status->setColour(Label::textColourId, Colours::red);
  m_status->setColour(Label::backgroundColourId, Colours::white);

  // the delete button is greyed out (disabled) while no list entry is
  // selected (toggled in listEntryClicked)
  addAndMakeVisible(m_delete = makeDeleteButton());
  m_delete->setEnabled(false);
  m_delete->addListener(this);
  addAndMakeVisible(m_okButton = new FocusTextButton("Save"));
  addAndMakeVisible(m_cancelButton = new FocusTextButton("Cancel"));
  styleButton(m_okButton);
  styleButton(m_cancelButton);
  m_okButton->addListener(this);
  m_cancelButton->addListener(this);

  m_pPMM->rescanDirs();
  refreshLists();
  setSize(460, 660);
}

void PlugMapSaveDialog::paint(Graphics &g) { g.fillAll(Colours::white); }

void PlugMapSaveDialog::resized() {
  const int w = getWidth();
  const int h = getHeight();
  const int m = 12;
  const int listH = 230; // 11 rows at the 20px row height

  if (m_labelName)
    m_labelName->setBounds(m, 8, w - 2 * m, 20);
  if (m_mapName)
    m_mapName->setBounds(m, 30, w - 2 * m, 24);
  if (m_labelUser)
    m_labelUser->setBounds(m, 60, w - 2 * m, 20);
  if (m_mapList)
    m_mapList->setBounds(m, 82, w - 2 * m, listH);
  if (m_labelInstalled)
    m_labelInstalled->setBounds(m, 82 + listH + 8, w - 2 * m, 20);
  if (m_mapInstalled)
    m_mapInstalled->setBounds(m, 82 + listH + 8 + 22, w - 2 * m, listH);
  if (m_status)
    m_status->setBounds(m, h - 70, w - 2 * m, 24);
  // right-aligned button row (same metrics as the ChannelStrip dialogs)
  if (m_delete)
    m_delete->setBounds(w - 220, h - 34, 64, 26);
  if (m_okButton)
    m_okButton->setBounds(w - 148, h - 34, 64, 26);
  if (m_cancelButton)
    m_cancelButton->setBounds(w - 76, h - 34, 64, 26);
}

void PlugMapSaveDialog::refreshLists() {
  if (m_userModel) {
    StringArray files;
    m_pPMM->rescanDirs();
    const String user = m_pPMM->getUserMapsAsString();
    if (!user.isEmpty())
      files.addTokens(user.trim(), "\n");
    m_userModel->setFiles(files);
  }
  if (m_installedModel) {
    StringArray files;
    const String installed = m_pPMM->getInstalledMapsAsString();
    if (!installed.isEmpty())
      files.addTokens(installed.trim(), "\n");
    m_installedModel->setFiles(files);
  }
  m_mapList->updateContent();
  m_mapInstalled->updateContent();
}

void PlugMapSaveDialog::refreshDeleteState() {
  if (m_delete)
    m_delete->setEnabled(m_selectedModel != NULL && m_selectedRow >= 0);
}

void PlugMapSaveDialog::listEntryClicked(PlugMapListModel *which, int row) {
  m_selectedModel = which;
  m_selectedRow = row;
  // pre-fill the name field with the clicked map's name
  if (m_mapName) {
    m_mapName->setText(which->displayName(row), dontSendNotification);
    m_mapName->selectAll();
  }
  refreshDeleteState();
}

void PlugMapSaveDialog::buttonClicked(Button *buttonThatWasClicked) {
  if (buttonThatWasClicked == m_cancelButton) {
    getParentComponent()->exitModalState(SAVEMAP_CANCEL);
    return;
  }

  if (buttonThatWasClicked == m_delete) {
    if (!m_selectedModel || m_selectedRow < 0)
      return; // nothing selected - stay open
    const bool isUserList = (m_selectedModel == m_userModel);
    // list entries carry the name without extension; map files are always
    // .xml (scanDir only searches for *.xml)
    const String file = m_selectedModel->fileName(m_selectedRow) + String(".xml");
    // note: the currently active map may be deleted as well (no reload is
    // triggered) - it stays loaded in memory until the next plug is
    // selected; with autosave on it could be re-created on the next
    // parameter change
    // A short synchronous AlertWindow is safe here (tested in this plugin,
    // see AGENTS.md); only the DialogWindow::runModalLoop content pattern
    // of this dialog itself is the one kept on purpose.
    const MessageBoxOptions confirm =
        MessageBoxOptions()
            .withIconType(AlertWindow::QuestionIcon)
            .withTitle("Delete map?")
            .withMessage("Really delete \"" + file
                         + "\"?\nThis cannot be undone.")
            .withButton("Yes")
            .withButton("No");
    // JUCE 8: button index 0 ("Yes") returns 1, button 1 ("No") returns 0
    if (AlertWindow::show(confirm) != 1)
      return; // answered No
    if (!m_pPMM->deleteMapFile(isUserList, file)) {
      m_status->setText(
          "Could not delete the selected map.", dontSendNotification);
      return;
    }
    refreshLists();
    m_selectedModel = NULL;
    m_selectedRow = -1;
    refreshDeleteState();
    return;
  }

  if (buttonThatWasClicked != m_okButton)
    return;

  const String name = m_mapName->getText().trim();
  if (name.isEmpty()) {
    m_status->setText(
        "Please enter a name for the map.", dontSendNotification);
    return;
  }
  if (!m_pPMM->isValidAdditionalName(name)) {
    m_status->setText("The name '" + name + "' collides with the existing "
                      "map '" + m_pPMM->nameIsInvalidBecauseOf() + "'.",
                      dontSendNotification);
    return;
  }
  if (!m_pPMM->isMapNameInPlugName(name)) {
    const MessageBoxOptions nameCheck =
        MessageBoxOptions()
            .withIconType(MessageBoxIconType::WarningIcon)
            .withTitle("Map name check")
            .withMessage(String("The map name '" + name
                                + "' is not contained in the plugin name '"
                                + m_pPMM->getActivePlugName()
                                + "'. A map whose name is part of the plugin "
                                  "name can be found automatically when the "
                                  "plugin is selected. Save it anyway?"))
            .withButton("Yes")
            .withButton("No");
    if (AlertWindow::show(nameCheck) == 0) // "No" - keep the dialog open
      return;
  }
  if (m_pPMM->writeMapFile(name))
    getParentComponent()->exitModalState(SAVEMAP_OK);
  else
    m_status->setText(
        "Could not save the map.", dontSendNotification);
}
