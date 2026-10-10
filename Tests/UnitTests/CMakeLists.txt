#======================== Unit tests configuration ============================#
# Unit tests of Rcc module. Registered in module CMakeLists.txt by
# UnitTesting_AddPath() and created by UnitTesting_Generate() in "UnitTest"
# build (see EmBi_Platform/CMake/UnitTesting/README.md).
#
# All components of the module are tested together (Rcc_Reg, Rcc_ClkSrc,
# Rcc_Pll, Rcc_ClkBus, Rcc_ClkMux, Rcc_ClkOut). Sequences waiting for HW
# reaction (oscillator / PLL ready, system clock switch) use HW model thread
# of RegMem, therefore the test set runs serially.

get_filename_component(RCC_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
get_filename_component(MCAL_DIR "${RCC_DIR}/.." ABSOLUTE)

UnitTesting_Add_Test(
    NAME            Rcc
    TEST_SOURCE     Test_Rcc.c
    UUT_SOURCES     ${RCC_DIR}/Rcc.c
                    ${RCC_DIR}/Rcc_ClkBus.c
                    ${RCC_DIR}/Rcc_ClkMux.c
                    ${RCC_DIR}/Rcc_ClkOut.c
                    ${RCC_DIR}/Rcc_ClkSrc.c
                    ${RCC_DIR}/Rcc_Pll.c
                    ${RCC_DIR}/Rcc_Reg.c
    MOCK_HEADERS    ${MCAL_DIR}/Gpio/Gpio_Port.h        # Clock output pins (Rcc_ClkOut)
    INCLUDE_DIRS    ${RCC_DIR}
                    ${MCAL_DIR}/Gpio
    LINK_LIBS       ut_RegMem                           # Registers in host memory, HW model
                    ut_CmsisHost                        # CMSIS core intrinsics on host
    FORCE_INCLUDES  ${UNIT_TESTING_CMSIS_HOST_HEADER}   # Replaces cmsis_gcc.h
    HOST_LIBS       Ral_Lib                             # ST LL compiled for host
    COVERAGE_EXCLUDE ".*/Ral/.*"
    RUN_SERIAL                                          # HW model thread (RegMem_Set_ModelActive)
)
