cmake_minimum_required(VERSION 3.16)
project(pj_find_package_test C CXX)

find_package(Pj REQUIRED)

# C consumer: exercise pjsua-lib (C API) and the full dependency chain
# (pjsua-lib -> pjsip -> pjmedia -> Pj::Dep::SRTP etc.)
add_executable(test_consumer test_consumer.c)
target_link_libraries(test_consumer PRIVATE Pj::pjsua-lib)

# C++ consumer: exercise pjsua2 (C++ API) and transitive dependencies
add_executable(test_consumer2 test_consumer2.cpp)
target_link_libraries(test_consumer2 PRIVATE Pj::pjsua2)
