find_package(Qt6 REQUIRED COMPONENTS Widgets Network Concurrent)

set(CMAKE_AUTOMOC ON)

add_executable(jobeval WIN32
  main.cpp
  MainWindow.cpp
  MainWindow.hpp
  RegionPage.cpp
  RegionPage.hpp
  EvaluatePage.cpp
  EvaluatePage.hpp
  PackService.cpp
  PackService.hpp
  NetworkFetcher.cpp
  NetworkFetcher.hpp
  JobModel.cpp
  JobModel.hpp
  ComparePage.cpp
  ComparePage.hpp
  JobPickDialog.cpp
  JobPickDialog.hpp
  Format.hpp
  Config.hpp)
target_link_libraries(jobeval PRIVATE evalcore Qt6::Widgets Qt6::Network Qt6::Concurrent)
target_compile_definitions(jobeval PRIVATE QT_NO_KEYWORDS JOBEVAL_VERSION="${PROJECT_VERSION}")

# Tests for the app's (GUI-free) project model; needs Qt Test.
find_package(Qt6 QUIET COMPONENTS Test)
if(JOBEVAL_BUILD_TESTS AND Qt6Test_FOUND)
  add_executable(jobmodel_test tests/jobmodel_test.cpp JobModel.cpp JobModel.hpp)
  target_link_libraries(jobmodel_test PRIVATE evalcore Qt6::Core Qt6::Test)
  target_compile_definitions(jobmodel_test PRIVATE QT_NO_KEYWORDS JOBEVAL_VERSION="${PROJECT_VERSION}")
  add_test(NAME jobmodel_test COMMAND jobmodel_test)
endif()
