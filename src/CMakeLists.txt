set(LIB_SRC
    aws_device_shadow.cc
)

find_package(aws-crt-cpp REQUIRED)
find_package(IotShadow-cpp REQUIRED)

add_library(${APP_LIB_NAME} ${LIB_SRC})

target_include_directories(${APP_LIB_NAME} PUBLIC ../include)

target_link_libraries(
    ${APP_LIB_NAME}
    PRIVATE
    AWS::aws-crt-cpp
    AWS::IotShadow-cpp
)

set(SRC
    main.cc
)

add_executable(${APP_NAME} ${SRC})
target_link_libraries(${APP_NAME} ${APP_LIB_NAME})

add_custom_command(TARGET ${APP_NAME}
    COMMAND cpplint --recursive src/* include/*
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    POST_BUILD
)
