#!/usr/bin/env bash
#
# slam-mvp.sh -- bring up the minimal SLAM MVP stack one node at a time.
#
# Each node is launched as a separate backgrounded process with its own
# log file so we can tell at a glance which one is unhappy. Ctrl+C (or
# the `stop` subcommand) cleanly tears everything down.
#
# Usage:
#   scripts/slam-mvp.sh start [--lidar /dev/ttyUSBx] [--mcu /dev/ttyACM0]
#   scripts/slam-mvp.sh status
#   scripts/slam-mvp.sh stop
#   scripts/slam-mvp.sh logs <node>

set +u  # ROS2 setup.bash references unbound vars

STAR_ROOT=/workspaces/STAR
ROS_WS=${STAR_ROOT}/star-ros2
LOGDIR=/tmp/slam-mvp-logs
PIDFILE=/tmp/slam-mvp.pids

MCU=/dev/star-mcu
LIDAR=/dev/star-lidar
COMMAND=${1:-start}
shift || true

while [[ $# -gt 0 ]]; do
    case $1 in
        --lidar) LIDAR=$2; shift 2 ;;
        --mcu)   MCU=$2; shift 2 ;;
        *) shift ;;
    esac
done

# ---------------------------------------------------------------------------

source_ros() {
    source /opt/ros/jazzy/setup.bash
    [[ -f ${ROS_WS}/install/local_setup.bash ]] && source ${ROS_WS}/install/local_setup.bash
    export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
}

auto_detect_lidar() {
    # /dev/star-lidar is a persistent udev symlink installed by
    # /etc/udev/rules.d/71-star-symlinks.rules.
    if [[ -e $LIDAR ]]; then return; fi
    echo "ERROR: ${LIDAR} symlink not present. Install the udev rules" >&2
    echo "       at /etc/udev/rules.d/71-star-symlinks.rules and replug USB." >&2
    return 1
}

stop_nodes() {
    if [[ -f $PIDFILE ]]; then
        while IFS=: read -r name pid; do
            if kill -0 $pid 2>/dev/null; then
                kill -TERM $pid 2>/dev/null || true
            fi
        done < $PIDFILE
        sleep 2
        while IFS=: read -r name pid; do
            if kill -0 $pid 2>/dev/null; then
                kill -KILL $pid 2>/dev/null || true
            fi
        done < $PIDFILE
        rm -f $PIDFILE
    fi
    # Sweep any stray children the pidfile missed.
    pkill -9 -f "star_simple_bridge\|sllidar_node\|async_slam_toolbox_node\|foxglove_bridge\|static_odom_tf" 2>/dev/null || true
}

start_node() {
    local name=$1; shift
    local log=${LOGDIR}/${name}.log
    echo ">> starting ${name} (log: ${log})"
    "$@" > ${log} 2>&1 &
    local pid=$!
    echo "${name}:${pid}" >> $PIDFILE
    sleep 1
    if ! kill -0 $pid 2>/dev/null; then
        echo "   FAILED: ${name} died immediately. Last log:"
        tail -8 ${log} | sed 's/^/   /'
        return 1
    fi
    echo "   pid=${pid}"
}

# ---------------------------------------------------------------------------

case $COMMAND in
status)
    source_ros
    if [[ -f $PIDFILE ]]; then
        echo "Known PIDs:"
        while IFS=: read -r name pid; do
            if kill -0 $pid 2>/dev/null; then
                echo "  [UP]   ${name} pid=${pid}"
            else
                echo "  [DEAD] ${name} pid=${pid}"
            fi
        done < $PIDFILE
    else
        echo "No pidfile at ${PIDFILE}"
    fi
    echo
    echo "Key topic rates (4s each):"
    for topic in /scan /odom/unfiltered /tf /map; do
        printf "  %-20s " $topic
        timeout 4 ros2 topic hz $topic 2>&1 | grep "average rate" | head -1 \
            || echo "no publisher"
    done
    ;;

stop)
    stop_nodes
    echo "stopped."
    ;;

logs)
    name=${1:-}
    if [[ -z $name ]]; then
        ls -1 ${LOGDIR} 2>/dev/null
    else
        tail -50 ${LOGDIR}/${name}.log 2>/dev/null || echo "no log for ${name}"
    fi
    ;;

start)
    stop_nodes
    mkdir -p $LOGDIR
    rm -f $PIDFILE
    source_ros

    if ! auto_detect_lidar; then exit 1; fi
    echo "lidar=${LIDAR}  mcu=${MCU}"

    if [[ ! -e $MCU ]]; then
        echo "!! ${MCU} not present. Attempting RX72N reflash to wake Cypress."
        bash ${STAR_ROOT}/scripts/flash-rx72n.sh \
            ${STAR_ROOT}/star-rx72n-firmware/build/star-rx72n-firmware.elf \
            2>&1 | tail -3
        sleep 3
        if [[ ! -e $MCU ]]; then
            echo "ERROR: reflash did not recover ${MCU}. Replug USB manually."
            exit 1
        fi
        echo ">> ${MCU} recovered."
    fi

    # Ensure permissions.
    sudo chmod a+rw $MCU $LIDAR 2>/dev/null || true

    # -----------------------------------------------------------------
    # Launch each node. Order matters only for debugging: bridge first
    # (it depends on /dev/ttyACM0 being there), then rest.
    # -----------------------------------------------------------------

    start_node simple_bridge \
        ros2 run star_simple_bridge simple_bridge_node

    start_node rplidar \
        ros2 run sllidar_ros2 sllidar_node --ros-args \
            -p serial_port:=${LIDAR} \
            -p serial_baudrate:=460800 \
            -p frame_id:=laser_frame \
            -p inverted:=false \
            -p angle_compensate:=true \
            -p scan_mode:=Standard

    start_node static_odom_tf \
        ros2 run tf2_ros static_transform_publisher \
            0 0 0 0 0 0 odom base_link

    start_node static_laser_tf \
        ros2 run tf2_ros static_transform_publisher \
            0 0 0.1 0 0 0 base_link laser_frame

    start_node slam_toolbox \
        ros2 run slam_toolbox async_slam_toolbox_node --ros-args \
            -p odom_frame:=odom -p base_frame:=base_link -p map_frame:=map \
            -p scan_topic:=/scan -p mode:=mapping \
            -p use_scan_matching:=true -p use_scan_barycenter:=true \
            -p resolution:=0.05 -p max_laser_range:=12.0 \
            -p minimum_time_interval:=0.5

    start_node foxglove \
        ros2 run foxglove_bridge foxglove_bridge --ros-args \
            -p port:=8765 -p address:=0.0.0.0

    echo
    echo "All launched. Use:"
    echo "  scripts/slam-mvp.sh status     # topic rates"
    echo "  scripts/slam-mvp.sh logs <name>"
    echo "  scripts/slam-mvp.sh stop"
    echo
    echo "Foxglove: ws://<pi5>:8765"
    ;;

*)
    echo "usage: $0 {start|stop|status|logs} [--lidar /dev/ttyUSBx] [--mcu /dev/ttyACM0]"
    exit 2
    ;;
esac
