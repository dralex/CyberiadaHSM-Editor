#ifndef CYBERIADASM_EDITOR_SM_ITEM_H
#define CYBERIADASM_EDITOR_SM_ITEM_H

#include "cyberiadasm_editor_items.h"

/* -----------------------------------------------------------------------------
 * State Machine Item
 * ----------------------------------------------------------------------------- */

class CyberiadaSMEditorSMItem: public CyberiadaSMEditorAbstractItem {
public:
    CyberiadaSMEditorSMItem(CyberiadaSMModel* model,
                            Cyberiada::Element* element,
                            QGraphicsItem* parent = NULL);

    virtual int type() const { return SMItem; }

    // virtual QRectF boundingRect() const;
    QRectF boundingRect() const override;

    // virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
};


#endif // CYBERIADASM_EDITOR_SM_ITEM_H
