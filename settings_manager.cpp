#include <QCoreApplication>
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

bool SettingsManager::adoptSettings(QSettings& legacy, QSettings& current)
{
    if (!current.allKeys().isEmpty()) return false;
    const QStringList keys = legacy.allKeys();
    if (keys.isEmpty()) return false;
    for (const QString& key: keys) {
        current.setValue(key, legacy.value(key));
    }
    current.sync();
    return true;
}

// CYBERIADA_PRO_CONFIG_HOME moves the preferences into an ini file under that folder,
// on every system (no variable moves the registry of Windows); applied as the
// application object is created, before any setting is read
static void applyConfigHome()
{
    const QString home = qEnvironmentVariable(CONFIG_HOME_VARIABLE);
    if (home.isEmpty()) return;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, home);
}
Q_COREAPP_STARTUP_FUNCTION(applyConfigHome)

bool SettingsManager::adoptLegacySettings(const QString& legacyApplication)
{
    // the store of the preferences, the ini file of CYBERIADA_PRO_CONFIG_HOME included
    QSettings legacy(QSettings::defaultFormat(), QSettings::UserScope,
                     QCoreApplication::organizationName(), legacyApplication);
    QSettings current;
    return adoptSettings(legacy, current);
}

bool snapActive(Qt::KeyboardModifiers mods)
{
    SettingsManager& sm = SettingsManager::instance();
    // holding Alt suspends snapping; the gesture carries the modifier, so a
    // scripted alt suspends it too (EDIT-TOOL-9)
    return sm.getSnapMode() && sm.getGridSpacing() > 0.0 && !(mods & Qt::AltModifier);
}

qreal snapCoord(qreal v)
{
    double g = SettingsManager::instance().getGridSpacing();
    return qRound(v / g) * g;
}

QPointF snapToGrid(const QPointF& p, Qt::KeyboardModifiers mods)
{
    if (!snapActive(mods)) return p;
    return QPointF(snapCoord(p.x()), snapCoord(p.y()));
}

SettingsManager::SettingsManager() {
    load();
}

void SettingsManager::load() {
    QSettings s;

    showGrid = s.value("display/showGrid", true).toBool();
    gridSpacing = s.value("display/gridSpacing", GRID_SPACING_DEFAULT).toInt();

    showTransitionText = s.value("display/showTransitionText", true).toBool();
    showStructureTree = s.value("display/showStructureTree", true).toBool();
    showProperties = s.value("display/showProperties", true).toBool();
    showServiceObjects = s.value("display/showServiceObjects", false).toBool();
    showComponentComments = s.value("display/showComponentComments", false).toBool();
    loggingEnabled = s.value("logging/enabled", false).toBool();

    printMode = s.value("display/printMode", false).toBool();
    snapMode = s.value("display/snapMode", false).toBool();

    interfaceLanguage = s.value("interface/language", QString()).toString();

    fontFamily = s.value("display/fontFamily", QString()).toString();
    for (int role = 0; role < fontRolesCount; role++) {
        fontSizes[role] = s.value(fontSizeKey(FontRole(role)), FONT_SIZE).toInt();
    }
    boldActionPrefix = s.value("display/boldActionPrefix", true).toBool();
    boldEventName = s.value("display/boldEventName", true).toBool();
    boldTransitionEventName = s.value("display/boldTransitionEventName", true).toBool();
    highlightCode = s.value("display/highlightCode", true).toBool();
    highlightInExports = s.value("export/highlightCode", false).toBool();

    lastDirectory = s.value("files/lastDirectory", QDir::currentPath()).toString();
    recentFiles = s.value("files/recentFiles").toStringList();
    // the Qt default leaves too little room for the file view
    dialogSize = s.value("files/dialogSize", QSize(1000, 680)).toSize();
    optionsExpanded = s.value("files/optionsExpanded", true).toBool();
    exportDpi = s.value("export/dpi", 96).toInt();
    exportFontFamily = s.value("export/fontFamily", QString()).toString();

    selectionColor = QColor(s.value("display/selectionColor", QColor(Qt::darkGray).name()).toString());
    selectionBorderWidth = s.value("display/selectionBorderWidth", 2).toInt();
    selectionInvertText = s.value("display/selectionInvertText", false).toBool();
}

void SettingsManager::loadDefaults()
{
    setShowGrid(true);
    setGridSpacing(GRID_SPACING_DEFAULT);

    setShowTransitionText(true);
    setShowStructureTree(true);
    setShowProperties(true);
    setShowServiceObjects(false);
    setShowComponentComments(false);
    setLoggingEnabled(false);

    setPrintMode(false);
    setSnapMode(false);
    setInterfaceLanguage(QString());

    setFontFamily(QString());
    for (int role = 0; role < fontRolesCount; role++) {
        setFontSize(FontRole(role), FONT_SIZE);
    }
    setBoldActionPrefix(true);
    setBoldEventName(true);
    setBoldTransitionEventName(true);
    setHighlightCode(true);
    setHighlightInExports(false);

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

void SettingsManager::setBoldActionPrefix(bool value)
{
    if (boldActionPrefix != value) {
        boldActionPrefix = value;
        QSettings().setValue("display/boldActionPrefix", value);
        emit fontSettingsChanged();
    }
}

void SettingsManager::setBoldEventName(bool value)
{
    if (boldEventName != value) {
        boldEventName = value;
        QSettings().setValue("display/boldEventName", value);
        emit fontSettingsChanged();
    }
}

void SettingsManager::setBoldTransitionEventName(bool value)
{
    if (boldTransitionEventName != value) {
        boldTransitionEventName = value;
        QSettings().setValue("display/boldTransitionEventName", value);
        emit fontSettingsChanged();
    }
}

void SettingsManager::setHighlightCode(bool value)
{
    if (highlightCode != value) {
        highlightCode = value;
        QSettings().setValue("display/highlightCode", value);
        emit highlightSettingsChanged();
    }
}

void SettingsManager::setHighlightInExports(bool value)
{
    if (highlightInExports != value) {
        highlightInExports = value;
        QSettings().setValue("export/highlightCode", value);
        emit highlightSettingsChanged();
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

void SettingsManager::setShowStructureTree(bool value) {
    if (showStructureTree != value) {
        showStructureTree = value;
        QSettings().setValue("display/showStructureTree", value);
        emit structureTreeChanged(value);
    }
}

void SettingsManager::setShowProperties(bool value) {
    if (showProperties != value) {
        showProperties = value;
        QSettings().setValue("display/showProperties", value);
        emit propertiesChanged(value);
    }
}

void SettingsManager::setShowServiceObjects(bool value) {
    if (showServiceObjects != value) {
        showServiceObjects = value;
        QSettings().setValue("display/showServiceObjects", value);
        emit serviceObjectsChanged(value);
    }
}

void SettingsManager::setShowComponentComments(bool value) {
    if (showComponentComments != value) {
        showComponentComments = value;
        QSettings().setValue("display/showComponentComments", value);
        emit componentCommentsChanged(value);
    }
}

void SettingsManager::setLoggingEnabled(bool value) {
    if (loggingEnabled != value) {
        loggingEnabled = value;
        QSettings().setValue("logging/enabled", value);
        emit loggingChanged(value);
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

void SettingsManager::setInterfaceLanguage(const QString& value)
{
    if (interfaceLanguage != value) {
        interfaceLanguage = value;
        QSettings().setValue("interface/language", value);
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

void SettingsManager::addRecentFile(const QString& path)
{
    if (path.isEmpty()) return;
    recentFiles.removeAll(path);      // move an existing entry to the front
    recentFiles.prepend(path);
    const int maxRecent = 10;
    while (recentFiles.size() > maxRecent) recentFiles.removeLast();
    QSettings().setValue("files/recentFiles", recentFiles);
    emit recentFilesChanged();
}

void SettingsManager::clearRecentFiles()
{
    if (recentFiles.isEmpty()) return;
    recentFiles.clear();
    QSettings().setValue("files/recentFiles", recentFiles);
    emit recentFilesChanged();
}

void SettingsManager::setDialogSize(const QSize& value)
{
    if (dialogSize != value && value.isValid()) {
        dialogSize = value;
        QSettings().setValue("files/dialogSize", value);
    }
}

void SettingsManager::setOptionsExpanded(bool value)
{
    if (optionsExpanded != value) {
        optionsExpanded = value;
        QSettings().setValue("files/optionsExpanded", value);
    }
}

void SettingsManager::setExportDpi(int value)
{
    if (exportDpi != value) {
        exportDpi = value;
        QSettings().setValue("export/dpi", value);
    }
}

void SettingsManager::setExportFontFamily(const QString& value)
{
    if (exportFontFamily != value) {
        exportFontFamily = value;
        QSettings().setValue("export/fontFamily", value);
    }
}

void SettingsManager::overrideFontFamily(const QString& value)
{
    // no persistence: set the in-memory family and let the text items re-apply
    fontFamily = value;
    emit fontSettingsChanged();
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

static QString pluginKey(const QString& plugin, const QString& rest)
{
    return QString("plugins/%1/%2").arg(plugin, rest);
}

QString SettingsManager::getPluginValue(const QString& plugin, const QString& parameter,
                                        bool* found) const
{
    QSettings s;
    QString key = pluginKey(plugin, "values/" + parameter);
    if (found) *found = s.contains(key);
    return s.value(key).toString();
}

void SettingsManager::setPluginValue(const QString& plugin, const QString& parameter,
                                     const QString& value)
{
    QSettings().setValue(pluginKey(plugin, "values/" + parameter), value);
}

QString SettingsManager::getPluginDocumentValue(const QString& plugin, const QString& document,
                                                const QString& parameter, bool* found) const
{
    QSettings s;
    QString key = pluginKey(plugin, QString("documents/%1/%2").arg(document, parameter));
    if (found) *found = s.contains(key);
    return s.value(key).toString();
}

void SettingsManager::setPluginDocumentValue(const QString& plugin, const QString& document,
                                             const QString& parameter, const QString& value)
{
    QSettings().setValue(pluginKey(plugin, QString("documents/%1/%2").arg(document, parameter)), value);
}

bool SettingsManager::getPluginEnabled(const QString& plugin) const
{
    return QSettings().value(pluginKey(plugin, "enabled"), true).toBool();
}

void SettingsManager::setPluginEnabled(const QString& plugin, bool value)
{
    if (getPluginEnabled(plugin) != value) {
        QSettings().setValue(pluginKey(plugin, "enabled"), value);
        emit pluginSwitchesChanged();
    }
}

bool SettingsManager::getPluginToolbarVisible(const QString& plugin) const
{
    return QSettings().value(pluginKey(plugin, "toolbar"), true).toBool();
}

void SettingsManager::setPluginToolbarVisible(const QString& plugin, bool value)
{
    QSettings().setValue(pluginKey(plugin, "toolbar"), value);
}

QMap<QString, QString> SettingsManager::getInterpreterPaths() const
{
    QSettings s;
    QMap<QString, QString> paths;
    s.beginGroup("plugin-interpreters");
    for (const QString& name: s.childKeys()) paths.insert(name, s.value(name).toString());
    s.endGroup();
    return paths;
}

void SettingsManager::setInterpreterPath(const QString& name, const QString& path)
{
    QSettings s;
    QString key = "plugin-interpreters/" + name;
    if (s.value(key).toString() == path) return;
    if (path.isEmpty()) s.remove(key);
    else s.setValue(key, path);
    emit pluginsChanged();
}
