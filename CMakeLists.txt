cmake_minimum_required(VERSION 3.25.1)
project(json-formater
        VERSION 0.3.0
        LANGUAGES CXX
        HOMEPAGE_URL "https://github.com/khromenokroman/json-formater")

set(CMAKE_CXX_STANDARD 20)

find_package(fmt REQUIRED)

configure_file(Version.in Version @ONLY)

add_executable(${PROJECT_NAME}
        src/main.cpp
        src/json-formater.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt cpp-httplib)

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES configuration/cfg.json
        DESTINATION /etc/json-formater/
)

install(FILES systemd/json-formater.service
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system
)

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "khromenokroman")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "khromenokroman")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Json formater server")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")

set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "khromenokroman")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

include(CPack)
