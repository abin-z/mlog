add_executable(log_manager_test log_manager_test.cc)
target_link_libraries(log_manager_test PRIVATE logger)

add_test(NAME log_manager_test COMMAND log_manager_test)
