cmake_minimum_required(VERSION 3.10 )

# Set some basic project attributes
project (homework LANGUAGES C CXX ASM-ATT)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -Wpedantic -ggdb -O0")
#set(CMAKE_VERBOSE_MAKEFILE ON)

set(BOOST_INCLUDE_PATH    /home/edaniley/Toolbox/boost_1_81_0 )
set(BOOST_LIB_PATH        /home/edaniley/Toolbox/boost_1_81_0/stage/lib )
set(NUMPY_INCLUDE_PATH    /usr/local/lib/python3.6/site-packages/numpy/core/include )
set(PYBIND_INCLUDE_PATH   /usr/include/pybind11 )
set(PYTHON_INCLUDE_PATH   /usr/include/python3.10 )


# This project will output an executable file
add_executable(${PROJECT_NAME} homework-main.cpp homework-rbc.cpp)


# Include the configuration header in the build
target_include_directories(${PROJECT_NAME} PUBLIC "${PROJECT_BINARY_DIR}")
target_include_directories(${PROJECT_NAME} PRIVATE
  "${CMAKE_HOME_DIRECTORY}/include"
  ${BOOST_INCLUDE_PATH}
  )


#get_cmake_property(_variableNames VARIABLES)
#list (SORT _variableNames)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
