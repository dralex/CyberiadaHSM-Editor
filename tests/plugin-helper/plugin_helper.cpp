/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The plugin test helper: the example plugins run it (see docs/TESTING.md, Plugins)
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

// plugin-helper OP args... [-- OP args...]...
//
// The operations run in order; a failing one exits with HELPER_FAILED. The
// helper has no dependencies, so the plugin cases need no interpreter.

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#define HELPER_FAILED 3
#define PATH_BUFFER   4096

typedef std::vector<std::string> Args;

static bool fail(const std::string& message)
{
    std::cerr << "plugin-helper: " << message << std::endl;
    return false;
}

static bool readFile(const std::string& path, std::string* out)
{
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    *out = ss.str();
    return true;
}

static bool writeFile(const std::string& path, const std::string& data)
{
    std::ofstream f(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << data;
    return static_cast<bool>(f);
}

// \n in a written text is a newline
static std::string unescape(const std::string& text)
{
    std::string out;
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '\\' && i + 1 < text.size() && text[i + 1] == 'n') {
            out += '\n';
            i++;
        } else {
            out += text[i];
        }
    }
    return out;
}

static std::string join(const Args& args, size_t from)
{
    std::string s;
    for (size_t i = from; i < args.size(); i++) {
        if (i > from) s += " ";
        s += args[i];
    }
    return s;
}

static bool makeDir(const std::string& path)
{
    std::string partial;
    for (size_t i = 0; i <= path.size(); i++) {
        if (i == path.size() || ((path[i] == '/' || path[i] == '\\') && i > 0)) {
            partial = path.substr(0, i);
#ifdef _WIN32
            _mkdir(partial.c_str());
#else
            mkdir(partial.c_str(), 0755);
#endif
        }
    }
    std::ofstream probe((path + "/.probe").c_str());
    if (!probe) return false;
    probe.close();
    std::remove((path + "/.probe").c_str());
    return true;
}

// a field of a compact JSON line: "key":"value" as the text, "key":123 as the digits
static std::string field(const std::string& line, const std::string& key)
{
    std::string quoted = "\"" + key + "\":";
    size_t at = line.find(quoted);
    if (at == std::string::npos) return std::string();
    at += quoted.size();
    if (at < line.size() && line[at] == '"') {
        size_t end = line.find('"', at + 1);
        return end == std::string::npos ? std::string() : line.substr(at + 1, end - at - 1);
    }
    size_t end = line.find_first_not_of("-0123456789", at);
    return line.substr(at, end == std::string::npos ? std::string::npos : end - at);
}

static void emitLine(const std::string& line)
{
    std::cout << line << std::endl;
    std::cout.flush();
}

// {"a":1} with the jsonrpc member put first
static std::string message(const std::string& json)
{
    return "{\"jsonrpc\":\"2.0\"," + json.substr(json.find('{') + 1);
}

// session FILE: a session plugin answering the editor from FILE. A turn starts with
// "on KEY" (the method, control:<command> or reply); its lines run for the k-th
// message with the key, the last turn again for the later ones:
//   result JSON | error JSON   reply to the request
//   send JSON                  a message of the plugin
//   ask ID JSON                an ask request with the id and the params
//   print TEXT | eprint TEXT | sleep MS | exit N
static bool session(const std::string& path)
{
    std::string text;
    if (!readFile(path, &text)) return fail("cannot read " + path);
    std::vector<std::pair<std::string, std::vector<std::string> > > turns;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        size_t start = line.find_first_not_of(" \t\r");
        if (start == std::string::npos || line[start] == '#') continue;
        line = line.substr(start);
        if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
        if (line.compare(0, 3, "on ") == 0) {
            turns.push_back(std::make_pair(line.substr(3), std::vector<std::string>()));
        } else if (!turns.empty()) {
            turns.back().second.push_back(line);
        }
    }
    std::vector<std::pair<std::string, int> > seen;
    while (std::getline(std::cin, line)) {
        std::string method = field(line, "method");
        std::string key = method.empty() ? "reply" : method;
        if (method == "control") key += ":" + field(line, "command");
        std::string id = field(line, "id");
        int count = 0;
        for (size_t i = 0; i < seen.size(); i++) {
            if (seen[i].first == key) count = ++seen[i].second;
        }
        if (count == 0) seen.push_back(std::make_pair(key, count = 1));
        const std::vector<std::string>* actions = nullptr;
        int k = 0;
        for (size_t i = 0; i < turns.size(); i++) {
            if (turns[i].first != key) continue;
            actions = &turns[i].second;
            if (++k == count) break;
        }
        if (!actions) continue;
        for (const std::string& a: *actions) {
            size_t space = a.find(' ');
            std::string verb = a.substr(0, space);
            std::string rest = space == std::string::npos ? std::string() : a.substr(space + 1);
            if (verb == "result" || verb == "error") {
                emitLine("{\"jsonrpc\":\"2.0\",\"id\":" + id + ",\"" + verb + "\":" + rest + "}");
            } else if (verb == "send") {
                emitLine(message(rest));
            } else if (verb == "ask") {
                size_t at = rest.find(' ');
                emitLine("{\"jsonrpc\":\"2.0\",\"id\":" + rest.substr(0, at) +
                         ",\"method\":\"ask\",\"params\":" + rest.substr(at + 1) + "}");
            } else if (verb == "print") {
                emitLine(rest);
            } else if (verb == "eprint") {
                std::cerr << rest << std::endl;
            } else if (verb == "sleep") {
                std::this_thread::sleep_for(std::chrono::milliseconds(std::atoi(rest.c_str())));
            } else if (verb == "exit") {
                std::cout.flush();
                std::exit(std::atoi(rest.c_str()));
            } else {
                return fail("unknown session action '" + verb + "'");
            }
        }
    }
    // stdin closed: the session is over
    return true;
}

static bool run(const Args& op)
{
    const std::string& name = op[0];
    size_t n = op.size() - 1;
    if (name == "print") {
        std::cout << join(op, 1) << std::endl;
    } else if (name == "eprint") {
        std::cerr << join(op, 1) << std::endl;
    } else if (name == "args") {
        for (size_t i = 1; i < op.size(); i++) std::cout << "arg" << i << ": " << op[i] << std::endl;
    } else if (name == "cwd") {
        char buffer[PATH_BUFFER];
        if (!getcwd(buffer, sizeof(buffer))) return fail("cannot read the working directory");
        std::cout << "cwd: " << buffer << std::endl;
    } else if (name == "env" && (n == 1 || n == 2)) {
        // the value is never printed: a test checks it against the expected one
        const char* value = std::getenv(op[1].c_str());
        std::cout << op[1] << ": ";
        if (!value) std::cout << "unset";
        else if (n == 2) std::cout << (op[2] == value ? "ok" : "mismatch");
        else std::cout << "set (" << std::strlen(value) << " chars)";
        std::cout << std::endl;
    } else if (name == "cat" && n == 1) {
        std::string text;
        if (!readFile(op[1], &text)) return fail("cannot read " + op[1]);
        std::cout << text;
        if (!text.empty() && text[text.size() - 1] != '\n') std::cout << std::endl;
    } else if (name == "exists" && n == 1) {
        std::ifstream f(op[1].c_str());
        std::cout << "exists: " << (f ? "yes" : "no") << std::endl;
    } else if (name == "contains" && n == 2) {
        std::string text;
        if (!readFile(op[1], &text)) return fail("cannot read " + op[1]);
        std::cout << "contains: " << (text.find(op[2]) != std::string::npos ? "yes" : "no") << std::endl;
    } else if (name == "copy" && n == 2) {
        std::string text;
        if (!readFile(op[1], &text)) return fail("cannot read " + op[1]);
        if (!writeFile(op[2], text)) return fail("cannot write " + op[2]);
    } else if (name == "replace" && n == 3) {
        std::string text;
        if (!readFile(op[1], &text)) return fail("cannot read " + op[1]);
        size_t at = text.find(op[2]);
        if (at == std::string::npos) return fail("no '" + op[2] + "' to replace");
        text.replace(at, op[2].size(), op[3]);
        if (!writeFile(op[1], text)) return fail("cannot write " + op[1]);
    } else if (name == "write" && (n == 1 || n == 2)) {
        if (!writeFile(op[1], n == 2 ? unescape(op[2]) : std::string())) return fail("cannot write " + op[1]);
    } else if (name == "mkdir" && n == 1) {
        if (!makeDir(op[1])) return fail("cannot make " + op[1]);
    } else if (name == "sleep" && n == 1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(std::atoi(op[1].c_str())));
    } else if (name == "ignore-term" && n == 0) {
        // only the kill after the grace period stops it
        std::signal(SIGTERM, SIG_IGN);
    } else if (name == "exit" && n == 1) {
        std::cout.flush();
        std::exit(std::atoi(op[1].c_str()));
    } else if (name == "crash" && n == 0) {
        std::cout.flush();
        std::abort();
    } else if (name == "session" && n == 1) {
        return session(op[1]);
    } else if (name == "spam" && n == 1) {
        int lines = std::atoi(op[1].c_str());
        for (int i = 0; i < lines; i++) std::cout << "line " << i << std::endl;
    } else {
        return fail("unknown operation '" + name + "' with " + std::to_string(n) + " arguments");
    }
    return true;
}

int main(int argc, char** argv)
{
    std::vector<Args> ops(1);
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--") {
            ops.push_back(Args());
        } else {
            ops.back().push_back(argv[i]);
        }
    }
    for (const Args& op: ops) {
        if (op.empty()) continue;
        if (!run(op)) return HELPER_FAILED;
    }
    return 0;
}
