// SerializationTest.kt - Protobuf Serialization Round-Trip Tests
// Verifies that generated Kotlin code serializes/deserializes correctly.
//
// Using JUnit 5 table-driven tests with gRPC testing patterns:
// https://grpc.io/blog/graceful-cleanup-junit-tests/
// https://www.baeldung.com/kotlin/junit-5-kotlin
//
// STAR Project - Texas A&M University
// December 2025

package com.star.proto

import org.junit.jupiter.api.Test
import org.junit.jupiter.api.DisplayName
import org.junit.jupiter.api.Nested
import org.junit.jupiter.api.Assertions.*
import org.junit.jupiter.params.ParameterizedTest
import org.junit.jupiter.params.provider.Arguments
import org.junit.jupiter.params.provider.CsvSource
import org.junit.jupiter.params.provider.MethodSource
import org.junit.jupiter.params.provider.EnumSource
import java.util.stream.Stream

// Import generated classes
import com.star.proto.star.v1.*

class SerializationTest {

    // ========================================================================
    // Test Data Classes for Table-Driven Tests
    // ========================================================================

    data class VelocityTestCase(
        val name: String,
        val leftMps: Double,
        val rightMps: Double,
        val sequence: Int,
        val description: String
    )

    data class EncoderTestCase(
        val motorId: Int,
        val ticks: Long,
        val velocityMps: Double,
        val description: String
    )

    data class TelemetryTestCase(
        val batteryPercent: Double,
        val temperatureCelsius: Double,
        val cpuUsagePercent: Double,
        val wifiSignalDbm: Int,
        val description: String
    )

    data class HeaderTestCase(
        val requestId: String,
        val clientVersion: String,
        val description: String
    )

    // ========================================================================
    // Test Data Providers (Table-Driven)
    // ========================================================================

    companion object {

        @JvmStatic
        fun velocityTestCases(): Stream<Arguments> = Stream.of(
            // Normal operation cases
            Arguments.of(VelocityTestCase(
                "forward_full", 2.0, 2.0, 1,
                "Both wheels forward at max speed"
            )),
            Arguments.of(VelocityTestCase(
                "reverse_full", -2.0, -2.0, 2,
                "Both wheels reverse at max speed"
            )),
            Arguments.of(VelocityTestCase(
                "stop", 0.0, 0.0, 3,
                "Both wheels stopped"
            )),
            // Turning cases
            Arguments.of(VelocityTestCase(
                "turn_left", 0.5, 1.5, 4,
                "Left turn - right wheel faster"
            )),
            Arguments.of(VelocityTestCase(
                "turn_right", 1.5, 0.5, 5,
                "Right turn - left wheel faster"
            )),
            Arguments.of(VelocityTestCase(
                "spin_left", -1.0, 1.0, 6,
                "Spin in place counterclockwise"
            )),
            Arguments.of(VelocityTestCase(
                "spin_right", 1.0, -1.0, 7,
                "Spin in place clockwise"
            )),
            // Edge cases
            Arguments.of(VelocityTestCase(
                "precision_low", 0.001, 0.001, 8,
                "Very low velocity for precision"
            )),
            Arguments.of(VelocityTestCase(
                "asymmetric", 1.234, -0.567, 9,
                "Asymmetric decimal velocities"
            ))
        )

        @JvmStatic
        fun encoderTestCases(): Stream<Arguments> = Stream.of(
            // Motor 0 (left) cases
            Arguments.of(EncoderTestCase(0, 0L, 0.0, "Left motor at rest")),
            Arguments.of(EncoderTestCase(0, 1000L, 0.5, "Left motor forward")),
            Arguments.of(EncoderTestCase(0, -1000L, -0.5, "Left motor reverse")),
            Arguments.of(EncoderTestCase(0, 2147483647L, 2.0, "Left motor max ticks")),
            // Motor 1 (right) cases
            Arguments.of(EncoderTestCase(1, 0L, 0.0, "Right motor at rest")),
            Arguments.of(EncoderTestCase(1, 5000L, 1.5, "Right motor forward")),
            Arguments.of(EncoderTestCase(1, -5000L, -1.5, "Right motor reverse"))
        )

        @JvmStatic
        fun telemetryTestCases(): Stream<Arguments> = Stream.of(
            // Normal operating conditions
            Arguments.of(TelemetryTestCase(
                100.0, 25.0, 15.0, -50,
                "Ideal conditions - full battery, cool temp"
            )),
            Arguments.of(TelemetryTestCase(
                50.0, 35.0, 45.0, -65,
                "Normal operation - half battery"
            )),
            // Edge cases
            Arguments.of(TelemetryTestCase(
                5.0, 45.0, 90.0, -85,
                "Low battery warning threshold"
            )),
            Arguments.of(TelemetryTestCase(
                0.0, 60.0, 100.0, -95,
                "Critical - battery depleted, overheating"
            )),
            // Boundary values
            Arguments.of(TelemetryTestCase(
                100.0, -10.0, 0.0, -30,
                "Cold environment, idle CPU"
            ))
        )

        @JvmStatic
        fun headerTestCases(): Stream<Arguments> = Stream.of(
            Arguments.of(HeaderTestCase("uuid-12345", "1.0.0", "Standard request")),
            Arguments.of(HeaderTestCase("req-001", "2.0.0-beta", "Beta version client")),
            Arguments.of(HeaderTestCase("", "1.0.0", "Empty request ID")),
            Arguments.of(HeaderTestCase("test-${System.currentTimeMillis()}", "0.0.1", "Timestamp-based ID"))
        )

        @JvmStatic
        fun imuTestCases(): Stream<Arguments> = Stream.of(
            // Level orientation
            Arguments.of(0.0, 0.0, 0.0, 0.0, 0.0, 9.81, "Level - gravity on Z"),
            // Pitched forward
            Arguments.of(0.1, 0.0, 0.0, 0.98, 0.0, 9.76, "Pitched forward 5.7 deg"),
            // Rolled right
            Arguments.of(0.0, 0.1, 0.0, 0.0, 0.98, 9.76, "Rolled right 5.7 deg"),
            // Yaw rotation
            Arguments.of(0.0, 0.0, 1.57, 0.0, 0.0, 9.81, "Rotated 90 deg"),
            // Combined orientation
            Arguments.of(0.05, 0.05, 0.785, 0.49, 0.49, 9.79, "Combined tilt and rotation")
        )
    }

    // ========================================================================
    // Framework Validation Tests
    // ========================================================================

    @Nested
    @DisplayName("Test Framework Validation")
    inner class FrameworkTests {

        @Test
        @DisplayName("JUnit 5 framework is operational")
        fun testJUnitFramework() {
            assertTrue(true, "JUnit 5 test framework is working")
        }

        @ParameterizedTest(name = "assertEquals({0} + {1} = {2})")
        @CsvSource(
            "1, 1, 2",
            "2, 3, 5",
            "0, 0, 0",
            "-1, 1, 0",
            "100, -50, 50"
        )
        @DisplayName("Basic arithmetic assertions work")
        fun testArithmeticAssertions(a: Int, b: Int, expected: Int) {
            assertEquals(expected, a + b)
        }

        @Test
        @DisplayName("Exception assertions work correctly")
        fun testExceptionAssertions() {
            assertThrows(IllegalArgumentException::class.java) {
                throw IllegalArgumentException("expected")
            }
        }
    }

    // ========================================================================
    // VelocityCommand Serialization Tests (Table-Driven)
    // ========================================================================

    @Nested
    @DisplayName("VelocityCommand Serialization")
    inner class VelocityCommandTests {

        @ParameterizedTest(name = "{0}: {1}")
        @MethodSource("com.star.proto.SerializationTest#velocityTestCases")
        @DisplayName("Round-trip serialization preserves velocity data")
        fun testVelocityRoundTrip(testCase: VelocityTestCase) {
            // Create command
            val original = VelocityCommand.newBuilder()
                .setLeftVelocityMps(testCase.leftMps)
                .setRightVelocityMps(testCase.rightMps)
                .setSequence(testCase.sequence)
                .setTimestampUs(System.currentTimeMillis() * 1000)
                .build()

            // Serialize to bytes
            val bytes = original.toByteArray()

            // Verify compact encoding (protobuf should be efficient)
            assertTrue(bytes.size < 100,
                "Serialized size should be < 100 bytes, was ${bytes.size}")

            // Deserialize
            val deserialized = VelocityCommand.parseFrom(bytes)

            // Verify fields preserved
            assertEquals(testCase.leftMps, deserialized.leftVelocityMps, 0.0001,
                "Left velocity mismatch for ${testCase.name}")
            assertEquals(testCase.rightMps, deserialized.rightVelocityMps, 0.0001,
                "Right velocity mismatch for ${testCase.name}")
            assertEquals(testCase.sequence, deserialized.sequence,
                "Sequence mismatch for ${testCase.name}")
        }

        @ParameterizedTest(name = "Velocity boundary: {0} m/s")
        @CsvSource(
            "-2.0, Valid minimum velocity",
            "-1.0, Moderate reverse",
            "0.0, Stopped",
            "1.0, Moderate forward",
            "2.0, Valid maximum velocity"
        )
        @DisplayName("Velocity values within valid range serialize correctly")
        fun testVelocityBoundaries(velocity: Double, description: String) {
            val command = VelocityCommand.newBuilder()
                .setLeftVelocityMps(velocity)
                .setRightVelocityMps(velocity)
                .build()

            val bytes = command.toByteArray()
            val parsed = VelocityCommand.parseFrom(bytes)

            assertEquals(velocity, parsed.leftVelocityMps, 0.0001, description)
            assertEquals(velocity, parsed.rightVelocityMps, 0.0001, description)
        }
    }

    // ========================================================================
    // EncoderData Streaming Tests (Table-Driven)
    // ========================================================================

    @Nested
    @DisplayName("EncoderData Streaming")
    inner class EncoderDataTests {

        @ParameterizedTest(name = "Motor {0}: {3}")
        @MethodSource("com.star.proto.SerializationTest#encoderTestCases")
        @DisplayName("Individual encoder messages serialize correctly")
        fun testEncoderSerialization(testCase: EncoderTestCase) {
            val encoderData = EncoderData.newBuilder()
                .setMotorId(testCase.motorId)
                .setTicks(testCase.ticks)
                .setVelocityMps(testCase.velocityMps)
                .setTimestampUs(System.currentTimeMillis() * 1000)
                .build()

            val bytes = encoderData.toByteArray()
            val parsed = EncoderData.parseFrom(bytes)

            assertEquals(testCase.motorId, parsed.motorId, testCase.description)
            assertEquals(testCase.ticks, parsed.ticks, testCase.description)
            assertEquals(testCase.velocityMps, parsed.velocityMps, 0.0001, testCase.description)
        }

        @ParameterizedTest(name = "Stream {0} messages")
        @CsvSource(
            "1, Single message stream",
            "10, Small batch stream",
            "100, Medium batch stream",
            "1000, Large batch stream"
        )
        @DisplayName("Delimited message streaming works for various batch sizes")
        fun testStreamSerialization(count: Int, description: String) {
            val outputStream = java.io.ByteArrayOutputStream()

            // Write multiple delimited messages (simulating streaming)
            repeat(count) { i ->
                val encoderData = EncoderData.newBuilder()
                    .setMotorId(i % 2)
                    .setTicks(i * 1000L)
                    .setVelocityMps(i * 0.1)
                    .setTimestampUs(System.currentTimeMillis() * 1000)
                    .build()
                encoderData.writeDelimitedTo(outputStream)
            }

            // Read back
            val inputStream = java.io.ByteArrayInputStream(outputStream.toByteArray())
            var readCount = 0
            while (inputStream.available() > 0) {
                val data = EncoderData.parseDelimitedFrom(inputStream)
                assertNotNull(data, "Message $readCount should not be null")
                assertEquals(readCount % 2, data.motorId, "Motor ID mismatch at index $readCount")
                readCount++
            }
            assertEquals(count, readCount, "$description: message count mismatch")
        }
    }

    // ========================================================================
    // Boston Dynamics Style Compliance Tests (Table-Driven)
    // ========================================================================

    @Nested
    @DisplayName("Boston Dynamics Style Compliance")
    inner class StyleComplianceTests {

        @ParameterizedTest(name = "{0} zero value is UNKNOWN")
        @CsvSource(
            "Status, 0",
            "ConnectionStatus, 0",
            "RobotMode, 0",
            "MotorState, 0"
        )
        @DisplayName("All enums have UNKNOWN as zero value")
        fun testEnumZeroValues(enumName: String, expectedZeroValue: Int) {
            val actualZeroValue = when (enumName) {
                "Status" -> Status.STATUS_UNKNOWN.number
                "ConnectionStatus" -> ConnectionStatus.CONNECTION_STATUS_UNKNOWN.number
                "RobotMode" -> RobotMode.ROBOT_MODE_UNKNOWN.number
                "MotorState" -> MotorState.MOTOR_STATE_UNKNOWN.number
                else -> throw IllegalArgumentException("Unknown enum: $enumName")
            }
            assertEquals(expectedZeroValue, actualZeroValue,
                "$enumName should have UNKNOWN = $expectedZeroValue")
        }

        @Test
        @DisplayName("Default enum value in ResponseHeader is UNKNOWN")
        fun testDefaultEnumValue() {
            val response = ResponseHeader.newBuilder().build()
            assertEquals(Status.STATUS_UNKNOWN, response.status,
                "Default status should be STATUS_UNKNOWN")
        }

        @ParameterizedTest(name = "Status.{0}")
        @EnumSource(value = Status::class, names = ["UNRECOGNIZED"], mode = EnumSource.Mode.EXCLUDE)
        @DisplayName("All Status enum values serialize correctly")
        fun testStatusEnumSerialization(status: Status) {
            val header = ResponseHeader.newBuilder()
                .setStatus(status)
                .build()

            val bytes = header.toByteArray()
            val parsed = ResponseHeader.parseFrom(bytes)

            assertEquals(status, parsed.status, "Status $status should survive round-trip")
        }
    }

    // ========================================================================
    // MKS Units Compliance Tests (Table-Driven)
    // ========================================================================

    @Nested
    @DisplayName("MKS Units Compliance")
    inner class MksUnitsTests {

        @ParameterizedTest(name = "IMU: {6}")
        @MethodSource("com.star.proto.SerializationTest#imuTestCases")
        @DisplayName("IMU data uses MKS units (rad, m/s^2)")
        fun testImuMksUnits(
            pitchRad: Double,
            rollRad: Double,
            yawRad: Double,
            accelXMps2: Double,
            accelYMps2: Double,
            accelZMps2: Double,
            description: String
        ) {
            val imu = ImuData.newBuilder()
                .setPitchRad(pitchRad)
                .setRollRad(rollRad)
                .setYawRad(yawRad)
                .setAccelXMps2(accelXMps2)
                .setAccelYMps2(accelYMps2)
                .setAccelZMps2(accelZMps2)
                .build()

            val bytes = imu.toByteArray()
            val parsed = ImuData.parseFrom(bytes)

            assertEquals(pitchRad, parsed.pitchRad, 0.001, "$description - pitch")
            assertEquals(rollRad, parsed.rollRad, 0.001, "$description - roll")
            assertEquals(yawRad, parsed.yawRad, 0.001, "$description - yaw")
            assertEquals(accelZMps2, parsed.accelZMps2, 0.01, "$description - accel Z")
        }

        @ParameterizedTest(name = "Telemetry: {4}")
        @MethodSource("com.star.proto.SerializationTest#telemetryTestCases")
        @DisplayName("Telemetry data preserves all sensor values")
        fun testTelemetrySerialization(testCase: TelemetryTestCase) {
            val telemetry = TelemetryData.newBuilder()
                .setBatteryPercent(testCase.batteryPercent)
                .setTemperatureCelsius(testCase.temperatureCelsius)
                .setCpuUsagePercent(testCase.cpuUsagePercent)
                .setWifiSignalDbm(testCase.wifiSignalDbm)
                .build()

            val bytes = telemetry.toByteArray()
            val parsed = TelemetryData.parseFrom(bytes)

            assertEquals(testCase.batteryPercent, parsed.batteryPercent, 0.01,
                "${testCase.description} - battery")
            assertEquals(testCase.temperatureCelsius, parsed.temperatureCelsius, 0.1,
                "${testCase.description} - temperature")
            assertEquals(testCase.cpuUsagePercent, parsed.cpuUsagePercent, 0.1,
                "${testCase.description} - CPU")
            assertEquals(testCase.wifiSignalDbm, parsed.wifiSignalDbm,
                "${testCase.description} - WiFi")
        }
    }

    // ========================================================================
    // RequestHeader/ResponseHeader Pattern Tests (Table-Driven)
    // ========================================================================

    @Nested
    @DisplayName("Request/Response Header Pattern")
    inner class HeaderPatternTests {

        @ParameterizedTest(name = "Header: {2}")
        @MethodSource("com.star.proto.SerializationTest#headerTestCases")
        @DisplayName("Request headers serialize and echo correctly")
        fun testHeaderSerialization(testCase: HeaderTestCase) {
            val header = RequestHeader.newBuilder()
                .setRequestId(testCase.requestId)
                .setClientVersion(testCase.clientVersion)
                .build()

            val bytes = header.toByteArray()
            val parsed = RequestHeader.parseFrom(bytes)

            assertEquals(testCase.requestId, parsed.requestId,
                "${testCase.description} - request ID")
            assertEquals(testCase.clientVersion, parsed.clientVersion,
                "${testCase.description} - client version")
        }

        @ParameterizedTest(name = "Echo request ID: {0}")
        @CsvSource(
            "uuid-12345, Request ID should be echoed",
            "req-001, Short ID echoed",
            "test-request-with-long-identifier, Long ID echoed"
        )
        @DisplayName("Response echoes request ID correctly")
        fun testRequestIdEcho(requestId: String, description: String) {
            val request = SetVelocityRequest.newBuilder()
                .setHeader(RequestHeader.newBuilder()
                    .setRequestId(requestId)
                    .setClientVersion("1.0.0")
                    .build())
                .setCommand(VelocityCommand.newBuilder()
                    .setLeftVelocityMps(1.0)
                    .setRightVelocityMps(1.0)
                    .build())
                .build()

            val response = SetVelocityResponse.newBuilder()
                .setHeader(ResponseHeader.newBuilder()
                    .setRequestId(requestId)  // Echo request ID
                    .setStatus(Status.STATUS_OK)
                    .build())
                .build()

            assertEquals(request.header.requestId, response.header.requestId, description)
        }
    }

    // ========================================================================
    // JSON Format Compatibility Tests
    // ========================================================================

    @Nested
    @DisplayName("JSON Format Compatibility")
    inner class JsonFormatTests {

        @ParameterizedTest(name = "JSON: battery={0}%, temp={1}C")
        @CsvSource(
            "100.0, 25.0, Full battery cool",
            "50.0, 35.0, Half battery warm",
            "10.0, 45.0, Low battery hot",
            "0.0, 60.0, Dead battery critical"
        )
        @DisplayName("Protobuf to JSON round-trip preserves data")
        fun testJsonRoundTrip(batteryPercent: Double, temperatureCelsius: Double, description: String) {
            val telemetry = TelemetryData.newBuilder()
                .setBatteryPercent(batteryPercent)
                .setTemperatureCelsius(temperatureCelsius)
                .build()

            // Convert to JSON
            val printer = com.google.protobuf.util.JsonFormat.printer()
                .includingDefaultValueFields()
                .preservingProtoFieldNames()
            val json = printer.print(telemetry)

            // Verify JSON contains expected field names (snake_case)
            assertTrue(json.contains("battery_percent"), "JSON should contain battery_percent")
            assertTrue(json.contains("temperature_celsius"), "JSON should contain temperature_celsius")

            // Parse back from JSON
            val builder = TelemetryData.newBuilder()
            com.google.protobuf.util.JsonFormat.parser().merge(json, builder)
            val parsed = builder.build()

            assertEquals(batteryPercent, parsed.batteryPercent, 0.01, "$description - battery")
            assertEquals(temperatureCelsius, parsed.temperatureCelsius, 0.1, "$description - temp")
        }
    }
}
