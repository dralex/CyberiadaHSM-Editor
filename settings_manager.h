#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <QObject>
#include <QSettings>
#include <QColor>

class SettingsManager : public QObject {
    Q_OBJECT

public:
    static SettingsManager& instance();

    void load();
    void loadDefaults();
    void save();

    bool getShowGrid() const { return showGrid; }
    void setShowGrid(bool value);
    double getGridSpacing() const { return gridSpacing; }
    void setGridSpacing(double value);

    bool getShowTransitionText() const { return showTransitionText; }
    void setShowTransitionText(bool value);

    bool getInspectorMode() const { return inspectorMode; }
    void setInspectorMode(bool value);
    bool getPrintMode() const { return printMode; }
    void setPrintMode(bool value);
    bool getSnapMode() const { return snapMode; }
    void setSnapMode(bool value);

    QColor getSelectionColor() const { return selectionColor; }
    void setSelectionColor(QColor value);
    int getSelectionBorderWidth() const { return selectionBorderWidth; }
    void setSelectionBorderWidth(int value);
    bool getSelectionInvertText() const { return selectionInvertText; }
    void setSelectionInvertText(bool value);

signals:
    void settingsChanged();

    void gridSettingsChanged();
    void showTransitionTextChanged(bool);
    void inspectorModeChanged(bool);
    void printModeChanged(bool);
    void snapModeChanged(bool);

    void selectionSettingsChanged();

private:
    SettingsManager();
    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;

    // grid
    bool showGrid;
    int gridSpacing;

    // visualisation
    bool showTransitionText;

    // modes
    bool inspectorMode;
    bool printMode;
    bool snapMode;

    // selection
    QColor selectionColor;
    int selectionBorderWidth;
    bool selectionInvertText;
};

#endif // SETTINGS_MANAGER_H
