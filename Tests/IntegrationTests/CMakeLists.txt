#===================== Integration tests configuration ========================#
# Integration tests of Rcc module on target (Nucleo-144 / 32F746GDISCOVERY). Registered in
# module CMakeLists.txt by IntegrationTesting_AddPath() and created by
# IntegrationTesting_Generate() in "IntegrationTest" build
# (see EmBi_Platform/CMake/IntegrationTesting/README.md).
#
# The test set is independent - target interface (entry point, initialization,
# fault handlers, system reset) is implemented by ItTarget_Rcc.c and
# all libraries of the firmware are listed here.

# StartUp calls BspMain() - the entry point of the test firmware is provided by
# the test set (BspMain.h, ItTarget_Rcc.c)
if(NOT TARGET BspMain_Lib)
    add_library(BspMain_Lib INTERFACE)
    target_include_directories(BspMain_Lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
endif()

if(MCU_FAMILY_ID STREQUAL "STM32H7RSxx")

    # STM32H7R / STM32H7S (NUCLEO-H7S3L8)
    IntegrationTesting_Add_Test(
        NAME            Rcc
        TEST_SOURCE     ItTest_Rcc_H7RS.c
        SOURCES         ItTarget_Rcc.c      # Target interface of the test set
        LINK_LIBS       StartUp_Lib         # Startup code (calls BspMain)
                        Rcc_Lib
                        Nvic_Lib            # SysTick handler, system reset
                        Gpio_Lib            # MCO pin level
                        Tim_Lib             # timer clocked from RCC frequency
        TIMEOUT         60
    )

else()

    # Classic STM32H7 (NUCLEO-H723ZG, NUCLEO-H7A3ZI-Q)
    IntegrationTesting_Add_Test(
        NAME            Rcc
        TEST_SOURCE     ItTest_Rcc.c
        SOURCES         ItTarget_Rcc.c      # Target interface of the test set
        LINK_LIBS       StartUp_Lib         # Startup code (calls BspMain)
                        Nvic_Lib            # Fault handlers, system reset (ItTarget, reset source test)
                        Gpio_Lib            # Clock output pins, peripheral reset check
                        Rcc_Lib             # Module under test
        TIMEOUT         90
    )

endif()
