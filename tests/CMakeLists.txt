add_executable(test_order_book tests/test_order_book.cpp)

target_link_libraries(test_order_book PRIVATE Threads::Threads)
