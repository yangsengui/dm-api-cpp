#ifndef DM_API_HPP
#define DM_API_HPP

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#if __cplusplus < 201703L
namespace dm_optional_detail {
struct nullopt_t {
    explicit constexpr nullopt_t(int) {}
};

constexpr nullopt_t nullopt{0};

template <typename T>
class Optional {
public:
    Optional() = default;
    Optional(nullopt_t) : has_(false) {}
    Optional(const T& value) : has_(true), value_(value) {}
    Optional(T&& value) : has_(true), value_(std::move(value)) {}

    explicit operator bool() const { return has_; }
    const T& operator*() const { return value_; }
    T& operator*() { return value_; }
    const T* operator->() const { return &value_; }
    T* operator->() { return &value_; }

private:
    bool has_ = false;
    T value_{};
};
}  // namespace dm_optional_detail

namespace std {
template <typename T>
using optional = dm_optional_detail::Optional<T>;
using nullopt_t = dm_optional_detail::nullopt_t;
constexpr nullopt_t nullopt = dm_optional_detail::nullopt;
}  // namespace std
#endif

extern "C" {
#include "dm_api.h"
}

namespace dm {

inline constexpr const char* kDevLicenseErrorText =
    "Development license is missing or corrupted. Run `distromate sdk renew` to regenerate the dev certificate.";
inline constexpr uint32_t kDefaultBufferSize = 256;
inline constexpr uint32_t kDefaultModeBufferSize = 64;
inline constexpr uint32_t kDefaultVersionBufferSize = 32;

class JsonValue {
public:
    enum Type { Null, Bool, Number, String, Array, Object };

    JsonValue() : type_(Null) {}
    JsonValue(bool value) : type_(Bool), bool_(value) {}
    JsonValue(double value) : type_(Number), number_(value) {}
    JsonValue(int value) : type_(Number), number_(static_cast<double>(value)) {}
    JsonValue(int64_t value) : type_(Number), number_(static_cast<double>(value)) {}
    JsonValue(const std::string& value) : type_(String), string_(value) {}
    JsonValue(const char* value) : type_(String), string_(value ? value : "") {}
    JsonValue(std::map<std::string, JsonValue> value) : type_(Object), object_(std::move(value)) {}
    JsonValue(std::vector<JsonValue> value) : type_(Array), array_(std::move(value)) {}

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

    bool contains(const std::string& key) const {
        return type_ == Object && object_.find(key) != object_.end();
    }

    const JsonValue& operator[](const std::string& key) const {
        static JsonValue nullValue;
        if (type_ != Object) {
            return nullValue;
        }

        auto it = object_.find(key);
        return it == object_.end() ? nullValue : it->second;
    }

private:
    Type type_;
    bool bool_ = false;
    double number_ = 0.0;
    std::string string_;
    std::vector<JsonValue> array_;
    std::map<std::string, JsonValue> object_;
};

class JsonParser {
public:
    static std::optional<JsonValue> parse(const std::string& json) {
        size_t pos = 0;
        auto value = parseValue(json, pos);
        if (!value) {
            return std::nullopt;
        }

        skipWhitespace(json, pos);
        if (pos != json.size()) {
            return std::nullopt;
        }

        return value;
    }

private:
    static void skipWhitespace(const std::string& json, size_t& pos) {
        while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos])) != 0) {
            ++pos;
        }
    }

    static std::optional<JsonValue> parseValue(const std::string& json, size_t& pos) {
        skipWhitespace(json, pos);
        if (pos >= json.size()) {
            return std::nullopt;
        }

        const char current = json[pos];
        if (current == 'n') {
            return parseNull(json, pos);
        }
        if (current == 't' || current == 'f') {
            return parseBool(json, pos);
        }
        if (current == '"') {
            return parseString(json, pos);
        }
        if (current == '[') {
            return parseArray(json, pos);
        }
        if (current == '{') {
            return parseObject(json, pos);
        }
        if (current == '-' || std::isdigit(static_cast<unsigned char>(current)) != 0) {
            return parseNumber(json, pos);
        }

        return std::nullopt;
    }

    static std::optional<JsonValue> parseNull(const std::string& json, size_t& pos) {
        if (json.compare(pos, 4, "null") != 0) {
            return std::nullopt;
        }
        pos += 4;
        return JsonValue();
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
        const size_t start = pos;

        if (json[pos] == '-') {
            ++pos;
        }

        while (pos < json.size() && std::isdigit(static_cast<unsigned char>(json[pos])) != 0) {
            ++pos;
        }

        if (pos < json.size() && json[pos] == '.') {
            ++pos;
            while (pos < json.size() && std::isdigit(static_cast<unsigned char>(json[pos])) != 0) {
                ++pos;
            }
        }

        if (pos < json.size() && (json[pos] == 'e' || json[pos] == 'E')) {
            ++pos;
            if (pos < json.size() && (json[pos] == '+' || json[pos] == '-')) {
                ++pos;
            }
            while (pos < json.size() && std::isdigit(static_cast<unsigned char>(json[pos])) != 0) {
                ++pos;
            }
        }

        try {
            return JsonValue(std::stod(json.substr(start, pos - start)));
        } catch (...) {
            return std::nullopt;
        }
    }

    static std::optional<JsonValue> parseString(const std::string& json, size_t& pos) {
        if (pos >= json.size() || json[pos] != '"') {
            return std::nullopt;
        }

        ++pos;
        std::string output;

        while (pos < json.size()) {
            const char current = json[pos];
            if (current == '"') {
                ++pos;
                return JsonValue(output);
            }

            if (current == '\\') {
                ++pos;
                if (pos >= json.size()) {
                    return std::nullopt;
                }

                const char escaped = json[pos];
                switch (escaped) {
                    case '"': output.push_back('"'); break;
                    case '\\': output.push_back('\\'); break;
                    case '/': output.push_back('/'); break;
                    case 'b': output.push_back('\b'); break;
                    case 'f': output.push_back('\f'); break;
                    case 'n': output.push_back('\n'); break;
                    case 'r': output.push_back('\r'); break;
                    case 't': output.push_back('\t'); break;
                    case 'u': {
                        if (pos + 4 >= json.size()) {
                            return std::nullopt;
                        }

                        const std::string hex = json.substr(pos + 1, 4);
                        try {
                            const int codepoint = std::stoi(hex, nullptr, 16);
                            if (codepoint < 0x80) {
                                output.push_back(static_cast<char>(codepoint));
                            } else if (codepoint < 0x800) {
                                output.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
                                output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
                            } else {
                                output.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
                                output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                                output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
                            }
                        } catch (...) {
                            return std::nullopt;
                        }

                        pos += 4;
                        break;
                    }
                    default:
                        return std::nullopt;
                }
            } else {
                output.push_back(current);
            }

            ++pos;
        }

        return std::nullopt;
    }

    static std::optional<JsonValue> parseArray(const std::string& json, size_t& pos) {
        if (pos >= json.size() || json[pos] != '[') {
            return std::nullopt;
        }

        ++pos;
        std::vector<JsonValue> values;

        skipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == ']') {
            ++pos;
            return JsonValue(std::move(values));
        }

        while (true) {
            auto value = parseValue(json, pos);
            if (!value) {
                return std::nullopt;
            }
            values.push_back(*value);

            skipWhitespace(json, pos);
            if (pos >= json.size()) {
                return std::nullopt;
            }

            if (json[pos] == ']') {
                ++pos;
                return JsonValue(std::move(values));
            }

            if (json[pos] != ',') {
                return std::nullopt;
            }
            ++pos;
        }
    }

    static std::optional<JsonValue> parseObject(const std::string& json, size_t& pos) {
        if (pos >= json.size() || json[pos] != '{') {
            return std::nullopt;
        }

        ++pos;
        std::map<std::string, JsonValue> object;

        skipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == '}') {
            ++pos;
            return JsonValue(std::move(object));
        }

        while (true) {
            skipWhitespace(json, pos);
            auto key = parseString(json, pos);
            if (!key || !key->isString()) {
                return std::nullopt;
            }

            skipWhitespace(json, pos);
            if (pos >= json.size() || json[pos] != ':') {
                return std::nullopt;
            }
            ++pos;

            auto value = parseValue(json, pos);
            if (!value) {
                return std::nullopt;
            }

            object[key->asString()] = *value;

            skipWhitespace(json, pos);
            if (pos >= json.size()) {
                return std::nullopt;
            }

            if (json[pos] == '}') {
                ++pos;
                return JsonValue(std::move(object));
            }

            if (json[pos] != ',') {
                return std::nullopt;
            }
            ++pos;
        }
    }
};

class Api {
public:
    explicit Api(uint32_t pipeTimeoutMs = DM_DEFAULT_TIMEOUT_MS)
        : pipeTimeout_(pipeTimeoutMs == 0 ? DM_DEFAULT_TIMEOUT_MS : pipeTimeoutMs) {}

    void setPipeTimeout(uint32_t timeoutMs) {
        pipeTimeout_ = timeoutMs == 0 ? DM_DEFAULT_TIMEOUT_MS : timeoutMs;
    }

    static bool shouldSkipCheck(
        const std::string& appId = "",
        const std::string& publicKey = "")
    {
        const char* launcherEndpoint = std::getenv("DM_LAUNCHER_ENDPOINT");
        const char* launcherToken = std::getenv("DM_LAUNCHER_TOKEN");
        if (launcherEndpoint != nullptr && launcherEndpoint[0] != '\0' &&
            launcherToken != nullptr && launcherToken[0] != '\0') {
            return false;
        }

        std::string resolvedAppId = trim(appId);
        std::string resolvedPublicKey = trim(publicKey);

        if (resolvedAppId.empty()) {
            resolvedAppId = trim(readEnv("DM_APP_ID"));
        }
        if (resolvedPublicKey.empty()) {
            resolvedPublicKey = trim(readEnv("DM_PUBLIC_KEY"));
        }

        if (resolvedAppId.empty() || resolvedPublicKey.empty()) {
            throw std::runtime_error(
                "App identity is required for dev-license checks. Provide appId/publicKey or set DM_APP_ID and DM_PUBLIC_KEY.");
        }

        const std::string homePath = resolveHomePath();
        if (homePath.empty()) {
            throw std::runtime_error(kDevLicenseErrorText);
        }

        const std::string pubkeyPath = joinPath(
            joinPath(
                joinPath(
                    joinPath(homePath, ".distromate-cli"),
                    "dev_licenses"),
                resolvedAppId),
            "pubkey");

        std::ifstream file(pubkeyPath, std::ios::in | std::ios::binary);
        if (!file) {
            throw std::runtime_error(kDevLicenseErrorText);
        }

        std::ostringstream stream;
        stream << file.rdbuf();
        const std::string devPublicKey = trim(stream.str());

        if (devPublicKey.empty() || devPublicKey != resolvedPublicKey) {
            throw std::runtime_error(kDevLicenseErrorText);
        }

        return true;
    }

    bool restartAppIfNecessary() const {
        return ::DM_RestartAppIfNecessary() != 0;
    }

    std::string getVersion() const {
        const char* version = ::DM_GetVersion();
        return version == nullptr ? std::string() : std::string(version);
    }

    std::string getLastError() const {
        return ptrToOwnedString(::DM_GetLastError());
    }

    bool setProductData(const std::string& productData) const {
        return ::SetProductData(productData.c_str()) == 0;
    }

    bool setProductId(const std::string& productId) const {
        return ::SetProductId(productId.c_str(), 0) == 0;
    }

    bool setDataDirectory(const std::string& directoryPath) const {
        return ::SetDataDirectory(directoryPath.c_str()) == 0;
    }

    bool setDebugMode(bool enable) const {
        return ::SetDebugMode(enable ? 1U : 0U) == 0;
    }

    bool setCustomDeviceFingerprint(const std::string& fingerprint) const {
        return ::SetCustomDeviceFingerprint(fingerprint.c_str()) == 0;
    }

    bool setLicenseKey(const std::string& licenseKey) const {
        return ::SetLicenseKey(licenseKey.c_str()) == 0;
    }

    bool setLicenseCallback(CallbackType callback) const {
        return ::SetLicenseCallback(callback) == 0;
    }

    bool setActivationMetadata(const std::string& key, const std::string& value) const {
        return ::SetActivationMetadata(key.c_str(), value.c_str()) == 0;
    }

    bool activateLicense() const {
        return ::ActivateLicense() == 0;
    }

    bool activateLicenseOffline(const std::string& filePath) const {
        return ::ActivateLicenseOffline(filePath.c_str()) == 0;
    }

    bool generateOfflineDeactivationRequest(const std::string& filePath) const {
        return ::GenerateOfflineDeactivationRequest(filePath.c_str()) == 0;
    }

    std::optional<uint32_t> getLastActivationError() const {
        return callU32Out(::GetLastActivationError);
    }

    std::string getActivationErrorName(uint32_t code) const {
        switch (code) {
            case 0: return "DM_ERR_OK";
            case 1: return "DM_ERR_FAIL";
            case 2: return "DM_ERR_INVALID_PARAMETER";
            case 3: return "DM_ERR_APPID_NOT_SET";
            case 4: return "DM_ERR_LICENSE_KEY_NOT_SET";
            case 5: return "DM_ERR_NOT_ACTIVATED";
            case 6: return "DM_ERR_LICENSE_EXPIRED";
            case 7: return "DM_ERR_NETWORK";
            case 8: return "DM_ERR_FILE_IO";
            case 9: return "DM_ERR_SIGNATURE";
            case 10: return "DM_ERR_BUFFER_TOO_SMALL";
            default: return "UNKNOWN(" + std::to_string(code) + ")";
        }
    }

    bool isLicenseGenuine() const {
        return ::IsLicenseGenuine() == 0;
    }

    bool isLicenseValid() const {
        return ::IsLicenseValid() == 0;
    }

    std::optional<uint32_t> getServerSyncGracePeriodExpiryDate() const {
        return callU32Out(::GetServerSyncGracePeriodExpiryDate);
    }

    std::optional<JsonValue> getActivationMode(uint32_t bufferSize = kDefaultModeBufferSize) const {
        const uint32_t size = bufferSize == 0 ? kDefaultModeBufferSize : bufferSize;
        std::vector<char> initial(size, '\0');
        std::vector<char> current(size, '\0');

        if (::GetActivationMode(initial.data(), size, current.data(), size) != 0) {
            return std::nullopt;
        }

        std::map<std::string, JsonValue> result;
        result["initial_mode"] = JsonValue(initial.data());
        result["current_mode"] = JsonValue(current.data());
        return JsonValue(std::move(result));
    }

    std::optional<std::string> getLicenseKey(uint32_t bufferSize = kDefaultBufferSize) const {
        return callStringOut(::GetLicenseKey, bufferSize, kDefaultBufferSize);
    }

    std::optional<uint32_t> getLicenseExpiryDate() const {
        return callU32Out(::GetLicenseExpiryDate);
    }

    std::optional<uint32_t> getLicenseCreationDate() const {
        return callU32Out(::GetLicenseCreationDate);
    }

    std::optional<uint32_t> getLicenseActivationDate() const {
        return callU32Out(::GetLicenseActivationDate);
    }

    std::optional<uint32_t> getActivationCreationDate() const {
        return callU32Out(::GetActivationCreationDate);
    }

    std::optional<uint32_t> getActivationLastSyncedDate() const {
        return callU32Out(::GetActivationLastSyncedDate);
    }

    std::optional<std::string> getActivationId(uint32_t bufferSize = kDefaultBufferSize) const {
        return callStringOut(::GetActivationId, bufferSize, kDefaultBufferSize);
    }

    std::optional<std::string> getLibraryVersion(uint32_t bufferSize = kDefaultVersionBufferSize) const {
        return callStringOut(::GetLibraryVersion, bufferSize, kDefaultVersionBufferSize);
    }

    bool reset() const {
        return ::Reset() == 0;
    }

    std::optional<JsonValue> checkForUpdates(const std::string& optionsJson = "{}") const {
        return callPipeJson([&]() {
            return ::DM_CheckForUpdates(optionsJson.c_str());
        });
    }

    std::optional<JsonValue> checkForUpdate(const std::string& optionsJson = "{}") const {
        return checkForUpdates(optionsJson);
    }

    std::optional<JsonValue> downloadUpdate(const std::string& optionsJson = "{}") const {
        return callPipeJson([&]() {
            return ::DM_DownloadUpdate(optionsJson.c_str());
        });
    }

    std::optional<JsonValue> cancelUpdateDownload(const std::string& optionsJson = "{}") const {
        return callPipeJson([&]() {
            return ::DM_CancelUpdateDownload(optionsJson.c_str());
        });
    }

    std::optional<JsonValue> getUpdateState() const {
        return callPipeJson([&]() {
            return ::DM_GetUpdateState();
        });
    }

    std::optional<JsonValue> getPostUpdateInfo() const {
        return callPipeJson([&]() {
            return ::DM_GetPostUpdateInfo();
        });
    }

    std::optional<JsonValue> ackPostUpdateInfo(const std::string& optionsJson = "{}") const {
        return callPipeJson([&]() {
            return ::DM_AckPostUpdateInfo(optionsJson.c_str());
        });
    }

    std::optional<JsonValue> waitForUpdateStateChange(uint64_t lastSequence, uint32_t timeoutMs = 30000) const {
        return callPipeJson([&]() {
            return ::DM_WaitForUpdateStateChange(lastSequence, timeoutMs);
        });
    }

    int32_t quitAndInstall(const std::string& optionsJson = "{}") const {
        return ::DM_QuitAndInstall(optionsJson.c_str());
    }

    bool connect(const std::string& pipeName, uint32_t timeoutMs = 0) const {
        const uint32_t timeout = timeoutMs == 0 ? resolvePipeTimeout() : timeoutMs;
        return ::DM_Connect(pipeName.c_str(), timeout) == 0;
    }

    void close() const {
        ::DM_Close();
    }

    bool isConnected() const {
        return ::DM_IsConnected() == 1;
    }

    static std::string jsonToCanonical(const std::string& jsonStr) {
        return ptrToOwnedString(::DM_JsonToCanonical(jsonStr.c_str()));
    }

private:
    using U32OutCall = int32_t (*)(uint32_t*);
    using StringOutCall = int32_t (*)(char*, uint32_t);

    uint32_t pipeTimeout_;

    static std::string trim(const std::string& input) {
        size_t start = 0;
        size_t end = input.size();

        while (start < end && std::isspace(static_cast<unsigned char>(input[start])) != 0) {
            ++start;
        }

        while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1])) != 0) {
            --end;
        }

        return input.substr(start, end - start);
    }

    static std::string joinPath(const std::string& left, const std::string& right) {
        if (left.empty()) {
            return right;
        }

        const char last = left.back();
        if (last == '/' || last == '\\') {
            return left + right;
        }

#ifdef _WIN32
        return left + "\\" + right;
#else
        return left + "/" + right;
#endif
    }

    static std::string readEnv(const char* key) {
        const char* value = std::getenv(key);
        return value == nullptr ? std::string() : std::string(value);
    }

    static std::string resolveHomePath() {
        std::string home = readEnv("USERPROFILE");
        if (!home.empty()) {
            return home;
        }

        home = readEnv("HOME");
        return home;
    }

    static std::string ptrToOwnedString(char* ptr) {
        if (ptr == nullptr) {
            return std::string();
        }

        std::string value(ptr);
        ::DM_FreeString(ptr);
        return value;
    }

    static std::optional<uint32_t> callU32Out(U32OutCall call) {
        uint32_t value = 0;
        if (call(&value) != 0) {
            return std::nullopt;
        }
        return value;
    }

    static std::optional<std::string> callStringOut(
        StringOutCall call,
        uint32_t bufferSize,
        uint32_t defaultSize)
    {
        const uint32_t size = bufferSize == 0 ? defaultSize : bufferSize;
        std::vector<char> buffer(size, '\0');
        if (call(buffer.data(), size) != 0) {
            return std::nullopt;
        }
        return std::string(buffer.data());
    }

    uint32_t resolvePipeTimeout() const {
        return pipeTimeout_ == 0 ? DM_DEFAULT_TIMEOUT_MS : pipeTimeout_;
    }

    template <typename Fn>
    std::optional<JsonValue> callPipeJson(Fn&& fn) const {
        char* ptr = fn();
        if (ptr == nullptr) {
            return std::nullopt;
        }

        const std::string response = ptrToOwnedString(ptr);
        auto envelope = JsonParser::parse(response);
        if (!envelope || !envelope->isObject()) {
            return std::nullopt;
        }

        return *envelope;
    }
};

}  // namespace dm

#endif  // DM_API_HPP
