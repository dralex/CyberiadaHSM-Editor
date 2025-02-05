#ifndef FONTMANAGER_H
#define FONTMANAGER_H

#include <QFont>
#include <QObject>

class FontManager : public QObject {
    Q_OBJECT

public:
    FontManager(FontManager &other) = delete;

    void operator=(const FontManager &) = delete;

    static FontManager& instance() {
        static FontManager instance;
        return instance;
    }

    QFont getFont() const {
        return currentFont;
    }

    void setFont(const QFont &font) {
        if (currentFont != font) {
            currentFont = font;
            emit fontChanged(currentFont);
        }
    }

signals:
    void fontChanged(const QFont &newFont);

private:
    FontManager() = default;
    QFont currentFont;
};
#endif // FONTMANAGER_H
