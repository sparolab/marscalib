cmake_minimum_required(VERSION 3.16)
project(marscalib)

set(CMAKE_CXX_STANDARD 17)
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()


find_package(OpenCV REQUIRED)
find_package(ament_cmake REQUIRED)
find_package(ament_cmake_python REQUIRED)
find_package(PCL 1.14 REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclpy REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(visualization_msgs REQUIRED)
# find_package(tf2 REQUIRED)
# find_package(tf2_geometry_msgs REQUIRED)
# find_package(tf2_sensor_msgs REQUIRED)
# find_package(tf2_eigen REQUIRED)
# find_package(tf2_ros REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(Ceres REQUIRED)
find_package(Boost REQUIRED COMPONENTS filesystem program_options)
find_package(OpenMP)
find_package(Eigen3 REQUIRED)
find_package(rosbag2_cpp REQUIRED)
find_package(pcl_conversions REQUIRED)

link_directories(${Eigen_INCLUDE_DIRS})

if (OPENMP_FOUND)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()


if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # uncomment the line when a copyright and license is not present in all source files
  #set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # uncomment the line when this package is not in a git repo
  #set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

add_executable(preprocess src/preprocess.cpp)
ament_target_dependencies(preprocess rclcpp rosbag2_cpp cv_bridge pcl_conversions OpenCV)

add_executable(lidar src/lidar_center.cpp)
ament_target_dependencies(lidar rclcpp PCL)


add_executable(camera src/camera_center.cpp)
ament_target_dependencies(camera rclcpp PCL OpenCV)


add_executable(rt src/Rt.cpp)
ament_target_dependencies(rt rclcpp PCL)





target_link_libraries(preprocess ${PCL_LIBRARIES} ${OpenCV_LIBS} ${Boost_LIBRARIES} ${rclcpp_LIBRARIES} ${cv_bridge_LIBRARIES})
target_link_libraries(lidar ${PCL_LIBRARIES} ${OpenCV_LIBS} ${Boost_LIBRARIES} ${rclcpp_LIBRARIES})
target_link_libraries(camera ${PCL_LIBRARIES} ${OpenCV_LIBS} ${Boost_LIBRARIES} ${rclcpp_LIBRARIES})
target_link_libraries(rt fmt ${PCL_LIBRARIES} ${OpenCV_LIBS} ${Boost_LIBRARIES} ${CERES_LIBRARIES} ${GTSAM_LIBRARIES}  ${rclcpp_LIBRARIES})


target_include_directories(rt PUBLIC
        include
        thirdparty/Sophus
        ${CERES_INCLUDE_DIRS}
        ${Boost_INCLUDE_DIRS}
        ${GTSAM_INCLUDE_DIRS}
        ${OpenCV_INCLUDE_DIRS}
        ${rclcpp_INCLUDE_DIRS}
)


install(
        TARGETS
        preprocess
        camera
        lidar
        rt
        DESTINATION
        lib/${PROJECT_NAME}
)

ament_python_install_package(${PROJECT_NAME})
install(PROGRAMS scripts/amg.py DESTINATION lib/${PROJECT_NAME})
install(PROGRAMS scripts/hough.py DESTINATION lib/${PROJECT_NAME})
ament_export_libraries(sphere_calibration)
ament_package()