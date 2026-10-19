# Tuning Persistence Implementation Plan

**Goal:** Retain acknowledged live parameters across power loss.
**Architecture:** A host-tested two-sector journal is adapted to STM32 Flash. Runtime tuning owns dirty/persisted snapshots; a stopped-control gate allows the telemetry task to save. The browser polls status without touching drafts.
**Tech Stack:** C/FreeRTOS/STM32 HAL, Node test runner, Playwright CLI.

- [x] Write failing journal tests for torn writes, corrupt records, rollover, deduplication and recovery.
- [x] Implement tuning_storage.h/.c and checked STM32 adapter tuning_flash.h/.c; reserve sectors 10/11 in linker and flashing selection.
- [x] Extend runtime core restore/status/retry APIs and test restore precedence and revision-safe completion.
- [x] Add fresh stopped-motor gate and schedule deferred saving from telemetry task.
- [x] Extend browser protocol, separate persistence status, polling and retry; test pending/error/saved and draft preservation.
- [x] Update documentation, run C/Node/browser/build validation and request independent review.
- [x] Commit only task changes, preserving original worktree edits.

Validation: 46 Node tests passed; USB transport/core/journal, Flash adapter and DJI gate C tests passed. Journal tests inject failure at every word boundary during append and sector rotation, and cover partial erase, CRC/commit corruption, duplicate suppression and sequence wrap. The actual HAL adapter runs against simulated Flash with scheduler/gate assertions and restore/retry checks. Browser persistence, existing tuning and monitor tests passed, including 320px layout and draft retention. Telemetry and vision ARM builds passed; Flash load sections end below 0x080C0000. Independent review's source-mode isolation issue was fixed and re-reviewed; no remaining Important/Critical findings. No board flashing or physical power-cycle acceptance was performed.
