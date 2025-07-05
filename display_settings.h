#ifndef DISPLAY_SETTINGS_H
#define DISPLAY_SETTINGS_H

#include <QObject>

class DisplaySettings : public QObject {
    Q_OBJECT

public:
    explicit DisplaySettings(QObject* parent = nullptr);

    bool isInspectorMode() const;
    void setInspectorMode(bool enabled);

    bool isShowTransitionText() const;
    void setShowTransitionText(bool enabled);

    bool isPrintMode() const;
    void setPrintMode(bool enabled);

signals:
    void inspectorModeChanged(bool);
    void showTransitionTextChanged(bool);
    void printModeChanged(bool);

private:
    bool inspectorMode = false;
    bool showTransitionText = true;
    bool printMode = false;
};


#endif // DISPLAY_SETTINGS_H
