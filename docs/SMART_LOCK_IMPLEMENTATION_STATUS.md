# Smart Lock Implementation Status

## Summary

This document summarizes the work completed after integrating the backend, web panel, iOS app, and ESP32 firmware flows. The main architectural change is that user enrollment, fingerprint verification, lock state reporting, and user deletion are now driven by ESP32 hardware events through backend APIs.

The project now assumes a single ESP32 lock device using:

- `device_id = lock-1`
- `backend user.id == ESP32 fingerprint template id`
- ESP32 polling for backend hardware commands
- iOS/web communicating only with the backend, not directly with ESP32

## Completed Work

### 1. ESP32 Fingerprint Enrollment Flow

Previously, adding a user from web/iOS only created a backend DB user. There was no hardware enrollment flow.

Now:

- `POST /api/users` creates a user with `PENDING_ENROLLMENT`.
- Backend creates a `HardwareCommand` with type `ENROLL_FINGERPRINT`.
- ESP32 polls:
  - `GET /api/hardware/commands/next?device_id=lock-1`
- ESP32 receives the command and starts fingerprint enrollment.
- ESP32 uses the backend `user_id` as the fingerprint template ID.
- ESP32 reports result to:
  - `POST /api/hardware/commands/:id/result`
- Backend updates user status:
  - success -> `ENROLLED`
  - failure -> `ENROLL_FAILED`

Web/iOS now show enrollment badges:

- `Pending`
- `Enrolled`
- `Failed`

### 2. Automatic Fingerprint Verification

Previously, firmware verification depended on the Serial `v` command. That meant real access attempts were not automatically detected.

Now:

- ESP32 loop performs lightweight fingerprint presence polling.
- `hasFinger()` checks whether a finger is present.
- If no finger is present, no failed backend log is created.
- If a finger is detected, `verifyFingerprint()` runs automatically.
- Success creates backend access log:
  - `success: true`
  - `status: "Access granted"`
  - `fail_count: 0`
  - `user_id: matched id`
- Failure creates backend access log:
  - `success: false`
  - `status: "Failed fingerprint"`
  - `fail_count: failedAttempts`
  - `user_id: null`
- Cooldown prevents one long finger press from generating repeated logs.
- Enrollment mode pauses automatic verification.

Debug logs were added to make hardware testing easier later:

- `[AUTO]`
- `[FP]`
- `[LOG]`
- `[SEC]`
- `[LOCK]`
- `[IOT]`

### 3. Real Lock State Reporting

Previously, dashboard door status was inferred from the last access log. This was wrong because a successful access log does not mean the door is still unlocked.

Now:

- Backend has a persistent `SystemState` model.
- ESP32 reports real lock state to:
  - `POST /api/hardware/state`
- Accepted values:
  - `Locked`
  - `Unlocked`
- `/api/status` still has the same response shape for web/iOS, but `lock_status` now comes from `SystemState`.
- `last_event` still comes from the latest access log.
- `fail_count` still comes from the latest access log.

ESP32 reports state when:

- successful access unlocks the door -> `Unlocked`, `Access granted`
- auto relock happens -> `Locked`, `Auto relock`
- manual `o` command unlocks -> `Unlocked`, `Manual unlock`
- manual `k` command locks -> `Locked`, `Manual lock`

Auto hardware test mode does not send backend state, so it does not pollute dashboard state.

Debug logs were added on both sides:

- Backend:
  - `[HW_STATE] Received lock state...`
  - `[HW_STATE] Persisted lock state...`
  - `[STATUS] lock_status=...`
- ESP32:
  - prints request URL
  - prints JSON body
  - prints HTTP status
  - prints backend response body

### 4. User Delete Now Deletes ESP32 Fingerprint Record

Previously, `DELETE /api/users/:id` immediately deleted the backend DB user, but the fingerprint record could remain on ESP32. This was a security issue.

Now:

- `DELETE /api/users/:id` no longer immediately deletes the user.
- Backend sets user status to `PENDING_DELETE`.
- Backend creates a `DELETE_FINGERPRINT` hardware command.
- Duplicate pending/claimed delete commands are not created for the same user.
- ESP32 polls and receives `DELETE_FINGERPRINT`.
- ESP32 calls `fpManager.deleteID(template_id)`.
- ESP32 reports result through the existing command result endpoint.

On success:

- command becomes `DONE`
- backend user is hard-deleted
- command history remains because `HardwareCommand.user_id` is now nullable

On failure:

- command becomes `FAILED`
- user status becomes `DELETE_FAILED`
- error message is stored

Important firmware fix:

- `deleteID(id)` no longer only calls `_finger->deleteModel(id)`.
- It now also removes the actual ESP32 NVS records:
  - `fp_<id>`
  - `ex_<id>`
- It decrements the stored count when the record existed.
- Sensor flash delete remains best-effort because this firmware primarily verifies against NVS.

Web/iOS now show:

- `Deleting`
- `Delete failed`

Delete debug logs were added:

- Backend:
  - `[HW_COMMAND] Delete queued...`
  - `[HW_COMMAND] Claimed...`
  - `[HW_COMMAND] Result received...`
  - `[HW_COMMAND] Fingerprint delete completed...`
  - `[HW_COMMAND] Fingerprint delete failed...`
- ESP32:
  - command received
  - delete started
  - NVS record found/not found
  - sensor flash delete result
  - backend result send

### 5. Web/iOS Hardware Result Feedback

Previously, web/iOS showed `Pending` or `Deleting` after Add/Delete, but did not automatically detect when ESP32 later completed or failed the hardware command.

Now:

- Web Users page polls every 3 seconds while any user is:
  - `PENDING_ENROLLMENT`
  - `PENDING_DELETE`
- iOS Users screen starts polling while visible if pending hardware work exists.
- Polling stops automatically when no pending hardware work remains.
- Polling is silent, so the whole list does not enter a loading state every 3 seconds.
- Status transitions now produce user-facing feedback:
  - `PENDING_ENROLLMENT -> ENROLLED`
    - success/info message
  - `PENDING_ENROLLMENT -> ENROLL_FAILED`
    - error message
  - `PENDING_DELETE -> user missing`
    - success/info message
  - `PENDING_DELETE -> DELETE_FAILED`
    - error message

This means ESP32 enrollment/delete results now reach the user through web/iOS without requiring manual refresh.

## Verification Completed

Backend/web checks completed:

- `node --check index.js`
- `npx prisma validate`
- `npx prisma db push`
- web `npm run build`

Backend smoke tests completed:

- user creation creates `PENDING_ENROLLMENT`
- enrollment success sets user `ENROLLED`
- delete queues `DELETE_FINGERPRINT`
- duplicate delete does not create duplicate command
- ESP32 poll simulation claims delete command
- failed delete result sets `DELETE_FAILED`
- retry delete creates a new command
- successful delete result hard-deletes user
- completed command remains with `user_id = null`
- lock state endpoint updates `/api/status.lock_status`
- web Users page builds with hardware polling feedback

Firmware checks completed:

- `git diff --check`

Not completed:

- ESP32 firmware was not compiled locally because PlatformIO/Arduino build tooling is not installed in this Windows environment.
- iOS was not built locally because Xcode is not available on this machine.
- Real hardware testing has not been done yet.

## Remaining Issues

### 1. Enrollment Retry Flow ✅ COMPLETED

Implemented:

- `POST /api/users/:id/enrollment/retry` added to backend.
- Returns 409 if user is not `ENROLL_FAILED`.
- Transaction: sets user back to `PENDING_ENROLLMENT`, deduplicates existing PENDING/CLAIMED `ENROLL_FINGERPRINT` commands before creating a new one.
- Web `UsersPage.jsx`: `handleRetry()` added, orange "Retry Enrollment" button shown in detail drawer when `enrollment_status === 'ENROLL_FAILED'`.
- iOS `APIUserService`: `retryEnrollment(backendId:)` method added.
- iOS `UsersViewModel`: `retryEnrollment(_ user:)` optimistically sets status to `PENDING_ENROLLMENT` and starts polling.
- iOS `UserDetailSheet`: `onRetry` parameter added, "Retry Enrollment" button shown above delete button for `ENROLL_FAILED` non-admin users. Sheet height increased from 320 to 400.
- No ESP32 firmware changes required — `enrollFingerprint(id)` is idempotent and safe to call again with the same template ID.

### 2. Delete Retry Is Basic ✅ COMPLETED

Implemented:

- No separate endpoint was needed — `DELETE /api/users/:id` already resets status to `PENDING_DELETE` and deduplicates commands regardless of current status, making it a safe idempotent retry.
- Web `UsersPage.jsx`: detail drawer now shows "Retry Delete" (orange) instead of "Delete User" (red) when `enrollment_status === 'DELETE_FAILED'`.
- iOS `UserDetailSheet`: delete button now shows "Retry Delete" with `arrow.clockwise` icon and orange styling when `enrollmentStatus == "DELETE_FAILED"`.

### 3. Lockout / Multiple Failed Attempts ✅ COMPLETED

Implemented in `main.cpp`:

- `LOCKOUT_THRESHOLD = 3` — 3 ardışık başarısız denemede lockout tetiklenir.
- `LOCKOUT_DURATION_MS = 30000` — 30 saniye kilit.
- `lockedUntil` global değişkeni ile kilit bitiş zamanı takip edilir.
- `handleFingerprintAccess()` başında guard var: lockout aktifse LCD'de kalan süre gösterilir, return edilir.
- Failure branch'de `failedAttempts >= LOCKOUT_THRESHOLD` olunca:
  - `lockedUntil` set edilir.
  - LCD: "SISTEM KILITLI / 30 sn bekleyin"
  - Çift buzzer.
  - Backend'e `sendLockState("Locked", "Lockout")` gönderilir.
  - `failedAttempts` sıfırlanır.
- `handleAutomaticFingerprintScan()` her döngüde lockout kontrolü yapar, kalan süreyi saniye saniye LCD'ye yazar.
- Lockout bitince LCD otomatik "Sistem Hazir" e döner.
- Başarılı giriş `failedAttempts = 0` zaten yapıyor (değişiklik gerekmedi).
- Backend tarafı değişiklik gerektirmedi — `sendLockState` ve `sendAccessLog` zaten lockout logunu kapsamaktadır.

### 4. Alerts Source Is Still Split ✅ COMPLETED

Implemented:

- Single alert source: everything now goes through `GET /api/alerts` (backend `Alert` table).
- Read state is persisted on the backend (`status: UNREAD/READ`) — no more localStorage or in-memory cache drift.
- Web `AlertsPage`: rewrote to use `GET /api/alerts`. Filters: All / Critical / Warning / Unread. "Mark Read" per row, "Mark All Read" button. Unread rows highlighted with blue dot.
- Web `Layout.jsx`: banner now shows UNREAD alerts from backend instead of log-derived failures. "Got it / Review Alerts" both call `PATCH /api/alerts/read-all`. Removed `localStorage` last-seen timestamp logic.
- iOS `APIAlertService`: rewrote `fetchAlerts()` to call `GET /api/alerts`. Maps `AlertDTO` → `SmartAlert`. `markAsRead` and `dismiss` fire `PATCH /api/alerts/:id/read` to backend. No more log-derived alert creation.
- `SmartAlert` model: added `backendId: Int` to carry the backend integer ID for PATCH calls.
- `MockAlertService`: updated with sequential `backendId` values (1–8) to match new struct.
- Debug logs added: backend `[ALERT]` prefix on all 3 endpoints (GET, read-all, read/:id). Web `[Layout]` logs on load and acknowledge. iOS `[AlertsVM]` logs on markAsRead, dismiss, markAllAsRead, clearBadge.

### 5. Access Log User Validation Is Weak

Current behavior:

- ESP32 sends `user_id` from fingerprint match.
- Backend accepts the access log.

Risk:

- If ESP32 sends a stale/deleted user ID, backend behavior should be clearly defined.

Needed:

- Backend should validate that successful logs reference an existing user.
- If user does not exist, either reject the log or convert it into a failed/unknown access event.

### 6. ESP32 Offline Queue Is Missing

Current behavior:

- If WiFi/backend is unavailable, ESP32 continues local behavior.
- But access logs, lock state updates, and command results may be lost.

Needed:

- For stronger reliability, ESP32 should keep a small local queue for unsent events.
- V1 can skip this if demo network is stable.

### 7. Security For Hardware Endpoints Is Minimal

Current behavior:

- Hardware endpoints are reachable by URL.
- There is no hardware API key or device secret shown in the current implementation.

Needed:

- Add simple shared secret header for ESP32:
  - `X-Device-Key`
- Backend validates it for:
  - command polling
  - command result
  - lock state updates
  - access logs if desired

### 8. Manual Firmware Commands Can Drift From Backend State

Current behavior:

- Manual `o` and `k` now report lock state.
- Manual local enrollment `e` still exists and can create local fingerprint records without backend user records.
- Manual `c` deletes all local fingerprints without syncing backend users.

Needed:

- Keep these commands as debug-only.
- For final demo, avoid using `e` and `c` unless backend DB is reset accordingly.
- Optionally guard dangerous commands behind a debug flag.

### 9. Real Hardware Compile/Test Is Still Required

Current behavior:

- Firmware code was inspected and syntax/diff sanity checked.
- It was not compiled or flashed here.

Needed during hardware testing:

- Configure `include/network_config.local.h`.
- Confirm WiFi connects.
- Confirm ESP32 can reach backend PC local IP.
- Confirm enrollment command appears on LCD/Serial.
- Confirm NVS enrollment and delete both work.
- Confirm success/failure logs appear in backend/web/iOS.
- Confirm real lock state changes appear on dashboard.

## Current Priority Order

Recommended next work:

1. Build/flash firmware with hardware attached.
2. Test Add User -> ESP32 enroll -> user becomes `ENROLLED`.
3. Test fingerprint success/failure logs from real ESP32.
4. Test lock state `Unlocked` then `Locked` from real relay flow.
5. Test Delete User -> ESP32 delete -> user disappears.
6. ~~Add retry enrollment UI/API.~~ ✅ Done
7. ~~Add retry delete UI.~~ ✅ Done
8. ~~Firmware lockout after 3 failed attempts (30s).~~ ✅ Done
9. ~~Unify alert source to backend Alert table.~~ ✅ Done
10. Add simple hardware API key.
