# RCC MCAL Module for STM32H7

This module provides an abstraction layer for the Reset and Clock Control (RCC) peripheral on STM32H7 microcontrollers.  
It is part of the **MCAL (Microcontroller Abstraction Layer)** and allows safe and portable configuration of system clocks, PLLs, peripheral clocks, low-power states, and reset management.

Supported lines: STM32H742 / H743 / H745 / H747 / H750 / H753 / H755 / H757, STM32H723 / H725 / H730 / H733 / H735, STM32H7A3 / H7B0 / H7B3 (Cortex-M7 core of dual-core devices) and STM32H7R3 / H7R7 / H7S3 / H7S7 (Ral family STM32H7RS - see [STM32H7R / H7S Specifics](#stm32h7r--h7s-specifics)).

---

## Features

- Module initialization and de-initialization
- Peripheral clock enable/disable and state management, kernel clock multiplexers
- Peripheral reset handling
- Power and sleep state configuration
- PLL configuration and management (PLL1 - PLL3)
- Internal oscillators (HSI64 with divider, HSI48, CSI, LSI) and HSE (crystal, analog or digital input, clock security system)
- RTC clock source selection
- System clock, CPU / AXI / AHB and APB1 - APB4 bus clock dividers (APB1 / APB2 / APB4 / APB5 on STM32H7R / H7S)
- Supply configuration, voltage scaling and flash latency
- SysTick interval configuration
- Clock outputs MCO1 (PA8) and MCO2 (PC9)
- Reset source flags

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

`Rcc_Set_PeriphActive()` starts the internal oscillator (HSI, HSI48, CSI, LSI) of the selected kernel
clock if it is not running yet (eg. `RCC_PERIPH_RNG_HSI48`). External sources (HSE, LSE), PLL outputs and
the peripheral clock `per_ck` (CKPER, `RCC_PERIPH_LPCLK_*`) are not started automatically.
`Rcc_Set_PeriphInactive()` releases the kernel clock multiplexer unless it is shared with another enabled
peripheral (eg. SPI1 / SPI2 / SPI3) or it is the RTC / CKPER selection.

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

### Internal Oscillators
- `rcc_RequestState_t Rcc_Set_OscActive(rcc_OscId_t oscId)`
- `rcc_RequestState_t Rcc_Set_OscInactive(rcc_OscId_t oscId)`
- `rcc_RequestState_t Rcc_Get_OscState(rcc_OscId_t oscId, rcc_FunctionState_t * const retState)`
- `rcc_RequestState_t Rcc_Set_OscDiv(rcc_OscId_t oscId, rcc_OscDiv_t oscDiv)`
- `rcc_RequestState_t Rcc_Get_OscDiv(rcc_OscId_t oscId, rcc_OscDiv_t * const oscDiv)`

### RTC Clock
- `rcc_RequestState_t Rcc_Set_RtcClkSource(rcc_Rtc_ClkSource_t clkSource)`
- `rcc_RequestState_t Rcc_Get_RtcClkSource(rcc_Rtc_ClkSource_t * const clkSource)`

### Clock Bus Configuration
- `rcc_RequestState_t Rcc_Set_ClkBusDivider(rcc_ClkBusId_t clkBusId, rcc_ClkBusDiv_t clkBusDivider)`
- `rcc_RequestState_t Rcc_Get_ClkBusDivider(rcc_ClkBusId_t clkBusId, rcc_ClkBusDiv_t * const clkBusDivider)`
- `rcc_RequestState_t Rcc_Get_ClkBusClk(rcc_ClkBusId_t clkBusId, rcc_FreqHz_t * const clkBusFreq)`

### Flash and System Configuration
- `rcc_RequestState_t Rcc_Set_PwrRange(rcc_ConfigStruct_t * const clockConfig)`
- `rcc_RequestState_t Rcc_Set_FlashLatency(rcc_ConfigStruct_t * const clockConfig)`
- `rcc_RequestState_t Rcc_Set_FlashPrefetchActive(void)` - not available on STM32H7, returns error
- `rcc_RequestState_t Rcc_Set_FlashPrefetchInactive(void)` - not available on STM32H7, returns error
- `rcc_RequestState_t Rcc_Set_SysTickInterval(rcc_Time_ms_t sysTickInterval)`
- `rcc_RequestState_t Rcc_Get_SysTickInterval(rcc_Time_ms_t * const sysTickInterval)`

### Clock Outputs
- `rcc_RequestState_t Rcc_Set_ClkOutSource(rcc_ClkOut_Id_t outId, rcc_ClkOut_Source_t clkSource)`
- `rcc_RequestState_t Rcc_Get_ClkOutSource(rcc_ClkOut_Id_t outId, rcc_ClkOut_Source_t * const clkSource)`
- `rcc_RequestState_t Rcc_Set_ClkOutDivider(rcc_ClkOut_Id_t outId, rcc_ClkOut_Div_t clkDivider)`
- `rcc_RequestState_t Rcc_Get_ClkOutDivider(rcc_ClkOut_Id_t outId, rcc_ClkOut_Div_t * const clkDivider)`

Low speed clock output (LSCO) is not available on STM32H7 - requests return error.

### Reset Source Flags
- `rcc_RequestState_t Rcc_Get_ResetSource(rcc_ResetSrc_t resetSrc, rcc_FlagState_t * const flagState)`
- `rcc_RequestState_t Rcc_Set_ResetSourceClear(void)`

---

## STM32H7 Specifics

- **Supply configuration** (`PowerSupply`) can be written only once after power-on reset and must match the
  board (LDO / SMPS wiring). `RCC_PWR_SUPPLY_DEFAULT` keeps the reset configuration - only voltage scale 3 is
  available then. Locked supply configuration is accepted only if it matches the requested one.
- **Voltage scale 0** of STM32H742 / H743 / H745 / H747 / H750 / H753 / H755 / H757 is VOS1 with SYSCFG
  overdrive and needs the LDO supply.
- **Flash latency** is set by `Rcc_Init()` automatically: the maximal latency is used during the clock switch,
  the latency required by the AXI clock in the active voltage scale afterwards (reference manual tables - the
  ST LL driver `LL_SetFlashLatency()` sets too few wait states above 210 MHz on STM32H74x / H75x).
  `FlashLatency` of the configuration structure is not used.
- **PLLs** share one clock source - the source can be changed only while all PLLs are stopped
  (`Rcc_Init()` stops all PLLs first). PLL1 P output supports even dividers only on STM32H74x / H75x.
- **Trace clock** (`RCC_PERIPH_TRACE`, used by SWO of the Itm module) is switched together with the system
  clock: PLL1 R output with PLL1 system clock (R divider must not be 0), otherwise the system clock
  oscillator (HSI / CSI / HSE) - not divided by the system clock prescaler.
- **CMSIS variables** `SystemCoreClock` (CPU clock) and `SystemD2Clock` (AXI clock) are updated by
  `Rcc_Init()`, `Rcc_Set_OscDiv()` and `Rcc_Set_ClkBusDivider()` (AHB prescaler).
- **Device reset state**: `Rcc_Init()` applies the AXI SRAM errata workaround of revision Y devices
  (STM32H74x / H75x) and disables FMC bank 1 (if FMC is not used yet), as the ST CMSIS `SystemInit()` does.
- Default configuration (`Rcc_Get_DefaultConfig()`) is valid for every board: HSI 64 MHz, PLLs stopped,
  voltage scale 3, supply configuration of the reset.

---

## STM32H7R / H7S Specifics

The STM32H7R3 / H7R7 / H7S3 / H7S7 lines are compiled from the same sources (`STM32H7RS` defined by the CMSIS
device header of the Ral family STM32H7RS). Differences to the other STM32H7 lines:

- **Buses**: CPU prescaler CPRE, bus matrix prescaler BMPRE (HCLK of AHB1 - AHB5), APB prescalers PPRE1 /
  PPRE2 / PPRE4 / PPRE5. `RCC_CLK_BUS_AHB5` and `RCC_CLK_BUS_APB5` are added, there is no APB3 bus
  (`RCC_CLK_BUS_APB3`, `rcc_APB3_Div_t` and `APB3_Divider` are replaced by `RCC_CLK_BUS_APB5`, `rcc_APB5_Div_t`
  and `APB5_Divider`).
- **PLLs**: multiplier 8 - 420, P / Q / R dividers 1 - 128 (PLL1 P also odd), wide VCO range 400 - 1600 MHz
  (LL / HAL limit), medium 150 - 420 MHz. Outputs S (all PLLs) and T (PLL2 only) with dividers 1 - 8 are
  configured by `S_Divider` / `T_Divider` of `rcc_PllConfigStruct_t` (0 - output not used, `T_Divider` must be 0
  for PLL1 / PLL3); their frequencies are available through the kernel clocks of the peripherals (eg.
  `RCC_PERIPH_XSPI1_PLL2S`, `RCC_PERIPH_SDMMC1_PLL2T`).
- **Kernel clock multiplexers** in CCIPR1 - CCIPR4: the multiplexer identifications follow the ST LL names
  (eg. `RCC_CLK_MUX_USART1_*`, `RCC_CLK_MUX_SPI23_*`), the peripheral identifications are the same as on the
  other lines (eg. `RCC_PERIPH_USART1_HSI`, `RCC_PERIPH_I2C1_PCLK1`). RNG has no kernel clock multiplexer -
  `RCC_PERIPH_RNG_HSI48` only. SDMMC kernel clock is PLL2 S (default) or PLL2 T. HCLK / 4 of FMC / XSPI is a
  state of the clock switch protection (not selectable).
- **Voltage scaling**: VOS high (`RCC_PWR_VOLTAGE_SCALE_0`) and VOS low (`RCC_PWR_VOLTAGE_SCALE_1`, reset value
  and default configuration); scales 2 / 3 return error.
- **Supply configuration** (PWR_CSR2): LDO, direct SMPS, SMPS 1.8 V (LDO, external and LDO, external) and
  external source - SMPS 2.5 V configurations return error.
- **Flash latency**: 0 - 7 wait states (VOS high 40 MHz per wait state up to 300 MHz, VOS low 36 MHz per wait
  state up to 200 MHz), the programming delay WRHIGHFREQ is set together with the wait states.
- **CMSIS variables**: `SystemCoreClock` only (no `SystemD2Clock`).
- **Device reset state**: no workarounds (FMC bank 1 is not disabled - not done by the ST system
  initialization of these lines).

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
