if(EXISTS "/home/rotem/os/os-hw4/build/tests/malloc_2_test_tests-b12d07c.cmake")
  include("/home/rotem/os/os-hw4/build/tests/malloc_2_test_tests-b12d07c.cmake")
else()
  add_test(malloc_2_test_NOT_BUILT-b12d07c malloc_2_test_NOT_BUILT-b12d07c)
endif()
