cmake_minimum_required(VERSION 3.14)
project(Orderbook)

set(CMAKE_CXX_STANDARD 20)

include_directories(include)

add_executable(main
    main.cpp
    src/OrderManager.cpp
    # src/MatchingEngine.cpp
)

enable_testing()
# add_subdirectory(tests)
