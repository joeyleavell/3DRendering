cmake_minimum_required (VERSION 3.22)

include(FetchContent)

Set(FETCHCONTENT_QUIET FALSE)

FetchContent_Declare(
	NewEngine
	GIT_REPOSITORY https://github.com/joeyleavell/NewEngine.git
	GIT_TAG        33579e1c5b7132622a8978a59bf877243f4a3500
	GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(NewEngine)

# Add source to this project's executable.
add_executable (3DRendering "3DRendering.cpp")

target_link_libraries(3DRendering NewEngine-Runtime)
target_link_libraries(3DRendering assimp)

if(APPLE)
  set_target_properties(3DRendering PROPERTIES INSTALL_RPATH "@executable_path/Lib")
endif()

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET 3DRendering PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
install(TARGETS 3DRendering)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/Shaders/ DESTINATION ${CMAKE_INSTALL_PREFIX}/Shaders)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/Content/ DESTINATION ${CMAKE_INSTALL_PREFIX}/Content)
