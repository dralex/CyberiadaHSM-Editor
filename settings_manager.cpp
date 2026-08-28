#include <QDir>

#include "settings_manager.h"


// the formal comment is drawn in the comment size, so it stores nothing
static FontRole storedFontRole(FontRole role)
{
    return role == fontRoleFormalComment ? fontRoleComment : role;
}

static const char* fontSizeKey(FontRole role)
{
    switch (storedFontRole(role)) {
    case fontRoleStateTitle:  return "display/stateTitleFontSize";
    case fontRoleStateAction: return "display/stateActionFontSize";
    case fontRoleTransition:  return "display/transitionFontSize";
    default:                  return "display/commentFontSize";
    }
}

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
    showServiceObjects = s.value("display/showServiceObjects", false).toBool();

    printMode = s.value("display/printMode", false).toBool();
    snapMode = s.value("display/snapMode", false).toBool();

    fontFamily = s.value("display/fontFamily", QString()).toString();
    for (int role = 0; role < fontRolesCount; role++) {
        fontSizes[role] = s.value(fontSizeKey(FontRole(role)), FONT_SIZE).toInt();
    }

    lastDirectory = s.value("files/lastDirectory", QDir::currentPath()).toString();
    // the Qt default leaves too little room for the file view
    dialogSize = s.value("files/dialogSize", QSize(900, 600)).toSize();

    selectionColor = QColor(s.value("display/selectionColor", QColor(Qt::darkGray).name()).toString());
    selectionBorderWidth = s.value("display/selectionBorderWidth", 2).toInt();
    selectionInvertText = s.value("display/selectionInvertText", false).toBool();
}

void SettingsManager::loadDefaults()
{
    setShowGrid(true);
    setGridSpacing(25);

    setShowTransitionText(true);
    setShowServiceObjects(false);

    setPrintMode(false);
    setSnapMode(false);

    setFontFamily(QString());
    for (int role = 0; role < fontRolesCount; role++) {
        setFontSize(FontRole(role), FONT_SIZE);
    }

    setSelectionColor(QColor(Qt::red));
    setSelectionBorderWidth(2);
    setSelectionInvertText(false);
}

void SettingsManager::setFontFamily(const QString& value)
{
    if (fontFamily != value) {
        fontFamily = value;
        QSettings().setValue("display/fontFamily", value);
        emit fontSettingsChanged();
    }
}

int SettingsManager::getFontSize(FontRole role) const
{
    return fontSizes[storedFontRole(role)];
}

void SettingsManager::setFontSize(FontRole role, int value)
{
    if (value < FONT_SIZE_MIN || value > FONT_SIZE_MAX) return;
    FontRole stored = storedFontRole(role);
    if (fontSizes[stored] != value) {
        fontSizes[stored] = value;
        QSettings().setValue(fontSizeKey(stored), value);
        emit fontSettingsChanged();
    }
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

void SettingsManager::setShowServiceObjects(bool value) {
    if (showServiceObjects != value) {
        showServiceObjects = value;
        QSettings().setValue("display/showServiceObjects", value);
        emit serviceObjectsChanged(value);
    }
}

void SettingsManager::setInspectorMode(bool value) {
    if (inspectorMode != value) {
        inspectorMode = value;
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

void SettingsManager::setLastDirectory(const QString& value)
{
    if (lastDirectory != value) {
        lastDirectory = value;
        QSettings().setValue("files/lastDirectory", value);
    }
}

void SettingsManager::setDialogSize(const QSize& value)
{
    if (dialogSize != value && value.isValid()) {
        dialogSize = value;
        QSettings().setValue("files/dialogSize", value);
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
