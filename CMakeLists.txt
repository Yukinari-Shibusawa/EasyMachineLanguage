cmake_minimum_required(VERSION 3.16)

project(EasyMachineLanguage LANGUAGES CXX)

add_library(easy_machine_language
    src/Program.cpp
)
add_library(EasyMachineLanguage::easy_machine_language ALIAS easy_machine_language)

target_include_directories(easy_machine_language PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(easy_machine_language PUBLIC cxx_std_17)

include(CTest)
if(BUILD_TESTING)
    add_executable(easy_machine_language_tests tests/ProgramTests.cpp)
    target_link_libraries(easy_machine_language_tests PRIVATE
        EasyMachineLanguage::easy_machine_language)
    add_test(NAME easy_machine_language_tests COMMAND easy_machine_language_tests)
endif()

include(GNUInstallDirs)
install(TARGETS easy_machine_language
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
