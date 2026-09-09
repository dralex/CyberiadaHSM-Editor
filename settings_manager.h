#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <QObject>
#include <QSettings>
#include <QColor>
#include <QSize>
#include <QString>

#include "cyberiada_constants.h"

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

    // the auxiliary objects explaining the structure: the region borders and
    // the coordinate origins of the scene, the states and the regions
    bool getShowServiceObjects() const { return showServiceObjects; }
    void setShowServiceObjects(bool value);
    // the batch mode draws them without touching the stored preferences
    void overrideShowServiceObjects(bool value) { showServiceObjects = value; }
    // a runtime override, like the service objects one: no signal, not persisted
    void overrideShowGrid(bool value) { showGrid = value; }

    // runtime-only, never persisted: the batch mode hides all text elements
    // to keep the test output independent of the font metrics
    bool getShowText() const { return showText; }
    void setShowText(bool value) { showText = value; }

    // runtime-only, never persisted: the mode belongs to the open document,
    // every document is opened for the inspection first
    bool getInspectorMode() const { return inspectorMode; }
    void setInspectorMode(bool value);
    bool getPrintMode() const { return printMode; }
    void setPrintMode(bool value);
    bool getSnapMode() const { return snapMode; }
    void setSnapMode(bool value);

    // the shared family of the element text, empty means the bundled font
    QString getFontFamily() const { return fontFamily; }
    void setFontFamily(const QString& value);
    // the point size of the text of the role; the formal comment follows the
    // comment size, so it has no setting of its own
    int getFontSize(FontRole role) const;
    void setFontSize(FontRole role, int value);

    QString getLastDirectory() const { return lastDirectory; }
    void setLastDirectory(const QString& value);
    QSize getDialogSize() const { return dialogSize; }
    void setDialogSize(const QSize& value);

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
    void serviceObjectsChanged(bool);
    void inspectorModeChanged(bool);
    void printModeChanged(bool);
    void snapModeChanged(bool);

    void fontSettingsChanged();

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
    bool showServiceObjects;
    bool showText = true;

    // modes
    bool inspectorMode = false;
    bool printMode;
    bool snapMode;

    // text
    QString fontFamily;
    int fontSizes[fontRolesCount];

    // files
    QString lastDirectory;
    QSize dialogSize;

    // selection
    QColor selectionColor;
    int selectionBorderWidth;
    bool selectionInvertText;
};

#endif // SETTINGS_MANAGER_H
