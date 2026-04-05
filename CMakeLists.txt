cmake_minimum_required(VERSION 3.16)

project(SuNiffing VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 데몬 먼저 빌드
add_subdirectory(suseong)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(SuNiffing
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        device.h device.cpp
        suseongtrace.h suseongtrace.cpp
        runner.h runner.cpp
        radiotap.h radiotap.cpp
        wireless.h wireless.cpp
        mac.h
    )

# 안드로이드 빌드에 경우 패키지 소스 지정
# Define target properties for Android with Qt 6 as:
#if(ANDROID)
    set_target_properties(SuNiffing PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
#endif()

else()
    if(ANDROID)
        add_library(SuNiffing SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(SuNiffing
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(SuNiffing PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

# APPLE -> 테스트, ANDROID -> 빌드
if(APPLE)
    target_link_libraries(SuNiffing PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../libpcap/build/Qt_6_10_2_for_macOS-Debug/libpcap.a")
elseif(ANDROID)
    target_link_libraries(SuNiffing PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../libpcap/build/Qt_6_10_2_for_Android_arm64_v8a-Debug/libpcap.a")
endif()

# pcap 헤더 파일 포함 | pcap 소스 파일에서 VERSION 파일 이름 변경 필요
target_include_directories(SuNiffing PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../libpcap")


# llvm-strip
if(NOT CMAKE_BUILD_TYPE MATCHES Debug)
    add_custom_command(TARGET ssdaemon POST_BUILD
        COMMAND llvm-strip --strip-all $<TARGET_FILE:ssdaemon>
    )
endif()



# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.SuNiffing)
endif()
set_target_properties(SuNiffing PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS SuNiffing
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(SuNiffing)
endif()
