#ifndef CYBERIADASM_EDITOR_CHOICE_ITEM_H
#define CYBERIADASM_EDITOR_CHOICE_ITEM_H

#include "cyberiadasm_editor_items.h"

/* -----------------------------------------------------------------------------
 * Choice Item
 * ----------------------------------------------------------------------------- */

class CyberiadaSMEditorChoiceItem: public CyberiadaSMEditorAbstractItem {
public:
    CyberiadaSMEditorChoiceItem(CyberiadaSMModel* model,
                                Cyberiada::Element* element,
                                QGraphicsItem* parent = NULL);

    virtual int type() const { return ChoiceItem; }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
};


#endif // CYBERIADASM_EDITOR_CHOICE_ITEM_H
