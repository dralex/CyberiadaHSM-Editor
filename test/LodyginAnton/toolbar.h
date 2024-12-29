#ifndef TOOLBAR_H
#define TOOLBAR_H


#include <QVBoxLayout>


class ToolBar : public QVBoxLayout {
    Q_OBJECT
public:
    ToolBar(QWidget *parent = nullptr);
    // void addWidget(QWidget *widget);
};

#endif // TOOLBAR_H
