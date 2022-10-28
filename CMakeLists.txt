cmake_minimum_required(VERSION 3.10)



# set the project name
project(sleepmonitor)

set(CMAKE_CXX_STANDARD 17)

#if (H_DEBUG=="1")
set ( CMAKE_CXX_FLAGS ${flag})
#endif()



# add the executable
add_executable(holozer sleepmonitor.cpp
)

set(include
C:\Users\99gab\Desktop\SZTAKI\include
C:\opencv\build\include
)

target_include_directories(holozer PUBLIC ${include} )
target_link_libraries(holozer PUBLIC C:\opencv\build\x64\vc15\lib\opencv_world460.lib
C:\Users\99gab\Desktop\SZTAKI\lib64\vs2015\Spinnaker_v140.lib
C:\Users\99gab\Desktop\SZTAKI\lib64\vs2015\SpinVideo_v140.lib
)






