cmake_minimum_required(VERSION 3.16)

project(suseong LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    main.cpp
    ../suseongtrace.h ../suseongtrace.cpp
    ../runner.h ../runner.cpp
    ../wireless.h ../wireless.cpp
    ../radiotap.h ../radiotap.cpp
    ../mac.h ../mac.cpp
)

add_executable(suseong ${SOURCES})

target_include_directories(suseong PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../libpcap")

# library "libc++_shared.so" not found: needed by main executable 해결을 위한
# 정적 라이브러리 링크
target_link_options(suseong PRIVATE "-static-libstdc++")

if(APPLE)
    target_link_libraries(suseong PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../libpcap/build/Qt_6_10_2_for_macOS-Debug/libpcap.a")
elseif(ANDROID)
    target_link_libraries(suseong PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../libpcap/build/Qt_6_10_2_for_Android_arm64_v8a-Debug/libpcap.a")
endif()

# POST_BUILD: 명령이 대상이 빌드된 후에 실행된다는 것을 지정합니다.
if(APPLE)
    add_custom_command(TARGET suseong POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:suseong> ${CMAKE_CURRENT_BINARY_DIR}/../SuNiffing.app/Contents/MacOS/suseong
    )
elseif(ANDROID)
    add_custom_command(TARGET suseong POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/../android/assets
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:suseong> ${CMAKE_CURRENT_SOURCE_DIR}/../android/assets/suseong
    )
endif()
