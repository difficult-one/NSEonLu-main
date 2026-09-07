# F334 super capacitor driver

This module connects the F407 chassis application to the F334 super capacitor
controller over classic CAN.

## Interface

```c
SuperCapInstance *SuperCapInit(const SuperCap_Init_Config_s *config);
bool SuperCapSendCommand(SuperCapInstance *instance,
                         const SuperCapCommand_s *command);
bool SuperCapGetStatus(SuperCapInstance *instance,
                       SuperCapStatus_s *status);
```

`SuperCapSendCommand()` transmits an 8-byte command frame on standard ID
`0x061`. `SuperCapGetStatus()` returns the most recently decoded 8-byte status
frame received on standard ID `0x051`, together with the daemon-derived online
state. The controller is considered offline when no valid status frame has been
received for 200 ms.

## Initialization example

```c
SuperCap_Init_Config_s config = {
    .can_config = {
        .can_handle = &hcan1,
        .tx_id = SUPERCAP_COMMAND_CAN_ID,
        .rx_id = SUPERCAP_STATUS_CAN_ID,
    },
    .offline_reload_count = 20U,
};

SuperCapInstance *supercap = SuperCapInit(&config);
```

## Sending a command

```c
SuperCapCommand_s command = {
    .enable_dcdc = true,
    .referee_power_limit_w = 100U,
    .referee_buffer_energy_j = 50U,
};

SuperCapSendCommand(supercap, &command);
```

The byte-level protocol and power-budget behavior are documented in
`docs/supercap-can-protocol.md`. The codec implementation in
`super_cap_protocol.c` is the single source of truth for packing and unpacking
frames.
