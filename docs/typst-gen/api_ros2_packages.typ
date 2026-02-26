= ROS2 Packages API Reference

== message_converter.hpp <compound-message-converter-hpp>

*Includes:*

- `<cmath>`
- `<geometry_msgs/msg/twist.hpp>`
- `<rclcpp/rclcpp.hpp>`
- `<std_msgs/msg/string.hpp>`
- `"star/v1/motor_control.pb.h"`
- `"star/v1/telemetry.pb.h"`

== star_gateway_bridge_node.hpp <compound-star-gateway-bridge-node-hpp>

*Includes:*

- `<memory>`
- `<mutex>`
- `<optional>`
- `<string>`
- `<diagnostic_msgs/msg/diagnostic_array.hpp>`
- `<diagnostic_msgs/msg/diagnostic_status.hpp>`
- `<diagnostic_msgs/msg/key_value.hpp>`
- `<geometry_msgs/msg/twist.hpp>`
- `<grpcpp/grpcpp.h>`
- `<rclcpp/rclcpp.hpp>`
- `<std_msgs/msg/string.hpp>`
- `<std_srvs/srv/set_bool.hpp>`
- `"star/v1/gateway_service.grpc.pb.h"`
- `"star_gateway_bridge/message_converter.hpp"`

== main.cpp <compound-main-cpp>

*Includes:*

- `<memory>`
- `"rclcpp/rclcpp.hpp"`
- `"star_gateway_bridge/star_gateway_bridge_node.hpp"`

=== main <main-cpp--main>

#func-sig("int main(int argc, char **argv)")

Main entry point for star_gateway_bridge standalone executable.

Usage: ros2 run star_gateway_bridge star_gateway_bridge_main

Parameters can be set via command line: ros2 run star_gateway_bridge star_gateway_bridge_main \ ros-args -p gateway_address:=192.168.1.100:50051 \ -p telemetry_rate_hz:=20.0

_Defined in `src/star_gateway_bridge/src/main.cpp:23`_

---

== main.cpp <compound-main-cpp>

*Includes:*

- `"star_spi_bridge/star_spi_driver_node.hpp"`
- `<rclcpp/rclcpp.hpp>`

=== main <main-cpp--main>

#func-sig("int main(int argc, char **argv)")

_Defined in `src/star_spi_bridge/src/main.cpp:7`_

---

== message_converter.cpp <compound-message-converter-cpp>

*Includes:*

- `"star_gateway_bridge/message_converter.hpp"`
- `<chrono>`

== star_gateway_bridge_node.cpp <compound-star-gateway-bridge-node-cpp>

*Includes:*

- `"star_gateway_bridge/star_gateway_bridge_node.hpp"`
- `<chrono>`
- `<thread>`
- `"rclcpp_components/register_node_macro.hpp"`

== test_message_converter.cpp <compound-test-message-converter-cpp>

*Includes:*

- `"star_gateway_bridge/message_converter.hpp"`
- `<cmath>`
- `<limits>`
- `<geometry_msgs/msg/twist.hpp>`
- `<gtest/gtest.h>`
- `"star/v1/motor_control.pb.h"`

=== main <test-message-converter-cpp--main>

#func-sig("int main(int argc, char **argv)")

_Defined in `src/star_gateway_bridge/test/test_message_converter.cpp:508`_

---

== safety_monitor.hpp <compound-safety-monitor-hpp>

*Includes:*

- `<chrono>`
- `<map>`
- `<memory>`
- `<string>`
- `<diagnostic_msgs/msg/diagnostic_array.hpp>`
- `<geometry_msgs/msg/twist.hpp>`
- `<nav_msgs/msg/odometry.hpp>`
- `<rclcpp/rclcpp.hpp>`
- `<rclcpp_lifecycle/lifecycle_node.hpp>`
- `<std_msgs/msg/bool.hpp>`

== safety_monitor.cpp <compound-safety-monitor-cpp>

*Includes:*

- `"star_safety_monitor/safety_monitor.hpp"`
- `<cmath>`
- `<iomanip>`
- `<sstream>`

== safety_monitor_node.cpp <compound-safety-monitor-node-cpp>

*Includes:*

- `<memory>`
- `<rclcpp/rclcpp.hpp>`
- `"star_safety_monitor/safety_monitor.hpp"`

=== main <safety-monitor-node-cpp--main>

#func-sig("int main(int argc, char **argv)")

_Defined in `src/star_safety_monitor/src/safety_monitor_node.cpp:26`_

---

== test_safety_monitor.cpp <compound-test-safety-monitor-cpp>

*Includes:*

- `<gtest/gtest.h>`
- `<chrono>`
- `<thread>`
- `<rclcpp/rclcpp.hpp>`
- `"star_safety_monitor/safety_monitor.hpp"`
- `<nav_msgs/msg/odometry.hpp>`
- `<diagnostic_msgs/msg/diagnostic_array.hpp>`
- `<diagnostic_msgs/msg/diagnostic_status.hpp>`
- `<std_msgs/msg/bool.hpp>`

=== TEST_F <test-safety-monitor-cpp--TEST-F>

#func-sig("TEST_F(SafetyMonitorTest, NodeConstruction)")

_Defined in `src/star_safety_monitor/test/test_safety_monitor.cpp:57`_

---

=== TEST_F <test-safety-monitor-cpp--TEST-F>

#func-sig("TEST_F(SafetyMonitorTest, LifecycleConfiguration)")

_Defined in `src/star_safety_monitor/test/test_safety_monitor.cpp:64`_

---

=== TEST_F <test-safety-monitor-cpp--TEST-F>

#func-sig("TEST_F(SafetyMonitorTest, LifecycleActivation)")

_Defined in `src/star_safety_monitor/test/test_safety_monitor.cpp:74`_

---

=== TEST_F <test-safety-monitor-cpp--TEST-F>

#func-sig("TEST_F(SafetyMonitorTest, LifecycleDeactivation)")

_Defined in `src/star_safety_monitor/test/test_safety_monitor.cpp:88`_

---

=== TEST_F <test-safety-monitor-cpp--TEST-F>

#func-sig("TEST_F(SafetyMonitorTest, ParameterLoading)")

_Defined in `src/star_safety_monitor/test/test_safety_monitor.cpp:104`_

---

=== TEST_F <test-safety-monitor-cpp--TEST-F>

#func-sig("TEST_F(SafetyMonitorTest, DiagnosticsPublication)")

_Defined in `src/star_safety_monitor/test/test_safety_monitor.cpp:123`_

---

=== TEST_F <test-safety-monitor-cpp--TEST-F>

#func-sig("TEST_F(SafetyMonitorTest, OdometrySubscription)")

_Defined in `src/star_safety_monitor/test/test_safety_monitor.cpp:154`_

---

=== TEST_F <test-safety-monitor-cpp--TEST-F>

#func-sig("TEST_F(SafetyMonitorTest, EmergencyStopTrigger)")

_Defined in `src/star_safety_monitor/test/test_safety_monitor.cpp:192`_

---

=== TEST_F <test-safety-monitor-cpp--TEST-F>

#func-sig("TEST_F(SafetyMonitorTest, DiagnosticPublishing)")

_Defined in `src/star_safety_monitor/test/test_safety_monitor.cpp:198`_

---

=== main <test-safety-monitor-cpp--main>

#func-sig("int main(int argc, char **argv)")

_Defined in `src/star_safety_monitor/test/test_safety_monitor.cpp:204`_

---

== spi_driver.hpp <compound-spi-driver-hpp>

*Includes:*

- `<cstdint>`
- `<mutex>`
- `<string>`
- `<vector>`

== spi_message_converter.hpp <compound-spi-message-converter-hpp>

*Includes:*

- `<geometry_msgs/msg/twist.hpp>`
- `<nav_msgs/msg/odometry.hpp>`
- `<sensor_msgs/msg/joint_state.hpp>`
- `"star/v1/motor_control.pb.h"`
- `"star/v1/telemetry.pb.h"`

== star_spi_driver_node.hpp <compound-star-spi-driver-node-hpp>

*Includes:*

- `<memory>`
- `<string>`
- `<geometry_msgs/msg/twist.hpp>`
- `<nav_msgs/msg/odometry.hpp>`
- `<rclcpp/rclcpp.hpp>`
- `<rclcpp_lifecycle/lifecycle_node.hpp>`
- `<sensor_msgs/msg/imu.hpp>`
- `<sensor_msgs/msg/joint_state.hpp>`
- `"star_spi_bridge/spi_driver.hpp"`
- `"star_spi_bridge/spi_message_converter.hpp"`

== spi_driver.cpp <compound-spi-driver-cpp>

*Includes:*

- `"star_spi_bridge/spi_driver.hpp"`
- `<fcntl.h>`
- `<sys/ioctl.h>`
- `<unistd.h>`
- `<cstring>`
- `<stdexcept>`
- `<rclcpp/rclcpp.hpp>`

=== SPI_IOC_MESSAGE <spi-driver-cpp--SPI-IOC-MESSAGE>

```c
#define SPI_IOC_MESSAGE 0
```

---

=== SPI_MODE_0 <spi-driver-cpp--SPI-MODE-0>

```c
uint8_t SPI_MODE_0
```

---

=== SPI_IOC_WR_MODE <spi-driver-cpp--SPI-IOC-WR-MODE>

```c
int SPI_IOC_WR_MODE
```

---

=== SPI_IOC_WR_BITS_PER_WORD <spi-driver-cpp--SPI-IOC-WR-BITS-PER-WORD>

```c
int SPI_IOC_WR_BITS_PER_WORD
```

---

=== SPI_IOC_WR_MAX_SPEED_HZ <spi-driver-cpp--SPI-IOC-WR-MAX-SPEED-HZ>

```c
int SPI_IOC_WR_MAX_SPEED_HZ
```

---

== spi_message_converter.cpp <compound-spi-message-converter-cpp>

*Includes:*

- `"star_spi_bridge/spi_message_converter.hpp"`
- `<algorithm>`
- `<cmath>`
- `<limits>`
- `<tf2/LinearMath/Quaternion.h>`
- `<tf2_geometry_msgs/tf2_geometry_msgs.hpp>`

== star_spi_driver_node.cpp <compound-star-spi-driver-node-cpp>

*Includes:*

- `"star_spi_bridge/star_spi_driver_node.hpp"`
- `<chrono>`
- `<tf2/LinearMath/Quaternion.h>`

== test_spi_driver.cpp <compound-test-spi-driver-cpp>

*Includes:*

- `"star_spi_bridge/spi_driver.hpp"`
- `<gtest/gtest.h>`

=== TEST_F <test-spi-driver-cpp--TEST-F>

#func-sig("TEST_F(SpiDriverTest, CRC32Calculation)")

#figure(
  image("docs/typst-gen/graphs/test__spi__driver_8cpp_1af8af1136c87dec07a1a11d991d41e116_callgraph.svg", width: 90%),
  caption: [Call graph for TEST_F],
)
_Defined in `src/star_spi_bridge/test/test_spi_driver.cpp:19`_

---

=== TEST_F <test-spi-driver-cpp--TEST-F>

#func-sig("TEST_F(SpiDriverTest, FrameEncoding)")

#figure(
  image("docs/typst-gen/graphs/test__spi__driver_8cpp_1a76102d5690877304aa1ba9e4c3c8c630_callgraph.svg", width: 90%),
  caption: [Call graph for TEST_F],
)
_Defined in `src/star_spi_bridge/test/test_spi_driver.cpp:28`_

---

=== TEST_F <test-spi-driver-cpp--TEST-F>

#func-sig("TEST_F(SpiDriverTest, FrameDecoding)")

#figure(
  image("docs/typst-gen/graphs/test__spi__driver_8cpp_1a664d498d52b5199b4c6ee04afb75341d_callgraph.svg", width: 90%),
  caption: [Call graph for TEST_F],
)
_Defined in `src/star_spi_bridge/test/test_spi_driver.cpp:54`_

---

=== TEST_F <test-spi-driver-cpp--TEST-F>

#func-sig("TEST_F(SpiDriverTest, EncodeDecodeRoundTrip)")

#figure(
  image("docs/typst-gen/graphs/test__spi__driver_8cpp_1af104f74ab798647bc986c05d042446f1_callgraph.svg", width: 90%),
  caption: [Call graph for TEST_F],
)
_Defined in `src/star_spi_bridge/test/test_spi_driver.cpp:71`_

---

=== TEST_F <test-spi-driver-cpp--TEST-F>

#func-sig("TEST_F(SpiDriverTest, DecodeRejectsCRCCorruption)")

#figure(
  image("docs/typst-gen/graphs/test__spi__driver_8cpp_1a3f8408cc5e15809ea1ef9650e7ec79c5_callgraph.svg", width: 90%),
  caption: [Call graph for TEST_F],
)
_Defined in `src/star_spi_bridge/test/test_spi_driver.cpp:89`_

---

=== TEST_F <test-spi-driver-cpp--TEST-F>

#func-sig("TEST_F(SpiDriverTest, EncodeRejectsOversizedPayload)")

#figure(
  image("docs/typst-gen/graphs/test__spi__driver_8cpp_1a0e37b881e1d95011b5438c32955c6fec_callgraph.svg", width: 90%),
  caption: [Call graph for TEST_F],
)
_Defined in `src/star_spi_bridge/test/test_spi_driver.cpp:106`_

---

=== FrameType <test-spi-driver-cpp--FrameType>

Frame type identifiers for SPI protocol messages.

Each frame carries a one-byte type field at offset 6 in the header. Values match the RX72N firmware rx_frame_type_t enum so both sides agree on the semantics of every message.

*See also:* `SpiDriver::encode_frame Frame construction`, `SpiDriver::decode_frame Frame parsing`

_Since Version 1.0.0_

---

== test_spi_message_converter.cpp <compound-test-spi-message-converter-cpp>

*Includes:*

- `<cmath>`
- `<gtest/gtest.h>`
- `"star_spi_bridge/spi_message_converter.hpp"`

=== TEST_F <test-spi-message-converter-cpp--TEST-F>

#func-sig("TEST_F(SpiMessageConverterTest, TwistToVelocity_Forward)")

\_Defined in `src/star\_spi\_bridge/test/test\_spi\_message\_converter.cpp:18`\_

---

=== TEST_F <test-spi-message-converter-cpp--TEST-F>

#func-sig("TEST_F(SpiMessageConverterTest, TwistToVelocity_RotateLeft)")

\_Defined in `src/star\_spi\_bridge/test/test\_spi\_message\_converter.cpp:31`\_

---

=== TEST_F <test-spi-message-converter-cpp--TEST-F>

#func-sig("TEST_F(SpiMessageConverterTest, TwistToVelocity_NaN)")

\_Defined in `src/star\_spi\_bridge/test/test\_spi\_message\_converter.cpp:46`\_

---

=== TEST_F <test-spi-message-converter-cpp--TEST-F>

#func-sig("TEST_F(SpiMessageConverterTest, TelemetryToOdometry_FirstMessage)")

\_Defined in `src/star\_spi\_bridge/test/test\_spi\_message\_converter.cpp:59`\_

---

=== TEST_F <test-spi-message-converter-cpp--TEST-F>

#func-sig("TEST_F(SpiMessageConverterTest, TelemetryToOdometry_ForwardMovement)")

\_Defined in `src/star\_spi\_bridge/test/test\_spi\_message\_converter.cpp:75`\_

---

=== TEST_F <test-spi-message-converter-cpp--TEST-F>

#func-sig("TEST_F(SpiMessageConverterTest, TelemetryToOdometry_WithVelocity)")

#figure(
  image("docs/typst-gen/graphs/test__spi__message__converter_8cpp_1aa8b7550b4025ef2586044a6eef39653e_callgraph.svg", width: 90%),
  caption: [Call graph for TEST_F],
)
\_Defined in `src/star\_spi\_bridge/test/test\_spi\_message\_converter.cpp:104`\_

---

=== TEST_F <test-spi-message-converter-cpp--TEST-F>

#func-sig("TEST_F(SpiMessageConverterTest, TelemetryToJointState_Names)")

\_Defined in `src/star\_spi\_bridge/test/test\_spi\_message\_converter.cpp:140`\_

---

=== TEST_F <test-spi-message-converter-cpp--TEST-F>

#func-sig("TEST_F(SpiMessageConverterTest, TelemetryToJointState_Position)")

\_Defined in `src/star\_spi\_bridge/test/test\_spi\_message\_converter.cpp:158`\_

---

=== TEST_F <test-spi-message-converter-cpp--TEST-F>

#func-sig("TEST_F(SpiMessageConverterTest, TelemetryToJointState_Velocity)")

\_Defined in `src/star\_spi\_bridge/test/test\_spi\_message\_converter.cpp:178`\_

---

=== TEST_F <test-spi-message-converter-cpp--TEST-F>

#func-sig("TEST_F(SpiMessageConverterTest, RoundTrip_TwistToVelocityToJointState)")

\_Defined in `src/star\_spi\_bridge/test/test\_spi\_message\_converter.cpp:203`\_

---

= Index


== F

- #link(label("test-spi-driver-cpp--FrameType"))[`FrameType`] (enum, test_spi_driver.cpp)

== M

- #link(label("main-cpp--main"))[`main`] (function, main.cpp)
- #link(label("main-cpp--main"))[`main`] (function, main.cpp)
- #link(label("test-message-converter-cpp--main"))[`main`] (function, test_message_converter.cpp)
- #link(label("safety-monitor-node-cpp--main"))[`main`] (function, safety_monitor_node.cpp)
- #link(label("test-safety-monitor-cpp--main"))[`main`] (function, test_safety_monitor.cpp)

== S

- #link(label("spi-driver-cpp--SPI-IOC-MESSAGE"))[`SPI_IOC_MESSAGE`] (macro, spi_driver.cpp)
- #link(label("spi-driver-cpp--SPI-IOC-WR-BITS-PER-WORD"))[`SPI_IOC_WR_BITS_PER_WORD`] (variable, spi_driver.cpp)
- #link(label("spi-driver-cpp--SPI-IOC-WR-MAX-SPEED-HZ"))[`SPI_IOC_WR_MAX_SPEED_HZ`] (variable, spi_driver.cpp)
- #link(label("spi-driver-cpp--SPI-IOC-WR-MODE"))[`SPI_IOC_WR_MODE`] (variable, spi_driver.cpp)
- #link(label("spi-driver-cpp--SPI-MODE-0"))[`SPI_MODE_0`] (variable, spi_driver.cpp)

== T

- #link(label("test-safety-monitor-cpp--TEST-F"))[`TEST_F`] (function, test_safety_monitor.cpp)
- #link(label("test-safety-monitor-cpp--TEST-F"))[`TEST_F`] (function, test_safety_monitor.cpp)
- #link(label("test-safety-monitor-cpp--TEST-F"))[`TEST_F`] (function, test_safety_monitor.cpp)
- #link(label("test-safety-monitor-cpp--TEST-F"))[`TEST_F`] (function, test_safety_monitor.cpp)
- #link(label("test-safety-monitor-cpp--TEST-F"))[`TEST_F`] (function, test_safety_monitor.cpp)
- #link(label("test-safety-monitor-cpp--TEST-F"))[`TEST_F`] (function, test_safety_monitor.cpp)
- #link(label("test-safety-monitor-cpp--TEST-F"))[`TEST_F`] (function, test_safety_monitor.cpp)
- #link(label("test-safety-monitor-cpp--TEST-F"))[`TEST_F`] (function, test_safety_monitor.cpp)
- #link(label("test-safety-monitor-cpp--TEST-F"))[`TEST_F`] (function, test_safety_monitor.cpp)
- #link(label("test-spi-driver-cpp--TEST-F"))[`TEST_F`] (function, test_spi_driver.cpp)
- #link(label("test-spi-driver-cpp--TEST-F"))[`TEST_F`] (function, test_spi_driver.cpp)
- #link(label("test-spi-driver-cpp--TEST-F"))[`TEST_F`] (function, test_spi_driver.cpp)
- #link(label("test-spi-driver-cpp--TEST-F"))[`TEST_F`] (function, test_spi_driver.cpp)
- #link(label("test-spi-driver-cpp--TEST-F"))[`TEST_F`] (function, test_spi_driver.cpp)
- #link(label("test-spi-driver-cpp--TEST-F"))[`TEST_F`] (function, test_spi_driver.cpp)
- #link(label("test-spi-message-converter-cpp--TEST-F"))[`TEST_F`] (function, test_spi_message_converter.cpp)
- #link(label("test-spi-message-converter-cpp--TEST-F"))[`TEST_F`] (function, test_spi_message_converter.cpp)
- #link(label("test-spi-message-converter-cpp--TEST-F"))[`TEST_F`] (function, test_spi_message_converter.cpp)
- #link(label("test-spi-message-converter-cpp--TEST-F"))[`TEST_F`] (function, test_spi_message_converter.cpp)
- #link(label("test-spi-message-converter-cpp--TEST-F"))[`TEST_F`] (function, test_spi_message_converter.cpp)
- #link(label("test-spi-message-converter-cpp--TEST-F"))[`TEST_F`] (function, test_spi_message_converter.cpp)
- #link(label("test-spi-message-converter-cpp--TEST-F"))[`TEST_F`] (function, test_spi_message_converter.cpp)
- #link(label("test-spi-message-converter-cpp--TEST-F"))[`TEST_F`] (function, test_spi_message_converter.cpp)
- #link(label("test-spi-message-converter-cpp--TEST-F"))[`TEST_F`] (function, test_spi_message_converter.cpp)
- #link(label("test-spi-message-converter-cpp--TEST-F"))[`TEST_F`] (function, test_spi_message_converter.cpp)
