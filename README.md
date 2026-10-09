# RCC MCAL Module for STM32L4 / STM32L4+

This module provides an abstraction layer for the Reset and Clock Control (RCC) peripheral on STM32L4 and STM32L4+ microcontrollers.  
It is part of the **MCAL (Microcontroller Abstraction Layer)** and allows safe and portable configuration of system clocks, PLLs, peripheral clocks, low-power states, and reset management.

Supported MCUs: STM32L412, L422, L431, L432, L433, L442, L443, L451, L452, L462, L471, L475, L476, L485, L486, L496, L4A6 (STM32L4)
and STM32L4P5, L4Q5, L4R5, L4R7, L4R9, L4S5, L4S7, L4S9 (STM32L4+).
Peripherals, PLLs and kernel clock sources not present on the selected MCU are excluded at compile time
(enumerators are guarded by RCC register bit definitions of the device header).

---

## Features

- Module initialization and de-initialization
- Peripheral clock enable/disable and state management
- Peripheral reset handling
- Power and sleep state configuration
- PLL configuration and management (main PLL, PLLSAI1, PLLSAI2 with outputs P, Q, R)
- Clock sources configuration (HSE, HSI16, MSI with range selection, HSI48, LSE, LSI)
- RTC clock source selection (HSE / 32, LSE, LSI)
- Kernel clock multiplexers (USART1-3, UART4-5, LPUART1, I2C1-4, LPTIM1-2, SWPMI1, DFSDM1, ADC,
  48 MHz clock of USB / RNG / SDMMC, RTC)
- System clock and bus clock dividers (AHB, APB1, APB2)
- Voltage range 1 / 2, range 1 boost mode of STM32L4+, flash latency, prefetch, instruction and data cache
- Independent supplies VDDIO2 (port G) and VDDUSB (USB) validated with the peripheral clock
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

Default configuration is valid for every MCU of the family: main PLL from HSI16 (16 MHz / 1),
SYSCLK = HCLK = 80 MHz (STM32L4, N 10, VCO 160 MHz) or 120 MHz (STM32L4+, N 15, VCO 240 MHz, range 1
boost mode), output Q 40 MHz (RNG), APB1 and APB2 not divided, voltage range 1. HSE, MSI, PLLSAI1,
PLLSAI2, output P and clock outputs are not used.

`Rcc_Init()` checks the expected system clock against the voltage range first - an invalid
configuration is rejected without any register change. The system clock is switched to HSI16 before
the PLLs are reconfigured (the reset system clock is MSI 4 MHz). The flash latency is set to the
highest value of the family during the change and to the value of the new clock afterwards. A switch
to a system clock above 80 MHz (STM32L4+) passes through the AHB / 2 transition state required by
RM0432 (HCLK divided by 2 for at least 1 us). The MSI range is configured only if MSI is the system
clock or the PLL source.

### Peripheral Clock Management
- `rcc_RequestState_t Rcc_Set_PeriphActive(rcc_PeriphId_t periphId)`
- `rcc_RequestState_t Rcc_Set_PeriphInactive(rcc_PeriphId_t periphId)`
- `rcc_RequestState_t Rcc_Get_PeriphState(rcc_PeriphId_t periphId, rcc_FunctionState_t * const funcState)`
- `rcc_RequestState_t Rcc_Get_PeriphClk(rcc_PeriphId_t periphId, rcc_FreqHz_t * const periphClk)`
- `rcc_RequestState_t Rcc_Get_PeriphClkSrc(rcc_PeriphId_t periphId, rcc_PeriphId_t * const periphClkSrc)`

Peripherals with a kernel clock multiplexer have one identifier per clock source
(eg. `RCC_PERIPH_USART1_PCLK2`, `RCC_PERIPH_USART1_SYSCLK`, `RCC_PERIPH_USART1_HSI`,
`RCC_PERIPH_USART1_LSE`). `Rcc_Set_PeriphActive()` selects the source and starts the internal
oscillator of the selected kernel clock if it is not running yet (HSI16, MSI, HSI48, LSI). External
sources (HSE, LSE) and PLL outputs are not started automatically, RTC with HSE / 32 requires a
configured HSE frequency. The multiplexer can be changed only while the peripheral is disabled.

The 48 MHz clock (CLK48SEL) is shared by USB, RNG and SDMMC (STM32L4+ SDMMC can use main PLL output P
selected by SDMMCSEL). Its selection is kept while another user of the clock is enabled and released by
the last disabled user. On STM32L47x / L48x (no HSI48) the reset selection of CLK48SEL is "no clock".

Port G (PG[15:2] supplied by VDDIO2) and USB (VDDUSB) - the independent supply is validated
(PWR_CR2 IOSV / USV) before the peripheral clock is enabled.

Kernel clocks of SAI, LTDC, DSI, OCTOSPI and LCD are not handled (`Rcc_Get_PeriphClk()` returns error).
ADC1, ADC2 and ADC3 share one clock enable, reset control and kernel clock multiplexer (ADCSEL not
present on STM32L41x / L42x - ADC is clocked synchronously by HCLK).

### Peripheral Reset Management
- `rcc_RequestState_t Rcc_Set_ResetActive(rcc_PeriphId_t periphId)`
- `rcc_RequestState_t Rcc_Set_ResetInactive(rcc_PeriphId_t periphId)`
- `rcc_RequestState_t Rcc_Get_ResetState(rcc_PeriphId_t periphId, rcc_FunctionState_t * const funcState)`

WWDG, firewall, RTC APB interface and RTC (backup domain) have no reset control - reset activation returns error.

### Power and Sleep Management
- `rcc_RequestState_t Rcc_Set_SleepActive(rcc_PeriphId_t periphId)`
- `rcc_RequestState_t Rcc_Set_SleepInactive(rcc_PeriphId_t periphId)`
- `rcc_RequestState_t Rcc_Get_SleepState(rcc_PeriphId_t periphId, rcc_FunctionState_t * const funcState)`

SRAM1, SRAM2 and SRAM3 have sleep mode clock control only.

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

PLLs: `RCC_PLL_1` main PLL (system clock from output R), `RCC_PLL_2` PLLSAI1, `RCC_PLL_3` PLLSAI2
(where available). All PLLs share the source multiplexer (PLLSRC: MSI, HSI16, HSE). PLLSAI1 / PLLSAI2
of STM32L4 share the M divider of the main PLL, STM32L4+ have own M dividers. A shared setting can be
changed only if no other PLL using it is active, the source of an active PLL can not be changed.

PLL limits: M 1 - 8 (STM32L4) / 1 - 16 (STM32L4+), N 8 - 86 (PLLSAI of STM32L4+ 8 - 127), input
4 - 16 MHz, VCO 64 - 344 MHz, P 2 - 31 (MCUs with PDIV) or 7 / 17, Q / R 2, 4, 6, 8. Divider 0 marks
an unused output (output disabled by its enable bit). Main PLL of STM32L41x / L42x has no output P,
PLLSAI2 of STM32L4 has no output Q. The PLL can not be reconfigured while it is the system clock.

### Oscillators
- `rcc_RequestState_t Rcc_Set_OscActive(rcc_OscId_t oscId)`
- `rcc_RequestState_t Rcc_Set_OscInactive(rcc_OscId_t oscId)`
- `rcc_RequestState_t Rcc_Get_OscState(rcc_OscId_t oscId, rcc_FunctionState_t * const retState)`
- `rcc_RequestState_t Rcc_Set_OscDiv(rcc_OscId_t oscId, rcc_OscDiv_t oscDiv)`
- `rcc_RequestState_t Rcc_Get_OscDiv(rcc_OscId_t oscId, rcc_OscDiv_t * const oscDiv)`

Oscillators: HSI16, MSI, HSI48 (not on STM32L47x / L48x), LSI, LSE (HSE is configured by `Rcc_Init()`).
The oscillators have no output divider (only divider 1 is accepted), the MSI frequency is selected
by `MSI_Range` of the configuration (100 kHz - 48 MHz, MSIRGSEL is set). Backup domain write
protection (LSE, RTC, LSCO) is released automatically.

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

Voltage scaling: `RCC_PWR_VOLTAGE_SCALE_1` - range 1 (max. 80 MHz, STM32L4+ max. 120 MHz with boost mode
selected automatically above 80 MHz), `RCC_PWR_VOLTAGE_SCALE_2` - range 2 (max. 26 MHz). A range whose
maximum is below the running system clock is rejected. Flash wait states are calculated from the
expected HCLK of the selected range (RM0351 / RM0394: 16 MHz per wait state in range 1, RM0432:
20 MHz per wait state). The configured `FlashLatency` is used as minimal value.

### Clock Outputs
- `rcc_RequestState_t Rcc_Set_ClkOutSource(rcc_ClkOut_Id_t outId, rcc_ClkOut_Source_t clkSource)`
- `rcc_RequestState_t Rcc_Get_ClkOutSource(rcc_ClkOut_Id_t outId, rcc_ClkOut_Source_t * const clkSource)`
- `rcc_RequestState_t Rcc_Set_ClkOutDivider(rcc_ClkOut_Id_t outId, rcc_ClkOut_Div_t clkDivider)`
- `rcc_RequestState_t Rcc_Get_ClkOutDivider(rcc_ClkOut_Id_t outId, rcc_ClkOut_Div_t * const clkDivider)`

MCO (PA8, alternate function 0): SYSCLK, MSI, HSI16, HSE, PLL R, LSI, LSE, HSI48, divider 1, 2, 4, 8, 16.
LSCO (PA2, analog mode): LSI or LSE, no divider. LSCO is in backup domain - it keeps running after
system reset. `RCC_CLK_SOURCE_NONE` leaves the output unchanged.

### Reset Source Flags
- `rcc_RequestState_t Rcc_Get_ResetSource(rcc_ResetSrc_t resetSrc, rcc_FlagState_t * const flagState)`
- `rcc_RequestState_t Rcc_Set_ResetSourceClear(void)`

Reset sources: NRST pin, BOR, software, IWDG, WWDG, low-power, option byte loader, firewall.

---

## Tests

- Unit tests (`Tests/UnitTests`): all components of the module with RCC / PWR / FLASH registers in host
  memory and HW model thread (ready flags, system clock switch). Tests of the HW model are sensitive
  to host load (busy-wait timeouts of the module vs. scheduling of the model thread, EmBi_Platform AB#966).
- Integration tests (`Tests/IntegrationTests`): clock tree configurations measured on target.

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
