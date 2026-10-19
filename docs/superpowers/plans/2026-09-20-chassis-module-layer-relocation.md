# Chassis Module Layer Relocation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move every auxiliary file out of `application/chassis` so only `chassis.c` and `chassis.h` remain, while preserving all control behavior and exposing power feedback through a module-owned singleton.

**Architecture:** Pure chassis kinematics live in `modules/algorithm`, the referee-to-super-cap command adapter lives in `modules/super_cap`, and power-feedback aggregation becomes part of `power_model`. `application/chassis/chassis.c` remains the orchestration layer and accesses feedback only through init/update/get APIs.

**Tech Stack:** C11, GNU Make, ARM GCC firmware build, host-side GCC tests.

## Global Constraints

- Keep all existing kinematics formulas, CAN protocol data, control periods, and published message layout unchanged.
- Chassis helper code moved into `modules/algorithm` and `modules/super_cap` must not include `application/robot_def.h`; pre-existing referee UI and vision configuration dependencies are outside this relocation.
- Store `static Chassis_Power_Control_Data_s *chassis_power_feedback;` near the top of `modules/algorithm/power_model.c`.
- Use `malloc()` once in `ChassisPowerFeedbackInit()`; repeated initialization returns `false` without replacing the instance.
- Do not add FreeRTOS dependencies or new tasks.
- Preserve unrelated user changes and do not commit build artifacts.

---

### Task 1: Lock the new power-feedback API with failing tests

**Files:**
- Modify: `tests/power_model/test_power_model.c`
- Delete after merge: `tests/chassis_power_feedback/test_chassis_power_feedback.c`
- Delete after merge: `tests/chassis_power_feedback/Makefile`
- Delete after merge: `tests/chassis_power_feedback/stubs/ins_task.h`
- Delete after merge: `tests/chassis_power_feedback/stubs/master_process.h`

**Interfaces:**
- Consumes: `ChassisPowerOutput_s`, `PowerModelStatus_e`.
- Produces: tests for `bool ChassisPowerFeedbackInit(void)`, `void ChassisPowerFeedbackUpdate(bool, const ChassisPowerOutput_s *)`, and `bool ChassisPowerFeedbackGet(Chassis_Power_Control_Data_s *)`.

- [ ] **Step 1: Merge feedback behavior tests into the power-model suite**

  Add compile-time layout checks for the three-float `Chassis_Power_Control_Data_s` and runtime cases that verify: get-before-init clears output and fails; update-before-init is harmless; first init succeeds; second init fails; valid four-wheel output is summed; unregistered, null, invalid-status, and non-finite input clear the stored snapshot; null output passed to get fails.

  Use the new call sequence in every case:

  ```c
  ChassisPowerFeedbackUpdate(true, &power);
  ASSERT_TRUE(ChassisPowerFeedbackGet(&feedback));
  ```

- [ ] **Step 2: Run the suite and verify RED**

  Run: `make -C tests/power_model clean test`

  Expected: compilation fails because `Chassis_Power_Control_Data_s` and the three new feedback functions are not yet declared in `power_model.h`.

- [ ] **Step 3: Keep the failing tests for the implementation task**

  Do not weaken assertions or add test-only production stubs. The test process owns one singleton, so run pre-init and first-init cases before all post-init cases.

### Task 2: Move kinematics and power-link modules

**Files:**
- Move: `application/chassis/chassis_kinematics.c` → `modules/algorithm/chassis_kinematics.c`
- Move: `application/chassis/chassis_kinematics.h` → `modules/algorithm/chassis_kinematics.h`
- Move: `application/chassis/chassis_power_link.c` → `modules/super_cap/chassis_power_link.c`
- Move: `application/chassis/chassis_power_link.h` → `modules/super_cap/chassis_power_link.h`
- Modify: `tests/chassis_kinematics/Makefile`
- Modify: `tests/chassis_power_link/Makefile`
- Modify: `tests/referee_integration/Makefile`

**Interfaces:**
- Consumes: existing `RefereePowerSnapshot_s` and `SuperCapCommand_s` definitions.
- Produces: unchanged `ChassisMecanumCalculate`, `ChassisOmniXCalculate`, and `ChassisPowerLinkBuild` symbols from module-layer paths.

- [ ] **Step 1: Point host tests at the intended module paths**

  Replace `../../application/chassis/chassis_kinematics.c` with `../../modules/algorithm/chassis_kinematics.c`; replace each `../../application/chassis/chassis_power_link.c` with `../../modules/super_cap/chassis_power_link.c`. Remove `-I../../application/chassis` where it is no longer needed and retain/add `-I../../modules/algorithm` or `-I../../modules/super_cap` as appropriate.

- [ ] **Step 2: Run tests and verify the relocation is RED**

  Run:

  ```powershell
  make -C tests/chassis_kinematics clean test
  make -C tests/chassis_power_link clean test
  make -C tests/referee_integration clean test
  ```

  Expected: source/header-not-found failures because the module files have not moved yet.

- [ ] **Step 3: Move the four files without changing implementation**

  Move each source/header pair to the exact paths above. Keep public names and include guards intact so consumers need no API changes.

- [ ] **Step 4: Run tests and verify GREEN**

  Run the same three commands. Expected: all three suites print `PASS`.

- [ ] **Step 5: Commit the isolated relocation**

  ```powershell
  git add application/chassis modules/algorithm modules/super_cap tests/chassis_kinematics tests/chassis_power_link tests/referee_integration
  git commit -m "refactor: move chassis helpers into modules"
  ```

### Task 3: Merge power feedback into `power_model`

**Files:**
- Modify: `modules/algorithm/power_model.h`
- Modify: `modules/algorithm/power_model.c`
- Modify: `application/robot_def.h`
- Modify: `application/chassis/chassis.c`
- Modify: `tests/power_model/test_power_model.c`
- Delete: `application/chassis/chassis_power_feedback.c`
- Delete: `application/chassis/chassis_power_feedback.h`
- Delete: `tests/chassis_power_feedback/test_chassis_power_feedback.c`
- Delete: `tests/chassis_power_feedback/Makefile`
- Delete: `tests/chassis_power_feedback/stubs/ins_task.h`
- Delete: `tests/chassis_power_feedback/stubs/master_process.h`

**Interfaces:**
- Consumes: `ChassisPowerOutput_s` produced by DJI chassis power control.
- Produces:

  ```c
  typedef struct
  {
      float predicted_unlimited_power_w;
      float predicted_limited_power_w;
      float effective_limit_w;
  } Chassis_Power_Control_Data_s;

  bool ChassisPowerFeedbackInit(void);
  void ChassisPowerFeedbackUpdate(bool registered,
                                  const ChassisPowerOutput_s *power);
  bool ChassisPowerFeedbackGet(Chassis_Power_Control_Data_s *feedback);
  ```

- [ ] **Step 1: Declare the data type and API in `power_model.h`**

  Place the feedback structure after `ChassisPowerOutput_s`, then declare init/update/get with the exact signatures above. Remove the duplicate structure from `robot_def.h` and include `power_model.h` there so `Chassis_Upload_Data_s.power_control` keeps the same concrete layout.

- [ ] **Step 2: Implement the module-owned singleton**

  Add `<stdlib.h>` to `power_model.c` and place this at file scope near the existing constants:

  ```c
  static Chassis_Power_Control_Data_s *chassis_power_feedback;
  ```

  `ChassisPowerFeedbackInit()` rejects an existing pointer, allocates `sizeof(*chassis_power_feedback)`, clears it, and returns whether allocation succeeded. `ChassisPowerFeedbackUpdate()` returns immediately before initialization; otherwise it clears stored data first, accepts only registered/non-null/OK input, sums all four unlimited and limited values, copies the effective limit, and clears again if any result is non-finite. `ChassisPowerFeedbackGet()` rejects null output, clears a non-null caller buffer before checking initialization, then copies the stored structure and returns `true`.

- [ ] **Step 3: Run power-model tests and verify GREEN**

  Run: `make -C tests/power_model clean test`

  Expected: `PASS: power model tests`.

- [ ] **Step 4: Update chassis orchestration**

  Remove `#include "chassis_power_feedback.h"`. Add a file-scope `static bool chassis_power_feedback_ready;`, assign it from `ChassisPowerFeedbackInit()` in `ChassisInit()`, then replace direct external-buffer update with:

  ```c
  ChassisPowerFeedbackUpdate(
      power_state_valid && power_state.registered,
      power_state_valid ? &power_state.power : NULL);
  (void)ChassisPowerFeedbackGet(&chassis_feedback_data.power_control);
  ```

  The ready flag records initialization status for diagnosis; update/get retain their own safety checks.

- [ ] **Step 5: Delete the obsolete source, header, and standalone test suite**

  Delete only the six explicitly listed tracked files. Confirm `rg "chassis_power_feedback|ChassisPowerFeedbackUpdate"` finds no obsolete include or old three-argument call.

- [ ] **Step 6: Commit the feedback encapsulation**

  ```powershell
  git add application/chassis application/robot_def.h modules/algorithm/power_model.c modules/algorithm/power_model.h tests/power_model tests/chassis_power_feedback
  git commit -m "refactor: encapsulate chassis power feedback"
  ```

### Task 4: Finish tree cleanup and build integration

**Files:**
- Move: `application/chassis/chassis.md` → `docs/chassis.md`
- Delete: `application/chassis/balance.h`
- Delete: `application/chassis/mecanum.h`
- Delete: `application/chassis/steering.h`
- Modify: `Makefile`

**Interfaces:**
- Consumes: the relocated module source paths from Tasks 2 and 3.
- Produces: a root build whose chassis application directory contains only `chassis.c` and `chassis.h`.

- [ ] **Step 1: Update root source paths**

  In `Makefile`, replace application-layer kinematics and power-link sources with `modules/algorithm/chassis_kinematics.c` and `modules/super_cap/chassis_power_link.c`, and remove `application/chassis/chassis_power_feedback.c`. Do not add include paths because both module directories are already included.

- [ ] **Step 2: Move documentation and delete the three verified empty headers**

  Move `chassis.md` to `docs/chassis.md`. Delete exactly `balance.h`, `mecanum.h`, and `steering.h`; they are empty and unreferenced.

- [ ] **Step 3: Verify the dependency and directory invariants**

  Run:

  ```powershell
  rg "robot_def.h" modules/algorithm modules/super_cap
  Get-ChildItem application/chassis -File | Select-Object -ExpandProperty Name
  ```

  Expected: the first command has no matches; the second prints only `chassis.c` and `chassis.h`.

- [ ] **Step 4: Run all affected host tests**

  Run clean/test in `tests/chassis_kinematics`, `tests/chassis_power_link`, `tests/referee_integration`, `tests/power_model`, `tests/chassis_power_budget`, `tests/dji_motor_power_outer_loop`, `tests/dji_motor_power_buffer`, `tests/power_meter`, and `tests/super_cap`. Expected: every suite passes.

- [ ] **Step 5: Build both firmware configurations**

  Run:

  ```powershell
  make clean
  make -j4 POWER_METER_ENABLE=1
  make clean
  make -j4 POWER_METER_ENABLE=0
  ```

  Expected: both builds exit zero and produce no new compile/link errors.

- [ ] **Step 6: Commit final integration**

  ```powershell
  git add Makefile application/chassis docs modules tests application/robot_def.h
  git commit -m "refactor: finish chassis module layer cleanup"
  ```

- [ ] **Step 7: Review the complete diff**

  Run `git status --short`, `git diff HEAD~3 --check`, and inspect `git diff HEAD~3 --stat`. Confirm no executable test files, firmware outputs, or unrelated files are staged.
