add_executable(test_tokenizer test_tokenizer.cpp)
target_link_libraries(test_tokenizer PRIVATE eds_core)
add_test(NAME tokenizer COMMAND test_tokenizer)
