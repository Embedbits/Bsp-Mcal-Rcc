# RCC MCAL Module for STM32

This module provides an abstraction layer for the Reset and Clock Control (RCC) peripheral on STM32 microcontrollers.  
It is part of the **MCAL (Microcontroller Abstraction Layer)** and allows safe and portable configuration of system clocks, PLLs, peripheral clocks, low-power states, and reset management.

---

## Features

- Module initialization and de-initialization
- Peripheral clock enable/disable and state management
- Peripheral reset handling
- Power and sleep state configuration
- PLL configuration and management
- Clock sources configuration (HSE, HSI64, HSI48, CSI, LSI, LSE - crystal or digital bypass)
- RTC clock source selection
- System clock and bus clock dividers
- Flash latency and prefetch control
- SysTick interval configuration

---

## Public API

### Module Management
- `rcc_ModuleVersion_t Rcc_Get_ModuleVersion(void)`
- `rcc_RequestState_t Rcc_Init(rcc_ConfigStruct_t * const clockConfig)`
- `void Rcc_Deinit(rcc_ConfigStruct_t * const clockConfig)`
- `void Rcc_Task(void)`
- `rcc_RequestState_t Rcc_Get_DefaultConfig(rcc_ConfigStruct_t * const clockConfig)`

### Peripheral Clock Management
- `rcc_RequestState_t Rcc_Set_PeriphActive(rcc_PeriphId_t periphId)`
- `rcc_RequestState_t Rcc_Set_PeriphInactive(rcc_PeriphId_t periphId)`
- `rcc_RequestState_t Rcc_Get_PeriphState(rcc_PeriphId_t periphId, rcc_FunctionState_t * const funcState)`
- `rcc_RequestState_t Rcc_Get_PeriphClk(rcc_PeriphId_t periphId, rcc_FreqHz_t * const periphClk)`
- `rcc_RequestState_t Rcc_Get_PeriphClkSrc(rcc_PeriphId_t periphId, rcc_PeriphId_t * const periphClkSrc)`

`Rcc_Set_PeriphActive()` starts the internal oscillator (HSI64, HSI48, CSI, LSI) of the selected kernel
clock if it is not running yet (eg. `RCC_PERIPH_RNG_HSI48`). External sources (HSE, LSE) and PLL outputs
are not started automatically.

RTC peripheral IDs (`RCC_PERIPH_RTC_HSE_DIV32`, `RCC_PERIPH_RTC_LSE`, `RCC_PERIPH_RTC_LSI`) control the
RTC interface clock (`RCC_APB3ENR.RTCAPBEN`) and the RTC / TAMP kernel clock (`RCC_BDCR.RTCEN`):
`Rcc_Set_PeriphActive()` selects the RTC clock (`RTCSEL`) and enables both clocks,
`Rcc_Set_PeriphInactive()` disables the kernel clock first and keeps `RTCSEL` (write-once until
backup domain reset), `Rcc_Get_PeriphState()` reports the RTC active only with both clocks enabled.

### Peripheral Reset Management
- `rcc_RequestState_t Rcc_Set_ResetActive(rcc_PeriphId_t periphId)`
- `rcc_RequestState_t Rcc_Set_ResetInactive(rcc_PeriphId_t periphId)`
- `rcc_RequestState_t Rcc_Get_ResetState(rcc_PeriphId_t periphId, rcc_FunctionState_t * const funcState)`

### Power and Sleep Management
- `rcc_RequestState_t Rcc_Set_SleepActive(rcc_PeriphId_t periphId)`
- `rcc_RequestState_t Rcc_Set_SleepInactive(rcc_PeriphId_t periphId)`
- `rcc_RequestState_t Rcc_Get_SleepState(rcc_PeriphId_t periphId, rcc_FunctionState_t * const funcState)`

### PLL Configuration
- `rcc_RequestState_t Rcc_Set_PllConfig(rcc_PllId_t pllId, rcc_PllConfigStruct_t * const configStruct)`
- `rcc_RequestState_t Rcc_Get_PllInternalClk(rcc_PllId_t pllId, rcc_FreqHz_t * const pllClk)`
- `rcc_RequestState_t Rcc_Set_PllActive(rcc_PllId_t pllId)`
- `rcc_RequestState_t Rcc_Set_PllInactive(rcc_PllId_t pllId)`
- `rcc_RequestState_t Rcc_Get_PllState(rcc_PllId_t pllId, rcc_FunctionState_t * const retState)`
- `rcc_RequestState_t Rcc_Set_PllsSource(rcc_PllId_t pllId, rcc_PllClkSrc_t clkSource)`
- `rcc_RequestState_t Rcc_Get_PllsSource(rcc_PllId_t pllId, rcc_PllClkSrc_t * const clkSource)`
- `rcc_RequestState_t Rcc_Get_PllClk_OutP(rcc_PllId_t pllId, rcc_FreqHz_t *pllClk)`
- `rcc_RequestState_t Rcc_Get_PllClk_OutQ(rcc_PllId_t pllId, rcc_FreqHz_t *pllClk)`
- `rcc_RequestState_t Rcc_Get_PllClk_OutR(rcc_PllId_t pllId, rcc_FreqHz_t *pllClk)`

### Clock Sources
Oscillators except HSE (`rcc_OscId_t`: HSI64, HSI48, CSI, LSI, LSE). HSE is configured by
`rcc_ConfigStruct_t` (`HSE_ClockType`, `HSE_Frequency_Hz`) and started by `Rcc_Init()`.
- `rcc_RequestState_t Rcc_Set_OscActive(rcc_OscId_t oscId)`
- `rcc_RequestState_t Rcc_Set_OscInactive(rcc_OscId_t oscId)`
- `rcc_RequestState_t Rcc_Get_OscState(rcc_OscId_t oscId, rcc_FunctionState_t * const retState)`
- `rcc_RequestState_t Rcc_Set_OscDiv(rcc_OscId_t oscId, rcc_OscDiv_t oscDiv)`
- `rcc_RequestState_t Rcc_Get_OscDiv(rcc_OscId_t oscId, rcc_OscDiv_t * const oscDiv)`

**HSE clock security system** (`CSS_Enable` of the `Rcc_Init()` configuration, default inactive) is
enabled by `Rcc_Init()` when HSE is ready; the CSS without HSE (`RCC_HSE_TYPE_NONE`) or an invalid state
is rejected. Once enabled, the CSS is disabled by a system reset or Standby mode only. A HSE failure
switches the system clock to HSI (or CSI) by hardware and generates NMI, which is repeated until the
`HSECSSF` flag is cleared (`RCC_CICR.HSECSSC`) - the NMI handler is registered by the Nvic module.
HSE stopped by a repeated `Rcc_Init()` disables the clock detector (not detected as a failure).

LSE and LSI are located in the backup domain - the backup domain write protection
(`PWR_DBPCR.DBP`) is released by the module before `RCC_BDCR` is written.

**LSE** is enabled by `Rcc_Init()` (and by `Rcc_Set_OscActive(RCC_OSC_LSE)`) with the input type
and driving capability of the `Rcc_Init()` configuration. The start-up of the crystal is not
awaited - the ready state is reported by `Rcc_Get_OscState(RCC_OSC_LSE, ...)`:
- `LSE_ClockType` (`rcc_LseType_t`): `RCC_LSE_TYPE_CRYSTAL` (default), `RCC_LSE_TYPE_SIG_DIGITAL_IN`
  (digital bypass, `LSEBYP = 1`, `LSEEXT = 1`) or `RCC_LSE_TYPE_NONE` (`Rcc_Init()` leaves LSE untouched, activation returns error).
- `LSE_Drive` (`rcc_LseDrive_t`): `RCC_LSE_DRIVE_MEDIUM_LOW` (default), `RCC_LSE_DRIVE_MEDIUM_HIGH`,
  `RCC_LSE_DRIVE_HIGH` - selected according to the crystal (AN2867), ignored in bypass mode.

LSE keeps running over system resets (backup domain). Enabled LSE (running or starting) with the
requested configuration is kept, otherwise it is stopped, configured (`LSEBYP`, `LSEEXT` and
`LSEDRV` are writable only with LSE stopped) and enabled again.

Device errata (ES0561, ES0565, ES0621): the lowest drive (`LSEDRV = 00`, backup domain reset
value) is forbidden ("LSE low drive mode is not functional") and the analog bypass ("might not
work properly") is replaced by the digital bypass - both are not offered. RTC must not use the
digital bypass (RM0481), RTC with LSE requires the LSE crystal.

### RTC Clock Source
- `rcc_RequestState_t Rcc_Set_RtcClkSource(rcc_Rtc_ClkSource_t clkSource)`
- `rcc_RequestState_t Rcc_Get_RtcClkSource(rcc_Rtc_ClkSource_t * const clkSource)`

### Power Supply Validity
Supplies of isolated domains validated by software (PWR register access is part of the RCC module,
`rcc_PwrSupplyId_t`). STM32H5 has the VDDUSB supply of the USB peripheral (`PWR_USBSCR.USB33SV`,
reset value 0 - the USB peripheral is isolated until the supply is validated). STM32H503 has no
isolated domain - the functions return error.
- `rcc_RequestState_t Rcc_Set_PwrSupplyActive(rcc_PwrSupplyId_t supplyId)`
- `rcc_RequestState_t Rcc_Set_PwrSupplyInactive(rcc_PwrSupplyId_t supplyId)`
- `rcc_RequestState_t Rcc_Get_PwrSupplyState(rcc_PwrSupplyId_t supplyId, rcc_FunctionState_t * const retState)`

### HSI48 Automatic Trimming
Clock recovery system (CRS) keeping the accuracy of the HSI48 oscillator (USB needs +-0.25 %). The
synchronization is configured for the 48 MHz target (`rcc_Hsi48TrimSrc_t`: USB start of frame 1 kHz,
LSE 32.768 kHz), the CRS clock and the HSI48 oscillator are activated by the function.
- `rcc_RequestState_t Rcc_Set_Hsi48TrimActive(rcc_Hsi48TrimSrc_t trimSource)`
- `rcc_RequestState_t Rcc_Set_Hsi48TrimInactive(void)`
- `rcc_RequestState_t Rcc_Get_Hsi48TrimState(rcc_FunctionState_t * const retState)`

### Clock Bus Configuration
- `rcc_RequestState_t Rcc_Set_ClkBusDivider(rcc_ClkBusId_t clkBusId, rcc_ClkBusDiv_t clkBusDivider)`
- `rcc_RequestState_t Rcc_Get_ClkBusDivider(rcc_ClkBusId_t clkBusId, rcc_ClkBusDiv_t * const clkBusDivider)`
- `rcc_RequestState_t Rcc_Get_ClkBusClk(rcc_ClkBusId_t clkBusId, rcc_FreqHz_t * const clkBusFreq)`

### Flash and System Configuration
- `rcc_RequestState_t Rcc_Set_PwrRange(rcc_ConfigStruct_t * const clockConfig)`
- `rcc_RequestState_t Rcc_Set_FlashLatency(rcc_ConfigStruct_t * const clockConfig)`
- `rcc_RequestState_t Rcc_Set_FlashPrefetchActive(void)`
- `rcc_RequestState_t Rcc_Set_FlashPrefetchInactive(void)`
- `rcc_RequestState_t Rcc_Set_SysTickInterval(rcc_Time_ms_t sysTickInterval)`
- `rcc_RequestState_t Rcc_Get_SysTickInterval(rcc_Time_ms_t * const sysTickInterval)`

### Clock Outputs (MCO1, MCO2, LSCO)
- `rcc_RequestState_t Rcc_Set_ClkOutSource(rcc_ClkOut_Id_t outId, rcc_ClkOut_Source_t clkSource)`
- `rcc_RequestState_t Rcc_Get_ClkOutSource(rcc_ClkOut_Id_t outId, rcc_ClkOut_Source_t * const clkSource)`
- `rcc_RequestState_t Rcc_Set_ClkOutDivider(rcc_ClkOut_Id_t outId, rcc_ClkOut_Div_t clkDivider)`
- `rcc_RequestState_t Rcc_Get_ClkOutDivider(rcc_ClkOut_Id_t outId, rcc_ClkOut_Div_t * const clkDivider)`

### Reset Source Flags
- `rcc_RequestState_t Rcc_Get_ResetSource(rcc_ResetSrc_t resetSrc, rcc_FlagState_t * const flagState)`
- `rcc_RequestState_t Rcc_Set_ResetSourceClear(void)`

---

## 🛠 CMake Integration

1. Include `Rcc_Lib` in your CMake library.
2. Include `Rcc_Port.h` in your project.
3. Link against the Rcc module implementation files.
4. Configure the module as needed for your hardware.

---

## License

This project is licensed under the **Creative Commons Attribution–NonCommercial 4.0 International (CC BY-NC 4.0)**.

You are free to use, modify, and share this work for **non-commercial purposes**, provided appropriate credit is given.

See [LICENSE.md](LICENSE.md) for full terms or visit [creativecommons.org/licenses/by-nc/4.0](https://creativecommons.org/licenses/by-nc/4.0/).

---

## Authors

- **Mr.Nobody** — [embedbits.com](https://embedbits.com)

Contributions are welcome! Please open a pull request.

---

## 🌐 Useful Links

- [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html)
- [Azure DevOps](https://azure.microsoft.com/en-us/services/devops/)
- [Embedbits Github](https://github.com/Embedbits)
- [CC BY-NC 4.0 License](https://creativecommons.org/licenses/by-nc/4.0/)