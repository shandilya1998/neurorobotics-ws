
#!/bin/bash

export CMAKE_PREFIX_PATH=/Pangolin/build:$CMAKE_PREFIX_PATH
export CMAKE_PREFIX_PATH=/opencv:$CMAKE_PREFIX_PATH

echo "===============Building ROS2 Package=============="
cd /ws/ros_ws
echo "***********************************************"
echo $PWD
echo "***********************************************"
# rm -rf build install
colcon build --symlink-install --cmake-args '-DCMAKE_EXPORT_COMPILE_COMMANDS=1' '-DCMAKE_BUILD_TYPE=RelWithDebInfo' '-Wno-dev' --mixin debug
