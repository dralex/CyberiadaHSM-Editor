#ifndef CYBERIADASMEDITORVERTEXITEM_H
#define CYBERIADASMEDITORVERTEXITEM_H

#include "cyberiadasm_editor_items.h"

/* -----------------------------------------------------------------------------
 * Vertex Item
 * ----------------------------------------------------------------------------- */

class CyberiadaSMEditorVertexItem: public CyberiadaSMEditorAbstractItem {
public:
    CyberiadaSMEditorVertexItem(CyberiadaSMModel* model,
                                Cyberiada::Element* element,
                                QGraphicsItem* parent = NULL);

    virtual int type() const { return VertexItem; }

    // virtual QRectF boundingRect() const;
    // virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QRectF fullCircle() const;
    QRectF partialCircle() const;
};


#endif // CYBERIADASMEDITORVERTEXITEM_H
