# Nucleo-G071RB + SRC1M1 — USB PD Source with VDM

STM32G071RB acting as a **USB Power Delivery Source** that sends **Vendor Defined Messages (VDM)** to a connected sink device. Uses the **X-NUCLEO-SRC1M1** expansion board with TCPP02-M18 protection IC.

## Hardware

| Board | Role |
|-------|------|
| NUCLEO-G071RB | MCU — USB PD source / DFP |
| X-NUCLEO-SRC1M1 | USB-C connector + TCPP02-M18 protection + VBUS sensing |

### Pin Connections (SRC1M1 → Nucleo)

The SRC1M1 plugs directly onto the Nucleo-64 Arduino headers. Key signals:

| SRC1M1 Signal | Nucleo Pin | Function |
|---------------|-----------|----------|
| CC1 | PB6 | UCPD1_CC1 |
| CC2 | PB4 | UCPD1_CC2 |
| TCPP_EN | PB8 | TCPP02 enable (active high) |
| TCPP_FLG | PB9 | TCPP02 fault flag (active low) |
| I2C_SCL | PA9 | I2C1 SCL (TCPP02 control) |
| I2C_SDA | PA8 | I2C1 SDA (TCPP02 control) |
| VSENSE | PA4 | ADC1_IN4 (VBUS voltage sense) |
| VBUS | CN8 | Power rail output |
| GND | GND | Common ground |

### Hardware Setup

1. Plug the **X-NUCLEO-SRC1M1** onto the **NUCLEO-G071RB** Arduino headers
2. Connect a **USB-C cable** from the SRC1M1 Type-C port to your **sink device** (laptop, phone, PD analyzer, etc.)
3. Power the Nucleo board via the ST-LINK USB connector (CN1)
4. Open a serial terminal on the ST-LINK VCP (115200 8N1) for debug output

---

## Step-by-Step Setup Guide

### Prerequisites

| Tool | Minimum Version | Purpose |
|------|----------------|---------|
| **STM32CubeMX** | 6.11.0 | Project configuration & code generation |
| **STM32CubeIDE** or **arm-none-eabi-gcc** | 1.14.0 / 12.x | Compilation |
| **X-CUBE-TCPP** pack | 4.1.0 | TCPP02 BSP + USBPD source examples |
| **STM32CubeMonitor-UCPD** | latest | PD protocol monitoring (optional) |
| **CMake + Ninja** | 3.22+ | Build system (if not using CubeIDE) |

### Phase 1 — CubeMX Configuration

1. **Open** `nucleo-g071rb-usbpd.ioc` in STM32CubeMX

2. **Install the X-CUBE-TCPP pack** if not already installed:
   - Menu → `Help` → `Manage embedded software packages`
   - Find **X-CUBE-TCPP** under STMicroelectronics → install latest

3. **Verify peripheral configuration** (already set in the `.ioc`):
   - `Connectivity` → `UCPD1`: Mode = **Source**
   - `Connectivity` → `I2C1`: Enabled (PA8=SDA, PA9=SCL)
   - `Analog` → `ADC1`: Channel 4 enabled (PA4 = VSENSE)
   - `Connectivity` → `LPUART1`: Async mode (PA2/PA3)

4. **Configure USBPD Middleware**:
   - `Middleware and Software Packs` → `STM32_USBPD_Library` (or `USBPD`)
   - Port 0 Role: **SRC** (Source)
   - PD Revision: **3.0**
   - Data Role: **DFP**
   - VDM Support: **Enabled** ✓
   - Trace/Debug: **Enabled** ✓ (optional, for UART logging)

5. **Configure FreeRTOS**:
   - `Middleware` → `FREERTOS`: CMSIS_V1
   - Default task with 128-word stack is sufficient

6. **Configure PDOs** (Power Data Objects):
   - In the USBPD configuration panel, set source PDOs:
     - PDO0: 5V / 900mA (mandatory vSafe5V)
     - PDO1: 9V / 2A
     - PDO2: 15V / 2A
     - PDO3: 20V / 3A

7. **Generate Code**:
   - `Project Manager` → Toolchain: **Makefile** (or STM32CubeIDE)
   - Click **Generate Code**
   - This creates: `Drivers/`, `Middlewares/`, startup file, `USB_PD/Target/`, etc.

### Phase 2 — Integrate VDM Code

After CubeMX generates code, **replace or merge** these files with the versions in this repo:

| File | Action |
|------|--------|
| `USB_PD/App/usbpd_vdm_user.c` | **Replace** with our VDM implementation |
| `USB_PD/App/usbpd_vdm_user.h` | **Replace** with our header |
| `USB_PD/App/usbpd_dpm_user.c` | **Replace** with our DPM callbacks |
| `USB_PD/App/usbpd_dpm_user.h` | **Replace** with our header |
| `Core/Src/main.c` | **Replace** with our main (or merge USER CODE sections) |
| `Core/Inc/main.h` | **Replace** with our header |

> **Important:** Keep the CubeMX-generated files in `USB_PD/Target/`, `Drivers/`, and
> `Middlewares/` — those provide the USBPD stack, HAL, and RTOS.

### Phase 3 — Build

#### Option A: CMake + Ninja

```bash
cd stm32_usb_vdm
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Debug
ninja
```

#### Option B: STM32CubeIDE

1. Import as existing STM32CubeIDE project
2. Build with Ctrl+B

### Phase 4 — Flash

```bash
# STM32CubeProgrammer CLI
STM32_Programmer_CLI -c port=SWD -w build/nucleo-g071rb-usbpd.bin 0x08000000 -rst

# or OpenOCD
openocd -f interface/stlink.cfg -f target/stm32g0x.cfg \
  -c "program build/nucleo-g071rb-usbpd.elf verify reset exit"
```

### Phase 5 — Test VDM Messages

1. **Connect** a USB PD sink device to the SRC1M1 Type-C port
2. **Open** a serial terminal (115200 8N1) on the ST-LINK VCP
3. **Observe** the VDM discovery sequence in the trace output:

```
VDM_UserInit OK
VDM: start Discover Identity
VDM: Identity ACK -> request SVIDs
VDM: SVID ACK -> request Modes
VDM: Mode ACK -> enter mode 1
VDM: ModeEnter ACK — ready!
```

4. **Optional:** Use **STM32CubeMonitor-UCPD** to see the raw PD messages on the wire

---

## Architecture

```
main.c
 ├─ SystemClock_Config()
 ├─ MX_GPIO/I2C/ADC/UART_Init()
 ├─ TCPP02 enable (GPIO high)
 ├─ MX_USBPD_Init()                    ← ST middleware + FreeRTOS tasks
 │   └─ USBPD_DPM_UserInit()
 │       └─ USBPD_VDM_UserInit()       ← register vdmCallbacks
 │           └─ USBPD_PE_InitVDM_Callback()
 └─ osKernelStart()                    ← FreeRTOS scheduler

On explicit PD contract:
  USBPD_DPM_Notification(POWER_EXPLICIT_CONTRACT)
   └─ USBPD_VDM_StartDiscovery()
       └─ USBPD_PE_SVDM_RequestIdentity()      [1]
           └─ VDM_InformIdentity(ACK)
               └─ USBPD_PE_SVDM_RequestSVID()   [2]
                   └─ VDM_InformSVID(ACK)
                       └─ USBPD_PE_SVDM_RequestMode()  [3]
                           └─ VDM_InformMode(ACK)
                               └─ USBPD_PE_SVDM_RequestModeEnter() [4]
                                   └─ VDM_InformModeEnter(ACK)
                                       → VDM ready for custom messages
```

## VDM Callback Structure

The ST USBPD middleware uses a **callback table** (`USBPD_VDM_Callbacks`) registered
via `USBPD_PE_InitVDM_Callback()`. Two categories:

### Responder Callbacks (partner asks us)

| Callback | Purpose |
|----------|---------|
| `VDM_DiscoverIdentity` | Return our VID, PID, XID |
| `VDM_DiscoverSVIDs` | Return supported SVIDs |
| `VDM_DiscoverModes` | Return modes for a given SVID |
| `VDM_ModeEnter` | Accept/reject mode entry |
| `VDM_ModeExit` | Accept/reject mode exit |
| `VDM_SendAttention` | Fill attention VDO data |
| `VDM_ReceiveSpecific` | Handle vendor-specific commands |

### Inform Callbacks (we asked, partner replied)

| Callback | Purpose |
|----------|---------|
| `VDM_InformIdentity` | Process partner's identity response |
| `VDM_InformSVID` | Process partner's SVID list |
| `VDM_InformMode` | Process partner's modes |
| `VDM_InformModeEnter` | Mode entry accepted/rejected |
| `VDM_InformModeExit` | Mode exit confirmed |
| `VDM_InformSpecific` | Vendor-specific response |

## Source PDOs Advertised

| PDO | Voltage | Current |
|-----|---------|---------|
| 0 | 5 V | 0.9 A |
| 1 | 9 V | 2.0 A |
| 2 | 15 V | 2.0 A |
| 3 | 20 V | 3.0 A |

## File Structure

```
stm32_usb_vdm/
├── CMakeLists.txt                      # CMake build
├── STM32G071RBTX_FLASH.ld             # Linker script
├── nucleo-g071rb-usbpd.ioc            # CubeMX config
├── cmake/
│   └── arm-none-eabi.cmake            # Toolchain file
├── Core/
│   ├── Inc/main.h                     # Pin definitions, board config
│   └── Src/
│       ├── main.c                     # Entry point, peripherals, FreeRTOS
│       └── startup_stm32g071rbtx.s    # (from CubeMX)
├── USB_PD/
│   ├── App/
│   │   ├── usbpd_dpm_user.{h,c}      # Policy Manager — PDOs, contract handling
│   │   └── usbpd_vdm_user.{h,c}      # VDM callbacks + custom VDM logic
│   └── Target/                        # (from CubeMX — USBPD HW target layer)
├── Drivers/                           # (from CubeMX — HAL, CMSIS, BSP)
│   ├── CMSIS/
│   ├── STM32G0xx_HAL_Driver/
│   └── BSP/Components/tcpp0203/       # (from X-CUBE-TCPP)
└── Middlewares/                        # (from CubeMX)
    ├── ST/STM32_USBPD_Library/        # USBPD core stack
    └── Third_Party/FreeRTOS/          # FreeRTOS kernel
```

## Experimenting with VDMs

### Sending a Custom Structured VDM

After mode entry succeeds, you can send vendor-specific commands:

```c
USBPD_PE_SVDM_RequestSpecific(USBPD_PORT_0, USBPD_SOPTYPE_SOP,
                                SVDM_SPECIFIC_1, VDM_CUSTOM_SVID);
```

The `VDM_SendSpecific` callback fills the outgoing VDO payload,
and `VDM_InformSpecific` receives the response.

### Sending an Unstructured VDM (UVDM)

Enable `_UVDM` in compile definitions (already set in CMakeLists.txt), then:

```c
USBPD_PE_UVDM_Send(USBPD_PORT_0, USBPD_SOPTYPE_SOP);
```

The `VDM_SendUVDM` callback fills the header (VID) and payload VDOs.
The `VDM_ReceiveUVDM` callback processes incoming unstructured VDMs.

### Triggering VDM on Button Press

Add to the FreeRTOS default task in `main.c`:

```c
void StartDefaultTask(void const *argument)
{
    (void)argument;
    uint8_t btn_prev = GPIO_PIN_SET;
    for (;;)
    {
        HAL_GPIO_TogglePin(LED_GREEN_PORT, LED_GREEN_PIN);

        uint8_t btn = HAL_GPIO_ReadPin(BTN_USER_PORT, BTN_USER_PIN);
        if (btn == GPIO_PIN_RESET && btn_prev == GPIO_PIN_SET)
        {
            /* Button pressed — send a custom VDM */
            USBPD_PE_SVDM_RequestAttention(USBPD_PORT_0,
                                             USBPD_SOPTYPE_SOP,
                                             VDM_CUSTOM_SVID);
        }
        btn_prev = btn;
        osDelay(100);
    }
}
```

## Troubleshooting

| Symptom | Likely Cause |
|---------|-------------|
| No PD negotiation | TCPP02 not enabled (check PB8 high), CC pins not connected |
| VDM NAK on everything | Sink doesn't support VDM or the SVID/mode doesn't match |
| Hard fault on boot | FreeRTOS heap too small — increase `configTOTAL_HEAP_SIZE` in `FreeRTOSConfig.h` |
| No trace output | `_TRACE` not defined, or LPUART not configured for tracer |
| Build fails on missing headers | Run CubeMX code generation first |

## References

- [UM2973 — X-NUCLEO-SRC1M1 Getting Started](https://www.st.com/resource/en/user_manual/um2973.pdf)
- [STM32 Step-by-Step: USB PD Source](https://wiki.st.com/stm32mcu/wiki/STM32StepByStep:Getting_started_with_USB-Power_Delivery_Source)
- [X-CUBE-TCPP on GitHub](https://github.com/STMicroelectronics/x-cube-tcpp)
- [USB PD Spec §6.4.4 — Vendor Defined Messages](https://www.usb.org/document-library/usb-power-delivery)
- [AN5225 — USB Type-C Power Delivery using STM32](https://www.st.com/resource/en/application_note/an5225.pdf)
