# STM32 J-Link Development Environment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Configure the STM32F407IG project for reproducible Arm GNU builds, J-Link flashing and Cortex-Debug debugging, plus an Ozone debugging entry point.

**Architecture:** Keep the existing Makefile as the single build source of truth. VS Code tasks call the installed tools as direct processes with explicit paths, Cortex-Debug uses SEGGER's native GDB server and the generated ELF, and Ozone loads that same ELF from a project script.

**Tech Stack:** Arm GNU Toolchain 13.3.Rel1, GNU Make 4.4.1, VS Code, Cortex-Debug 1.12.1, SEGGER J-Link V9.38a, SEGGER Ozone, STM32F407IG, FreeRTOS.

## Global Constraints

- Use the existing `Makefile`; do not introduce PlatformIO or migrate to CMake/Ninja.
- Target `STM32F407IG` over SWD at 4000 kHz.
- Build `build/basic_framework.elf`, `build/basic_framework.hex`, and `build/basic_framework.bin`.
- Use `D:\Arm_GNU_Toolchain\13.3 rel1\bin` for Arm GNU tools.
- Use `D:\mingw\mingw64\bin\mingw32-make.exe` for Make.
- Use `C:\Program Files\SEGGER\JLink_V938a` for J-Link tools.
- Use `C:\Program Files\SEGGER\Ozone` for Ozone.
- Do not modify application, BSP, module, HAL, startup, or linker-script code.
- Use command-level acceptance checks instead of unit-test TDD because this change consists of editor and debugger configuration.

---

### Task 1: Make and IntelliSense workspace configuration

**Files:**
- Modify: `.vscode/settings.json`
- Modify: `.vscode/c_cpp_properties.json`
- Modify: `.vscode/tasks.json`

**Interfaces:**
- Consumes: the existing root `Makefile` and installed Arm GNU/Make executables.
- Produces: VS Code tasks named `Build: firmware` and `Clean: firmware`, plus ARM-aware code completion.

- [ ] **Step 1: Record the existing build baseline**

Run:

```powershell
& 'D:\mingw\mingw64\bin\mingw32-make.exe' -j24
```

Expected: exit code `0` and all three files below exist:

```text
build/basic_framework.elf
build/basic_framework.hex
build/basic_framework.bin
```

- [ ] **Step 2: Replace obsolete workspace tool settings**

Keep the existing `files.associations` object and replace the obsolete Cube/OpenOCD entries in `.vscode/settings.json` with:

```json
"makefile.makePath": "D:\\mingw\\mingw64\\bin\\mingw32-make.exe",
"makefile.makeDirectory": "${workspaceFolder}",
"cortex-debug.armToolchainPath.windows": "D:\\Arm_GNU_Toolchain\\13.3 rel1\\bin",
"cortex-debug.gdbPath.windows": "D:\\Arm_GNU_Toolchain\\13.3 rel1\\bin\\arm-none-eabi-gdb.exe",
"cortex-debug.JLinkGDBServerPath.windows": "C:\\Program Files\\SEGGER\\JLink_V938a\\JLinkGDBServerCL.exe"
```

- [ ] **Step 3: Make IntelliSense use the cross-compiler**

Replace `.vscode/c_cpp_properties.json` with:

```json
{
    "configurations": [
        {
            "name": "STM32F407IG",
            "compilerPath": "D:\\Arm_GNU_Toolchain\\13.3 rel1\\bin\\arm-none-eabi-gcc.exe",
            "compilerArgs": [
                "-mcpu=cortex-m4",
                "-mthumb",
                "-mfpu=fpv4-sp-d16",
                "-mfloat-abi=hard"
            ],
            "includePath": [
                "${workspaceFolder}/**"
            ],
            "defines": [
                "USE_HAL_DRIVER",
                "STM32F407xx",
                "ARM_MATH_CM4",
                "DISABLE_LOG_SYSTEM"
            ],
            "cStandard": "c11",
            "cppStandard": "gnu++14",
            "intelliSenseMode": "windows-gcc-arm"
        }
    ],
    "version": 4
}
```

- [ ] **Step 4: Define direct-process build tasks**

Replace `.vscode/tasks.json` with strict JSON containing these two initial tasks:

```json
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "Build: firmware",
            "type": "process",
            "command": "D:\\mingw\\mingw64\\bin\\mingw32-make.exe",
            "args": ["-j24"],
            "options": {
                "cwd": "${workspaceFolder}",
                "env": {
                    "PATH": "D:\\Arm_GNU_Toolchain\\13.3 rel1\\bin;${env:PATH}"
                }
            },
            "problemMatcher": "$gcc",
            "group": {
                "kind": "build",
                "isDefault": true
            }
        },
        {
            "label": "Clean: firmware",
            "type": "process",
            "command": "D:\\mingw\\mingw64\\bin\\mingw32-make.exe",
            "args": ["clean"],
            "options": {
                "cwd": "${workspaceFolder}",
                "env": {
                    "PATH": "D:\\Arm_GNU_Toolchain\\13.3 rel1\\bin;${env:PATH}"
                }
            },
            "problemMatcher": []
        }
    ]
}
```

- [ ] **Step 5: Validate JSON and rebuild through the configured executables**

Run:

```powershell
Get-Content -Raw .vscode\settings.json | ConvertFrom-Json | Out-Null
Get-Content -Raw .vscode\c_cpp_properties.json | ConvertFrom-Json | Out-Null
Get-Content -Raw .vscode\tasks.json | ConvertFrom-Json | Out-Null
& 'D:\mingw\mingw64\bin\mingw32-make.exe' -j24
```

Expected: all JSON parses, Make exits `0`, and no compiler error or warning is emitted.

- [ ] **Step 6: Commit the build workspace configuration**

```powershell
git add -- .vscode/settings.json .vscode/c_cpp_properties.json .vscode/tasks.json
git commit -m "build: configure Arm GNU workspace"
```

---

### Task 2: J-Link flashing and Cortex-Debug

**Files:**
- Create: `scripts/flash.jlink`
- Modify: `.vscode/tasks.json`
- Modify: `.vscode/launch.json`

**Interfaces:**
- Consumes: task `Build: firmware` and `build/basic_framework.elf` from Task 1.
- Produces: task `Flash: J-Link`, debug configurations `J-Link: Build and debug` and `J-Link: Attach`.

- [ ] **Step 1: Add the J-Link Commander script**

Create `scripts/flash.jlink`:

```text
halt
reset
loadfile build/basic_framework.elf
verifybin build/basic_framework.bin 0x08000000
reset
go
exit
```

The explicit `verifybin` command makes verification visible even though `loadfile` already checks programming errors.

- [ ] **Step 2: Add the sequential build-and-flash task**

Append this task to `.vscode/tasks.json`:

```json
{
    "label": "Flash: J-Link",
    "type": "process",
    "command": "C:\\Program Files\\SEGGER\\JLink_V938a\\JLink.exe",
    "args": [
        "-device", "STM32F407IG",
        "-if", "SWD",
        "-speed", "4000",
        "-autoconnect", "1",
        "-CommanderScript", "${workspaceFolder}\\scripts\\flash.jlink"
    ],
    "options": {
        "cwd": "${workspaceFolder}"
    },
    "dependsOn": "Build: firmware",
    "dependsOrder": "sequence",
    "problemMatcher": []
}
```

- [ ] **Step 3: Replace the mixed-probe launch list with J-Link launch and attach**

Replace `.vscode/launch.json` with:

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "J-Link: Build and debug",
            "type": "cortex-debug",
            "request": "launch",
            "cwd": "${workspaceFolder}",
            "executable": "${workspaceFolder}\\build\\basic_framework.elf",
            "servertype": "jlink",
            "serverpath": "C:\\Program Files\\SEGGER\\JLink_V938a\\JLinkGDBServerCL.exe",
            "armToolchainPath": "D:\\Arm_GNU_Toolchain\\13.3 rel1\\bin",
            "gdbPath": "D:\\Arm_GNU_Toolchain\\13.3 rel1\\bin\\arm-none-eabi-gdb.exe",
            "device": "STM32F407IG",
            "interface": "swd",
            "serverArgs": ["-speed", "4000"],
            "svdFile": "${workspaceFolder}\\STM32F407.svd",
            "rtos": "FreeRTOS",
            "runToEntryPoint": "main",
            "preLaunchTask": "Build: firmware",
            "showDevDebugOutput": "none"
        },
        {
            "name": "J-Link: Attach",
            "type": "cortex-debug",
            "request": "attach",
            "cwd": "${workspaceFolder}",
            "executable": "${workspaceFolder}\\build\\basic_framework.elf",
            "servertype": "jlink",
            "serverpath": "C:\\Program Files\\SEGGER\\JLink_V938a\\JLinkGDBServerCL.exe",
            "armToolchainPath": "D:\\Arm_GNU_Toolchain\\13.3 rel1\\bin",
            "gdbPath": "D:\\Arm_GNU_Toolchain\\13.3 rel1\\bin\\arm-none-eabi-gdb.exe",
            "device": "STM32F407IG",
            "interface": "swd",
            "serverArgs": ["-speed", "4000"],
            "svdFile": "${workspaceFolder}\\STM32F407.svd",
            "rtos": "FreeRTOS",
            "showDevDebugOutput": "none"
        }
    ]
}
```

- [ ] **Step 4: Validate configuration fields against the installed extension**

Run:

```powershell
Get-Content -Raw .vscode\tasks.json | ConvertFrom-Json | Out-Null
Get-Content -Raw .vscode\launch.json | ConvertFrom-Json | Out-Null
rg -n '"(armToolchainPath|gdbPath|serverpath|serverArgs|interface|rtos|runToEntryPoint)"' 'C:\Users\20802\.vscode\extensions\marus25.cortex-debug-1.12.1\package.json'
Test-Path 'C:\Program Files\SEGGER\JLink_V938a\JLink.exe'
Test-Path 'C:\Program Files\SEGGER\JLink_V938a\JLinkGDBServerCL.exe'
Test-Path 'D:\Arm_GNU_Toolchain\13.3 rel1\bin\arm-none-eabi-gdb.exe'
```

Expected: JSON parsing succeeds, each configuration property is present in the extension schema, and every `Test-Path` prints `True`.

- [ ] **Step 5: Run the real J-Link flash check when the target is safely powered**

Before this step, ensure the robot's wheels, launcher, and other actuators cannot move unexpectedly. Then run:

```powershell
& 'C:\Program Files\SEGGER\JLink_V938a\JLink.exe' -device STM32F407IG -if SWD -speed 4000 -autoconnect 1 -CommanderScript scripts\flash.jlink
```

Expected: J-Link identifies the Cortex-M4 target, programs `basic_framework.elf`, `verifybin` reports success, and exits with code `0`. If the board is unavailable or unsafe to run, record this single check as pending instead of treating it as passed.

- [ ] **Step 6: Commit the J-Link configuration**

```powershell
git add -- scripts/flash.jlink .vscode/tasks.json .vscode/launch.json
git commit -m "build: add J-Link flash and debug tasks"
```

---

### Task 3: Ozone project and final verification

**Files:**
- Create: `debug_ozone.jdebug`
- Modify: `.vscode/tasks.json`

**Interfaces:**
- Consumes: task `Build: firmware` and `build/basic_framework.elf` from Task 1.
- Produces: task `Debug: Ozone` and a reusable Ozone project for STM32F407IG/FreeRTOS.

- [ ] **Step 1: Add the Ozone project script**

Create `debug_ozone.jdebug`:

```javascript
void OnProjectLoad(void) {
  Project.SetDevice("STM32F407IG");
  Project.SetHostIF("USB", "");
  Project.SetTargetIF("SWD");
  Project.SetTIFSpeed("4 MHz");
  Project.SetOSPlugin("FreeRTOSPlugin_CM4");
  File.Open("$(ProjectDir)/build/basic_framework.elf");
}
```

- [ ] **Step 2: Add the Ozone launch task**

Append this task to `.vscode/tasks.json`:

```json
{
    "label": "Debug: Ozone",
    "type": "process",
    "command": "C:\\Program Files\\SEGGER\\Ozone\\Ozone.exe",
    "args": ["${workspaceFolder}\\debug_ozone.jdebug"],
    "options": {
        "cwd": "${workspaceFolder}"
    },
    "dependsOn": "Build: firmware",
    "dependsOrder": "sequence",
    "problemMatcher": []
}
```

- [ ] **Step 3: Validate all tracked configuration and perform a clean build**

Run:

```powershell
Get-Content -Raw .vscode\settings.json | ConvertFrom-Json | Out-Null
Get-Content -Raw .vscode\c_cpp_properties.json | ConvertFrom-Json | Out-Null
Get-Content -Raw .vscode\tasks.json | ConvertFrom-Json | Out-Null
Get-Content -Raw .vscode\launch.json | ConvertFrom-Json | Out-Null
Test-Path 'C:\Program Files\SEGGER\Ozone\Ozone.exe'
& 'D:\mingw\mingw64\bin\mingw32-make.exe' clean
& 'D:\mingw\mingw64\bin\mingw32-make.exe' -j24
Get-Item build\basic_framework.elf, build\basic_framework.hex, build\basic_framework.bin | Select-Object Name, Length
git diff --check
git status --short
```

Expected: all JSON parses; Ozone exists; the clean rebuild exits `0`; ELF, HEX, and BIN have nonzero sizes; `git diff --check` is silent; status contains only the planned configuration, script, and plan files.

- [ ] **Step 4: Open the Ozone project for interactive verification**

Run the VS Code task `Debug: Ozone`, then in Ozone select Download & Reset and verify that symbols are loaded, `main` is available, and the FreeRTOS task view opens. If the target is unavailable, verify that Ozone loads the ELF and project settings offline and record hardware connection as pending.

- [ ] **Step 5: Commit the Ozone integration**

```powershell
git add -- debug_ozone.jdebug .vscode/tasks.json docs/superpowers/plans/2026-09-06-stm32-jlink-development-environment.md
git commit -m "build: add Ozone debug project"
```
