/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "ChannelStripFileDialogs.h"
#include "KlinkeLookAndFeel.h"

// Shared helpers: both dialogs use the same button styling and the same
// right-aligned Delete / Ok-Load / Cancel row.

// The default LookAndFeel_V4 keyboard-focus emphasis only boosts the
// saturation of the button colour — invisible on the near-grey Ok/Cancel
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

// Shared layout metrics (400x300 content, both dialogs):
static const int kDialogW = 400;
static const int kDialogH = 300;
static const int kMargin = 12;
static const int kBtnW = 64;
static const int kBtnH = 26;

// ===== ChannelStripSaveDialog (name entry, PlugMap-style) =====

ChannelStripSaveDialog::ChannelStripSaveDialog(const String &title,
                                               const String &defaultName,
                                               const StringArray &existingFiles,
                                               std::function<bool(const String &)> onOk,
                                               std::function<bool(const String &)> onDelete)
    : m_label(NULL), m_status(NULL), m_name(NULL), m_files(NULL), m_ok(NULL),
      m_cancel(NULL), m_delete(NULL), m_existing(existingFiles),
      m_onOk(std::move(onOk)), m_onDelete(std::move(onDelete)) {
  addAndMakeVisible(m_label = new Label(String(), String()));
  m_label->setText(title, dontSendNotification);
  m_label->setFont(Font(Font::getDefaultSansSerifFontName(), 13.0f, Font::plain));
  m_label->setJustificationType(Justification::centredLeft);
  m_label->setEditable(false, false, false);
  m_label->setColour(Label::textColourId, Colours::black);
  m_label->setColour(Label::backgroundColourId, Colours::white);

  addAndMakeVisible(m_name = new TextEditor());
  // JUCE 8 gotcha: the text colour is captured into the editor's text model
  // at setText() time (TextEditorModel stores it per paragraph), so the
  // explicit colours MUST be set BEFORE the first setText() — otherwise the
  // default LookAndFeel's colours are baked in (white-on-white).
  m_name->setColour(TextEditor::textColourId, Colours::black);
  m_name->setColour(TextEditor::backgroundColourId, Colours::white);
  m_name->setColour(TextEditor::outlineColourId, Colours::darkgrey);
  m_name->setColour(TextEditor::highlightedTextColourId, Colours::white);
  m_name->setColour(TextEditor::highlightColourId, Colour(0xff4f6f9f));
  m_name->setFont(Font(Font::getDefaultSansSerifFontName(), 13.0f, Font::plain));
  m_name->setText(defaultName, dontSendNotification);
  // select the whole default so the user can just start typing
  m_name->selectAll();

  addAndMakeVisible(m_files = new ListBox("Existing"));
  m_files->setModel(this);
  m_files->setRowHeight(20);
  m_files->setColour(ListBox::outlineColourId, Colours::grey);
  m_files->setOutlineThickness(1);
  m_files->setColour(ListBox::backgroundColourId, Colours::white);
  m_files->updateContent();

  addAndMakeVisible(m_status = new Label(String(), String()));
  m_status->setFont(Font(Font::getDefaultSansSerifFontName(), 12.0f, Font::plain));
  m_status->setJustificationType(Justification::centredLeft);
  m_status->setEditable(false, false, false);
  m_status->setColour(Label::textColourId, Colours::red);
  m_status->setColour(Label::backgroundColourId, Colours::white);

  // the delete button is greyed out (disabled) while no list entry is
  // selected (toggled in listBoxItemClicked; JUCE 8 has no ListBox
  // selection listener, and clicking is the selection mechanism here)
  addAndMakeVisible(m_delete = makeDeleteButton());
  m_delete->setEnabled(false);
  m_delete->addListener(this);
  addAndMakeVisible(m_ok = new FocusTextButton("Ok"));
  addAndMakeVisible(m_cancel = new FocusTextButton("Cancel"));
  styleButton(m_ok);
  styleButton(m_cancel);
  m_ok->addListener(this);
  m_cancel->addListener(this);
  setSize(kDialogW, kDialogH);
}

void ChannelStripSaveDialog::resized() {
  if (m_label) m_label->setBounds(kMargin, 10, getWidth() - 2 * kMargin, 20);
  if (m_name)
    m_name->setBounds(kMargin, 36, getWidth() - 2 * kMargin, 24);
  // list gets the remaining space between the name field and the status line
  // (status line top at getHeight() - 70)
  if (m_files)
    m_files->setBounds(kMargin, 66, getWidth() - 2 * kMargin,
                       getHeight() - 66 - 74);
  if (m_status)
    m_status->setBounds(kMargin, getHeight() - 70, getWidth() - 2 * kMargin, 24);
  if (m_delete)
    m_delete->setBounds(getWidth() - 220, getHeight() - 34, kBtnW, kBtnH);
  if (m_ok) m_ok->setBounds(getWidth() - 148, getHeight() - 34, kBtnW, kBtnH);
  if (m_cancel)
    m_cancel->setBounds(getWidth() - 76, getHeight() - 34, kBtnW, kBtnH);
}

void ChannelStripSaveDialog::paintListBoxItem(int row, Graphics &g, int w,
                                              int h, bool sel) {
  if (row < 0 || row >= m_existing.size())
    return;
  g.fillAll(sel ? Colours::lightblue : Colours::white);
  g.setColour(Colours::black);
  g.setFont(Font(Font::getDefaultSansSerifFontName(), 13.0f, Font::plain));
  // display name without the .xml extension (the stored name keeps it)
  g.drawText(File(m_existing[row]).getFileNameWithoutExtension(), 4, 1, w - 8,
             h, Justification::centredLeft, true);
}

void ChannelStripSaveDialog::listBoxItemClicked(int row,
                                                const MouseEvent &) {
  // pre-fill the name field with the clicked file's name (without .xml)
  if (row < 0 || row >= m_existing.size() || !m_name)
    return;
  m_name->setText(
      File(m_existing[row]).getFileNameWithoutExtension(), dontSendNotification);
  m_name->selectAll();
  if (m_delete) m_delete->setEnabled(true); // an entry is selected now
}

void ChannelStripSaveDialog::buttonClicked(Button *button) {
  if (button == m_cancel) {
    getParentComponent()->exitModalState(0);
    return;
  }
  if (button == m_delete) {
    const int index = m_files ? m_files->getSelectedRow() : -1;
    if (index < 0 || index >= m_existing.size())
      return; // nothing selected — stay open
    const String file = m_existing[index];
    // A short synchronous AlertWindow is safe here (tested in this plugin,
    // see AGENTS.md); only the DialogWindow::runModalLoop pattern deadlocks.
    const MessageBoxOptions confirm = MessageBoxOptions()
                                          .withIconType(AlertWindow::QuestionIcon)
                                          .withTitle("Delete file?")
                                          .withMessage("Really delete \"" +
                                                       File(file).getFileName() +
                                                       "\"?\n"
                                                       "This cannot be undone.")
                                          .withButton("Yes")
                                          .withButton("No");
    // JUCE 8: button index 0 ("Yes") returns 1, button 1 ("No") returns 0
    if (AlertWindow::show(confirm) != 1 || !m_onDelete)
      return; // answered No
    if (!m_onDelete(file)) {
      m_status->setText("Could not delete the selected file.",
                        dontSendNotification);
      return;
    }
    m_existing.remove(m_existing.indexOf(file));
    m_files->updateContent();
    if (m_delete) m_delete->setEnabled(false); // selection is gone now
    return;
  }
  if (button != m_ok)
    return;
  const String name = chosenName();
  if (name.isEmpty())
    return; // stay open
  const bool ok = m_onOk ? m_onOk(name) : true;
  if (ok) {
    getParentComponent()->exitModalState(1);
  } else {
    m_status->setText("Could not save - check the name and try again.",
                      dontSendNotification);
  }
}

// ===== ChannelStripLoadDialog (file list) =====

ChannelStripLoadDialog::ChannelStripLoadDialog(const String &title,
                                               const StringArray &files,
                                               std::function<bool(const String &)> onOk,
                                               std::function<bool(const String &)> onDelete)
    : m_files(files), m_status(NULL), m_list(NULL), m_ok(NULL), m_cancel(NULL),
      m_delete(NULL), m_onOk(std::move(onOk)), m_onDelete(std::move(onDelete)) {
  addAndMakeVisible(m_list = new ListBox(title));
  m_list->setModel(this);
  m_list->setRowHeight(20);
  m_list->setColour(ListBox::outlineColourId, Colours::grey);
  m_list->setOutlineThickness(1);
  m_list->setColour(ListBox::backgroundColourId, Colours::white);
  m_list->updateContent();

  addAndMakeVisible(m_status = new Label(String(), String()));
  m_status->setFont(Font(Font::getDefaultSansSerifFontName(), 12.0f, Font::plain));
  m_status->setJustificationType(Justification::centredLeft);
  m_status->setEditable(false, false, false);
  m_status->setColour(Label::textColourId, Colours::red);
  m_status->setColour(Label::backgroundColourId, Colours::white);

  // the delete button is greyed out (disabled) while no list entry is
  // selected (toggled in listBoxItemClicked; JUCE 8 has no ListBox
  // selection listener, and clicking is the selection mechanism here)
  addAndMakeVisible(m_delete = makeDeleteButton());
  m_delete->setEnabled(false);
  m_delete->addListener(this);
  addAndMakeVisible(m_ok = new FocusTextButton("Load"));
  addAndMakeVisible(m_cancel = new FocusTextButton("Cancel"));
  styleButton(m_ok);
  styleButton(m_cancel);
  m_ok->addListener(this);
  m_cancel->addListener(this);
  setSize(kDialogW, kDialogH);
}

void ChannelStripLoadDialog::resized() {
  if (m_list)
    m_list->setBounds(kMargin, 10, getWidth() - 2 * kMargin,
                      getHeight() - 88);
  if (m_status)
    m_status->setBounds(kMargin, getHeight() - 70, getWidth() - 2 * kMargin, 24);
  if (m_delete)
    m_delete->setBounds(getWidth() - 220, getHeight() - 34, kBtnW, kBtnH);
  if (m_ok) m_ok->setBounds(getWidth() - 148, getHeight() - 34, kBtnW, kBtnH);
  if (m_cancel)
    m_cancel->setBounds(getWidth() - 76, getHeight() - 34, kBtnW, kBtnH);
}

void ChannelStripLoadDialog::paintListBoxItem(int row, Graphics &g, int w,
                                              int h, bool sel) {
  if (row < 0 || row >= m_files.size())
    return;
  g.fillAll(sel ? Colours::lightblue : Colours::white);
  g.setColour(Colours::black);
  g.setFont(Font(Font::getDefaultSansSerifFontName(), 13.0f, Font::plain));
  // display name without the .xml extension (the stored name keeps it)
  g.drawText(File(m_files[row]).getFileNameWithoutExtension(), 4, 1, w - 8, h,
             Justification::centredLeft, true);
}

void ChannelStripLoadDialog::listBoxItemClicked(int row, const MouseEvent &e) {
  // default model behavior = select the row, then show the Delete button
  ListBoxModel::listBoxItemClicked(row, e);
  if (m_delete && row >= 0 && row < m_files.size())
    m_delete->setEnabled(true);
}

void ChannelStripLoadDialog::buttonClicked(Button *button) {
  if (button == m_cancel) {
    getParentComponent()->exitModalState(0);
    return;
  }
  if (button == m_delete) {
    const String file = chosenFile();
    if (file.isEmpty())
      return; // nothing selected — stay open
    // A short synchronous AlertWindow is safe here (tested in this plugin,
    // see AGENTS.md); only the DialogWindow::runModalLoop pattern deadlocks.
    const MessageBoxOptions confirm = MessageBoxOptions()
                                          .withIconType(AlertWindow::QuestionIcon)
                                          .withTitle("Delete file?")
                                          .withMessage("Really delete \"" +
                                                       File(file).getFileName() +
                                                       "\"?\n"
                                                       "This cannot be undone.")
                                          .withButton("Yes")
                                          .withButton("No");
    // JUCE 8: button index 0 ("Yes") returns 1, button 1 ("No") returns 0
    if (AlertWindow::show(confirm) != 1 || !m_onDelete)
      return; // answered No
    if (!m_onDelete(file)) {
      m_status->setText("Could not delete the selected file.",
                        dontSendNotification);
      return;
    }
    m_files.remove(m_files.indexOf(file));
    m_list->updateContent();
    if (m_delete) m_delete->setEnabled(false); // selection is gone now
    return;
  }
  if (button != m_ok)
    return;
  const String file = chosenFile();
  if (file.isEmpty())
    return; // stay open
  const bool ok = m_onOk ? m_onOk(file) : true;
  if (ok) {
    getParentComponent()->exitModalState(1);
  } else {
    m_status->setText("Could not load the selected file.", dontSendNotification);
  }
}

// ===== Async launch helpers (launchAsync pattern) =====

void openStripSaveDialog(const String &title, const String &defaultName,
                         const StringArray &existingFiles,
                         std::function<bool(const String &)> onOk,
                         std::function<bool(const String &)> onDelete) {
  auto *content =
      new ChannelStripSaveDialog(title, defaultName, existingFiles,
                                 std::move(onOk), std::move(onDelete));
  static KlinkeLookAndFeel klf;
  content->setLookAndFeel(&klf);
  DialogWindow::LaunchOptions o;
  o.dialogTitle = title;
  o.content.setOwned(content);
  o.escapeKeyTriggersCloseButton = true;
  o.resizable = false;
  o.useNativeTitleBar = true;
  o.dialogBackgroundColour = Colours::white;
  o.launchAsync(); // the DialogWindow deletes itself when the modal ends
}

void openStripLoadDialog(const String &title, const StringArray &files,
                         std::function<bool(const String &)> onOk,
                         std::function<bool(const String &)> onDelete) {
  auto *content = new ChannelStripLoadDialog(
      title, files, std::move(onOk), std::move(onDelete));
  static KlinkeLookAndFeel klf;
  content->setLookAndFeel(&klf);
  DialogWindow::LaunchOptions o;
  o.dialogTitle = title;
  o.content.setOwned(content);
  o.escapeKeyTriggersCloseButton = true;
  o.resizable = false;
  o.useNativeTitleBar = true;
  o.dialogBackgroundColour = Colours::white;
  o.launchAsync(); // the DialogWindow deletes itself when the modal ends
}
