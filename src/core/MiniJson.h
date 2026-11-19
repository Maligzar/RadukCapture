#pragma once
// ============================================================
//  MiniJson -- a minimal JSON reader, shared
//
//  Objects, arrays, strings (with the common escapes), numbers, true,
//  false and null. A malformed document parses to what it could read and
//  sets ok = false; nothing here throws. Written for Opale's reply
//  (phase 141.0, sky/MutualEvents.h) and moved here unchanged in 165.0
//  so the weather parsers (core/SkyWeather.h) read with the same code.
//  Pure: standard headers only.
// ============================================================

#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace raduk::json {

struct JVal {
    enum Kind { Null, Bool, Num, Str, Arr, Obj } kind = Null;
    bool b = false;
    double n = 0.0;
    std::string s;
    std::vector<JVal> a;
    std::vector<std::pair<std::string, JVal>> o;

    const JVal* Get(const char* key) const {
        if (kind != Obj) return nullptr;
        for (const auto& kv : o) if (kv.first == key) return &kv.second;
        return nullptr;
    }
    double NumOr(double fallback) const { return kind == Num ? n : fallback; }
};

class JsonReader {
public:
    explicit JsonReader(const std::string& text) : t(text) {}
    bool Parse(JVal& out) {
        ok = true;
        Value(out, 0);
        Ws();
        return ok;
    }
private:
    const std::string& t;
    size_t p = 0;
    bool ok = true;

    void Ws() { while (p < t.size() && strchr(" \t\r\n", t[p])) p++; }
    bool Lit(const char* w) {
        const size_t n = strlen(w);
        if (t.compare(p, n, w) == 0) { p += n; return true; }
        return false;
    }
    void Value(JVal& v, int depth) {
        if (depth > 64) { ok = false; return; }
        Ws();
        if (p >= t.size()) { ok = false; return; }
        const char c = t[p];
        if (c == '{') Object(v, depth);
        else if (c == '[') Array(v, depth);
        else if (c == '"') { v.kind = JVal::Str; String(v.s); }
        else if (Lit("true"))  { v.kind = JVal::Bool; v.b = true; }
        else if (Lit("false")) { v.kind = JVal::Bool; v.b = false; }
        else if (Lit("null"))  { v.kind = JVal::Null; }
        else {
            char* end = nullptr;
            v.n = strtod(t.c_str() + p, &end);
            if (end == t.c_str() + p) { ok = false; return; }
            v.kind = JVal::Num;
            p = (size_t)(end - t.c_str());
        }
    }
    void String(std::string& s) {
        p++;   // opening quote
        while (p < t.size() && t[p] != '"') {
            if (t[p] == '\\' && p + 1 < t.size()) {
                const char e = t[++p];
                switch (e) {
                    case 'n': s += '\n'; break;
                    case 't': s += '\t'; break;
                    case 'r': s += '\r'; break;
                    case 'b': s += '\b'; break;
                    case 'f': s += '\f'; break;
                    case 'u': p += 4; s += '?'; break;   // not needed here
                    default:  s += e; break;
                }
                p++;
            } else {
                s += t[p++];
            }
        }
        if (p >= t.size()) { ok = false; return; }
        p++;   // closing quote
    }
    void Array(JVal& v, int depth) {
        v.kind = JVal::Arr;
        p++;
        Ws();
        if (p < t.size() && t[p] == ']') { p++; return; }
        while (ok && p < t.size()) {
            v.a.emplace_back();
            Value(v.a.back(), depth + 1);
            Ws();
            if (p < t.size() && t[p] == ',') { p++; continue; }
            if (p < t.size() && t[p] == ']') { p++; return; }
            ok = false;
        }
        ok = false;
    }
    void Object(JVal& v, int depth) {
        v.kind = JVal::Obj;
        p++;
        Ws();
        if (p < t.size() && t[p] == '}') { p++; return; }
        while (ok && p < t.size()) {
            Ws();
            if (p >= t.size() || t[p] != '"') { ok = false; return; }
            std::string key;
            String(key);
            Ws();
            if (p >= t.size() || t[p] != ':') { ok = false; return; }
            p++;
            v.o.emplace_back(std::move(key), JVal{});
            Value(v.o.back().second, depth + 1);
            Ws();
            if (p < t.size() && t[p] == ',') { p++; continue; }
            if (p < t.size() && t[p] == '}') { p++; return; }
            ok = false;
        }
        ok = false;
    }
};

} // namespace raduk::json
