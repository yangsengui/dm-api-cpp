# dm-api-cpp

Header-only C++ wrapper for DistroMate `dm_api` with CMake export support.

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

## Integration Flow

This SDK follows the same LexActivator-style flow as Python SDK:

1. `setProductData()` and `setProductId()`.
2. `setLicenseKey()` and `activateLicense()`.
3. `isLicenseGenuine()` or `isLicenseValid()` on every startup.
4. Optional version/update APIs: `getVersion()`, `getLibraryVersion()`, `checkForUpdates()`.

## Quick Start

```cpp
#include "dm_api.hpp"

int main() {
    dm::Api api;
    api.setProductData("<product_data>");
    api.setProductId("your-product-id", 0);
    api.setLicenseKey("XXXX-XXXX-XXXX");

    if (!api.activateLicense()) {
        throw std::runtime_error(api.getLastError());
    }

    if (!api.isLicenseGenuine()) {
        throw std::runtime_error(api.getLastError());
    }

    return 0;
}
```

## Dev License Skip Check

Use `dm::Api::shouldSkipCheck(appId, publicKey)` for local dev-license validation when needed.

## Note

No `{{PUBKEY}}` placeholder replacement is required.
