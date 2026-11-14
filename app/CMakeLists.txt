find_package(Qt6 REQUIRED COMPONENTS Widgets Network Concurrent)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
if(WIN32)
  enable_language(RC)
endif()

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
  ChartWidget.cpp
  ChartWidget.hpp
  ComparePage.cpp
  ComparePage.hpp
  JobResult.cpp
  JobResult.hpp
  JobPickDialog.cpp
  JobPickDialog.hpp
  FxRates.cpp
  FxRates.hpp
  Format.hpp
  Version.hpp
  resources.qrc
  Config.hpp)
if(WIN32)
  target_sources(jobeval PRIVATE app.rc)
endif()
target_link_libraries(jobeval PRIVATE evalcore Qt6::Widgets Qt6::Network Qt6::Concurrent)
target_compile_definitions(jobeval PRIVATE QT_NO_KEYWORDS JOBEVAL_VERSION="${PROJECT_VERSION}")

# Tests for the app's (GUI-free) project model; needs Qt Test.
find_package(Qt6 QUIET COMPONENTS Test Gui)
if(JOBEVAL_BUILD_TESTS AND Qt6Test_FOUND)
  add_executable(jobmodel_test tests/jobmodel_test.cpp Version.hpp FxRates.cpp FxRates.hpp JobModel.cpp JobModel.hpp JobResult.cpp JobResult.hpp)
  target_link_libraries(jobmodel_test PRIVATE evalcore Qt6::Core Qt6::Gui Qt6::Test)
  target_compile_definitions(jobmodel_test PRIVATE QT_NO_KEYWORDS JOBEVAL_VERSION="${PROJECT_VERSION}")
  add_test(NAME jobmodel_test COMMAND jobmodel_test)
endif()
