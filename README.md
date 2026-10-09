# RCC MCAL Module for STM32

This module provides an abstraction layer for the Reset and Clock Control (RCC) peripheral on STM32 microcontrollers.  
It is part of the **MCAL (Microcontroller Abstraction Layer)** and allows safe and portable configuration of system clocks, PLLs, peripheral clocks, low-power states, and reset management.

---

## Features

STM32U5 clock tree:

- Oscillators MSIS / MSIK (Multi-Speed Internal, 100 kHz - 48 MHz ranges), HSI16, HSI48, HSE
  (crystal or external analog / digital clock), LSI (prescaler 1 / 128), LSE
- PLL1 (system clock from output R), PLL2, PLL3 - input 4 - 16 MHz, VCO 128 - 544 MHz
- System clock source MSIS / HSI16 / HSE / PLL1, AHB, APB1, APB2 and APB3 dividers
- Voltage ranges 1 - 4 with automatic EPOD booster (range 1 / 2) and flash wait states
- Peripheral clock enable / reset / Sleep-Stop mode clock of all STM32U5 peripherals, kernel clock
  multiplexers (CCIPR1 / CCIPR2 / CCIPR3, RTCSEL)
- Independent supplies validated with the peripheral clock (VDDIO2 - port G, VDDA - ADC / DAC /
  COMP / OPAMP / VREFBUF, VDDUSB - USB)
- Clock outputs MCO (PA8) and LSCO (PA2), reset source flags, SysTick interval

Default configuration (`Rcc_Get_DefaultConfig`): system clock 160 MHz from PLL1 (MSIS 4 MHz, M = 1,
N = 80, R = 2), voltage range 1, all bus dividers 1, HSE / PLL2 / PLL3 not used, SysTick 1 ms.

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

`Rcc_Set_PeriphActive()` starts the internal oscillator (HSI16, HSI48, MSIS, MSIK, LSI) of the selected
kernel clock if it is not running yet (eg. `RCC_PERIPH_RNG_HSI48`). External sources (HSE, LSE) and PLL
outputs are not started automatically.

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

### Oscillators
- `rcc_RequestState_t Rcc_Set_OscActive(rcc_OscId_t oscId)`
- `rcc_RequestState_t Rcc_Set_OscInactive(rcc_OscId_t oscId)`
- `rcc_RequestState_t Rcc_Get_OscState(rcc_OscId_t oscId, rcc_FunctionState_t * const retState)`
- `rcc_RequestState_t Rcc_Set_OscDiv(rcc_OscId_t oscId, rcc_OscDiv_t oscDiv)`
- `rcc_RequestState_t Rcc_Get_OscDiv(rcc_OscId_t oscId, rcc_OscDiv_t * const oscDiv)`

MSIS / MSIK divider selects the MSI range as division of 48 MHz (1, 2, 3, 4, 12, 24, 36, 48, 120, 240,
360, 480), LSI divider is 1 or 128. HSE is configured by `Rcc_Init()` (`rcc_ConfigStruct_t`).

### Power Supply Validity
Independent supplies validated by software (PWR register access is part of the RCC module,
`rcc_PwrSupplyId_t`, `PWR_SVMCR`): VDDUSB (USB, `USV`), VDDIO2 (PG[15:2], `IO2SV`) and VDDA (ADC, DAC,
COMP, OPAMP, VREFBUF, `ASV`). `Rcc_Set_PeriphActive()` validates the supply of the activated peripheral
automatically, the functions below validate / isolate the domain explicitly.
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

### RTC Clock
- `rcc_RequestState_t Rcc_Set_RtcClkSource(rcc_Rtc_ClkSource_t clkSource)`
- `rcc_RequestState_t Rcc_Get_RtcClkSource(rcc_Rtc_ClkSource_t * const clkSource)`

RTC clock source can be selected once (changed only after backup domain reset).

### Clock Bus Configuration
- `rcc_RequestState_t Rcc_Set_ClkBusDivider(rcc_ClkBusId_t clkBusId, rcc_ClkBusDiv_t clkBusDivider)`
- `rcc_RequestState_t Rcc_Get_ClkBusDivider(rcc_ClkBusId_t clkBusId, rcc_ClkBusDiv_t * const clkBusDivider)`
- `rcc_RequestState_t Rcc_Get_ClkBusClk(rcc_ClkBusId_t clkBusId, rcc_FreqHz_t * const clkBusFreq)`

### Flash, Power and System Configuration
- `rcc_RequestState_t Rcc_Set_PwrRange(rcc_ConfigStruct_t * const clockConfig)`
- `rcc_RequestState_t Rcc_Set_FlashLatency(rcc_ConfigStruct_t * const clockConfig)`
- `rcc_RequestState_t Rcc_Set_FlashPrefetchActive(void)`
- `rcc_RequestState_t Rcc_Set_FlashPrefetchInactive(void)`
- `rcc_RequestState_t Rcc_Set_SysTickInterval(rcc_Time_ms_t sysTickInterval)`
- `rcc_RequestState_t Rcc_Get_SysTickInterval(rcc_Time_ms_t * const sysTickInterval)`

### Clock Outputs
- `rcc_RequestState_t Rcc_Set_ClkOutSource(rcc_ClkOut_Id_t outId, rcc_ClkOut_Source_t clkSource)`
- `rcc_RequestState_t Rcc_Get_ClkOutSource(rcc_ClkOut_Id_t outId, rcc_ClkOut_Source_t * const clkSource)`
- `rcc_RequestState_t Rcc_Set_ClkOutDivider(rcc_ClkOut_Id_t outId, rcc_ClkOut_Div_t clkDivider)`
- `rcc_RequestState_t Rcc_Get_ClkOutDivider(rcc_ClkOut_Id_t outId, rcc_ClkOut_Div_t * const clkDivider)`

### Reset Source
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