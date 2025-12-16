// SerializationTest.kt - Protobuf Serialization Round-Trip Tests
// Verifies that generated Kotlin code serializes/deserializes correctly.
//
// Using JUnit 5 with gRPC testing patterns:
// https://grpc.io/blog/graceful-cleanup-junit-tests/
//
// STAR Project - Texas A&M University
// December 2025

package com.star.proto

import org.junit.jupiter.api.Test
import org.junit.jupiter.api.DisplayName
import org.junit.jupiter.api.Nested
import org.junit.jupiter.api.BeforeEach
import org.junit.jupiter.api.AfterEach
import org.junit.jupiter.api.Assertions.*
import org.junit.jupiter.params.ParameterizedTest
import org.junit.jupiter.params.provider.ValueSource
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream

// gRPC testing imports (for future service tests)
// import io.grpc.testing.GrpcCleanupRule
// import io.grpc.inprocess.InProcessServerBuilder
// import io.grpc.inprocess.InProcessChannelBuilder

// Import generated classes (will be available after buf generate)
// import com.star.proto.v1.*

class SerializationTest {

    @Nested
    @DisplayName("Test Framework Validation")
    inner class FrameworkTests {

        @Test
        @DisplayName("JUnit 5 framework is operational")
        fun testJUnitFramework() {
            assertTrue(true, "JUnit 5 test framework is working")
        }

        @Test
        @DisplayName("Assertions work correctly")
        fun testAssertions() {
            assertEquals(4, 2 + 2)
            assertNotNull("test")
            assertThrows(IllegalArgumentException::class.java) {
                throw IllegalArgumentException("expected")
            }
        }
    }

    // TODO: Uncomment after buf generate produces Kotlin code
    /*
    @Nested
    @DisplayName("VelocityCommand Serialization")
    inner class VelocityCommandTests {

        @Test
        @DisplayName("Round-trip serialization preserves all fields")
        fun testRoundTrip() {
            // Create command with MKS units (m/s)
            val original = VelocityCommand.newBuilder()
                .setLeftVelocityMps(1.5)   // 1.5 m/s
                .setRightVelocityMps(-0.5) // -0.5 m/s
                .setSequence(42)
                .setTimestampUs(System.currentTimeMillis() * 1000)
                .build()

            // Serialize to bytes
            val bytes = original.toByteArray()

            // Verify compact encoding
            assertTrue(bytes.size < 100, "Serialized size should be < 100 bytes, was ${bytes.size}")

            // Deserialize
            val deserialized = VelocityCommand.parseFrom(bytes)

            // Verify fields
            assertEquals(original.leftVelocityMps, deserialized.leftVelocityMps, 0.001)
            assertEquals(original.rightVelocityMps, deserialized.rightVelocityMps, 0.001)
            assertEquals(original.sequence, deserialized.sequence)
            assertEquals(original.timestampUs, deserialized.timestampUs)
        }

        @ParameterizedTest
        @ValueSource(doubles = [-2.0, -1.0, 0.0, 1.0, 2.0])
        @DisplayName("Velocity values within valid range serialize correctly")
        fun testVelocityRange(velocity: Double) {
            val command = VelocityCommand.newBuilder()
                .setLeftVelocityMps(velocity)
                .setRightVelocityMps(velocity)
                .build()

            val bytes = command.toByteArray()
            val parsed = VelocityCommand.parseFrom(bytes)

            assertEquals(velocity, parsed.leftVelocityMps, 0.001)
            assertEquals(velocity, parsed.rightVelocityMps, 0.001)
        }
    }

    @Nested
    @DisplayName("EncoderData Streaming")
    inner class EncoderDataTests {

        @Test
        @DisplayName("Delimited messages serialize for streaming")
        fun testStreamSerialization() {
            val outputStream = ByteArrayOutputStream()

            // Write multiple delimited messages (simulating streaming)
            repeat(10) { i ->
                val encoderData = EncoderData.newBuilder()
                    .setMotorId(i % 2)
                    .setTicks(i * 1000L)
                    .setVelocityMps(i * 0.1)
                    .setTimestampUs(System.currentTimeMillis() * 1000)
                    .build()
                encoderData.writeDelimitedTo(outputStream)
            }

            // Read back
            val inputStream = ByteArrayInputStream(outputStream.toByteArray())
            var count = 0
            while (inputStream.available() > 0) {
                val data = EncoderData.parseDelimitedFrom(inputStream)
                assertNotNull(data)
                assertEquals(count % 2, data.motorId)
                count++
            }
            assertEquals(10, count)
        }
    }

    @Nested
    @DisplayName("Boston Dynamics Style Compliance")
    inner class StyleComplianceTests {

        @Test
        @DisplayName("Enum zero value is UNKNOWN")
        fun testEnumZeroValue() {
            // Verify Boston Dynamics convention: enum 0 = UNKNOWN
            assertEquals(0, Status.STATUS_UNKNOWN.number)
            assertEquals(0, ConnectionStatus.CONNECTION_STATUS_UNKNOWN.number)
            assertEquals(0, RobotMode.ROBOT_MODE_UNKNOWN.number)
            assertEquals(0, MotorState.MOTOR_STATE_UNKNOWN.number)
            assertEquals(0, GpsFix.GPS_FIX_UNKNOWN.number)
        }

        @Test
        @DisplayName("Default enum value is UNKNOWN")
        fun testDefaultEnumValue() {
            val response = ResponseHeader.newBuilder().build()
            assertEquals(Status.STATUS_UNKNOWN, response.status)
        }
    }

    @Nested
    @DisplayName("MKS Units Compliance")
    inner class MksUnitsTests {

        @Test
        @DisplayName("TelemetryData uses MKS units")
        fun testTelemetryMksUnits() {
            val telemetry = TelemetryData.newBuilder()
                .setImu(ImuData.newBuilder()
                    .setPitchRad(0.1)           // radians
                    .setRollRad(-0.05)
                    .setYawRad(1.57)
                    .setAccelXMps2(0.0)
                    .setAccelYMps2(0.0)
                    .setAccelZMps2(9.81)        // m/s^2
                    .setGyroXRadPerS(0.01)      // rad/s
                    .setGyroYRadPerS(0.0)
                    .setGyroZRadPerS(0.0)
                    .build())
                .setBatteryPercent(85.5)
                .setWifiSignalDbm(-45)
                .setCpuUsagePercent(32.5)
                .setTemperatureCelsius(42.0)
                .build()

            val bytes = telemetry.toByteArray()
            val parsed = TelemetryData.parseFrom(bytes)

            // Verify MKS units preserved
            assertEquals(9.81, parsed.imu.accelZMps2, 0.01)
            assertEquals(0.1, parsed.imu.pitchRad, 0.001)
            assertEquals(42.0, parsed.temperatureCelsius, 0.1)
        }
    }

    @Nested
    @DisplayName("RequestHeader/ResponseHeader Pattern")
    inner class HeaderPatternTests {

        @Test
        @DisplayName("Request ID is echoed in response")
        fun testRequestIdEcho() {
            val requestId = "test-${System.currentTimeMillis()}"

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

            // Verify request ID is echoed in response
            assertEquals(request.header.requestId, response.header.requestId)
        }

        @Test
        @DisplayName("Headers serialize correctly")
        fun testHeaderSerialization() {
            val header = RequestHeader.newBuilder()
                .setRequestId("uuid-12345")
                .setClientVersion("1.0.0")
                .build()

            val bytes = header.toByteArray()
            val parsed = RequestHeader.parseFrom(bytes)

            assertEquals("uuid-12345", parsed.requestId)
            assertEquals("1.0.0", parsed.clientVersion)
        }
    }

    @Nested
    @DisplayName("JSON Format Compatibility")
    inner class JsonFormatTests {

        @Test
        @DisplayName("Protobuf to JSON conversion")
        fun testJsonConversion() {
            val telemetry = TelemetryData.newBuilder()
                .setBatteryPercent(75.0)
                .setTemperatureCelsius(35.0)
                .build()

            // Convert to JSON
            val printer = com.google.protobuf.util.JsonFormat.printer()
                .includingDefaultValueFields()
                .preservingProtoFieldNames()
            val json = printer.print(telemetry)

            // Verify JSON contains expected fields
            assertTrue(json.contains("battery_percent"))
            assertTrue(json.contains("temperature_celsius"))

            // Parse back from JSON
            val builder = TelemetryData.newBuilder()
            com.google.protobuf.util.JsonFormat.parser().merge(json, builder)
            val parsed = builder.build()

            assertEquals(telemetry.batteryPercent, parsed.batteryPercent, 0.01)
        }
    }
    */

    // TODO: Add gRPC service tests after buf generate
    /*
    @Nested
    @DisplayName("gRPC Service Tests")
    inner class GrpcServiceTests {

        private lateinit var grpcCleanup: GrpcCleanupRule
        private lateinit var serverName: String

        @BeforeEach
        fun setUp() {
            grpcCleanup = GrpcCleanupRule()
            serverName = InProcessServerBuilder.generateName()
        }

        @AfterEach
        fun tearDown() {
            // GrpcCleanupRule handles cleanup automatically
        }

        @Test
        @DisplayName("MotorControlService responds to SetVelocity")
        fun testSetVelocity() {
            // Register in-process server
            val server = grpcCleanup.register(
                InProcessServerBuilder.forName(serverName)
                    .directExecutor()
                    .addService(MockMotorControlService())
                    .build()
                    .start()
            )

            // Register in-process channel
            val channel = grpcCleanup.register(
                InProcessChannelBuilder.forName(serverName)
                    .directExecutor()
                    .build()
            )

            // Create stub and make call
            val stub = MotorControlServiceGrpc.newBlockingStub(channel)
            val request = SetVelocityRequest.newBuilder()
                .setHeader(RequestHeader.newBuilder()
                    .setRequestId("test-001")
                    .build())
                .setCommand(VelocityCommand.newBuilder()
                    .setLeftVelocityMps(1.0)
                    .setRightVelocityMps(1.0)
                    .build())
                .build()

            val response = stub.setVelocity(request)

            assertEquals(Status.STATUS_OK, response.header.status)
            assertEquals("test-001", response.header.requestId)
        }
    }
    */
}
