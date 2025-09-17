#include "settings_manager.h"


SettingsManager& SettingsManager::instance() {
    static SettingsManager instance;
    return instance;
}

SettingsManager::SettingsManager() {
    load();
}

void SettingsManager::load() {
    QSettings s;

    showGrid = s.value("display/showGrid", true).toBool();
    gridSpacing = s.value("display/gridSpacing", 25).toInt();

    showTransitionText = s.value("display/showTransitionText", true).toBool();

    inspectorMode = s.value("display/inspectorMode", false).toBool();
    printMode = s.value("display/printMode", false).toBool();
    snapMode = s.value("display/snapMode", false).toBool();
    polylineMode = s.value("display/polylineMode", false).toBool();

    selectionColor = QColor(s.value("display/selectionColor", QColor(Qt::darkGray).name()).toString());
    selectionBorderWidth = s.value("display/selectionBorderWidth", 2).toInt();
    selectionInvertText = s.value("display/selectionInvertText", false).toBool();
}

void SettingsManager::loadDefaults()
{
    setShowGrid(true);
    setGridSpacing(25);

    setShowTransitionText(true);

    setInspectorMode(false);
    setPrintMode(false);
    setSnapMode(false);
    setPolylineMode(false);

    setSelectionColor(QColor(Qt::red));
    setSelectionBorderWidth(2);
    setSelectionInvertText(false);
}

void SettingsManager::setShowGrid(bool value)
{
    if (showGrid != value) {
        showGrid = value;
        QSettings().setValue("display/showGrid", value);
        emit gridSettingsChanged();
    }
}

void SettingsManager::setGridSpacing(double value)
{
    if (gridSpacing != value) {
        gridSpacing = value;
        QSettings().setValue("display/gridSpacing", value);
        emit gridSettingsChanged();
    }
}

void SettingsManager::setShowTransitionText(bool value) {
    if (showTransitionText != value) {
        showTransitionText = value;
        QSettings().setValue("display/showTransitionText", value);
        emit showTransitionTextChanged(value);
    }
}

void SettingsManager::setInspectorMode(bool value) {
    if (inspectorMode != value) {
        inspectorMode = value;
        QSettings().setValue("display/inspectorMode", value);
        emit inspectorModeChanged(value);
    }
}

void SettingsManager::setPrintMode(bool value) {
    if (printMode != value) {
        printMode = value;
        QSettings().setValue("display/printMode", value);
        emit printModeChanged(value);
    }
}

void SettingsManager::setSnapMode(bool value)
{
    if (snapMode != value) {
        snapMode = value;
        QSettings().setValue("display/snapMode", value);
        emit snapModeChanged(value);
    }
}

void SettingsManager::setPolylineMode(bool value)
{
    if (polylineMode != value) {
        polylineMode = value;
        QSettings().setValue("display/polylineMode", value);
        emit PolylineModeChanged(value);
    }
}

void SettingsManager::setSelectionColor(QColor value)
{
    if (selectionColor != value) {
        selectionColor = value;
        QSettings().setValue("display/selectionColor", value);
        emit selectionSettingsChanged();
    }
}

void SettingsManager::setSelectionBorderWidth(int value)
{
    if (selectionBorderWidth != value) {
        selectionBorderWidth = value;
        QSettings().setValue("display/selectionBorderWidth", value);
        emit selectionSettingsChanged();
    }
}

void SettingsManager::setSelectionInvertText(bool value)
{
    if (selectionInvertText != value) {
        selectionInvertText = value;
        QSettings().setValue("display/selectionInvertText", value);
        emit selectionSettingsChanged();
    }
}
