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

`dm_api_cpp` links `OpenSSL::Crypto` as an interface dependency.

## Integration Flow (Launcher Profile)

- Startup guard: `restartAppIfNecessary`.
- Signed verify/activate flow: `verify`, `activate`, `verifyAndActivate`.
- Update APIs: `checkForUpdates`, `downloadUpdate`, `getUpdateState`, `waitForUpdateStateChange`, `quitAndInstall`.

## Note

`dm_api.hpp` currently contains `{{PUBKEY}}` placeholder for signature verification.
Replace it with your real PEM public key before production release.
