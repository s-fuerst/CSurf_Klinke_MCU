/**
* Copyright (C) 2009-2012 Steffen Fuerst 
* Distributed under the GNU GPL v2. For full terms see the file gplv2.txt.
*/

#pragma once

#include <src/juce_WithoutMacros.h> // includes everything in juce.h, but
#include "PlugMap.h"


//==============================================================================
/**
This class shows how to implement a TableListBoxModel to show in a TableListBox.
*/
class PlugModeVPOTTableComponent : 
        public Component,
        public TableListBoxModel
{
public:
        //==============================================================================
        PlugModeVPOTTableComponent(PMVPot::tSteps*);
        ~PlugModeVPOTTableComponent(void);
        //==============================================================================
        // This is overloaded from TableListBoxModel, and must return the total number of rows in our table
        int getNumRows();

        void updateEverything(){m_table->updateContent();}

        // This is overloaded from TableListBoxModel, and should fill in the background of the whole row
        void paintRowBackground (Graphics& g, int rowNumber, int width, int height, bool rowIsSelected)
        {
                if (rowNumber == m_lastChangedRow)
                        g.fillAll (Colours::lightblue);
        }

        void paintCell (Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected);

        void setLastChangedRow(int row){m_lastChangedRow = row;m_table->selectRow(row);}

        Component* refreshComponentForCell (int rowNumber, int columnId, bool isRowSelected, Component* existingComponentToUpdate);
        // This is overloaded from TableListBoxModel, and must paint any cells that aren't using custom
        // components.
//      void PlugModeVPOTTableComponent::paintCell (Graphics& g,
//              int rowNumber,
//              int columnId,
//              int width, int height,
//              bool rowIsSelected)
//      {
//              g.setColour (Colours::black);
//              g.setFont (font);
// 
//              const XmlElement* rowElement = dataList->getChildElement (rowNumber);
// 
//              if (rowElement != 0)
//              {
//                      const String text (rowElement->getStringAttribute (getAttributeNameForColumnId (columnId)));
// 
//                      g.drawText (text, 2, 0, width - 4, height, Justification::centredLeft, true);
//              }
// 
//              g.setColour (Colours::black.withAlpha (0.2f));
//              g.fillRect (width - 1, 0, 1, height);
//      }



        //==============================================================================

private:
        PMVPot::tSteps* m_pStepMap;

        TableListBox* m_table;    // the table component itself

        int m_lastChangedRow;
};

//==============================================================================
// This is a custom component containing a combo box, which we're going to put inside
// our table's "rating" column.
class TableLabel : public Component, LabelListener
{
public:
        TableLabel(PlugModeVPOTTableComponent& owner, PMVPot::tSteps* pMap);

        ~TableLabel()
        {
                deleteAllChildren();
        }

        void resized()
        {
                m_label->setBoundsInset (BorderSize (2));
        }

        void setRowAndColumn (const int newRow, const int newColumn);

        void labelTextChanged (Label* labelThatHasChanged);

        Label* getLabel() {
                return m_label;
        }

private:
        PlugModeVPOTTableComponent& m_owner;
        Label* m_label;
        int m_row, m_columnId;
        PMVPot::tSteps* m_pMap;
};
