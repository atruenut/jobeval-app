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
  Format.hpp
  Config.hpp)
target_link_libraries(jobeval PRIVATE evalcore Qt6::Widgets Qt6::Network Qt6::Concurrent)
target_compile_definitions(jobeval PRIVATE QT_NO_KEYWORDS JOBEVAL_VERSION="${PROJECT_VERSION}")
