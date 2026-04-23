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
    # Pin Cyclone to loopback so ros2 CLI from any shell (which sources this
    # via ~/.bashrc) and the stack agree on a discovery interface. Without
    # this, the stack auto-discovers on wlan0/eth0 while CLI shells with
    # CYCLONEDDS_URI set use lo, and `ros2 lifecycle/topic/node` calls hang.
    export CYCLONEDDS_URI=file:///workspaces/STAR/config/cyclonedds.xml
}

auto_detect_lidar() {
    # /dev/star-lidar is a persistent udev symlink installed by
    # /etc/udev/rules.d/71-star-symlinks.rules.
    if [[ -e $LIDAR ]]; then return; fi
    echo "ERROR: ${LIDAR} symlink not present. Install the udev rules" >&2
    echo "       at /etc/udev/rules.d/71-star-symlinks.rules and replug USB." >&2
    return 1
}

ensure_mcu() {
    # At boot the Cypress USB-UART (RX72N's /dev/ttyACM0) is sometimes
    # missing because the E2 Lite holds the RX72N in reset during power-up
    # ordering, starving the Cypress 3.3V rail (see BOARD_CONFIG.md).
    # Cycling the E2 Lite USB device releases reset and the Cypress
    # enumerates within ~1 s. Cheaper than reflashing the whole firmware.
    if [[ -e $MCU ]]; then return 0; fi

    local e2l_path
    e2l_path=$(for d in /sys/bus/usb/devices/*/; do
        [[ -r "$d/idVendor" && -r "$d/idProduct" ]] || continue
        [[ "$(cat "$d/idVendor")" == "045b" && "$(cat "$d/idProduct")" == "82a0" ]] || continue
        basename "$d"; break
    done)

    if [[ -n "$e2l_path" ]]; then
        echo "!! ${MCU} missing. Cycling E2 Lite (${e2l_path}) to release RX72N reset."
        echo "$e2l_path" | sudo tee /sys/bus/usb/drivers/usb/unbind >/dev/null 2>&1 || true
        sleep 1
        echo "$e2l_path" | sudo tee /sys/bus/usb/drivers/usb/bind   >/dev/null 2>&1 || true
        for _ in 1 2 3 4 5; do sleep 1; [[ -e $MCU ]] && break; done
    fi

    if [[ -e $MCU ]]; then
        echo ">> ${MCU} recovered via USB rebind."
        return 0
    fi

    echo "!! ${MCU} still missing. Attempting RX72N reflash to wake Cypress."
    bash ${STAR_ROOT}/scripts/flash-rx72n.sh \
        ${STAR_ROOT}/star-rx72n-firmware/build/star-rx72n-firmware.elf \
        2>&1 | tail -3
    sleep 3
    if [[ -e $MCU ]]; then
        echo ">> ${MCU} recovered via reflash."
        return 0
    fi
    echo "ERROR: could not recover ${MCU}. Replug USB manually." >&2
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
    # `\|` in BRE is treated literally by procps' pkill -- alternations need
    # bare `|` (procps interprets the regex as ERE for this case). The old
    # form with `\|` silently matched nothing, letting respawn=True orphans
    # (simple_bridge, supervisor, robot_state_publisher) survive every
    # `slam-mvp.sh stop` and accumulate across iterations until the host
    # ran out of /dev/ttyUSB0 and DDS-participant slots.
    pkill -9 -f \
      "star_simple_bridge|star_supervisor|sllidar_node|async_slam_toolbox_node|foxglove_bridge|static_odom_tf|robot_state_publisher|ekf_node|ekf_filter_node|nav2_|opennav_docking|lifecycle_manager|ros2 launch star_bringup" \
      2>/dev/null || true
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

    if ! ensure_mcu; then exit 1; fi

    # Ensure permissions.
    sudo chmod a+rw $MCU $LIDAR 2>/dev/null || true

    # -----------------------------------------------------------------
    # Launch each node. Order matters only for debugging: bridge first
    # (it depends on /dev/ttyACM0 being there), then rest.
    # -----------------------------------------------------------------

    # publish_odom_tf:=false hands ownership of the odom->base_link TF
    # to ekf_filter_node (launched below). Dual publishers on the same
    # TF edge race and produce jitter.
    start_node simple_bridge \
        ros2 run star_simple_bridge simple_bridge_node --ros-args \
            -p publish_odom_tf:=false

    # star_supervisor arbitrates teleop vs Nav2 vs e-stop. Must come
    # up before the bridge consumes /cmd_vel_out, but the bridge
    # tolerates the topic being unpublished so this ordering is soft.
    start_node star_supervisor \
        ros2 run star_supervisor supervisor_node

    start_node rplidar \
        ros2 run sllidar_ros2 sllidar_node --ros-args \
            -p serial_port:=${LIDAR} \
            -p serial_baudrate:=460800 \
            -p frame_id:=laser_frame \
            -p inverted:=false \
            -p angle_compensate:=true \
            -p scan_mode:=Standard

    # robot_state_publisher reads the URDF (base_link + laser_frame + imu_link
    # static joints) and also advertises /robot_description so Foxglove's URDF
    # panel can render the robot. Multi-line xacro output doesn't survive
    # `-p robot_description:=...` CLI parsing, so bring it up through the
    # existing static_transforms.launch.py which handles the substitution.
    start_node robot_state_publisher \
        ros2 launch star_bringup static_transforms.launch.py

    # robot_localization EKF. Fuses wheel vx from simple_bridge with BNO055
    # yaw+gyro_z from imu_task. Owns odom->base_link TF (publish_tf: true
    # in ekf.yaml). The -r __node remap is required because the YAML uses
    # ekf_filter_node as its top-level key; without it rlkf_node loads no
    # parameters and sits at default identity-output.
    start_node ekf \
        ros2 run robot_localization ekf_node --ros-args \
            --params-file ${STAR_ROOT}/star-ros2/src/star_bringup/config/ekf.yaml \
            -r __node:=ekf_filter_node

    # minimum_travel_{distance,heading}=0 forces slam_toolbox to integrate
    # every scan, not just after 0.5m / 0.5rad of motion. Essential for
    # bench testing / stationary-robot debugging where the odom never
    # changes but you still want to see the map populate.
    # autostart:=true makes the lifecycle node self-transition through
    # configure->activate at startup so we don't depend on external
    # `ros2 lifecycle set` calls that hang on any DDS discovery hiccup.
    start_node slam_toolbox \
        ros2 run slam_toolbox async_slam_toolbox_node --ros-args \
            -p autostart:=true \
            -p odom_frame:=odom -p base_frame:=base_link -p map_frame:=map \
            -p scan_topic:=/scan -p mode:=mapping \
            -p use_scan_matching:=true -p use_scan_barycenter:=true \
            -p resolution:=0.05 -p max_laser_range:=12.0 \
            -p minimum_time_interval:=0.5 \
            -p minimum_travel_distance:=0.0 \
            -p minimum_travel_heading:=0.0

    start_node foxglove \
        ros2 run foxglove_bridge foxglove_bridge --ros-args \
            -p port:=8765 -p address:=0.0.0.0

    # async_slam_toolbox_node's autostart event-handler can drop its initial
    # Configure event under boot-time DDS discovery load (the shared
    # launch_ros rclpy node gets starved while sllidar/foxglove/bridge all
    # spin up their participants at once). When that happens slam sits in
    # `unconfigured`, /map is never published, and https://star.local/map.png
    # returns 503 forever.
    #
    # Retry loop with service-call form (matches docs/ARCHITECTURE.md's
    # "two-line workaround"): poll state up to 10x 4s, firing configure (id=1)
    # or activate (id=3) as needed. Idempotent -- a no-op once active.
    echo ">> verifying slam_toolbox active (autostart backstop)"
    sleep 3
    for attempt in 1 2 3 4 5 6 7 8 9 10; do
        state=$(timeout 5 ros2 lifecycle get /slam_toolbox 2>/dev/null \
                | awk '{print $1}')
        echo "   attempt ${attempt}: state=${state:-unknown}"
        case "$state" in
            unconfigured)
                timeout 10 ros2 service call /slam_toolbox/change_state \
                    lifecycle_msgs/srv/ChangeState "{transition: {id: 1}}" \
                    2>&1 | grep -E "success" | sed 's/^/   /' || true
                ;;
            inactive)
                timeout 10 ros2 service call /slam_toolbox/change_state \
                    lifecycle_msgs/srv/ChangeState "{transition: {id: 3}}" \
                    2>&1 | grep -E "success" | sed 's/^/   /' || true
                ;;
            active)
                echo "   slam_toolbox active."
                break
                ;;
            *)
                sleep 4
                ;;
        esac
    done

    echo
    echo "All launched. Use:"
    echo "  scripts/slam-mvp.sh status     # topic rates"
    echo "  scripts/slam-mvp.sh logs <name>"
    echo "  scripts/slam-mvp.sh stop"
    echo
    echo "Foxglove: ws://<pi5>:8765"
    ;;

start-nav)
    # Same as 'start' but also launches the Nav2 navigation stack.
    # Uses the tuned nav2_params.yaml (odom_topic=/odom/unfiltered,
    # max_vel_x=0.25, controller_frequency=10Hz) via the Nav2 include
    # in slam_mvp.launch.py under use_nav2:=true.
    stop_nodes
    mkdir -p $LOGDIR
    rm -f $PIDFILE
    source_ros
    if ! auto_detect_lidar; then exit 1; fi
    echo "lidar=${LIDAR}  mcu=${MCU}"
    if ! ensure_mcu; then exit 1; fi
    sudo chmod a+rw $MCU $LIDAR 2>/dev/null || true

    start_node stack \
        ros2 launch star_bringup slam_mvp.launch.py \
            use_nav2:=true lidar_port:=${LIDAR}

    # Backstop slam_toolbox lifecycle activation. The launch file delays
    # nav2 by 12s (TimerAction in slam_mvp.launch.py) so slam_toolbox's
    # online_async_launch.py autostart event-handlers have a clear window
    # to fire without contention from nav2's 11 lifecycle nodes. This loop
    # is the safety net: if state is still `unconfigured` after the launch
    # window, drive it manually. Idempotent -- a no-op when autostart
    # already succeeded.
    echo ">> verifying slam_toolbox active (autostart backstop)"
    sleep 8
    for attempt in 1 2 3 4 5 6 7 8 9 10; do
        state=$(timeout 5 ros2 lifecycle get /slam_toolbox 2>/dev/null \
                | awk '{print $1}')
        echo "   attempt ${attempt}: state=${state:-unknown}"
        case "$state" in
            unconfigured)
                timeout 10 ros2 service call /slam_toolbox/change_state \
                    lifecycle_msgs/srv/ChangeState "{transition: {id: 1}}" \
                    2>&1 | grep -E "success" | sed 's/^/   /' || true
                ;;
            inactive)
                timeout 10 ros2 service call /slam_toolbox/change_state \
                    lifecycle_msgs/srv/ChangeState "{transition: {id: 3}}" \
                    2>&1 | grep -E "success" | sed 's/^/   /' || true
                ;;
            active)
                echo "   slam_toolbox active."
                break
                ;;
            *)
                sleep 4
                ;;
        esac
    done

    # Wait for nav2 lifecycle_manager. Nav2 starts at launch+12s and the
    # full 10-node configure+activate sequence takes another ~15s, so this
    # wait can run 30s+ from the start of this script.
    echo ">> waiting for lifecycle_manager_navigation"
    for attempt in 1 2 3 4 5 6 7 8 9 10 11 12; do
        resp=$(timeout 5 ros2 service call \
            /lifecycle_manager_navigation/is_active std_srvs/srv/Trigger \
            2>/dev/null | grep -oE 'success=(True|False)')
        echo "   attempt ${attempt}: ${resp:-no response}"
        if [[ "$resp" == "success=True" ]]; then break; fi
        sleep 4
    done

    echo
    echo "All launched (Nav2 active). Send a goal in Foxglove:"
    echo "  - Open ws://<pi5>:8765"
    echo "  - Use '2D Goal Pose' to publish /goal_pose"
    echo
    echo "Commands:"
    echo "  scripts/slam-mvp.sh status"
    echo "  scripts/slam-mvp.sh logs stack"
    echo "  scripts/slam-mvp.sh stop"
    ;;

*)
    echo "usage: $0 {start|start-nav|stop|status|logs} [--lidar /dev/ttyUSBx] [--mcu /dev/ttyACM0]"
    exit 2
    ;;
esac
