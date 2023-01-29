cmake_minimum_required(VERSION 3.10)
project(Homework)

set(CMAKE_CXX_STANDARD 11)

set(Boost_USE_STATIC_LIBS ON)

find_package(Boost REQUIRED COMPONENTS
        #program_options
        system
        #iostreams
        filesystem
        thread
        chrono
        date_time
        timer
        regex
        serialization
        context
        coroutine)

find_package(GTest REQUIRED)

list(APPEND LIBS
        ${Boost_COROUTINE_LIBRARY}
        ${Boost_CHRONO_LIBRARY}
        ${Boost_THREAD_LIBRARY}
        ${Boost_CONTEXT_LIBRARY}
        ${Boost_REGEX_LIBRARY}
        ${Boost_DATE_TIME_LIBRARY}
        ${Boost_TIMER_LIBRARY}
        #${Boost_IOSTREAMS_LIBRARY}
        ${Boost_SYSTEM_LIBRARY}
        gtest
        pthread)

add_executable(pubsubgateway network.cpp pubsub.cpp gateway.cpp)
target_link_libraries(pubsubgateway ${LIBS})

add_executable(pubsubserver network.cpp pubsub.cpp server.cpp)
target_link_libraries(pubsubserver ${LIBS})

#add_executable(test test.cpp)
#target_link_libraries(test ${LIBS} gtest)
