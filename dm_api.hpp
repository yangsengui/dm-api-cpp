/**
 * DM API - DistroMate License Verification SDK (C++ Wrapper)
 *
 * Provides an integration interface for launched programs to communicate
 * with the launcher via Windows Named Pipe.
 *
 * Dependencies: OpenSSL (for RSA signature verification)
 *
 * Example (轻度混淆，基础用法 - 使用 verifyAndActivate 一键完成验证和激活):
 *   dm::Api api;
 *
 *   // 是否由启动器启动，否则退出进程
 *   if (api.restartAppIfNecessary()) {
 *       return 0;
 *   }
 *
 *   auto result = api.verifyAndActivate();
 *   if (result && (*result)["success"].asBool()) {
 *       std::cout << "许可证验证成功" << std::endl;
 *       // 继续执行应用逻辑...
 *   } else {
 *       std::cout << "许可证验证失败" << std::endl;
 *       return 1;
 *   }
 *
 *   // 通知启动器我已初始化成功准备显示窗口
 *   api.initiated();
 *
 *   app.show();
 *
 * Example (中度/重度混淆，手动控制流程):
 *   dm::Api api;
 *
 *   const char* pipe = std::getenv("DM_PIPE");
 *
 *   // 是否由启动器启动，否则退出进程
 *   if (api.restartAppIfNecessary()) {
 *       return 0;
 *   }
 *
 *   // 连接到许可证服务
 *   if (!api.connect(pipe)) {
 *       std::cout << "连接失败" << std::endl;
 *       return 1;
 *   }
 *
 *   // 验证许可证
 *   auto data = api.verify();
 *   if (data && (*data)["verification"]["valid"].asBool()) {
 *       std::cout << "许可证有效" << std::endl;
 *   } else {
 *       // 需要激活
 *       auto activation = api.activate();
 *       if (activation) {
 *           std::cout << "激活成功" << std::endl;
 *       }
 *   }
 *
 *   // 通知启动器我已初始化成功准备显示窗口
 *   api.initiated();
 *
 *   app.show();
 *
 *   // 关闭连接
 *   api.close();
 */

#ifndef DM_API_HPP
#define DM_API_HPP

#include <string>
#include <optional>
#include <stdexcept>
#include <random>
#include <sstream>
#include <iomanip>
#include <map>
#include <vector>
#include <algorithm>
#include <cstdlib>

#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/err.h>

// Include the C API header
extern "C" {
#include "dm_api.h"
}

namespace dm {

const char* const publicKey = R"({{PUBKEY}})";

/**
 * Simple JSON value representation
 */
class JsonValue {
public:
    enum Type { Null, Bool, Number, String, Array, Object };

    JsonValue() : type_(Null) {}
    JsonValue(bool b) : type_(Bool), bool_(b) {}
    JsonValue(double n) : type_(Number), number_(n) {}
    JsonValue(int n) : type_(Number), number_(static_cast<double>(n)) {}
    JsonValue(int64_t n) : type_(Number), number_(static_cast<double>(n)) {}
    JsonValue(const std::string& s) : type_(String), string_(s) {}
    JsonValue(const char* s) : type_(String), string_(s) {}
    JsonValue(std::map<std::string, JsonValue> obj) : type_(Object), object_(std::move(obj)) {}
    JsonValue(std::vector<JsonValue> arr) : type_(Array), array_(std::move(arr)) {}

    Type type() const { return type_; }
    bool isNull() const { return type_ == Null; }
    bool isBool() const { return type_ == Bool; }
    bool isNumber() const { return type_ == Number; }
    bool isString() const { return type_ == String; }
    bool isArray() const { return type_ == Array; }
    bool isObject() const { return type_ == Object; }

    bool asBool() const { return bool_; }
    double asNumber() const { return number_; }
    int64_t asInt() const { return static_cast<int64_t>(number_); }
    const std::string& asString() const { return string_; }
    const std::vector<JsonValue>& asArray() const { return array_; }
    const std::map<std::string, JsonValue>& asObject() const { return object_; }
    std::map<std::string, JsonValue>& asObject() { return object_; }

    bool contains(const std::string& key) const {
        return type_ == Object && object_.find(key) != object_.end();
    }

    const JsonValue& operator[](const std::string& key) const {
        static JsonValue null;
        if (type_ != Object) return null;
        auto it = object_.find(key);
        return it != object_.end() ? it->second : null;
    }

    JsonValue& operator[](const std::string& key) {
        return object_[key];
    }

    // Serialize to compact JSON string (sorted keys for canonical form)
    std::string toJson(bool sortKeys = false) const {
        std::ostringstream oss;
        serialize(oss, sortKeys);
        return oss.str();
    }

private:
    void serialize(std::ostringstream& oss, bool sortKeys) const {
        switch (type_) {
            case Null:
                oss << "null";
                break;
            case Bool:
                oss << (bool_ ? "true" : "false");
                break;
            case Number: {
                int64_t intVal = static_cast<int64_t>(number_);
                if (static_cast<double>(intVal) == number_) {
                    oss << intVal;
                } else {
                    oss << std::setprecision(17) << number_;
                }
                break;
            }
            case String:
                oss << '"';
                for (char c : string_) {
                    switch (c) {
                        case '"': oss << "\\\""; break;
                        case '\\': oss << "\\\\"; break;
                        case '\b': oss << "\\b"; break;
                        case '\f': oss << "\\f"; break;
                        case '\n': oss << "\\n"; break;
                        case '\r': oss << "\\r"; break;
                        case '\t': oss << "\\t"; break;
                        default:
                            if (static_cast<unsigned char>(c) < 0x20) {
                                oss << "\\u" << std::hex << std::setfill('0') << std::setw(4) << static_cast<int>(c);
                            } else {
                                oss << c;
                            }
                    }
                }
                oss << '"';
                break;
            case Array:
                oss << '[';
                for (size_t i = 0; i < array_.size(); ++i) {
                    if (i > 0) oss << ',';
                    array_[i].serialize(oss, sortKeys);
                }
                oss << ']';
                break;
            case Object: {
                oss << '{';
                std::vector<std::string> keys;
                for (const auto& kv : object_) {
                    keys.push_back(kv.first);
                }
                if (sortKeys) {
                    std::sort(keys.begin(), keys.end());
                }
                for (size_t i = 0; i < keys.size(); ++i) {
                    if (i > 0) oss << ',';
                    oss << '"' << keys[i] << "\":";
                    object_.at(keys[i]).serialize(oss, sortKeys);
                }
                oss << '}';
                break;
            }
        }
    }

    Type type_;
    bool bool_ = false;
    double number_ = 0;
    std::string string_;
    std::vector<JsonValue> array_;
    std::map<std::string, JsonValue> object_;
};

/**
 * Simple JSON parser
 */
class JsonParser {
public:
    static std::optional<JsonValue> parse(const std::string& json) {
        size_t pos = 0;
        return parseValue(json, pos);
    }

private:
    static void skipWhitespace(const std::string& json, size_t& pos) {
        while (pos < json.size() && std::isspace(json[pos])) pos++;
    }

    static std::optional<JsonValue> parseValue(const std::string& json, size_t& pos) {
        skipWhitespace(json, pos);
        if (pos >= json.size()) return std::nullopt;

        char c = json[pos];
        if (c == 'n') return parseNull(json, pos);
        if (c == 't' || c == 'f') return parseBool(json, pos);
        if (c == '"') return parseString(json, pos);
        if (c == '[') return parseArray(json, pos);
        if (c == '{') return parseObject(json, pos);
        if (c == '-' || std::isdigit(c)) return parseNumber(json, pos);
        return std::nullopt;
    }

    static std::optional<JsonValue> parseNull(const std::string& json, size_t& pos) {
        if (json.compare(pos, 4, "null") == 0) {
            pos += 4;
            return JsonValue();
        }
        return std::nullopt;
    }

    static std::optional<JsonValue> parseBool(const std::string& json, size_t& pos) {
        if (json.compare(pos, 4, "true") == 0) {
            pos += 4;
            return JsonValue(true);
        }
        if (json.compare(pos, 5, "false") == 0) {
            pos += 5;
            return JsonValue(false);
        }
        return std::nullopt;
    }

    static std::optional<JsonValue> parseNumber(const std::string& json, size_t& pos) {
        size_t start = pos;
        if (json[pos] == '-') pos++;
        while (pos < json.size() && std::isdigit(json[pos])) pos++;
        if (pos < json.size() && json[pos] == '.') {
            pos++;
            while (pos < json.size() && std::isdigit(json[pos])) pos++;
        }
        if (pos < json.size() && (json[pos] == 'e' || json[pos] == 'E')) {
            pos++;
            if (pos < json.size() && (json[pos] == '+' || json[pos] == '-')) pos++;
            while (pos < json.size() && std::isdigit(json[pos])) pos++;
        }
        try {
            return JsonValue(std::stod(json.substr(start, pos - start)));
        } catch (...) {
            return std::nullopt;
        }
    }

    static std::optional<JsonValue> parseString(const std::string& json, size_t& pos) {
        if (json[pos] != '"') return std::nullopt;
        pos++;
        std::string result;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\') {
                pos++;
                if (pos >= json.size()) return std::nullopt;
                switch (json[pos]) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'u': {
                        if (pos + 4 >= json.size()) return std::nullopt;
                        std::string hex = json.substr(pos + 1, 4);
                        try {
                            int codepoint = std::stoi(hex, nullptr, 16);
                            if (codepoint < 0x80) {
                                result += static_cast<char>(codepoint);
                            } else if (codepoint < 0x800) {
                                result += static_cast<char>(0xC0 | (codepoint >> 6));
                                result += static_cast<char>(0x80 | (codepoint & 0x3F));
                            } else {
                                result += static_cast<char>(0xE0 | (codepoint >> 12));
                                result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                                result += static_cast<char>(0x80 | (codepoint & 0x3F));
                            }
                        } catch (...) {
                            return std::nullopt;
                        }
                        pos += 4;
                        break;
                    }
                    default: return std::nullopt;
                }
            } else {
                result += json[pos];
            }
            pos++;
        }
        if (pos >= json.size()) return std::nullopt;
        pos++; // skip closing quote
        return JsonValue(result);
    }

    static std::optional<JsonValue> parseArray(const std::string& json, size_t& pos) {
        if (json[pos] != '[') return std::nullopt;
        pos++;
        std::vector<JsonValue> arr;
        skipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == ']') {
            pos++;
            return JsonValue(arr);
        }
        while (true) {
            auto val = parseValue(json, pos);
            if (!val) return std::nullopt;
            arr.push_back(*val);
            skipWhitespace(json, pos);
            if (pos >= json.size()) return std::nullopt;
            if (json[pos] == ']') {
                pos++;
                return JsonValue(arr);
            }
            if (json[pos] != ',') return std::nullopt;
            pos++;
        }
    }

    static std::optional<JsonValue> parseObject(const std::string& json, size_t& pos) {
        if (json[pos] != '{') return std::nullopt;
        pos++;
        std::map<std::string, JsonValue> obj;
        skipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == '}') {
            pos++;
            return JsonValue(obj);
        }
        while (true) {
            skipWhitespace(json, pos);
            auto key = parseString(json, pos);
            if (!key || !key->isString()) return std::nullopt;
            skipWhitespace(json, pos);
            if (pos >= json.size() || json[pos] != ':') return std::nullopt;
            pos++;
            auto val = parseValue(json, pos);
            if (!val) return std::nullopt;
            obj[key->asString()] = *val;
            skipWhitespace(json, pos);
            if (pos >= json.size()) return std::nullopt;
            if (json[pos] == '}') {
                pos++;
                return JsonValue(obj);
            }
            if (json[pos] != ',') return std::nullopt;
            pos++;
        }
    }
};

/**
 * Base64 decoder
 */
class Base64 {
public:
    static std::vector<unsigned char> decode(const std::string& encoded) {
        static const std::string chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        std::vector<unsigned char> result;
        std::vector<int> T(256, -1);
        for (int i = 0; i < 64; i++) T[chars[i]] = i;

        int val = 0, valb = -8;
        for (unsigned char c : encoded) {
            if (c == '=') break;
            if (T[c] == -1) continue;
            val = (val << 6) + T[c];
            valb += 6;
            if (valb >= 0) {
                result.push_back(static_cast<unsigned char>((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
        return result;
    }
};

/**
 * DM API C++ Wrapper Class
 */
class Api {
public:
    /**
     * Construct API with hardcoded RSA public key for signature verification
     *
     * @throws std::runtime_error if public key parsing fails
     */
    Api() {
        BIO* bio = BIO_new_mem_buf(publicKey, -1);
        if (!bio) {
            throw std::runtime_error("Failed to create BIO for public key");
        }

        pubKey_ = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
        BIO_free(bio);

        if (!pubKey_) {
            throw std::runtime_error("Failed to parse public key: " + getOpenSSLError());
        }
    }

    ~Api() {
        close();
        if (pubKey_) {
            EVP_PKEY_free(pubKey_);
            pubKey_ = nullptr;
        }
    }

    // No copy
    Api(const Api&) = delete;
    Api& operator=(const Api&) = delete;

    /**
     * Check if the program was launched by the launcher, if not, restart via launcher.
     *
     * @return true if launcher was started and current process should exit
     * @return false if program was launched by launcher, continue normal flow
     * @throws std::runtime_error if failed to find or start launcher
     */
    bool restartAppIfNecessary() {
        int result = DM_RestartAppIfNecessary();
        if (result == 0) {
            return false;  // Launched by launcher, no restart needed
        } else if (result == 1) {
            return true;   // Launcher started, should exit
        } else {
            throw std::runtime_error(getLastError());
        }
    }

    /**
     * Electron-compatible: checkForUpdates
     */
    std::optional<JsonValue> checkForUpdates(const std::string& optionsJson = "{}") {
        char* result = DM_CheckForUpdates(optionsJson.c_str());
        if (!result) {
            return std::nullopt;
        }

        std::string respStr(result);
        DM_FreeString(result);
        auto resp = JsonParser::parse(respStr);
        if (!resp || !resp->isObject() || !resp->contains("data")) {
            return std::nullopt;
        }
        return (*resp)["data"];
    }

    std::optional<JsonValue> checkForUpdate(const std::string& optionsJson = "{}") {
        return checkForUpdates(optionsJson);
    }

    /**
     * Electron-compatible: downloadUpdate
     */
    std::optional<JsonValue> downloadUpdate(const std::string& optionsJson = "{}") {
        char* result = DM_DownloadUpdate(optionsJson.c_str());
        if (!result) {
            return std::nullopt;
        }

        std::string respStr(result);
        DM_FreeString(result);
        auto resp = JsonParser::parse(respStr);
        if (!resp || !resp->isObject() || !resp->contains("data")) {
            return std::nullopt;
        }
        return (*resp)["data"];
    }

    /**
     * Get current update state snapshot
     */
    std::optional<JsonValue> getUpdateState() {
        char* result = DM_GetUpdateState();
        if (!result) {
            return std::nullopt;
        }

        std::string respStr(result);
        DM_FreeString(result);
        auto resp = JsonParser::parse(respStr);
        if (!resp || !resp->isObject() || !resp->contains("data")) {
            return std::nullopt;
        }
        return (*resp)["data"];
    }

    /**
     * Wait for update state changes without native callbacks
     */
    std::optional<JsonValue> waitForUpdateStateChange(uint64_t lastSequence, uint32_t timeoutMs = 30000) {
        char* result = DM_WaitForUpdateStateChange(lastSequence, timeoutMs);
        if (!result) {
            return std::nullopt;
        }

        std::string respStr(result);
        DM_FreeString(result);
        auto resp = JsonParser::parse(respStr);
        if (!resp || !resp->isObject() || !resp->contains("data")) {
            return std::nullopt;
        }
        return (*resp)["data"];
    }

    /**
     * Electron-compatible: quitAndInstall
     * Returns true when launcher accepts the request.
     */
    bool quitAndInstall(const std::string& optionsJson = "{}") {
        return DM_QuitAndInstall(optionsJson.c_str()) == 1;
    }

    /**
     * Connect to the launcher
     *
     * @param pipeName Pipe name, format: \\\\.\\pipe\\<name>
     * @param timeoutMs Timeout in milliseconds, 0 for default (5000ms)
     * @return true on success
     */
    bool connect(const std::string& pipeName, uint32_t timeoutMs = 5000) {
        return DM_Connect(pipeName.c_str(), timeoutMs) == 0;
    }

    /**
     * Close the connection
     */
    void close() {
        DM_Close();
    }

    /**
     * Check if connected
     */
    bool isConnected() const {
        return DM_IsConnected() == 1;
    }

    /**
     * Verify license
     * Automatically generates nonce and verifies response signature
     *
     * @return Verified response data, or nullopt on failure
     */
    std::optional<JsonValue> verify() {
        std::string nonce = generateNonce();
        std::string req = "{\"nonce_str\":\"" + nonce + "\"}";

        char* result = DM_Verify(req.c_str());
        if (!result) {
            return std::nullopt;
        }

        std::string respStr(result);
        DM_FreeString(result);

        auto resp = JsonParser::parse(respStr);
        if (!resp || !resp->isObject()) {
            return std::nullopt;
        }

        if (!resp->contains("data") || !(*resp)["data"].isObject()) {
            return std::nullopt;
        }

        JsonValue data = (*resp)["data"];
        
        // Check success field
        if (!data.contains("success") || !data["success"].isBool() || !data["success"].asBool()) {
            return std::nullopt;
        }

        // New format: data.verification contains signed data from server
        if (!data.contains("verification") || !data["verification"].isObject()) {
            return std::nullopt;
        }

        JsonValue verification = data["verification"];
        
        // Verify signature (required)
        if (!checkSignature(verification, nonce)) {
            return std::nullopt;
        }

        // Build result object (nested format)
        std::map<std::string, JsonValue> resultMap;
        resultMap["success"] = JsonValue(true);
        if (data.contains("is_online")) {
            resultMap["is_online"] = data["is_online"];
        }
        resultMap["verification"] = verification;

        return JsonValue(resultMap);
    }

    /**
     * Activate license
     * Automatically generates nonce and verifies response signature
     *
     * @return Verified response data, or nullopt on failure
     */
    std::optional<JsonValue> activate() {
        std::string nonce = generateNonce();
        std::string req = "{\"nonce_str\":\"" + nonce + "\"}";

        char* result = DM_Activate(req.c_str());
        if (!result) {
            return std::nullopt;
        }

        std::string respStr(result);
        DM_FreeString(result);

        auto resp = JsonParser::parse(respStr);
        if (!resp || !resp->isObject()) {
            return std::nullopt;
        }

        if (!resp->contains("data") || !(*resp)["data"].isObject()) {
            return std::nullopt;
        }

        JsonValue data = (*resp)["data"];
        
        // Check success field
        if (!data.contains("success") || !data["success"].isBool() || !data["success"].asBool()) {
            return std::nullopt;
        }

        // New format: data.activation contains signed data from server
        if (!data.contains("activation") || !data["activation"].isObject()) {
            return std::nullopt;
        }

        JsonValue activation = data["activation"];
        
        // Verify signature (required)
        if (!checkSignature(activation, nonce)) {
            return std::nullopt;
        }

        // Build result object (nested format)
        std::map<std::string, JsonValue> resultMap;
        resultMap["success"] = JsonValue(true);
        resultMap["activation"] = activation;

        return JsonValue(resultMap);
    }

    /**
     * Notify launcher that initialization is complete
     *
     * @return true on success
     */
    bool initiated() {
        return DM_Initiated() == 0;
    }

    /**
     * Connect to pipe, verify license, and activate in a loop until successful
     *
     * @param timeout Connection timeout in milliseconds (default: 5000)
     * @return Result object containing success status and optional error message
     */
    std::optional<JsonValue> verifyAndActivate(uint32_t timeout = 5000) {
        const char* pipe = std::getenv("DM_PIPE");
        if (!pipe || std::string(pipe).empty()) {
            std::map<std::string, JsonValue> result;
            result["success"] = JsonValue(false);
            result["error"] = JsonValue("DM_PIPE environment variable not set");
            return JsonValue(result);
        }

        if (!connect(pipe, timeout)) {
            std::map<std::string, JsonValue> result;
            result["success"] = JsonValue(false);
            result["error"] = JsonValue("Failed to connect to license service");
            return JsonValue(result);
        }

        auto verifyResult = verify();
        if (verifyResult) {
            if (verifyResult->contains("verification")) {
                const auto& verification = (*verifyResult)["verification"];
                if (verification.contains("valid") && verification["valid"].asBool()) {
                    std::map<std::string, JsonValue> result;
                    result["success"] = JsonValue(true);
                    return JsonValue(result);
                }
            }
        }

        while (true) {
            auto activateResult = activate();
            if (activateResult && activateResult->contains("activation")) {
                std::map<std::string, JsonValue> result;
                result["success"] = JsonValue(true);
                return JsonValue(result);
            }
        }
    }

    /**
     * Get the last error message
     */
    std::string getLastError() const {
        char* err = DM_GetLastError();
        if (err) {
            std::string result(err);
            DM_FreeString(err);
            return result;
        }
        return "";
    }

    /**
     * Get DLL version
     */
    static const char* getVersion() {
        return DM_GetVersion();
    }

    /**
     * Convert JSON string to canonical format (sorted keys)
     * This ensures consistency without performing any hashing or verification.
     *
     * @param jsonStr JSON formatted string
     * @return Canonical JSON string, or empty string on failure
     */
    static std::string jsonToCanonical(const std::string& jsonStr) {
        char* result = DM_JsonToCanonical(jsonStr.c_str());
        if (!result) {
            return "";
        }
        std::string canonical(result);
        DM_FreeString(result);
        return canonical;
    }

private:
    EVP_PKEY* pubKey_ = nullptr;

    /**
     * Generate random hex nonce (32 characters = 16 bytes)
     */
    static std::string generateNonce() {
        static const char hexChars[] = "0123456789abcdef";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);

        std::string nonce;
        nonce.reserve(32);
        for (int i = 0; i < 32; ++i) {
            nonce += hexChars[dis(gen)];
        }
        return nonce;
    }

    /**
     * Verify RSA signature
     */
    bool checkSignature(const JsonValue& data, const std::string& nonce) {
        if (!data.contains("signature") || !data["signature"].isString()) {
            return false;
        }

        // Decode base64 signature
        std::vector<unsigned char> signature = Base64::decode(data["signature"].asString());
        if (signature.empty()) {
            return false;
        }

        // Build canonical payload (exclude signature, add nonce_str)
        std::map<std::string, JsonValue> payload;
        for (const auto& kv : data.asObject()) {
            if (kv.first != "signature") {
                payload[kv.first] = kv.second;
            }
        }
        payload["nonce_str"] = JsonValue(nonce);

        // Serialize to JSON first
        JsonValue payloadObj(payload);
        std::string jsonStr = payloadObj.toJson(false);

        // Use DM_JsonToCanonical to ensure consistency with Go version
        char* canonicalPtr = DM_JsonToCanonical(jsonStr.c_str());
        if (!canonicalPtr) {
            return false;
        }
        std::string canonical(canonicalPtr);
        DM_FreeString(canonicalPtr);

        // Verify signature using OpenSSL EVP API
        EVP_MD_CTX* mdCtx = EVP_MD_CTX_new();
        if (!mdCtx) {
            return false;
        }

        bool verified = false;
        if (EVP_DigestVerifyInit(mdCtx, nullptr, EVP_sha256(), nullptr, pubKey_) == 1) {
            if (EVP_DigestVerifyUpdate(mdCtx, canonical.c_str(), canonical.size()) == 1) {
                verified = (EVP_DigestVerifyFinal(mdCtx, signature.data(), signature.size()) == 1);
            }
        }

        EVP_MD_CTX_free(mdCtx);
        return verified;
    }

    /**
     * Get OpenSSL error string
     */
    static std::string getOpenSSLError() {
        unsigned long err = ERR_get_error();
        if (err == 0) return "Unknown error";
        char buf[256];
        ERR_error_string_n(err, buf, sizeof(buf));
        return std::string(buf);
    }
};

} // namespace dm

#endif // DM_API_HPP
