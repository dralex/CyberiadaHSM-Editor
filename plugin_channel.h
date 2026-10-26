/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Plugin Channel: JSON-RPC 2.0 over the lines of a session plugin
 *
 * Copyright (C) 2026 Alexey Fedoseev <aleksey@fedoseev.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see https://www.gnu.org/licenses/
 *
 * ----------------------------------------------------------------------------- */

#ifndef PLUGIN_CHANNEL_H
#define PLUGIN_CHANNEL_H

#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QString>

// one compact JSON object per line (EDIT-PLUG-38): a line that is not a JSON
// object is text for the output tab; the requests of the editor get increasing ids
class PluginChannel : public QObject {
    Q_OBJECT

public:
    explicit PluginChannel(QObject* parent = nullptr);

    // a line of the plugin's stdout, without the line end
    void        receive(const QString& line);
    // the id of the request, for the reply
    int         request(const QString& method, const QJsonObject& params = QJsonObject());
    void        notify(const QString& method, const QJsonObject& params = QJsonObject());
    void        reply(const QJsonValue& id, const QJsonValue& result);
    void        replyError(const QJsonValue& id, int code, const QString& message);

    // the compact line of a message, with the newline
    static QByteArray encode(const QJsonObject& message);

signals:
    // a line for the plugin's stdin; the message for the records
    void        send(const QByteArray& line, const QJsonObject& message);
    void        text(const QString& line);
    // the params as sent: an object, or the array of a diagnostics message
    void        requestReceived(const QJsonValue& id, const QString& method, const QJsonValue& params);
    void        notificationReceived(const QString& method, const QJsonValue& params);
    // a reply to a request of the editor: the result, or the error object
    void        replyReceived(int id, const QJsonValue& result, const QJsonObject& error);

private:
    void        write(QJsonObject message);

    int         nextId = 1;
};

#endif
