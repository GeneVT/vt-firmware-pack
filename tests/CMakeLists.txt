find_package(GTest REQUIRED)

set(SRC 
    aws_device_shadow_test.cc
)

add_executable(fitness_test ${SRC})
target_link_libraries(fitness_test ${GTEST_BOTH_LIBRARIES} pthread ${APP_LIB_NAME})

# Register the test, then ctest and make test will run it.
add_test(NAME testfitness COMMAND fitness_test)
