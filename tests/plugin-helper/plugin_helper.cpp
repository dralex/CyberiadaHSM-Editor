/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
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
