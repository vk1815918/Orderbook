cmake_minimum_required(VERSION 3.14)
project(Orderbook)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

add_executable(main
    main.cpp
    src/MatchingEngine.cpp
    src/OrderManager.cpp
)

enable_testing()
# add_subdirectory(tests)
