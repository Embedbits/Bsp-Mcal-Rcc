# RCC MCAL Module for STM32G4

This module provides an abstraction layer for the Reset and Clock Control (RCC) peripheral on STM32G4 microcontrollers.  
It is part of the **MCAL (Microcontroller Abstraction Layer)** and allows safe and portable configuration of system clocks, PLL, peripheral clocks, low-power states, and reset management.

Supported MCUs: STM32G411, G414, G431, G441, G471, G473, G474, G483, G484, G491, G4A1.
Peripherals and kernel clock sources not present on the selected MCU are excluded at compile time
(enumerators are guarded by RCC register bit definitions of the device header).

---

## Features

- Module initialization and de-initialization
- Peripheral clock enable/disable and state management
- Peripheral reset handling
- Power and sleep state configuration
- PLL configuration and management (one PLL with outputs P, Q, R)
- Clock sources configuration (HSE, HSI16, HSI48, LSE, LSI)
- RTC clock source selection (HSE / 32, LSE, LSI)
- Kernel clock multiplexers (USART1-3, UART4-5, LPUART1, I2C1-4, LPTIM1, SAI1, I2S23, FDCAN,
  CLK48 of USB / RNG, ADC12, ADC345, QUADSPI, RTC)
- System clock and bus clock dividers (AHB, APB1, APB2)
- Voltage range 1 (boost / normal mode) and range 2, flash latency, prefetch, instruction and data cache
- SysTick interval configuration
- Clock outputs MCO (PA8) and LSCO (PA2)
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

Default configuration is valid for every STM32G4 MCU: PLL from HSI16 (16 MHz / 4 * 85, VCO 340 MHz),
SYSCLK = HCLK = 170 MHz (output R / 2) in voltage range 1 boost mode, APB1 and APB2 170 MHz, PLL P
56.67 MHz (ADC kernel clock). HSE, PLL output Q and clock outputs are not used.

`Rcc_Init()` switches the system clock to HSI16 before the PLL is reconfigured. A switch to a system
clock above 80 MHz passes through the AHB / 2 transition state required by RM0440 (HCLK divided by 2
for at least 1 us). The flash latency is set to the safe value during the change and to the value
of the new clock afterwards.

### Peripheral Clock Management
- `rcc_RequestState_t Rcc_Set_PeriphActive(rcc_PeriphId_t periphId)`
- `rcc_RequestState_t Rcc_Set_PeriphInactive(rcc_PeriphId_t periphId)`
- `rcc_RequestState_t Rcc_Get_PeriphState(rcc_PeriphId_t periphId, rcc_FunctionState_t * const funcState)`
- `rcc_RequestState_t Rcc_Get_PeriphClk(rcc_PeriphId_t periphId, rcc_FreqHz_t * const periphClk)`
- `rcc_RequestState_t Rcc_Get_PeriphClkSrc(rcc_PeriphId_t periphId, rcc_PeriphId_t * const periphClkSrc)`

Peripherals with a kernel clock multiplexer have one identifier per clock source
(eg. `RCC_PERIPH_USART1_PCLK2`, `RCC_PERIPH_USART1_SYSCLK`, `RCC_PERIPH_USART1_HSI`,
`RCC_PERIPH_USART1_LSE`). `Rcc_Set_PeriphActive()` selects the source and starts the internal
oscillator of the selected kernel clock if it is not running yet (HSI16, HSI48, LSI). External
sources (HSE, LSE) and PLL outputs are not started automatically. The multiplexer can be changed only
while the peripheral is disabled. The CLK48 multiplexer is shared by USB and RNG - it is kept while
the other block is enabled. Kernel clock of the external I2S_CKIN pin is not known
(`Rcc_Get_PeriphClk()` returns error).

### Peripheral Reset Management
- `rcc_RequestState_t Rcc_Set_ResetActive(rcc_PeriphId_t periphId)`
- `rcc_RequestState_t Rcc_Set_ResetInactive(rcc_PeriphId_t periphId)`
- `rcc_RequestState_t Rcc_Get_ResetState(rcc_PeriphId_t periphId, rcc_FunctionState_t * const funcState)`

WWDG, RTC APB interface and RTC (backup domain) have no reset control - reset activation returns error.

### Power and Sleep Management
- `rcc_RequestState_t Rcc_Set_SleepActive(rcc_PeriphId_t periphId)`
- `rcc_RequestState_t Rcc_Set_SleepInactive(rcc_PeriphId_t periphId)`
- `rcc_RequestState_t Rcc_Get_SleepState(rcc_PeriphId_t periphId, rcc_FunctionState_t * const funcState)`

SRAM1, SRAM2, CCM SRAM and flash interface have sleep mode clock control only.

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

PLL limits: M 1 - 16, N 8 - 127, input 2.66 - 16 MHz, VCO 96 - 344 MHz, P 2 - 31, Q / R 2, 4, 6, 8.
Divider 0 marks an unused output (output disabled by PLLxEN bit). The PLL can not be reconfigured
while it is the system clock.

### Oscillators
- `rcc_RequestState_t Rcc_Set_OscActive(rcc_OscId_t oscId)`
- `rcc_RequestState_t Rcc_Set_OscInactive(rcc_OscId_t oscId)`
- `rcc_RequestState_t Rcc_Get_OscState(rcc_OscId_t oscId, rcc_FunctionState_t * const retState)`
- `rcc_RequestState_t Rcc_Set_OscDiv(rcc_OscId_t oscId, rcc_OscDiv_t oscDiv)`
- `rcc_RequestState_t Rcc_Get_OscDiv(rcc_OscId_t oscId, rcc_OscDiv_t * const oscDiv)`

Oscillators: HSI16, HSI48, LSI, LSE (HSE is configured by `Rcc_Init()`). The oscillators have no
output divider (only divider 1 is accepted). Backup domain write protection (LSE, RTC, LSCO) is
released automatically.

### RTC Clock Source
- `rcc_RequestState_t Rcc_Set_RtcClkSource(rcc_Rtc_ClkSource_t clkSource)`
- `rcc_RequestState_t Rcc_Get_RtcClkSource(rcc_Rtc_ClkSource_t * const clkSource)`

RTC clock source can be selected only once after backup domain reset. HSE is divided by fixed 32.

### Clock Bus Configuration
- `rcc_RequestState_t Rcc_Set_ClkBusDivider(rcc_ClkBusId_t clkBusId, rcc_ClkBusDiv_t clkBusDivider)`
- `rcc_RequestState_t Rcc_Get_ClkBusDivider(rcc_ClkBusId_t clkBusId, rcc_ClkBusDiv_t * const clkBusDivider)`
- `rcc_RequestState_t Rcc_Get_ClkBusClk(rcc_ClkBusId_t clkBusId, rcc_FreqHz_t * const clkBusFreq)`

AHB1, AHB2 and AHB3 share the AHB prescaler, APB1 groups 1 and 2 share the APB1 prescaler.

### Flash and System Configuration
- `rcc_RequestState_t Rcc_Set_PwrRange(rcc_ConfigStruct_t * const clockConfig)`
- `rcc_RequestState_t Rcc_Set_FlashLatency(rcc_ConfigStruct_t * const clockConfig)`
- `rcc_RequestState_t Rcc_Set_FlashPrefetchActive(void)`
- `rcc_RequestState_t Rcc_Set_FlashPrefetchInactive(void)`
- `rcc_RequestState_t Rcc_Set_SysTickInterval(rcc_Time_ms_t sysTickInterval)`
- `rcc_RequestState_t Rcc_Get_SysTickInterval(rcc_Time_ms_t * const sysTickInterval)`

Voltage scaling: `RCC_PWR_VOLTAGE_SCALE_0` - range 1 boost mode (max. 170 MHz), `RCC_PWR_VOLTAGE_SCALE_1` -
range 1 normal mode (max. 150 MHz), `RCC_PWR_VOLTAGE_SCALE_2` - range 2 (max. 26 MHz). A range whose
maximum is below the running system clock is rejected. Flash wait states are calculated from the
expected HCLK of the selected range (RM0440). The configured `FlashLatency` is used as minimal value.

### Clock Outputs
- `rcc_RequestState_t Rcc_Set_ClkOutSource(rcc_ClkOut_Id_t outId, rcc_ClkOut_Source_t clkSource)`
- `rcc_RequestState_t Rcc_Get_ClkOutSource(rcc_ClkOut_Id_t outId, rcc_ClkOut_Source_t * const clkSource)`
- `rcc_RequestState_t Rcc_Set_ClkOutDivider(rcc_ClkOut_Id_t outId, rcc_ClkOut_Div_t clkDivider)`
- `rcc_RequestState_t Rcc_Get_ClkOutDivider(rcc_ClkOut_Id_t outId, rcc_ClkOut_Div_t * const clkDivider)`

MCO (PA8, alternate function 0): SYSCLK, HSI16, HSE, PLL R, LSI, LSE, HSI48, divider 1, 2, 4, 8, 16.
LSCO (PA2, analog mode): LSI or LSE, no divider. LSCO is in backup domain - it keeps running after
system reset. `RCC_CLK_SOURCE_NONE` leaves the output unchanged.

### Reset Source Flags
- `rcc_RequestState_t Rcc_Get_ResetSource(rcc_ResetSrc_t resetSrc, rcc_FlagState_t * const flagState)`
- `rcc_RequestState_t Rcc_Set_ResetSourceClear(void)`

Reset sources: NRST pin, BOR, software, IWDG, WWDG, low-power, option byte loader.

---

## Tests

- Unit tests (`Tests/UnitTests`) - all components with emulated RCC, PWR, FLASH and SysTick registers,
  HW model thread for oscillator / PLL ready flags and system clock switch, GPIO mocked.
- Integration tests (`Tests/IntegrationTests`) - NUCLEO-G474RE and NUCLEO-G431RB (board MB1367: HSE 24 MHz
  crystal, LSE crystal).

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
