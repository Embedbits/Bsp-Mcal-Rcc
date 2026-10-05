#===================== Integration tests configuration ========================#
# Integration tests of Rcc module on target (Nucleo board). Registered in module
# CMakeLists.txt by IntegrationTesting_AddPath() and created by
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

IntegrationTesting_Add_Test(
    NAME            Rcc
    TEST_SOURCE     ItTest_Rcc.c
    SOURCES         ItTarget_Rcc.c      # Target interface of the test set
    LINK_LIBS       StartUp_Lib         # Startup code (calls BspMain)
                    Rcc_Lib
                    Nvic_Lib            # SysTick handler, system reset
                    Gpio_Lib            # MCO pin level
                    Tim_Lib             # timer clocked from RCC frequency
    TIMEOUT         60
)
