/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 *
 * WP-A: composite Display spanning N units. IS-A Display so modes hold it
 * unchanged. changeField(row, globalField 1..N*8) routes to the owning
 * unit's child Display. Rows 2-3 (ProX 2nd panel) are silently dropped on
 * non-ProX children.
 *
 *   RULE: DisplayHandler::switchTo(<MultiDisplay>) is INVALID.
 *   sendDifferences() early-returns unless pDisplay == m_pActualDisplay, and
 *   a handler's m_pActualDisplay must always be a real per-unit child.
 *   Switching goes through switchToAll() so each child is activated on its
 *   OWN handler.
 *
 * Stub in Step 1; fleshed out in Step 7.
 */
#ifndef MCU_MULTI_DISPLAY
#define MCU_MULTI_DISPLAY

#include "Display.h"
#include <vector>

class DisplayHandler;

class MultiDisplay : public Display {
public:
  // Constructs an empty composite. Children are added by the factory.
  // The base Display ctor needs a (throwaway) handler + numRows; the base
  // buffer is allocated but NEVER used for field content (every
  // buffer-touching virtual is overridden to delegate to children).
  MultiDisplay(DisplayHandler *pDH, int numRows);
  virtual ~MultiDisplay();

  // child is NOT owned by the composite (it lives on its unit's handler)
  void addChild(Display *child);
  std::vector<Display *> &children() { return m_children; }

  // ---- overrides: everything that touches the buffer delegates to children
  void changeText(int row, int pos, const char *text, int pad,
                  bool centered = false) override;
  void changeTextFullLine(int row, const char *text,
                          bool centered = false) override;
  void changeField(int row, int field, const char *text,
                   bool centered = false) override;
  void clearLine(int row) override;
  void activate() override;
  void clear() override;
  void resendRow(int iRow) override;
  void resendAllRows() override;

  // switch each child on its OWN handler so m_pActualDisplay == child
  void switchToAll();

  // Broadcast a field to every child unconditionally (e.g. ProX master
  // field 9). Unlike changeField(), which routes per-unit by global
  // field number, this sends the same field to ALL children.
  void broadcastField(int row, int field, const char *text,
                      bool centered = false);

  // Returns the child Display whose owning unit is the main unit, or NULL
  // if there is none. The master/assignment column (field 9) belongs to
  // the main unit, but children are ordered by channel position, so the
  // main unit can sit at any index.
  Display *mainChild();

private:
  std::vector<Display *> m_children;
};

#endif
