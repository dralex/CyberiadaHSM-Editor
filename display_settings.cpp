#include "display_settings.h"

DisplaySettings::DisplaySettings(QObject* parent) : QObject(parent) {}

bool DisplaySettings::isInspectorMode() const {
    return inspectorMode;
}

void DisplaySettings::setInspectorMode(bool enabled) {
    if (inspectorMode != enabled) {
        inspectorMode = enabled;
        emit inspectorModeChanged(enabled);
    }
}

bool DisplaySettings::isShowTransitionText() const {
    return showTransitionText;
}

void DisplaySettings::setShowTransitionText(bool enabled) {
    if (showTransitionText != enabled) {
        showTransitionText = enabled;
        emit showTransitionTextChanged(enabled);
    }
}

bool DisplaySettings::isPrintMode() const {
    return printMode;
}

void DisplaySettings::setPrintMode(bool enabled) {
    if (printMode != enabled) {
        printMode = enabled;
        emit printModeChanged(enabled);
    }
}
