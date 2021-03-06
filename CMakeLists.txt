cmake_minimum_required(VERSION 2.8.3)
project(stereo_dso_ros)

set(CMAKE_BUILD_TYPE Release)

find_package(cmake_modules REQUIRED)

find_package(catkin REQUIRED COMPONENTS
  cv_bridge
  dynamic_reconfigure
  image_transport
  rosbag
  roscpp
  sensor_msgs
  message_filters
  geometry_msgs
)
set(STEREO_DSO_PATH /home/tech/project-L/stereo-dso)
#set(DSO_PATH $ENV{DSO_PATH})
message("\n\n---- LOOKING FOR STEREO-DSO at\n\"${STEREO_DSO_PATH}\"")


find_package(Pangolin 0.2 REQUIRED)
message("\n\n---- FOUND Pangolin Headers at\n\"${Pangolin_INCLUDE_DIRS}\"")
message("---- FOUND Pangolin Libs at\n\"${Pangolin_LIBRARIES}\"")

find_package(OpenCV REQUIRED)
message("\n\n---- FOUND OpenCV Headers at\n\"${OpenCV_INCLUDE_DIRS}\"")
message("---- FOUND OpenCV Libs at\n\"${OpenCV_LIBS}\"")

find_package(Eigen3 REQUIRED)
find_package(Boost COMPONENTS system thread) 
find_library(STEREO_DSO_LIBRARY ${STEREO_DSO_PATH}/build/lib)

# SSE flags
set(CMAKE_CXX_FLAGS
   "${SSE_FLAGS} -O2 -g -std=c++0x -march=native -fno-omit-frame-pointer"
)

catkin_package(
  CATKIN_DEPENDS
    geometry_msgs
    roscpp
    rosbag
    sensor_msgs
    cv_bridge
    message_filters
    dynamic_reconfigure
    image_transport
)

include_directories(
  ${PROJECT_SOURCE_DIR}/src
  ${STEREO_DSO_PATH}/src
  ${STEREO_DSO_PATH}/thirdparty/Sophus
  ${OpenCV_INCLUDE_DIRS}
  ${Pangolin_INCLUDE_DIRS}
  ${EIGEN3_INCLUDE_DIR}
  ${CATKIN_INCLUDE_DIRS}
)  

add_executable(stereo_dso_live src/main.cpp )
target_link_libraries(stereo_dso_live ${STEREO_DSO_LIBRARY} ${Pangolin_LIBRARIES} ${OpenCV_LIBS} ${catkin_LIBRARIES})





