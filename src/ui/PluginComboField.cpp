/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "PluginComboField.h"

static const InstalledFx &emptySentinel() {
  static const InstalledFx s = {String("empty (no plugin)"), String()};
  return s;
}

PluginComboField::PluginComboField(PluginComboFieldOwner &o)
    : owner(o), m_editor(NULL), m_row(0), m_column(0) {
  addAndMakeVisible(m_editor = new TextEditor());
  m_editor->setFont(
      Font(FontOptions(Font::getDefaultSansSerifFontName(), 13.0f, Font::plain)));
  // Explicit colours: inside a TableListBox the window's KlinkeLookAndFeel
  // defaults are not resolved for this editor, and the default text colour is
  // white -> the picked plugin name was invisible. Same pattern as every
  // other editable cell in the codebase.
  m_editor->setColour(TextEditor::textColourId, Colours::black);
  m_editor->setColour(TextEditor::backgroundColourId, Colours::white);
  m_editor->setTextToShowWhenEmpty("type plugin name here", Colours::lightgrey);
  m_editor->addListener(this);
}

PluginComboField::~PluginComboField() {
  hidePopup();
  deleteAllChildren();
}

void PluginComboField::resized() {
  if (m_editor)
    m_editor->setBoundsInset(BorderSize(1));
}

void PluginComboField::setRowAndColumn(int r, int c,
                                       const String &displayText) {
  m_row = r;
  m_column = c;
  if (!m_editor)
    return;
  m_editor->setText(displayText, dontSendNotification);
  m_lastValidText = displayText;
  // Defer layout + repaint to after the table has positioned this cell.
  // During refreshComponentForCell the cell bounds may still be 0, so sizing
  // the TextEditor synchronously leaves it blank — most visible when the
  // editor is re-opened (the cell components are recreated then).
  SafePointer<PluginComboField> sp(this);
  MessageManager::callAsync([sp]() {
    if (!sp)
      return;
    sp->resized();
    if (sp->m_editor)
      sp->m_editor->repaint();
  });
}

String PluginComboField::getText() const {
  return m_editor ? m_editor->getText() : String();
}

void PluginComboField::applyFilter(const String &text) {
  String f = text.trim();
  m_filtered.clear();
  m_filtered.push_back(emptySentinel());
  for (const auto &item : owner.installedFxList()) {
    if (f.isEmpty() || item.name.containsIgnoreCase(f))
      m_filtered.push_back(item);
  }
  if (m_popupList) {
    m_popupList->updateContent();
    int h = jlimit(4, 12, (int)m_filtered.size()) * 22 + 4;
    m_popupList->setSize(m_popupList->getWidth(), h);
  }
}

void PluginComboField::showPopup() {
  if (m_popupList || !m_editor)
    return;
  applyFilter(m_editor->getText());
  if (m_filtered.empty())
    return;
  ListBox *lb = new ListBox();
  lb->setModel(this);
  lb->setRowHeight(22);
  lb->setColour(ListBox::outlineColourId, Colours::grey);
  lb->setOutlineThickness(1);
  int width = jmax(m_editor->getWidth(), 280);
  int h = jlimit(4, 12, (int)m_filtered.size()) * 22 + 4;
  lb->setSize(width, h);
  Component *top = getTopLevelComponent();
  Point<int> rel = m_editor->getScreenPosition() - top->getScreenPosition();
  lb->setTopLeftPosition(rel.x, rel.y + m_editor->getHeight());
  top->addAndMakeVisible(lb);
  lb->toFront(false);
  m_popupList = lb;
}

void PluginComboField::hidePopup() {
  if (m_popupList)
    delete m_popupList.getComponent();
}

void PluginComboField::pickFiltered(int filteredRow) {
  if (filteredRow < 0 || filteredRow >= (int)m_filtered.size())
    return;
  const InstalledFx picked = m_filtered[filteredRow];
  hidePopup();
  // The empty ident of the sentinel (row 0) clears the assignment.
  String displayText = picked.ident.isEmpty() ? String() : picked.name;
  if (m_editor)
    m_editor->setText(displayText, dontSendNotification);
  m_lastValidText = displayText;
  owner.pluginPicked(m_row, m_column, picked.ident, displayText);
  // The owner's refresh may DESTROY this field (TableListBox::setModel) —
  // it runs deferred and is guarded by a SafePointer there.
  SafePointer<PluginComboField> sp(this);
  int r = m_row;
  int c = m_column;
  MessageManager::callAsync([sp, r, c]() {
    if (!sp)
      return;
    sp->owner.pluginPickFinished(r, c);
  });
}

void PluginComboField::textEditorTextChanged(TextEditor &) {
  if (!m_popupList)
    showPopup();
  else
    applyFilter(m_editor->getText());
}

void PluginComboField::textEditorFocusLost(TextEditor &) {
  SafePointer<PluginComboField> sp(this);
  MessageManager::callAsync([sp]() {
    if (!sp || !sp->m_popupList)
      return;
    Component *focused = Component::getCurrentlyFocusedComponent();
    if (focused && (focused == sp->m_popupList ||
                    sp->m_popupList->isParentOf(focused)))
      return;
    sp->hidePopup();
  });
}

void PluginComboField::textEditorReturnKeyPressed(TextEditor &) {
  // pick the first real match, never the clear sentinel
  for (int i = 1; i < (int)m_filtered.size(); i++)
    if (!m_filtered[i].ident.isEmpty()) {
      pickFiltered(i);
      return;
    }
  pickFiltered(0); // only the sentinel -> clear
}

void PluginComboField::textEditorEscapeKeyPressed(TextEditor &) {
  hidePopup();
  if (m_editor)
    m_editor->setText(m_lastValidText, dontSendNotification);
}

int PluginComboField::getNumRows() { return (int)m_filtered.size(); }

void PluginComboField::paintListBoxItem(int r, Graphics &g, int w, int h,
                                        bool sel) {
  if (r < 0 || r >= (int)m_filtered.size())
    return;
  g.fillAll(sel ? Colours::lightblue : Colours::white);
  g.setColour(Colours::black);
  g.setFont(Font(FontOptions(Font::getDefaultSansSerifFontName(), 13.0f,
                             Font::plain)));
  g.drawText(m_filtered[r].name, 6, 0, w - 8, h, Justification::centredLeft,
             true);
}

void PluginComboField::listBoxItemClicked(int r, const MouseEvent &) {
  pickFiltered(r);
}
