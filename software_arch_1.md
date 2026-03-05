# Software Architecture: STM32 USB-PD VDM Stack

**Target Hardware:** STM32G071RB (Cortex-M0+) on NUCLEO-G071RB + X-NUCLEO-SRC1M1 shield
**Firmware:** USB-PD Source with Apple proprietary VDM (DFU / Reboot commands)
**RTOS:** FreeRTOS V10.3.1 (CMSIS-OS v1 wrapper)
**PD Stack:** STM32 USBPD Middleware (PD3.0 Full, CM0+ binary library)

---

## 1. Background: USB Power Delivery Protocol

### 1.1 Physical Layer

USB-PD communication takes place on the **CC1** and **CC2** pins of the USB-C connector, not on the VBUS/GND/data lines. The protocol uses **BMC (Biphase Mark Coding)** at 300 kbps to encode messages.

- **CC lines** serve dual purposes: role detection (pull-up = SRC, pull-down = SNK) and PD message signalling.
- **VBUS** carries power (5V initially, negotiated voltage afterwards). The source must assert vSafe5V on VBUS before PD negotiation can begin.
- **SOP types** define which entity a message is addressed to:

| SOP Type | Target |
|---|---|
| SOP | Port partner (the connected device) |
| SOP' (SOP1) | Cable plug — plug nearest to SRC |
| SOP'' (SOP2) | Cable plug — plug nearest to SNK |
| SOP'Debug / SOP''Debug | Cable plug debug access (rarely used) |

Apple proprietary VDMs are sent over **SOP** — directly to the Apple device.

### 1.2 USB-PD Specification Layers

```
┌─────────────────────────────────────────────────────────┐
│                   APPLICATION / DPM                     │
│         Power policy decisions, VDM handling            │
├─────────────────────────────────────────────────────────┤
│           PE  —  Policy Engine                          │
│   Negotiation state machine (caps, request, VDM, reset) │
├──────────────────────┬──────────────────────────────────┤
│   CAD                │   PRL  —  Protocol Layer         │
│   Attach detection   │   Message framing, CRC, GoodCRC  │
├──────────────────────┴──────────────────────────────────┤
│           PHY  —  Physical Layer                        │
│   BMC encode/decode, SOP* framing, DMA                 │
└─────────────────────────────────────────────────────────┘
```

### 1.3 CAD — Cable Attachment Detection

**CAD** is the module responsible for monitoring the CC1/CC2 lines to determine connection state and port roles.

**What it does:**
- Applies pull-up (Rp) or pull-down (Rd) resistors to CC lines depending on role configuration.
- Measures the resulting voltage to detect when a device is connected.
- Determines whether the attached device is a Source, Sink, or DRP (Dual-Role Power).
- Fires events to the DPM: `USBPD_CAD_EVENT_ATTACHED`, `USBPD_CAD_EVENT_DETACHED`, `USBPD_CAD_EVENT_ATTEMC` (audio mode cable), `USBPD_CAD_EVENT_EMC` (electronic marked cable).

**In this project:**
CAD runs as a dedicated FreeRTOS task (`OS_CAD_STACK_SIZE = 400 words`) created by `usbpd_dpm_core.c`. When attachment is detected, it calls `USBPD_DPM_UserCableDetection()` in `USBPD/Target/usbpd_dpm_user.c`.

### 1.4 PE — Policy Engine

**PE** is the core USB-PD state machine defined in the USB-PD specification. It handles the entire negotiation flow once a cable attachment is confirmed.

**What it does:**
- As SRC: advertises Source Capabilities (PDOs), receives Request from sink, sends Accept + PS_RDY to establish an explicit contract.
- As SNK: waits for Source Capabilities, evaluates and sends Request.
- Manages Hard Reset and Soft Reset recovery.
- Drives the VDM (Vendor Defined Message) state machine: Discovery Identity, Discover SVIDs, Discover Modes, Enter Mode, Specific commands.
- Notifies the DPM via `USBPD_DPM_Notification()` on key events (explicit contract, detach, hard reset).

**In this project:**
PE runs as a dedicated FreeRTOS task (`OS_PE_STACK_SIZE = 520 words`). The PE implementation is provided as a pre-compiled binary: `USBPDCORE_PD3_FULL_CM0PLUS_wc32.a`. User code interacts with PE through API calls such as `USBPD_PE_SVDM_RequestSpecific()`.

---

## 2. STM32 Software Stack

### 2.1 Layer Diagram

```
┌─────────────────────────────────────────────────────────┐
│                   YOUR APPLICATION                      │
│   Core/Src/main.c                                       │
│   • System init, clock, peripherals                     │
│   • Button press → USBPD_VDM_SendAppleCommand()         │
│   • Heartbeat task, heap diagnostics                    │
│   • HardFault_Handler with register dump                │
└─────────────────────┬───────────────────────────────────┘
                      │ osThreadCreate / DPM API
┌─────────────────────▼───────────────────────────────────┐
│                 DPM — Device Policy Manager             │
│   USBPD/Target/usbpd_dpm_user.c                        │
│   • USBPD_DPM_UserCableDetection() — attach/detach      │
│   • USBPD_DPM_Notification() — contract, reset events  │
│   • USBPD_DPM_EvaluateRequest() — accept SNK request   │
│   • USBPD_DPM_SetupNewPower() — apply negotiated power  │
│                                                         │
│   USBPD/Target/usbpd_vdm_user.c                        │
│   • USBPD_VDM_StartDiscovery() — trigger Disc Identity  │
│   • USBPD_VDM_SendAppleCommand() — Apple DFU/Reboot     │
│   • USBPD_VDM_SendSpecific() — fill VDM payload         │
│   • USBPD_VDM_InformIdentity/SVID/Mode() — log results  │
│                                                         │
│   USBPD/Target/usbpd_dpm_conf.h                        │
│   • Port role: SRC, PD3.0, VDM enabled, SOP only       │
│                                                         │
│   USBPD/App/usbpd_pdo_defs.h                           │
│   • PDO: 5V / 3A fixed supply                          │
└──────────┬──────────────────────┬───────────────────────┘
           │                      │
┌──────────▼──────────┐  ┌────────▼──────────────────────┐
│   PE Task           │  │   CAD Task                    │
│   Policy Engine     │  │   Cable Attach Detection      │
│   520 words stack   │  │   400 words stack             │
│                     │  │                               │
│  (ST binary lib)    │  │  (ST binary lib)              │
│  USBPDCORE_PD3_     │  │                               │
│  FULL_CM0PLUS_      │  │  Monitors CC1/CC2 lines       │
│  wc32.a             │  │  Determines SRC/SNK role      │
│                     │  │  Fires attach/detach events   │
└──────────┬──────────┘  └────────┬──────────────────────┘
           └──────────┬───────────┘
┌──────────────────────▼──────────────────────────────────┐
│             PRL — Protocol Layer  (ST binary)           │
│   • Builds/parses PD message frames                     │
│   • GoodCRC handshake                                   │
│   • Message ID tracking, retry on no GoodCRC           │
│   • Chunked extended message reassembly                 │
└─────────────────────┬───────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────┐
│              UCPD HAL Driver                            │
│   Drivers/STM32G0xx_HAL_Driver/                         │
│   stm32g0xx_hal_ucpd.c                                 │
│   • Configures UCPD1 peripheral registers               │
│   • Manages DMA transfers for TX/RX                    │
│   • BMC encoding/decoding handled in hardware           │
└─────────────────────┬───────────────────────────────────┘
                      │ CC1 / CC2 pins
┌─────────────────────▼───────────────────────────────────┐
│         STM32G071RB UCPD1 Peripheral (Hardware)         │
│   • BMC encode/decode at 300 kbps                       │
│   • SOP* preamble and framing                          │
│   • Hardware CRC (CRC-32 for PD)                       │
│   • Rp/Rd/Ra resistor control via internal switches     │
└─────────────────────┬───────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────┐
│   X-NUCLEO-SRC1M1 Shield  (TCPP03-M20 + Power Stage)   │
│   Drivers/BSP/X-NUCLEO-SRC1M1/src1m1_usbpd_pwr.c       │
│   TCPP/                                                 │
│   • TCPP03: USB-C protection IC + Vconn switch          │
│   • External power stage: J1 (12–24V DC) → 5V VBUS     │
│   • VBUS enable/disable controlled via GPIO + I2C       │
└─────────────────────────────────────────────────────────┘
```

### 2.2 FreeRTOS Task Layout

| Task | Stack | Priority | Purpose |
|---|---|---|---|
| `CAD_Task` | 400 words | `osPriorityRealtime` | CC line monitoring |
| `PE_Task` | 520 words | `osPriorityAboveNormal` | PD negotiation state machine |
| `defaultTask` | 512 words | `osPriorityNormal` | Heartbeat, button poll, heap diagnostics |

Tasks are created in `Middlewares/ST/STM32_USBPD_Library/Core/src/usbpd_dpm_core.c` (CAD/PE) and `Core/Src/app_freertos.c` (defaultTask).

**Key FreeRTOS configuration** (`Core/Inc/FreeRTOSConfig.h`):

| Parameter | Value |
|---|---|
| `configCPU_CLOCK_HZ` | `SystemCoreClock` (64 MHz) |
| `configTICK_RATE_HZ` | 1000 Hz (1 ms tick) |
| `configTOTAL_HEAP_SIZE` | 14000 bytes |
| `configCHECK_FOR_STACK_OVERFLOW` | 2 (runtime check) |
| `configMAX_PRIORITIES` | 7 |
| SVC_Handler | `vPortSVCHandler` |
| PendSV_Handler | `xPortPendSVHandler` |

---

## 3. Key Source Files

| File | Role |
|---|---|
| `Core/Src/main.c` | System init, clock config, UART debug, button handling, HardFault dump |
| `Core/Src/app_freertos.c` | FreeRTOS task creation, `StartDefaultTask` |
| `Core/Inc/FreeRTOSConfig.h` | FreeRTOS kernel configuration |
| `USBPD/App/usbpd.h` | Top-level USBPD include, `MX_USBPD_Init()` |
| `USBPD/Target/usbpd_dpm_user.c` | DPM user callbacks (attach, contract, power eval) |
| `USBPD/Target/usbpd_vdm_user.c` | VDM user callbacks (Apple commands, discovery) |
| `USBPD/Target/usbpd_vdm_user.h` | Apple VDM constants: `APPLE_SVID=0x05AC`, `APPLE_CMD_PERFORM_ACTION=0x12` |
| `USBPD/Target/usbpd_dpm_conf.h` | Port config: SRC, PD3.0, SOP only, VDM enabled |
| `USBPD/App/usbpd_pdo_defs.h` | PDO definitions: 5V/3A source |
| `USBPD/App/usbpd_pwr_if.c` | VBUS enable/disable, PDO retrieval |
| `Middlewares/ST/.../usbpd_dpm_core.c` | CAD/PE task creation, USBPD stack init |
| `Middlewares/Third_Party/FreeRTOS/.../port.c` | FreeRTOS CM0+ port: `vPortSVCHandler`, `xPortPendSVHandler` |
| `STM32G071RBTX_FLASH.ld` | Linker script: 128KB Flash, 36KB RAM, 4KB MSP stack |
| `Drivers/BSP/X-NUCLEO-SRC1M1/src1m1_usbpd_pwr.c` | SRC1M1 power stage control |

---

## 4. Complete Message Flow

### 4.1 Attach → Explicit Contract

```
Apple Device (SNK)          STM32 SRC               DPM / Your Code
      │                          │                          │
      │  Pull-down on CC ────────►                          │
      │                     CAD detects                     │
      │                          │──────────────────────────► UserCableDetection(ATTACHED)
      │                          │                          │  USBPD_PWR_IF_VBUSEnable()
      │                          │                          │  [needs J1 external supply]
      │◄════════ VBUS 5V ════════│                          │
      │                          │                          │
      │◄── Source_Capabilities ──│  PE sends SRC_CAP PDO    │
      │    [5V / 3A Fixed]       │                          │
      │──── Request ────────────►│  Sink selects PDO 1      │
      │◄── Accept ───────────────│                          │
      │◄── PS_RDY ───────────────│                          │
      │                          │──────────────────────────► DPM_Notification(EXPLICIT_CONTRACT)
      │               *** Explicit Contract ***             │  → VDM_StartDiscovery()
```

### 4.2 VDM Discovery Sequence

```
Apple Device                STM32 (Initiator)           vdm_user.c callbacks
      │                          │                          │
      │◄── Disc_Identity(SOP) ───│                          │
      │──── Identity ACK ───────►│                          │ InformIdentity() → RequestSVID()
      │                          │                          │
      │◄── Disc_SVIDs(SOP) ──────│                          │
      │──── SVIDs ACK ──────────►│  [0x05AC, ...]          │ InformSVID() → RequestMode()
      │                          │                          │
      │◄── Disc_Modes(SOP) ──────│  SVID=0x05AC            │
      │──── Modes ACK ──────────►│                          │ InformMode() → RequestModeEnter()
      │                          │                          │
      │◄── Enter_Mode(SOP) ──────│                          │
      │──── Enter ACK ──────────►│                          │ InformModeEnter()
```

### 4.3 Apple VDM Command (Button Press)

```
Apple Device                STM32                       main.c / vdm_user.c
      │                          │                          │
      │                          │        [Button pressed]  │
      │                          │◄─────────────────────────│ SendAppleCommand(REBOOT/DFU)
      │                          │    sApplePendingAction   │ USBPD_PE_SVDM_RequestSpecific(
      │                          │    = APPLE_ACTION_REBOOT │   SOP, CMD=0x12, SVID=0x05AC)
      │                          │                          │
      │                    PE calls SendSpecific            │
      │                          │──────────────────────────► VDM_SendSpecific()
      │                          │                          │  pVDO[0] = Action (0x0105/0x0106)
      │                          │                          │  pVDO[1] = 0x80000000 (reboot arg)
      │                          │                          │
      │◄── VDM(SOP, 0x05AC,  ───│                          │
      │        CMD=0x12,         │                          │
      │        VDO=0x0105)       │                          │
      │                          │                          │
      │  device reboots / DFU   │                          │
```

---

## 5. Apple Proprietary VDM Details

Apple uses a non-standard (proprietary) VDM protocol over USB-PD SOP.

| Field | Value | Notes |
|---|---|---|
| SVID | `0x05AC` | Apple's USB Vendor ID |
| Command | `0x12` (`APPLE_CMD_PERFORM_ACTION`) | Apple-specific action command |
| VDO[0] | `0x0105` = Reboot, `0x0106` = DFU | Action identifier |
| VDO[1] | `0x80000000` (reboot), `0x80010000` (DFU) | Action argument |
| SOP type | `USBPD_SOPTYPE_SOP` | Sent to port partner, NOT cable plug |

These constants are defined in `USBPD/Target/usbpd_vdm_user.h`:

```c
#define APPLE_SVID               0x05ACU
#define APPLE_CMD_PERFORM_ACTION 0x12U
#define APPLE_ACTION_REBOOT      0x105U
#define APPLE_ACTION_DFU         0x106U
```

The VDM payload is filled in `USBPD_VDM_SendSpecific()` in `usbpd_vdm_user.c` when the PE calls back to get the data to transmit.

---

## 6. Hardware Requirements for PD Negotiation

The X-NUCLEO-SRC1M1 shield requires an **external power supply on J1** (12–24V DC, barrel jack 5.5mm/2.1mm center-positive) to generate 5V VBUS output. Without it:

1. CC line detection may work (UCPD runs on 3.3V from MCU)
2. VBUS stays at 0V
3. The Apple device (sink) will not start PD negotiation — it requires vSafe5V on VBUS first
4. `USBPD_PWR_IF_VBUSEnable()` will fail → `NVIC_SystemReset()` is called in `usbpd_dpm_user.c:236`

**Expected serial output when everything works:**
```
[CAD] Port0: Cable ATTACHED
[CAD] Port0: VBUS Enabled (SRC)
[DPM] Port0: Explicit Contract -> VDM Discovery
[VDM] Port0: VDM:StartDiscovery->Id
[VDM] Port0: InformIdentity ACK VID=0x05AC
[VDM] Port0: InformSVID ACK count=1 SVID[0]=0x05AC
[VDM] Port0: VDM:InformMode ACK -> EnterMode
[VDM] Port0: VDM:ModeEnter confirmed
--- button pressed ---
[VDM] Port0: Sending Apple Action 0x0105
[VDM] Port0: RequestSpecific (SOP) returned 0
[VDM] Port0: SendSpecific SOP=0 CMD=0x12
```
