/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */
#include "ChannelStripComponent.h"
#include "ChannelStripMode.h"
#include "ChannelStripFileDialogs.h"
#include "McuDebugLog.h"

static const int kToolbarHeight = 30;

ChannelStripComponent::ChannelStripComponent(ChannelStripMode *pMode)
    : m_pMode(pMode), m_table(NULL), m_saveAllButton(NULL),
      m_loadAllButton(NULL) {
  addAndMakeVisible(m_table = new ChannelStripBindingTable(m_pMode));
  addAndMakeVisible(m_saveAllButton =
                        new TextButton("Save all 16..."));
  addAndMakeVisible(m_loadAllButton = new TextButton("Load all 16..."));
  m_saveAllButton->setTooltip(
      String("Store the complete set (all 16 slots) in a file of the Sets "
             "folder (name dialog)."));
  m_loadAllButton->setTooltip(
      String("REPLACE ALL 16 slots with the strips found in one of the existing "
             "set files (slots not present in the file become unassigned)."));
  m_saveAllButton->setColour(TextButton::buttonColourId, Colour(0xffe8e8e8));
  m_loadAllButton->setColour(TextButton::buttonColourId, Colour(0xffe8e8e8));
  m_saveAllButton->setColour(TextButton::buttonOnColourId, Colour(0xffc8c8c8));
  m_loadAllButton->setColour(TextButton::buttonOnColourId, Colour(0xffc8c8c8));
  m_saveAllButton->setColour(TextButton::textColourOffId, Colours::black);
  m_loadAllButton->setColour(TextButton::textColourOffId, Colours::black);
  m_saveAllButton->addListener(this);
  m_loadAllButton->addListener(this);
  // width: table columns (590 + 3 * 60) + margins
  setSize(820, 460);
  refreshFileButtonStates();
}

ChannelStripComponent::~ChannelStripComponent() { deleteAllChildren(); }

void ChannelStripComponent::resized() {
  const int m = 8;
  if (m_table)
    m_table->setBounds(m, m, getWidth() - 2 * m,
                       getHeight() - 2 * m - kToolbarHeight);
  if (m_saveAllButton)
    m_saveAllButton->setBounds(m, getHeight() - m - 26, 130, 26);
  if (m_loadAllButton)
    m_loadAllButton->setBounds(m + 138, getHeight() - m - 26, 130, 26);
}

void ChannelStripComponent::updateEverything() {
  if (m_table) {
    m_table->refreshInstalledFX();
    m_table->updateEverything();
  }
  refreshFileButtonStates();
}

void ChannelStripComponent::refreshFileButtonStates() {
  if (m_loadAllButton)
    m_loadAllButton->setEnabled(
        !ChannelStripMode::listXmlFiles(ChannelStripMode::setFilesDir())
             .isEmpty());
  if (m_table)
    m_table->refreshFileButtonStates();
}

void ChannelStripComponent::buttonClicked(Button *button) {
  if (!m_pMode)
    return;
  // complete sets live in the Sets/ folder (managed separately from the
  // single strips in Strips/)
  const File dir = ChannelStripMode::setFilesDir();
  if (button == m_saveAllButton) {
    openStripSaveDialog("Save all 16 channel strips", "channelstrips-all",
                        ChannelStripMode::listXmlFiles(dir),
                        [this, dir](const String &name) {
                          const File file =
                              ChannelStripMode::stripFileForName(name, dir);
                          if (!m_pMode->saveAllStripsToUserFile(file)) {
                            MCU_LOG("%s",
                                    ("CSM save all strips to " +
                                     file.getFullPathName() + " FAILED")
                                        .toRawUTF8());
                            return false;
                          }
                          // a new set file may now be loadable — un-grey the
                          // "Load all 16..." button (safe synchronously: the
                          // refresh only rebuilds the table's cell components,
                          // not these toolbar buttons)
                          refreshFileButtonStates();
                          return true;
                        },
                        [this, dir](const String &name) {
                          const File file = dir.getChildFile(name);
                          if (!file.deleteFile()) {
                            MCU_LOG("%s",
                                    ("CSM delete set file " +
                                     file.getFullPathName() + " FAILED")
                                        .toRawUTF8());
                            return false;
                          }
                          MCU_LOG("%s",
                                  ("CSM deleted set file " +
                                   file.getFullPathName()).toRawUTF8());
                          // one set file gone — "Load all 16..." may need to
                          // grey out (safe synchronously, see the save callback)
                          refreshFileButtonStates();
                          return true;
                        });
    return;
  }
  const StringArray files = ChannelStripMode::listXmlFiles(dir);
  openStripLoadDialog("Load all 16 channel strips", files,
                      [this, dir](const String &name) {
                        const File file = dir.getChildFile(name);
                        if (!m_pMode->loadAllStripsFromUserFile(file)) {
                          MCU_LOG("%s",
                                  ("CSM load all strips from " +
                                   file.getFullPathName() + " FAILED")
                                      .toRawUTF8());
                          return false;
                        }
                        return true;
                      },
                      [dir](const String &name) {
                        const File file = dir.getChildFile(name);
                        if (!file.deleteFile()) {
                          MCU_LOG("%s",
                                  ("CSM delete set file " +
                                   file.getFullPathName() + " FAILED")
                                      .toRawUTF8());
                          return false;
                        }
                        MCU_LOG("%s",
                                ("CSM deleted set file " +
                                 file.getFullPathName()).toRawUTF8());
                        return true;
                      });
}
