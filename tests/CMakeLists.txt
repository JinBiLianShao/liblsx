# ===================== LIBLSX 测试套件构建 =====================
# 用法：cmake -DENABLE_TESTS=ON .. && make -j && make run_tests

file(GLOB LSX_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(lsx_tests ${LSX_TEST_SOURCES})
target_include_directories(lsx_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${INCLUDE_DIRS})
target_compile_options(lsx_tests PRIVATE -Wall -Wextra)
target_link_libraries(lsx_tests PRIVATE
        ${PROJECT_NAME}_static
        Threads::Threads)

add_custom_target(run_tests
        COMMAND $<TARGET_FILE:lsx_tests>
        DEPENDS lsx_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "运行 LIBLSX 深度测试套件")

add_test(NAME lsx_deep_tests COMMAND lsx_tests)
set_tests_properties(lsx_deep_tests PROPERTIES TIMEOUT 120)
