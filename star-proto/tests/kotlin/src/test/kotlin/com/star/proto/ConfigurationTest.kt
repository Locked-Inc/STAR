// ConfigurationTest.kt - Configuration Protobuf Serialization Tests
// Verifies NVS configuration proto serialization/deserialization.
//
// Using JUnit 5 table-driven tests following existing SerializationTest patterns.
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

class ConfigurationTest {

    // ========================================================================
    // Test Data Classes for Table-Driven Tests
    // ========================================================================

    data class PidGainsTestCase(
        val name: String,
        val kp: Double,
        val ki: Double,
        val kd: Double,
        val description: String
    )

    data class SafetyThresholdsTestCase(
        val overcurrentMa: Double,
        val thermalShutdownC: Double,
        val cellImbalanceMv: Int,
        val description: String
    )

    data class TimingTestCase(
        val motorPeriodMs: Int,
        val telemetryPeriodMs: Int,
        val commTimeoutMs: Int,
        val description: String
    )

    data class ValidationErrorTestCase(
        val fieldPath: String,
        val errorCode: ConfigErrorCode,
        val message: String,
        val description: String
    )

    // ========================================================================
    // Test Data Providers (Table-Driven)
    // ========================================================================

    companion object {

        @JvmStatic
        fun pidGainsTestCases(): Stream<Arguments> = Stream.of(
            // Typical PID gains from MATLAB tuning
            Arguments.of(PidGainsTestCase(
                "matlab_tuned", 0.286, 8.01, 0.0,
                "MATLAB-tuned gains for velocity control"
            )),
            // Conservative gains
            Arguments.of(PidGainsTestCase(
                "conservative", 0.1, 1.0, 0.01,
                "Conservative gains for stability"
            )),
            // Aggressive gains
            Arguments.of(PidGainsTestCase(
                "aggressive", 2.0, 20.0, 0.5,
                "Aggressive gains for fast response"
            )),
            // P-only control
            Arguments.of(PidGainsTestCase(
                "p_only", 1.0, 0.0, 0.0,
                "Proportional-only control"
            )),
            // PI control
            Arguments.of(PidGainsTestCase(
                "pi_control", 0.5, 5.0, 0.0,
                "PI control without derivative"
            )),
            // PD control
            Arguments.of(PidGainsTestCase(
                "pd_control", 1.0, 0.0, 0.2,
                "PD control without integral"
            )),
            // Zero gains (disabled)
            Arguments.of(PidGainsTestCase(
                "disabled", 0.0, 0.0, 0.0,
                "All gains zero - motor disabled"
            ))
        )

        @JvmStatic
        fun safetyThresholdsTestCases(): Stream<Arguments> = Stream.of(
            // Standard thresholds
            Arguments.of(SafetyThresholdsTestCase(
                5000.0, 60.0, 100,
                "Standard safety thresholds"
            )),
            // Conservative thresholds
            Arguments.of(SafetyThresholdsTestCase(
                2000.0, 50.0, 50,
                "Conservative thresholds"
            )),
            // High-current motor
            Arguments.of(SafetyThresholdsTestCase(
                15000.0, 70.0, 200,
                "High-current motor thresholds"
            )),
            // Battery-sensitive
            Arguments.of(SafetyThresholdsTestCase(
                3000.0, 55.0, 30,
                "Battery-sensitive thresholds"
            ))
        )

        @JvmStatic
        fun timingTestCases(): Stream<Arguments> = Stream.of(
            // High-frequency control
            Arguments.of(TimingTestCase(
                1, 10, 100,
                "1kHz motor, 100Hz telemetry"
            )),
            // Standard control
            Arguments.of(TimingTestCase(
                10, 100, 500,
                "100Hz motor, 10Hz telemetry"
            )),
            // Low-frequency control
            Arguments.of(TimingTestCase(
                50, 500, 2000,
                "20Hz motor, 2Hz telemetry"
            )),
            // Fast telemetry
            Arguments.of(TimingTestCase(
                5, 20, 200,
                "200Hz motor, 50Hz telemetry"
            ))
        )

        @JvmStatic
        fun validationErrorTestCases(): Stream<Arguments> = Stream.of(
            Arguments.of(ValidationErrorTestCase(
                "motor_configs[0].pid_config.kp",
                ConfigErrorCode.CONFIG_ERROR_CODE_VALUE_TOO_LOW,
                "PID Kp must be non-negative",
                "Negative Kp gain"
            )),
            Arguments.of(ValidationErrorTestCase(
                "safety_thresholds.overcurrent_threshold_ma",
                ConfigErrorCode.CONFIG_ERROR_CODE_VALUE_TOO_HIGH,
                "Overcurrent threshold exceeds maximum",
                "Overcurrent too high"
            )),
            Arguments.of(ValidationErrorTestCase(
                "timing_config.motor_control_period_ms",
                ConfigErrorCode.CONFIG_ERROR_CODE_VALUE_TOO_LOW,
                "Motor control period must be >= 1ms",
                "Control period too low"
            )),
            Arguments.of(ValidationErrorTestCase(
                "config_crc",
                ConfigErrorCode.CONFIG_ERROR_CODE_CRC_FAILED,
                "Configuration CRC checksum mismatch",
                "CRC failure"
            ))
        )
    }

    // ========================================================================
    // PidConfig Serialization Tests
    // ========================================================================

    @Nested
    @DisplayName("PidConfig Serialization")
    inner class PidConfigTests {

        @ParameterizedTest(name = "{0}: {3}")
        @MethodSource("com.star.proto.ConfigurationTest#pidGainsTestCases")
        @DisplayName("PID gains serialize correctly")
        fun testPidGainsRoundTrip(testCase: PidGainsTestCase) {
            val pidConfig = PidConfig.newBuilder()
                .setKp(testCase.kp)
                .setKi(testCase.ki)
                .setKd(testCase.kd)
                .setOutputMinPercent(-100.0)
                .setOutputMaxPercent(100.0)
                .setIntegralMin(-50.0)
                .setIntegralMax(50.0)
                .build()

            val bytes = pidConfig.toByteArray()
            val parsed = PidConfig.parseFrom(bytes)

            assertEquals(testCase.kp, parsed.kp, 0.0001,
                "${testCase.description} - Kp")
            assertEquals(testCase.ki, parsed.ki, 0.0001,
                "${testCase.description} - Ki")
            assertEquals(testCase.kd, parsed.kd, 0.0001,
                "${testCase.description} - Kd")
        }

        @ParameterizedTest(name = "Motor {0} PID config")
        @CsvSource(
            "0, Left front motor",
            "1, Right front motor",
            "2, Left rear motor",
            "3, Right rear motor"
        )
        @DisplayName("Motor-specific PID configs serialize correctly")
        fun testMotorPidConfig(motorId: Int, description: String) {
            val motorConfig = MotorPidConfiguration.newBuilder()
                .setMotorId(motorId)
                .setPidConfig(PidConfig.newBuilder()
                    .setKp(0.5 + motorId * 0.1)
                    .setKi(5.0 + motorId * 0.5)
                    .setKd(0.05 + motorId * 0.01)
                    .build())
                .build()

            val bytes = motorConfig.toByteArray()
            val parsed = MotorPidConfiguration.parseFrom(bytes)

            assertEquals(motorId, parsed.motorId, description)
            assertEquals(0.5 + motorId * 0.1, parsed.pidConfig.kp, 0.0001, "$description - Kp")
        }
    }

    // ========================================================================
    // SafetyThresholds Serialization Tests
    // ========================================================================

    @Nested
    @DisplayName("SafetyThresholds Serialization")
    inner class SafetyThresholdsTests {

        @ParameterizedTest(name = "{3}")
        @MethodSource("com.star.proto.ConfigurationTest#safetyThresholdsTestCases")
        @DisplayName("Safety thresholds serialize correctly")
        fun testSafetyThresholdsRoundTrip(testCase: SafetyThresholdsTestCase) {
            val thresholds = SafetyThresholds.newBuilder()
                .setOvercurrentThresholdMa(testCase.overcurrentMa)
                .setThermalShutdownCelsius(testCase.thermalShutdownC)
                .setCellImbalanceThresholdMv(testCase.cellImbalanceMv)
                .build()

            val bytes = thresholds.toByteArray()
            val parsed = SafetyThresholds.parseFrom(bytes)

            assertEquals(testCase.overcurrentMa, parsed.overcurrentThresholdMa, 0.01,
                "${testCase.description} - overcurrent")
            assertEquals(testCase.thermalShutdownC, parsed.thermalShutdownCelsius, 0.1,
                "${testCase.description} - thermal")
            assertEquals(testCase.cellImbalanceMv, parsed.cellImbalanceThresholdMv,
                "${testCase.description} - cell imbalance")
        }

        @ParameterizedTest(name = "Overcurrent {0} mA")
        @CsvSource(
            "1000, Minimum threshold",
            "5000, Standard threshold",
            "10000, High-power motor",
            "20000, Maximum threshold"
        )
        @DisplayName("Overcurrent threshold values within valid range")
        fun testOvercurrentRange(thresholdMa: Double, description: String) {
            val thresholds = SafetyThresholds.newBuilder()
                .setOvercurrentThresholdMa(thresholdMa)
                .build()

            val bytes = thresholds.toByteArray()
            val parsed = SafetyThresholds.parseFrom(bytes)

            assertTrue(parsed.overcurrentThresholdMa in 1000.0..20000.0, description)
            assertEquals(thresholdMa, parsed.overcurrentThresholdMa, 0.01, description)
        }
    }

    // ========================================================================
    // TimingConfiguration Serialization Tests
    // ========================================================================

    @Nested
    @DisplayName("TimingConfiguration Serialization")
    inner class TimingConfigTests {

        @ParameterizedTest(name = "{3}")
        @MethodSource("com.star.proto.ConfigurationTest#timingTestCases")
        @DisplayName("Timing configuration serializes correctly")
        fun testTimingRoundTrip(testCase: TimingTestCase) {
            val timing = TimingConfiguration.newBuilder()
                .setMotorControlPeriodMs(testCase.motorPeriodMs)
                .setTelemetryPeriodMs(testCase.telemetryPeriodMs)
                .setCommunicationTimeoutMs(testCase.commTimeoutMs)
                .setBmsPollPeriodMs(1000)
                .build()

            val bytes = timing.toByteArray()
            val parsed = TimingConfiguration.parseFrom(bytes)

            assertEquals(testCase.motorPeriodMs, parsed.motorControlPeriodMs,
                "${testCase.description} - motor period")
            assertEquals(testCase.telemetryPeriodMs, parsed.telemetryPeriodMs,
                "${testCase.description} - telemetry period")
            assertEquals(testCase.commTimeoutMs, parsed.communicationTimeoutMs,
                "${testCase.description} - comm timeout")
        }

        @Test
        @DisplayName("Control frequency calculation from period")
        fun testControlFrequencyCalculation() {
            val timing = TimingConfiguration.newBuilder()
                .setMotorControlPeriodMs(10)  // 10ms = 100Hz
                .setTelemetryPeriodMs(100)    // 100ms = 10Hz
                .build()

            val bytes = timing.toByteArray()
            val parsed = TimingConfiguration.parseFrom(bytes)

            val motorFreqHz = 1000.0 / parsed.motorControlPeriodMs
            val telemetryFreqHz = 1000.0 / parsed.telemetryPeriodMs

            assertEquals(100.0, motorFreqHz, 0.01, "Motor control frequency")
            assertEquals(10.0, telemetryFreqHz, 0.01, "Telemetry frequency")
        }
    }

    // ========================================================================
    // CurrentSensorCalibration Serialization Tests
    // ========================================================================

    @Nested
    @DisplayName("CurrentSensorCalibration Serialization")
    inner class CalibrationTests {

        @ParameterizedTest(name = "{0} motors calibration")
        @CsvSource(
            "1, Single motor",
            "2, Differential drive",
            "4, Four-wheel drive"
        )
        @DisplayName("Current calibration for various motor counts")
        fun testCurrentCalibrationArrays(motorCount: Int, description: String) {
            val offsets = List(motorCount) { it * 10.0 }  // 0, 10, 20, 30 mA
            val scales = List(motorCount) { 1.0 + it * 0.01 }  // 1.0, 1.01, 1.02, 1.03

            val calibration = CurrentSensorCalibration.newBuilder()
                .addAllOffsetMa(offsets)
                .addAllScaleFactor(scales)
                .build()

            val bytes = calibration.toByteArray()
            val parsed = CurrentSensorCalibration.parseFrom(bytes)

            assertEquals(motorCount, parsed.offsetMaList.size, "$description - offset count")
            assertEquals(motorCount, parsed.scaleFactorList.size, "$description - scale count")
            assertEquals(offsets, parsed.offsetMaList, "$description - offsets")
            assertEquals(scales, parsed.scaleFactorList, "$description - scales")
        }

        @Test
        @DisplayName("Temperature calibration offset serializes correctly")
        fun testTemperatureCalibration() {
            val calibration = TemperatureCalibration.newBuilder()
                .setOffsetCelsius(-2.5)  // -2.5C offset correction
                .build()

            val bytes = calibration.toByteArray()
            val parsed = TemperatureCalibration.parseFrom(bytes)

            assertEquals(-2.5, parsed.offsetCelsius, 0.01, "Temperature offset")
        }
    }

    // ========================================================================
    // EncoderConfiguration Serialization Tests
    // ========================================================================

    @Nested
    @DisplayName("EncoderConfiguration Serialization")
    inner class EncoderConfigTests {

        @ParameterizedTest(name = "Encoder {0} PPR")
        @CsvSource(
            "500, 0.1, 10.0, Low-resolution encoder",
            "1000, 0.08, 20.0, Standard encoder",
            "2000, 0.065, 34.02, High-resolution encoder",
            "4000, 0.05, 50.0, Precision encoder"
        )
        @DisplayName("Encoder configuration values serialize correctly")
        fun testEncoderConfig(
            edgesPerRev: Int,
            wheelDiameterM: Double,
            gearRatio: Double,
            description: String
        ) {
            val config = EncoderConfiguration.newBuilder()
                .setEdgesPerRevolution(edgesPerRev)
                .setWheelDiameterM(wheelDiameterM)
                .setGearRatio(gearRatio)
                .build()

            val bytes = config.toByteArray()
            val parsed = EncoderConfiguration.parseFrom(bytes)

            assertEquals(edgesPerRev, parsed.edgesPerRevolution, "$description - edges")
            assertEquals(wheelDiameterM, parsed.wheelDiameterM, 0.001, "$description - diameter")
            assertEquals(gearRatio, parsed.gearRatio, 0.01, "$description - gear ratio")
        }
    }

    // ========================================================================
    // ConfigValidationStatus Enum Tests
    // ========================================================================

    @Nested
    @DisplayName("ConfigValidationStatus Enum Tests")
    inner class ValidationStatusEnumTests {

        @Test
        @DisplayName("ConfigValidationStatus zero value is UNKNOWN")
        fun testEnumZeroValue() {
            assertEquals(0, ConfigValidationStatus.CONFIG_VALIDATION_STATUS_UNKNOWN.number,
                "CONFIG_VALIDATION_STATUS_UNKNOWN should have value 0")
        }

        @ParameterizedTest(name = "ConfigValidationStatus.{0}")
        @EnumSource(value = ConfigValidationStatus::class, names = ["UNRECOGNIZED"], mode = EnumSource.Mode.EXCLUDE)
        @DisplayName("All ConfigValidationStatus enum values serialize correctly")
        fun testValidationStatusEnumSerialization(status: ConfigValidationStatus) {
            val result = ConfigValidationResult.newBuilder()
                .setStatus(status)
                .build()

            val bytes = result.toByteArray()
            val parsed = ConfigValidationResult.parseFrom(bytes)

            assertEquals(status, parsed.status, "Status $status should survive round-trip")
        }
    }

    // ========================================================================
    // ConfigErrorCode Enum Tests
    // ========================================================================

    @Nested
    @DisplayName("ConfigErrorCode Enum Tests")
    inner class ErrorCodeEnumTests {

        @Test
        @DisplayName("ConfigErrorCode zero value is UNKNOWN")
        fun testEnumZeroValue() {
            assertEquals(0, ConfigErrorCode.CONFIG_ERROR_CODE_UNKNOWN.number,
                "CONFIG_ERROR_CODE_UNKNOWN should have value 0")
        }

        @ParameterizedTest(name = "ConfigErrorCode.{0}")
        @EnumSource(value = ConfigErrorCode::class, names = ["UNRECOGNIZED"], mode = EnumSource.Mode.EXCLUDE)
        @DisplayName("All ConfigErrorCode enum values serialize correctly")
        fun testErrorCodeEnumSerialization(errorCode: ConfigErrorCode) {
            val error = ConfigValidationError.newBuilder()
                .setFieldPath("test.field")
                .setErrorCode(errorCode)
                .setMessage("Test error message")
                .build()

            val bytes = error.toByteArray()
            val parsed = ConfigValidationError.parseFrom(bytes)

            assertEquals(errorCode, parsed.errorCode, "ErrorCode $errorCode should survive round-trip")
        }
    }

    // ========================================================================
    // ConfigValidationError Serialization Tests
    // ========================================================================

    @Nested
    @DisplayName("ConfigValidationError Serialization")
    inner class ValidationErrorTests {

        @ParameterizedTest(name = "{3}")
        @MethodSource("com.star.proto.ConfigurationTest#validationErrorTestCases")
        @DisplayName("Validation errors serialize correctly")
        fun testValidationErrorRoundTrip(testCase: ValidationErrorTestCase) {
            val error = ConfigValidationError.newBuilder()
                .setFieldPath(testCase.fieldPath)
                .setErrorCode(testCase.errorCode)
                .setMessage(testCase.message)
                .setActualValue("-1.0")
                .setExpectedConstraint(">= 0")
                .build()

            val bytes = error.toByteArray()
            val parsed = ConfigValidationError.parseFrom(bytes)

            assertEquals(testCase.fieldPath, parsed.fieldPath,
                "${testCase.description} - field path")
            assertEquals(testCase.errorCode, parsed.errorCode,
                "${testCase.description} - error code")
            assertEquals(testCase.message, parsed.message,
                "${testCase.description} - message")
        }

        @Test
        @DisplayName("Multiple validation errors in result")
        fun testMultipleValidationErrors() {
            val errors = listOf(
                ConfigValidationError.newBuilder()
                    .setFieldPath("field1")
                    .setErrorCode(ConfigErrorCode.CONFIG_ERROR_CODE_VALUE_TOO_LOW)
                    .setMessage("Value too low")
                    .build(),
                ConfigValidationError.newBuilder()
                    .setFieldPath("field2")
                    .setErrorCode(ConfigErrorCode.CONFIG_ERROR_CODE_VALUE_TOO_HIGH)
                    .setMessage("Value too high")
                    .build(),
                ConfigValidationError.newBuilder()
                    .setFieldPath("field3")
                    .setErrorCode(ConfigErrorCode.CONFIG_ERROR_CODE_INVALID_NUMBER)
                    .setMessage("Invalid number")
                    .build()
            )

            val result = ConfigValidationResult.newBuilder()
                .setStatus(ConfigValidationStatus.CONFIG_VALIDATION_STATUS_INVALID)
                .addAllErrors(errors)
                .build()

            val bytes = result.toByteArray()
            val parsed = ConfigValidationResult.parseFrom(bytes)

            assertEquals(ConfigValidationStatus.CONFIG_VALIDATION_STATUS_INVALID, parsed.status)
            assertEquals(3, parsed.errorsCount, "Should have 3 errors")
            assertEquals("field1", parsed.getErrors(0).fieldPath)
            assertEquals("field2", parsed.getErrors(1).fieldPath)
            assertEquals("field3", parsed.getErrors(2).fieldPath)
        }
    }

    // ========================================================================
    // Complete SystemConfiguration Serialization Tests
    // ========================================================================

    @Nested
    @DisplayName("Complete SystemConfiguration Serialization")
    inner class CompleteConfigTests {

        @Test
        @DisplayName("Complete system configuration serializes correctly")
        fun testCompleteSystemConfiguration() {
            val config = SystemConfiguration.newBuilder()
                // Motor configs for 4 motors
                .addAllMotorConfigs((0..3).map { motorId ->
                    MotorPidConfiguration.newBuilder()
                        .setMotorId(motorId)
                        .setPidConfig(PidConfig.newBuilder()
                            .setKp(0.286)
                            .setKi(8.01)
                            .setKd(0.0)
                            .setOutputMinPercent(-100.0)
                            .setOutputMaxPercent(100.0)
                            .build())
                        .build()
                })
                // Current calibration
                .setCurrentCalibration(CurrentSensorCalibration.newBuilder()
                    .addAllOffsetMa(listOf(0.0, 0.0, 0.0, 0.0))
                    .addAllScaleFactor(listOf(1.0, 1.0, 1.0, 1.0))
                    .build())
                // Temperature calibration
                .setTemperatureCalibration(TemperatureCalibration.newBuilder()
                    .setOffsetCelsius(0.0)
                    .build())
                // Safety thresholds
                .setSafetyThresholds(SafetyThresholds.newBuilder()
                    .setOvercurrentThresholdMa(5000.0)
                    .setThermalShutdownCelsius(60.0)
                    .setCellImbalanceThresholdMv(100)
                    .build())
                // Encoder config
                .setEncoderConfig(EncoderConfiguration.newBuilder()
                    .setEdgesPerRevolution(2000)
                    .setWheelDiameterM(0.065)
                    .setGearRatio(34.02)
                    .build())
                // Timing config
                .setTimingConfig(TimingConfiguration.newBuilder()
                    .setMotorControlPeriodMs(10)
                    .setTelemetryPeriodMs(100)
                    .setCommunicationTimeoutMs(500)
                    .setBmsPollPeriodMs(1000)
                    .build())
                // Metadata
                .setConfigVersion(1)
                .setConfigCrc(0x12345678)
                .build()

            val bytes = config.toByteArray()
            assertTrue(bytes.size < 1000,
                "Complete config should be < 1KB, was ${bytes.size}")

            val parsed = SystemConfiguration.parseFrom(bytes)

            assertEquals(4, parsed.motorConfigsCount, "Should have 4 motor configs")
            assertEquals(0.286, parsed.getMotorConfigs(0).pidConfig.kp, 0.001, "Motor 0 Kp")
            assertEquals(5000.0, parsed.safetyThresholds.overcurrentThresholdMa, 0.01, "Overcurrent")
            assertEquals(10, parsed.timingConfig.motorControlPeriodMs, "Motor period")
            assertEquals(1, parsed.configVersion, "Config version")
            assertEquals(0x12345678, parsed.configCrc, "Config CRC")
        }

        @Test
        @DisplayName("Request/Response pattern works correctly")
        fun testGetSetConfigurationPattern() {
            // Simulate GetConfiguration request
            val getRequest = GetConfigurationRequest.newBuilder()
                .setHeader(RequestHeader.newBuilder()
                    .setRequestId("get-config-001")
                    .setClientVersion("1.0.0")
                    .build())
                .build()

            val getRequestBytes = getRequest.toByteArray()
            val parsedGetRequest = GetConfigurationRequest.parseFrom(getRequestBytes)
            assertEquals("get-config-001", parsedGetRequest.header.requestId)

            // Simulate GetConfiguration response
            val getResponse = GetConfigurationResponse.newBuilder()
                .setHeader(ResponseHeader.newBuilder()
                    .setRequestId("get-config-001")
                    .setStatus(Status.STATUS_OK)
                    .build())
                .setConfiguration(SystemConfiguration.newBuilder()
                    .setConfigVersion(1)
                    .build())
                .build()

            val getResponseBytes = getResponse.toByteArray()
            val parsedGetResponse = GetConfigurationResponse.parseFrom(getResponseBytes)
            assertEquals(Status.STATUS_OK, parsedGetResponse.header.status)
            assertEquals(1, parsedGetResponse.configuration.configVersion)
        }

        @Test
        @DisplayName("SetConfiguration with validation result")
        fun testSetConfigurationWithValidation() {
            val setRequest = SetConfigurationRequest.newBuilder()
                .setHeader(RequestHeader.newBuilder()
                    .setRequestId("set-config-001")
                    .build())
                .setConfiguration(SystemConfiguration.newBuilder()
                    .setConfigVersion(2)
                    .build())
                .setPersistToNvs(true)
                .build()

            val setResponse = SetConfigurationResponse.newBuilder()
                .setHeader(ResponseHeader.newBuilder()
                    .setRequestId("set-config-001")
                    .setStatus(Status.STATUS_OK)
                    .build())
                .setValidationResult(ConfigValidationResult.newBuilder()
                    .setStatus(ConfigValidationStatus.CONFIG_VALIDATION_STATUS_VALID)
                    .build())
                .build()

            val bytes = setResponse.toByteArray()
            val parsed = SetConfigurationResponse.parseFrom(bytes)

            assertEquals(Status.STATUS_OK, parsed.header.status)
            assertEquals(ConfigValidationStatus.CONFIG_VALIDATION_STATUS_VALID,
                parsed.validationResult.status)
        }
    }
}
