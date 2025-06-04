cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PICO_BOARD pico_w CACHE STRING "Board type")
include(pico_sdk_import.cmake)

# Adição do FreeRTOS no CMakeLists
set(FREERTOS_KERNEL_PATH "C:/EmbarcaTech/FreeRTOS-Kernel")
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

include_directories(${CMAKE_SOURCE_DIR}/lib) # Para chamar a pasta lib de forma mais elegante no codigo fonte

project(DogRoverMQTT C CXX ASM)

pico_sdk_init()

add_executable(DogRoverMQTT DogRoverMQTT.c lib/led_matrix.c lib/ssd1306.c)

pico_set_program_name(DogRoverMQTT "DogRoverMQTT")
pico_set_program_version(DogRoverMQTT "0.1")

# Adicionando o arquivo PIO
pico_generate_pio_header(DogRoverMQTT ${CMAKE_CURRENT_LIST_DIR}/lib/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)


# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(DogRoverMQTT 0)
pico_enable_stdio_usb(DogRoverMQTT 1)

# Add the standard library to the build
target_link_libraries(DogRoverMQTT
        pico_stdlib
        hardware_gpio
        hardware_adc
        hardware_pwm
        hardware_pio
        hardware_i2c
        hardware_clocks
        pico_cyw43_arch_lwip_threadsafe_background
        pico_lwip_mqtt
        pico_mbedtls
        pico_lwip_mbedtls
        FreeRTOS-Kernel 
        FreeRTOS-Kernel-Heap4)

# Add the standard include files to the build
target_include_directories(DogRoverMQTT PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)


pico_add_extra_outputs(DogRoverMQTT)

