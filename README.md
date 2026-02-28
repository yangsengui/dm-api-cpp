# dm-api-cpp

Header-only C++ wrapper for DistroMate `dm_api` native library with CMake export support.

## Build And Install

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix <install-prefix>
```

## Consume

```cmake
find_package(dm_api_cpp REQUIRED)
target_link_libraries(your_target PRIVATE dm::api_cpp)
```

## Quick Start (License)

```cpp
#include "dm_api.hpp"

int main() {
    dm::Api api;
    api.setProductData("<product-data>");
    api.setProductId("your-product-id");
    api.setLicenseKey("XXXX-XXXX-XXXX");

    if (!api.activateLicense()) {
        throw std::runtime_error(api.getLastError());
    }

    if (!api.isLicenseGenuine()) {
        auto code = api.getLastActivationError();
        std::string name = code.has_value() ? api.getActivationErrorName(*code) : "UNKNOWN";
        throw std::runtime_error("license check failed: " + name + ", err=" + api.getLastError());
    }

    return 0;
}
```

## API Groups

- License setup: `setProductData`, `setProductId`, `setDataDirectory`, `setDebugMode`, `setCustomDeviceFingerprint`
- License activation: `setLicenseKey`, `setLicenseCallback`, `activateLicense`, `getLastActivationError`
- License state: `isLicenseGenuine`, `isLicenseValid`, `getServerSyncGracePeriodExpiryDate`, `getActivationMode`
- License details: `getLicenseKey`, `getLicenseExpiryDate`, `getLicenseCreationDate`, `getLicenseActivationDate`, `getActivationCreationDate`, `getActivationLastSyncedDate`, `getActivationId`
- Update: `checkForUpdates`, `downloadUpdate`, `cancelUpdateDownload`, `getUpdateState`, `getPostUpdateInfo`, `ackPostUpdateInfo`, `waitForUpdateStateChange`, `quitAndInstall`
- General: `getLibraryVersion`, `jsonToCanonical`, `getLastError`, `reset`

## Update API Notes

- Update APIs return parsed JSON envelope (`dm::JsonValue`) when transport succeeds.
- If native API returns `NULL`, C++ SDK returns `std::nullopt`; check `getLastError()`.
- `quitAndInstall()` returns native `int` status code directly.

## Dev License Skip Check

Use `dm::Api::shouldSkipCheck(appId, publicKey)` for local dev-license validation when needed.

## Note

No `{{PUBKEY}}` placeholder replacement is required.
