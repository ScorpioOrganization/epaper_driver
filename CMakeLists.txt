cmake_minimum_required(VERSION 3.16)
project(epaper_driver VERSION 0.1.0 LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(EPAPER_DRIVER_TOP_LEVEL ON)
else()
  set(EPAPER_DRIVER_TOP_LEVEL OFF)
endif()

option(EPAPER_DRIVER_BUILD_TESTS "Build the epaper_driver unit tests" ${EPAPER_DRIVER_TOP_LEVEL})
option(EPAPER_DRIVER_INSTALL "Generate install rules for epaper_driver" ${EPAPER_DRIVER_TOP_LEVEL})

include(GNUInstallDirs)

add_library(epaper_driver STATIC
  src/canvas.cpp
  src/display.cpp
  src/errors.cpp
  src/font.cpp
  src/framebuffer.cpp
  src/hal.cpp
  src/types.cpp
  src/linux/gpio.cpp
  src/linux/spi.cpp
  src/linux/syscalls.cpp
  src/panels/epd_2in9_v1.cpp
  src/panels/epd_2in9_v2.cpp
  src/panels/pbm_file_panel.cpp
  src/panels/ssd16xx.cpp
)
add_library(epaper_driver::epaper_driver ALIAS epaper_driver)
target_compile_features(epaper_driver PUBLIC cxx_std_17)
set_target_properties(epaper_driver PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  CXX_EXTENSIONS OFF
)
target_include_directories(epaper_driver PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set(EPAPER_DRIVER_WARNINGS -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
target_compile_options(epaper_driver PRIVATE ${EPAPER_DRIVER_WARNINGS})

if(EPAPER_DRIVER_BUILD_TESTS)
  enable_testing()
  find_package(GTest CONFIG QUIET)
  if(NOT GTest_FOUND)
    if(NOT EPAPER_DRIVER_TOP_LEVEL)
      message(FATAL_ERROR "epaper_driver tests need GoogleTest (libgtest-dev)")
    endif()
    include(FetchContent)
    fetchcontent_declare(googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
      URL_HASH SHA256=8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7
    )
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    fetchcontent_makeavailable(googletest)
  endif()

  set(EPAPER_DRIVER_TESTS
    test_canvas
    test_display
    test_epd_2in9_v1
    test_epd_2in9_v2
    test_font
    test_framebuffer
    test_linux_gpio
    test_linux_spi
    test_linux_syscalls
    test_pbm_file_panel
    test_types
  )
  foreach(test_name ${EPAPER_DRIVER_TESTS})
    add_executable(epaper_${test_name} test/${test_name}.cpp)
    target_include_directories(epaper_${test_name} PRIVATE test)
    target_link_libraries(epaper_${test_name} PRIVATE epaper_driver GTest::gtest_main)
    target_compile_options(epaper_${test_name} PRIVATE ${EPAPER_DRIVER_WARNINGS})
    add_test(NAME epaper_${test_name}
      COMMAND epaper_${test_name}
        --gtest_output=xml:${CMAKE_BINARY_DIR}/test_results/epaper_driver/epaper_${test_name}.gtest.xml
    )
  endforeach()
endif()

if(EPAPER_DRIVER_INSTALL)
  include(CMakePackageConfigHelpers)
  install(TARGETS epaper_driver
    EXPORT epaper_driverTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )
  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
  install(EXPORT epaper_driverTargets
    NAMESPACE epaper_driver::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/epaper_driver
  )
  configure_package_config_file(cmake/epaper_driverConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/epaper_driverConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/epaper_driver
  )
  write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/epaper_driverConfigVersion.cmake
    COMPATIBILITY SameMajorVersion
  )
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/epaper_driverConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/epaper_driverConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/epaper_driver
  )
endif()
