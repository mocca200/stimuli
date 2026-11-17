# CMake generated Testfile for 
# Source directory: C:/Users/mocca200/Documents/Default Project/Tests
# Build directory: C:/Users/mocca200/Documents/Default Project/build/Tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
include("C:/Users/mocca200/Documents/Default Project/build/Tests/mvt_tests-b12d07c_include.cmake")
if(CTEST_CONFIGURATION_TYPE MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test("mvt_all" "C:/Users/mocca200/Documents/Default Project/build/Tests/Debug/mvt_tests.exe")
  set_tests_properties("mvt_all" PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/mocca200/Documents/Default Project/Tests/CMakeLists.txt;33;add_test;C:/Users/mocca200/Documents/Default Project/Tests/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test("mvt_all" "C:/Users/mocca200/Documents/Default Project/build/Tests/Release/mvt_tests.exe")
  set_tests_properties("mvt_all" PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/mocca200/Documents/Default Project/Tests/CMakeLists.txt;33;add_test;C:/Users/mocca200/Documents/Default Project/Tests/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test("mvt_all" "C:/Users/mocca200/Documents/Default Project/build/Tests/MinSizeRel/mvt_tests.exe")
  set_tests_properties("mvt_all" PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/mocca200/Documents/Default Project/Tests/CMakeLists.txt;33;add_test;C:/Users/mocca200/Documents/Default Project/Tests/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test("mvt_all" "C:/Users/mocca200/Documents/Default Project/build/Tests/RelWithDebInfo/mvt_tests.exe")
  set_tests_properties("mvt_all" PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/mocca200/Documents/Default Project/Tests/CMakeLists.txt;33;add_test;C:/Users/mocca200/Documents/Default Project/Tests/CMakeLists.txt;0;")
else()
  add_test("mvt_all" NOT_AVAILABLE)
endif()
