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

#include <QJsonDocument>

#include "plugin_channel.h"

#define JSONRPC_VERSION "2.0"

PluginChannel::PluginChannel(QObject* parent):
    QObject(parent)
{
}

QByteArray PluginChannel::encode(const QJsonObject& message)
{
    return QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n';
}

void PluginChannel::write(QJsonObject message)
{
    message.insert("jsonrpc", JSONRPC_VERSION);
    emit send(encode(message), message);
}

int PluginChannel::request(const QString& method, const QJsonObject& params)
{
    int id = nextId++;
    QJsonObject message;
    message.insert("id", id);
    message.insert("method", method);
    message.insert("params", params);
    write(message);
    return id;
}

void PluginChannel::notify(const QString& method, const QJsonObject& params)
{
    QJsonObject message;
    message.insert("method", method);
    message.insert("params", params);
    write(message);
}

void PluginChannel::reply(const QJsonValue& id, const QJsonValue& result)
{
    QJsonObject message;
    message.insert("id", id);
    message.insert("result", result);
    write(message);
}

void PluginChannel::replyError(const QJsonValue& id, int code, const QString& text)
{
    QJsonObject error;
    error.insert("code", code);
    error.insert("message", text);
    QJsonObject message;
    message.insert("id", id);
    message.insert("error", error);
    write(message);
}

void PluginChannel::receive(const QString& line)
{
    // a stray print of the plugin is text, not a broken session (EDIT-PLUG-38)
    QString trimmed = line.trimmed();
    QJsonDocument doc;
    if (trimmed.startsWith('{')) doc = QJsonDocument::fromJson(trimmed.toUtf8());
    if (!doc.isObject()) {
        emit text(line);
        return;
    }
    QJsonObject message = doc.object();
    QJsonValue method = message.value("method");
    QJsonValue params = message.value("params");
    if (method.isString()) {
        if (message.contains("id")) {
            emit requestReceived(message.value("id"), method.toString(), params);
        } else {
            emit notificationReceived(method.toString(), params);
        }
        return;
    }
    // a reply carries the id of a request of the editor, always a number here
    if (message.value("id").isDouble() && (message.contains("result") || message.contains("error"))) {
        emit replyReceived(message.value("id").toInt(), message.value("result"),
                           message.value("error").toObject());
        return;
    }
    emit text(line);
}
