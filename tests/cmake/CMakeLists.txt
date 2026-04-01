cmake_minimum_required(VERSION 3.16)
project(pj_find_package_test C)

find_package(Pj REQUIRED)

# Exercise the full dependency chain (pjsua-lib -> pjsip -> pjmedia ->
# Pj::Dep::SRTP etc.) to verify that exported targets and dependency
# aliases resolve correctly after install.
add_executable(test_consumer test_consumer.c)
target_link_libraries(test_consumer PRIVATE Pj::pjsua-lib)
