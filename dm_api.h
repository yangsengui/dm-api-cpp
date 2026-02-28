/**
 * DM API - DistroMate License Verification SDK
 *
 * Provides an integration interface for launched programs to communicate
 * with the launcher via local IPC.
 *
 * Usage flow:
 *   1. DM_Connect() - Connect to the launcher
 *   2. DM_Verify() / DM_Activate() - Verify or activate license
 *   3. DM_Initiated() - Notify the launcher that initialization is complete
 *   4. DM_Close() - Close connection (optional)
 *
 * Note: Strings returned by DM_Verify/DM_Activate/DM_GetLastError must be freed with DM_FreeString
 */


#ifndef DM_API_H
#define DM_API_H

/* Generated with cbindgen:0.29.2 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define DM_DEFAULT_TIMEOUT_MS 5000

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Connect to the launcher
 *
 * Creates a pipe connection to the launcher. Subsequent operations will use this connection.
 *
 * # Parameters
 * - `pipe_name`: Pipe name (C string), format: `\\.\pipe\<name>`
 * - `timeout_ms`: Timeout in milliseconds, 0 means use default timeout (5000ms)
 *
 * # Returns
 * - 0: Success
 * - -1: Connection failed
 * - -3: Invalid parameter
 */
int32_t DM_Connect(const char *pipe_name, uint32_t timeout_ms);

/**
 * Close the connection to the launcher
 *
 * # Returns
 * - 0: Success
 */
int32_t DM_Close(void);

/**
 * Check if connected to the launcher
 *
 * # Returns
 * - 1: Connected
 * - 0: Not connected
 */
int32_t DM_IsConnected(void);

/**
 * Verify license
 *
 * Sends a verification request to the launcher via Named Pipe.
 *
 * # Parameters
 * - `json_data`: JSON formatted request data (C string), must contain `nonce_str` field
 *   Example: `{"nonce_str": "random_string_32chars"}`
 *
 * # Returns
 * - Success: Returns a C string pointer to the response JSON, caller must free with DM_FreeString
 *   Response format: `{"id":"req_id","type":"verify","data":{"success":true,"is_online":true,"verification":{...}}}`
 * - Failure: Returns null pointer, error can be retrieved via DM_GetLastError
 */
char *DM_Verify(const char *json_data);

/**
 * Activate license
 *
 * Sends an activation request to the launcher via Named Pipe.
 *
 * # Parameters
 * - `json_data`: JSON formatted request data (C string), must contain `nonce_str` field
 *   Example: `{"nonce_str": "random_string_32chars"}`
 *
 * # Returns
 * - Success: Returns a C string pointer to the response JSON, caller must free with DM_FreeString
 *   Response format: `{"id":"req_id","type":"activate","data":{"success":true,"activation":{...}}}`
 * - Failure: Returns null pointer, error can be retrieved via DM_GetLastError
 */
char *DM_Activate(const char *json_data);

/**
 * Notify the launcher that the program has finished initialization
 *
 * # Returns
 * - 0: Success
 * - -1: Notification failed
 * - -2: Communication error
 * - -3: Not connected
 */
int32_t DM_Initiated(void);

/**
 * Get DLL version
 */
const char *DM_GetVersion(void);

/**
 * Check if the program was launched by the launcher, if not, restart via launcher
 *
 * This function checks if the `DM_PIPE` environment variable is set.
 * If set, it means the program was launched by the launcher and returns 0.
 * If not set, it will try to find the launcher executable in the parent directory
 * and launch it, then exit the current process.
 *
 * # Returns
 * - 0: Program was launched by launcher, no restart needed
 * - 1: Program was NOT launched by launcher, launcher has been started and current process should exit
 * - -1: Failed to find or start launcher
 * - -2: Failed to get current executable path
 */
int32_t DM_RestartAppIfNecessary(void);

/**
 * Write a mini dump file for crash diagnostics
 */
int32_t DM_WriteMiniDump(const char *crash_info);

/**
 * Get the last error message
 *
 * # Returns
 * - Success: Returns a C string pointer to the error message, caller must free with DM_FreeString
 * - Failure: Returns null pointer
 */
char *DM_GetLastError(void);

/**
 * Free a string returned by DM_GetLastError or other DM_* functions
 *
 * # Safety
 * Caller must ensure the pointer was allocated by this library
 */
void DM_FreeString(char *ptr);

/**
 * Convert JSON string to canonical format (sorted keys)
 *
 * This function takes a JSON string, parses it into a map, and serializes it back
 * to JSON in canonical format (sorted keys). This ensures consistency without
 * performing any hashing or verification.
 *
 * # Parameters
 * - `json_str`: JSON formatted string (C string)
 *
 * # Returns
 * - Success: Returns a C string pointer to the canonical JSON, caller must free with DM_FreeString
 * - Failure: Returns null pointer, error can be retrieved via DM_GetLastError
 */
char *DM_JsonToCanonical(const char *json_str);

/**
 * Check for updates (Electron-compatible API)
 *
 * Sends request to launcher and returns raw JSON response.
 * Caller must free the returned string with DM_FreeString.
 */
char *DM_CheckForUpdates(const char *options_json);

/**
 * Alias of DM_CheckForUpdates
 */
char *DM_CheckForUpdate(const char *options_json);

/**
 * Download update package (Electron-compatible API)
 *
 * Sends request to launcher and returns raw JSON response.
 * Caller must free the returned string with DM_FreeString.
 */
char *DM_DownloadUpdate(const char *options_json);

/**
 * Cancel update download task
 *
 * Sends request to launcher and returns raw JSON response.
 * Caller must free the returned string with DM_FreeString.
 */
char *DM_CancelUpdateDownload(const char *options_json);

/**
 * Get current update state
 *
 * Returns raw JSON response from launcher.
 * Caller must free the returned string with DM_FreeString.
 */
char *DM_GetUpdateState(void);

/**
 * Get post-update information
 *
 * Returns raw JSON response from launcher.
 * Caller must free the returned string with DM_FreeString.
 */
char *DM_GetPostUpdateInfo(void);

/**
 * Acknowledge post-update information
 *
 * Returns raw JSON response from launcher.
 * Caller must free the returned string with DM_FreeString.
 */
char *DM_AckPostUpdateInfo(const char *options_json);

/**
 * Wait for update state changes
 *
 * Returns raw JSON response from launcher.
 * Caller must free the returned string with DM_FreeString.
 */
char *DM_WaitForUpdateStateChange(uint64_t last_sequence, uint32_t timeout_ms);

/**
 * Request quit-and-install workflow
 *
 * # Returns
 * - 1: Request accepted, caller should exit app process soon
 * - -1: Request rejected
 * - -2: Communication error
 */
int32_t DM_QuitAndInstall(const char *options_json);

/* ================= distromate License API ================= */

typedef const char *CSTRTYPE;
typedef char *STRTYPE;

typedef void (*CallbackType)(void);

#define LA_USER 0
#define LA_SYSTEM 1

/* ================= 初始化与配置 (必须先调用) ================= */
int32_t SetProductData(CSTRTYPE productData);
int32_t SetProductId(CSTRTYPE productId, uint32_t flags);
int32_t SetDataDirectory(CSTRTYPE directoryPath);
int32_t SetDebugMode(uint32_t enable);
int32_t SetCustomDeviceFingerprint(CSTRTYPE fingerprint);

/* ================= 许可证激活 (License Activation) ================= */
int32_t SetLicenseKey(CSTRTYPE licenseKey);
int32_t SetLicenseCallback(CallbackType callback);
int32_t SetActivationMetadata(CSTRTYPE key, CSTRTYPE value);
int32_t ActivateLicense(void);
int32_t ActivateLicenseOffline(CSTRTYPE filePath);
int32_t GenerateOfflineDeactivationRequest(CSTRTYPE filePath);
int32_t GetLastActivationError(uint32_t *errorPtr);

/* ================= 验证与状态 (Validation - 每次启动调用) ================= */
int32_t IsLicenseGenuine(void);
int32_t IsLicenseValid(void);
int32_t GetServerSyncGracePeriodExpiryDate(uint32_t *expiryDate);
int32_t GetActivationMode(STRTYPE initialMode,
                          uint32_t initialModeLength,
                          STRTYPE currentMode,
                          uint32_t currentModeLength);

/* ================= 获取许可信息 (Get Info) ================= */
int32_t GetLicenseKey(STRTYPE licenseKey, uint32_t length);
int32_t GetLicenseExpiryDate(uint32_t *expiryDate);
int32_t GetLicenseCreationDate(uint32_t *creationDate);
int32_t GetLicenseActivationDate(uint32_t *activationDate);
int32_t GetActivationCreationDate(uint32_t *activationCreationDate);
int32_t GetActivationLastSyncedDate(uint32_t *activationLastSyncedDate);
int32_t GetActivationId(STRTYPE id, uint32_t length);

/* ================= 其他 (Misc) ================= */
int32_t GetLibraryVersion(STRTYPE libraryVersion, uint32_t length);
int32_t Reset(void);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  /* DM_API_H */
