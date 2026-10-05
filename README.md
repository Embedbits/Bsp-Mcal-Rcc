# RCC MCAL Module for STM32F4

This module provides an abstraction layer for the Reset and Clock Control (RCC) peripheral on STM32F4 microcontrollers.  
It is part of the **MCAL (Microcontroller Abstraction Layer)** and allows safe and portable configuration of system clocks, PLLs, peripheral clocks, low-power states, and reset management.

Supported MCUs: STM32F401, F405, F407, F410, F411, F412, F413, F415, F417, F423, F427, F429, F437, F439, F446, F469, F479.
Peripherals, PLLs and clock sources not present on the selected MCU are excluded at compile time
(enumerators are guarded by RCC register bit definitions of the device header).

---

## Features

- Module initialization and de-initialization
- Peripheral clock enable/disable and state management
- Peripheral reset handling
- Power and sleep state configuration
- PLL configuration and management (main PLL, PLLI2S, PLLSAI)
- Clock sources configuration (HSE, HSI, LSE, LSI)
- RTC clock source selection (HSE / RTCPRE, LSE, LSI)
- Kernel clock multiplexers (RTC, LPTIM1, FMPI2C1)
- System clock and bus clock dividers (AHB, APB1, APB2)
- Voltage scaling, over-drive mode, flash latency, prefetch and ART caches
- SysTick interval configuration
- Clock outputs MCO1 (PA8) and MCO2 (PC9)
- Reset source flags

---

## Public API

The public API (`Rcc_Port.h`) is common with other MCU families. Family specific are only the
enumerations and configuration structures (`Rcc_Types.h`).

### Module Management
- `rcc_ModuleVersion_t Rcc_Get_ModuleVersion(void)`
- `rcc_RequestState_t Rcc_Init(rcc_ConfigStruct_t * const clockConfig)`
- `void Rcc_Deinit(rcc_ConfigStruct_t * const clockConfig)`
- `void Rcc_Task(void)`
- `rcc_RequestState_t Rcc_Get_DefaultConfig(rcc_ConfigStruct_t * const clockConfig)`

Default configuration is valid for every STM32F4 MCU: main PLL from HSI (16 MHz / 16 * 336),
SYSCLK = HCLK = 84 MHz, APB1 42 MHz, APB2 84 MHz, PLL Q 48 MHz. HSE, PLLI2S, PLLSAI and clock
outputs are not used.

### Peripheral Clock Management
- `rcc_RequestState_t Rcc_Set_PeriphActive(rcc_PeriphId_t periphId)`
- `rcc_RequestState_t Rcc_Set_PeriphInactive(rcc_PeriphId_t periphId)`
- `rcc_RequestState_t Rcc_Get_PeriphState(rcc_PeriphId_t periphId, rcc_FunctionState_t * const funcState)`
- `rcc_RequestState_t Rcc_Get_PeriphClk(rcc_PeriphId_t periphId, rcc_FreqHz_t * const periphClk)`
- `rcc_RequestState_t Rcc_Get_PeriphClkSrc(rcc_PeriphId_t periphId, rcc_PeriphId_t * const periphClkSrc)`

`Rcc_Set_PeriphActive()` starts the internal oscillator (HSI, LSI) of the selected kernel clock if it
is not running yet (eg. `RCC_PERIPH_LPTIM1_LSI`). External sources (HSE, LSE) and PLL outputs are not
started automatically. Kernel clocks of I2S, SAI, LTDC, DSI, CEC and SPDIFRX are not handled
(`Rcc_Get_PeriphClk()` returns error). 48 MHz clock (CK48) and SDIO multiplexers are kept in reset
configuration, their actual selection is used for frequency calculation.

### Peripheral Reset Management
- `rcc_RequestState_t Rcc_Set_ResetActive(rcc_PeriphId_t periphId)`
- `rcc_RequestState_t Rcc_Set_ResetInactive(rcc_PeriphId_t periphId)`
- `rcc_RequestState_t Rcc_Get_ResetState(rcc_PeriphId_t periphId, rcc_FunctionState_t * const funcState)`

ADC1, ADC2 and ADC3 share one reset control.

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

All PLLs share the PLL source multiplexer. PLLSAI (and PLLI2S on MCUs without own PLLI2SM) shares the
M divider with the main PLL. Shared settings can be changed only while no other PLL using them is active.

### Oscillators
- `rcc_RequestState_t Rcc_Set_OscActive(rcc_OscId_t oscId)`
- `rcc_RequestState_t Rcc_Set_OscInactive(rcc_OscId_t oscId)`
- `rcc_RequestState_t Rcc_Get_OscState(rcc_OscId_t oscId, rcc_FunctionState_t * const retState)`
- `rcc_RequestState_t Rcc_Set_OscDiv(rcc_OscId_t oscId, rcc_OscDiv_t oscDiv)`
- `rcc_RequestState_t Rcc_Get_OscDiv(rcc_OscId_t oscId, rcc_OscDiv_t * const oscDiv)`

Oscillators: HSI, LSI, LSE (HSE is configured by `Rcc_Init()`). Backup domain write protection
(LSE, RTC) is released automatically.

### RTC Clock Source
- `rcc_RequestState_t Rcc_Set_RtcClkSource(rcc_Rtc_ClkSource_t clkSource)`
- `rcc_RequestState_t Rcc_Get_RtcClkSource(rcc_Rtc_ClkSource_t * const clkSource)`

RTC clock source can be selected only once after backup domain reset.

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

Flash wait states are calculated from the expected HCLK for supply voltage 2.7 - 3.6 V. The configured
`FlashLatency` is used as minimal value. Over-drive mode (F42x, F43x, F446, F469, F479) is activated
automatically above 168 MHz (scale 1) / 144 MHz (scale 2).

### Clock Outputs
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

Dependencies: `Ral_Lib`, `Gpio_Lib` (clock output pins).

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
