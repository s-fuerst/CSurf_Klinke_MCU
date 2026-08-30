/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "ChannelStripFileDialogs.h"
#include "KlinkeLookAndFeel.h"

// ===== ChannelStripSaveDialog (name entry, PlugMap-style) =====

ChannelStripSaveDialog::ChannelStripSaveDialog(const String &title,
                                               const String &defaultName,
                                               const StringArray &existingFiles,
                                               std::function<bool(const String &)> onOk)
    : m_label(NULL), m_status(NULL), m_name(NULL), m_files(NULL), m_ok(NULL),
      m_cancel(NULL), m_existing(existingFiles), m_onOk(std::move(onOk)) {
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

  addAndMakeVisible(m_ok = new TextButton("Ok"));
  addAndMakeVisible(m_cancel = new TextButton("Cancel"));
  m_ok->setColour(TextButton::buttonColourId, Colour(0xffe8e8e8));
  m_ok->setColour(TextButton::buttonOnColourId, Colour(0xffc8c8c8));
  m_ok->setColour(TextButton::textColourOffId, Colours::black);
  m_cancel->setColour(TextButton::buttonColourId, Colour(0xffe8e8e8));
  m_cancel->setColour(TextButton::buttonOnColourId, Colour(0xffc8c8c8));
  m_cancel->setColour(TextButton::textColourOffId, Colours::black);
  m_ok->addListener(this);
  m_cancel->addListener(this);
  setSize(380, 286);
}

void ChannelStripSaveDialog::resized() {
  if (m_label) m_label->setBounds(12, 10, getWidth() - 24, 20);
  if (m_name) m_name->setBounds(12, 36, getWidth() - 24, 24);
  if (m_files) m_files->setBounds(12, 66, getWidth() - 24, 150);
  if (m_status)
    m_status->setBounds(12, 222, getWidth() - 24, 18);
  if (m_ok) m_ok->setBounds(getWidth() / 2 - 76, 244, 64, 26);
  if (m_cancel) m_cancel->setBounds(getWidth() / 2 + 12, 244, 64, 26);
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
}

void ChannelStripSaveDialog::buttonClicked(Button *button) {
  if (button == m_cancel) {
    getParentComponent()->exitModalState(0);
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
                                               const String &note,
                                               std::function<bool(int)> onOk)
    : m_files(files), m_status(NULL), m_list(NULL), m_ok(NULL), m_cancel(NULL),
      m_onOk(std::move(onOk)) {
  addAndMakeVisible(m_list = new ListBox(title));
  m_list->setModel(this);
  m_list->setRowHeight(20);
  m_list->setColour(ListBox::outlineColourId, Colours::grey);
  m_list->setOutlineThickness(1);
  m_list->setColour(ListBox::backgroundColourId, Colours::white);
  m_list->updateContent();

  addAndMakeVisible(m_status = new Label(String(), note));
  m_status->setFont(Font(Font::getDefaultSansSerifFontName(), 12.0f, Font::plain));
  m_status->setJustificationType(Justification::centredLeft);
  m_status->setEditable(false, false, false);
  m_status->setColour(Label::textColourId, Colours::red);
  m_status->setColour(Label::backgroundColourId, Colours::white);

  addAndMakeVisible(m_ok = new TextButton("Load"));
  addAndMakeVisible(m_cancel = new TextButton("Cancel"));
  m_ok->setColour(TextButton::buttonColourId, Colour(0xffe8e8e8));
  m_ok->setColour(TextButton::buttonOnColourId, Colour(0xffc8c8c8));
  m_ok->setColour(TextButton::textColourOffId, Colours::black);
  m_cancel->setColour(TextButton::buttonColourId, Colour(0xffe8e8e8));
  m_cancel->setColour(TextButton::buttonOnColourId, Colour(0xffc8c8c8));
  m_cancel->setColour(TextButton::textColourOffId, Colours::black);
  m_ok->addListener(this);
  m_cancel->addListener(this);
  setSize(400, 300);
}

void ChannelStripLoadDialog::resized() {
  if (m_list)
    m_list->setBounds(12, 10, getWidth() - 24, getHeight() - 88);
  if (m_status)
    m_status->setBounds(12, getHeight() - 70, getWidth() - 24, 24);
  if (m_ok) m_ok->setBounds(getWidth() - 148, getHeight() - 34, 64, 26);
  if (m_cancel) m_cancel->setBounds(getWidth() - 76, getHeight() - 34, 64, 26);
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

void ChannelStripLoadDialog::buttonClicked(Button *button) {
  if (button == m_cancel) {
    getParentComponent()->exitModalState(0);
    return;
  }
  if (button != m_ok)
    return;
  const int index = chosenIndex();
  if (index < 0)
    return; // stay open
  const bool ok = m_onOk ? m_onOk(index) : true;
  if (ok) {
    getParentComponent()->exitModalState(1);
  } else {
    m_status->setText("Could not load the selected file.", dontSendNotification);
  }
}

// ===== Async launch helpers (launchAsync pattern) =====

void openStripSaveDialog(const String &title, const String &defaultName,
                         const StringArray &existingFiles,
                         std::function<bool(const String &)> onOk) {
  auto *content =
      new ChannelStripSaveDialog(title, defaultName, existingFiles,
                                 std::move(onOk));
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
                         const String &note, std::function<bool(int)> onOk) {
  auto *content = new ChannelStripLoadDialog(title, files, note, std::move(onOk));
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
