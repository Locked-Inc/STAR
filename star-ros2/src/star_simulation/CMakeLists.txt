cmake_minimum_required(VERSION 3.8)
project(star_simulation)

find_package(ament_cmake REQUIRED)

install(DIRECTORY launch/ DESTINATION share/${PROJECT_NAME}/launch)
install(DIRECTORY config/ DESTINATION share/${PROJECT_NAME}/config)
install(DIRECTORY urdf/ DESTINATION share/${PROJECT_NAME}/urdf)
install(DIRECTORY meshes/ DESTINATION share/${PROJECT_NAME}/meshes)
install(DIRECTORY worlds/ DESTINATION share/${PROJECT_NAME}/worlds)
install(DIRECTORY rviz/ DESTINATION share/${PROJECT_NAME}/rviz)
install(DIRECTORY test/ DESTINATION share/${PROJECT_NAME}/test)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()

  # Simulation launch tests require Gazebo -- skip in CI (no GPU / no display).
  if(NOT DEFINED ENV{CI})
    find_package(launch_testing_ament_cmake REQUIRED)
    add_launch_test(test/test_teleop_timeout.py TIMEOUT 120)
    add_launch_test(test/test_stall_detection.py TIMEOUT 180)
    add_launch_test(test/test_obstacle_estop_latency.py TIMEOUT 120)
    add_launch_test(test/test_heartbeat_loss.py TIMEOUT 120)
    add_launch_test(test/test_slam_stability.py TIMEOUT 180)
    add_launch_test(test/test_nav2_recovery.py TIMEOUT 300)
    add_launch_test(test/test_ekf_drift.py TIMEOUT 180)
  endif()
endif()

ament_package()
