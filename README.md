# RCC MCAL Module for STM32F7

This module provides an abstraction layer for the Reset and Clock Control (RCC) peripheral on STM32F7 microcontrollers.  
It is part of the **MCAL (Microcontroller Abstraction Layer)** and allows safe and portable configuration of system clocks, PLLs, peripheral clocks, low-power states, and reset management.

Supported MCUs: STM32F722, F723, F730, F732, F733, F745, F746, F750, F756, F765, F767, F768, F769, F777, F778, F779.
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
- Kernel clock multiplexers (RTC, LPTIM1, USART1-6 / UART4, 5, 7, 8, I2C1-4, SDMMC1 / 2, SAI1 / 2, CEC, DFSDM1)
- System clock and bus clock dividers (AHB, APB1, APB2)
- Voltage scaling, over-drive mode, flash latency, prefetch and ART accelerator
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

Default configuration is valid for every STM32F7 MCU: main PLL from HSI (16 MHz / 16 * 432),
SYSCLK = HCLK = 216 MHz (voltage scale 1, over-drive), APB1 54 MHz, APB2 108 MHz, PLL Q 48 MHz. HSE,
PLLI2S, PLLSAI and clock outputs are not used.

### Peripheral Clock Management
- `rcc_RequestState_t Rcc_Set_PeriphActive(rcc_PeriphId_t periphId)`
- `rcc_RequestState_t Rcc_Set_PeriphInactive(rcc_PeriphId_t periphId)`
- `rcc_RequestState_t Rcc_Get_PeriphState(rcc_PeriphId_t periphId, rcc_FunctionState_t * const funcState)`
- `rcc_RequestState_t Rcc_Get_PeriphClk(rcc_PeriphId_t periphId, rcc_FreqHz_t * const periphClk)`
- `rcc_RequestState_t Rcc_Get_PeriphClkSrc(rcc_PeriphId_t periphId, rcc_PeriphId_t * const periphClkSrc)`

Peripherals with a kernel clock multiplexer have one ID per clock source (eg. `RCC_PERIPH_USART1_PCLK2`,
`RCC_PERIPH_USART1_HSI`). `Rcc_Set_PeriphActive()` selects the multiplexer only if it holds the reset
value (peripheral released by `Rcc_Set_PeriphInactive()` first) and starts the internal oscillator
(HSI, LSI) of the selected kernel clock if it is not running yet (eg. `RCC_PERIPH_LPTIM1_LSI`). External
sources (HSE, LSE) and PLL outputs are not started automatically. Kernel clocks of I2S, LTDC, DSI and
SPDIFRX are not handled (`Rcc_Get_PeriphClk()` returns error), SAI with external clock pin I2S_CKIN has
no known frequency. 48 MHz clock (CK48 - main PLL Q or PLLSAI P, used by USB OTG FS, RNG and SDMMC) is
kept in reset configuration, its actual selection is used for frequency calculation.

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

All PLLs share the PLL source multiplexer and the M divider of the main PLL. Shared settings can be
changed only while no other PLL using them is active. Main PLL output R exists on STM32F76x/F77x only,
PLLI2S output P and PLLSAI output R on STM32F74x/F75x/F76x/F77x.

### Oscillators
- `rcc_RequestState_t Rcc_Set_OscActive(rcc_OscId_t oscId)`
- `rcc_RequestState_t Rcc_Set_OscInactive(rcc_OscId_t oscId)`
- `rcc_RequestState_t Rcc_Get_OscState(rcc_OscId_t oscId, rcc_FunctionState_t * const retState)`
- `rcc_RequestState_t Rcc_Set_OscDiv(rcc_OscId_t oscId, rcc_OscDiv_t oscDiv)`
- `rcc_RequestState_t Rcc_Get_OscDiv(rcc_OscId_t oscId, rcc_OscDiv_t * const oscDiv)`

Oscillators: HSI, LSI, LSE (HSE is configured by `Rcc_Init()`). Backup domain write protection
(LSE, RTC) is released automatically.

Device errata ES0334 without workaround: "PC13 signal transitions disturb LSE" - PC13 should not
toggle while LSE clocks the RTC or other peripherals. The LSE drive capability is kept at its reset
value (low drive).

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

Flash wait states are calculated from the expected HCLK for supply voltage 2.7 - 3.6 V (one wait state
per 30 MHz). The configured `FlashLatency` is used as minimal value. Over-drive mode is activated
automatically above 180 MHz (scale 1, max. 216 MHz) / 168 MHz (scale 2, max. 180 MHz). `Rcc_Init()`
enables flash prefetch and the ART accelerator.

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
