// BatteryManagementTest.kt - Battery Management Protobuf Serialization Tests
// Verifies BQ7850 BMS proto serialization/deserialization.
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

class BatteryManagementTest {

    // ========================================================================
    // Test Data Classes for Table-Driven Tests
    // ========================================================================

    data class CellVoltageTestCase(
        val name: String,
        val cellVoltages: List<Int>,
        val packMv: Int,
        val description: String
    )

    data class TemperatureTestCase(
        val name: String,
        val temps: List<Int>,
        val avgTemp: Int,
        val description: String
    )

    data class CurrentTestCase(
        val currentMa: Int,
        val avgCurrentMa: Int,
        val voltageMv: Int,
        val powerMw: Int,
        val description: String
    )

    data class SocTestCase(
        val remainingMah: Int,
        val fullMah: Int,
        val relativeSoc: Int,
        val absoluteSoc: Int,
        val cycleCount: Int,
        val description: String
    )

    data class ProtectionTestCase(
        val ovMv: Int,
        val uvMv: Int,
        val ocMa: Int,
        val odMa: Int,
        val otDeciC: Int,
        val utDeciC: Int,
        val description: String
    )

    // ========================================================================
    // Test Data Providers (Table-Driven)
    // ========================================================================

    companion object {

        @JvmStatic
        fun cellVoltageTestCases(): Stream<Arguments> = Stream.of(
            // 4S battery pack (typical)
            Arguments.of(CellVoltageTestCase(
                "4s_nominal",
                listOf(3700, 3710, 3720, 3730),
                14860,
                "4S pack at nominal voltage"
            )),
            Arguments.of(CellVoltageTestCase(
                "4s_full",
                listOf(4200, 4200, 4200, 4200),
                16800,
                "4S pack fully charged"
            )),
            Arguments.of(CellVoltageTestCase(
                "4s_low",
                listOf(3200, 3210, 3220, 3230),
                12860,
                "4S pack low voltage"
            )),
            // 6S battery pack
            Arguments.of(CellVoltageTestCase(
                "6s_nominal",
                listOf(3700, 3710, 3720, 3730, 3740, 3750),
                22350,
                "6S pack at nominal voltage"
            )),
            // Imbalanced pack
            Arguments.of(CellVoltageTestCase(
                "4s_imbalanced",
                listOf(3500, 3800, 3600, 3700),
                14600,
                "4S pack with cell imbalance"
            )),
            // Single cell
            Arguments.of(CellVoltageTestCase(
                "1s_nominal",
                listOf(3700),
                3700,
                "Single cell"
            )),
            // Maximum 16 cells
            Arguments.of(CellVoltageTestCase(
                "16s_nominal",
                List(16) { 3700 + it * 5 },
                16 * 3700 + (0..15).sum() * 5,
                "16S pack maximum cells"
            ))
        )

        @JvmStatic
        fun temperatureTestCases(): Stream<Arguments> = Stream.of(
            // Normal operating temperatures (in deci-celsius)
            Arguments.of(TemperatureTestCase(
                "room_temp", listOf(250, 255, 260), 255,
                "Room temperature 25C"
            )),
            Arguments.of(TemperatureTestCase(
                "warm", listOf(350, 360, 370), 360,
                "Warm operation 36C"
            )),
            Arguments.of(TemperatureTestCase(
                "hot", listOf(450, 460, 470), 460,
                "Hot operation 46C"
            )),
            // Cold environment
            Arguments.of(TemperatureTestCase(
                "cold", listOf(50, 60, 70), 60,
                "Cold environment 6C"
            )),
            Arguments.of(TemperatureTestCase(
                "freezing", listOf(-50, -40, -30), -40,
                "Below freezing -4C"
            )),
            // Single sensor
            Arguments.of(TemperatureTestCase(
                "single_sensor", listOf(300), 300,
                "Single temperature sensor"
            ))
        )

        @JvmStatic
        fun currentTestCases(): Stream<Arguments> = Stream.of(
            // Charging scenarios
            Arguments.of(CurrentTestCase(
                2000, 1800, 14800, 29600,
                "Charging at 2A"
            )),
            Arguments.of(CurrentTestCase(
                500, 450, 14600, 7300,
                "Trickle charging"
            )),
            // Discharging scenarios
            Arguments.of(CurrentTestCase(
                -3000, -2800, 14400, -43200,
                "Discharging at 3A"
            )),
            Arguments.of(CurrentTestCase(
                -10000, -9500, 14000, -140000,
                "High discharge 10A"
            )),
            // Idle
            Arguments.of(CurrentTestCase(
                0, 0, 14700, 0,
                "Idle - no current"
            )),
            // Edge cases
            Arguments.of(CurrentTestCase(
                50, 25, 14750, 738,
                "Low standby current"
            ))
        )

        @JvmStatic
        fun socTestCases(): Stream<Arguments> = Stream.of(
            // Full battery
            Arguments.of(SocTestCase(
                5000, 5000, 100, 100, 0,
                "Full battery, new"
            )),
            // Partial discharge
            Arguments.of(SocTestCase(
                2500, 5000, 50, 50, 100,
                "Half capacity, 100 cycles"
            )),
            // Low battery
            Arguments.of(SocTestCase(
                500, 5000, 10, 10, 250,
                "Low battery warning"
            )),
            // Degraded battery
            Arguments.of(SocTestCase(
                4000, 4500, 89, 80, 500,
                "Degraded battery, 500 cycles"
            )),
            // Empty
            Arguments.of(SocTestCase(
                0, 5000, 0, 0, 50,
                "Empty battery"
            )),
            // High cycle count
            Arguments.of(SocTestCase(
                3500, 4000, 88, 70, 1000,
                "High cycle count battery"
            ))
        )

        @JvmStatic
        fun protectionTestCases(): Stream<Arguments> = Stream.of(
            // Standard Li-ion thresholds
            Arguments.of(ProtectionTestCase(
                4200, 2800, 5000, 10000, 600, 0,
                "Standard Li-ion protection"
            )),
            // High voltage Li-ion
            Arguments.of(ProtectionTestCase(
                4350, 2500, 3000, 8000, 550, -100,
                "High voltage Li-ion"
            )),
            // LiFePO4 thresholds
            Arguments.of(ProtectionTestCase(
                3650, 2500, 10000, 15000, 650, -200,
                "LiFePO4 protection"
            )),
            // Conservative thresholds
            Arguments.of(ProtectionTestCase(
                4100, 3000, 2000, 5000, 450, 50,
                "Conservative protection"
            ))
        )
    }

    // ========================================================================
    // CellData Serialization Tests
    // ========================================================================

    @Nested
    @DisplayName("CellData Serialization")
    inner class CellDataTests {

        @ParameterizedTest(name = "{0}: {3}")
        @MethodSource("com.star.proto.BatteryManagementTest#cellVoltageTestCases")
        @DisplayName("Cell voltage data serializes correctly")
        fun testCellDataRoundTrip(testCase: CellVoltageTestCase) {
            val cellData = CellData.newBuilder()
                .addAllCellMv(testCase.cellVoltages)
                .setValidCells(testCase.cellVoltages.size)
                .setPackMv(testCase.packMv)
                .setMinCellMv(testCase.cellVoltages.minOrNull() ?: 0)
                .setMaxCellMv(testCase.cellVoltages.maxOrNull() ?: 0)
                .setDeltaMv((testCase.cellVoltages.maxOrNull() ?: 0) -
                    (testCase.cellVoltages.minOrNull() ?: 0))
                .build()

            val bytes = cellData.toByteArray()
            val parsed = CellData.parseFrom(bytes)

            assertEquals(testCase.cellVoltages.size, parsed.validCells,
                "${testCase.description} - valid cells count")
            assertEquals(testCase.packMv, parsed.packMv,
                "${testCase.description} - pack voltage")
            assertEquals(testCase.cellVoltages, parsed.cellMvList,
                "${testCase.description} - cell voltages")
        }

        @ParameterizedTest(name = "Cell count: {0}")
        @CsvSource(
            "1, Single cell",
            "4, 4S pack",
            "6, 6S pack",
            "12, 12S pack",
            "16, Maximum 16S pack"
        )
        @DisplayName("Various cell counts serialize correctly")
        fun testCellCounts(cellCount: Int, description: String) {
            val voltages = List(cellCount) { 3700 + it * 10 }
            val cellData = CellData.newBuilder()
                .addAllCellMv(voltages)
                .setValidCells(cellCount)
                .build()

            val bytes = cellData.toByteArray()
            val parsed = CellData.parseFrom(bytes)

            assertEquals(cellCount, parsed.cellMvList.size, description)
            assertEquals(cellCount, parsed.validCells, description)
        }
    }

    // ========================================================================
    // TemperatureData Serialization Tests
    // ========================================================================

    @Nested
    @DisplayName("TemperatureData Serialization")
    inner class TemperatureDataTests {

        @ParameterizedTest(name = "{0}: {3}")
        @MethodSource("com.star.proto.BatteryManagementTest#temperatureTestCases")
        @DisplayName("Temperature data serializes correctly")
        fun testTemperatureRoundTrip(testCase: TemperatureTestCase) {
            val tempData = TemperatureData.newBuilder()
                .addAllTempDeciCelsius(testCase.temps)
                .setValidSensors(testCase.temps.size)
                .setAvgTempDeciCelsius(testCase.avgTemp)
                .setMinTempDeciCelsius(testCase.temps.minOrNull() ?: 0)
                .setMaxTempDeciCelsius(testCase.temps.maxOrNull() ?: 0)
                .build()

            val bytes = tempData.toByteArray()
            val parsed = TemperatureData.parseFrom(bytes)

            assertEquals(testCase.temps.size, parsed.validSensors,
                "${testCase.description} - sensor count")
            assertEquals(testCase.avgTemp, parsed.avgTempDeciCelsius,
                "${testCase.description} - average temp")
            assertEquals(testCase.temps, parsed.tempDeciCelsiusList,
                "${testCase.description} - temperatures")
        }

        @ParameterizedTest(name = "Temp {0} deci-C = {1}C")
        @CsvSource(
            "250, 25.0, Room temperature",
            "0, 0.0, Freezing point",
            "-100, -10.0, Below freezing",
            "600, 60.0, Hot warning",
            "850, 85.0, Thermal shutdown"
        )
        @DisplayName("Deci-celsius values convert correctly")
        fun testDeciCelsiusConversion(deciCelsius: Int, expectedCelsius: Double, description: String) {
            val tempData = TemperatureData.newBuilder()
                .addTempDeciCelsius(deciCelsius)
                .setValidSensors(1)
                .setAvgTempDeciCelsius(deciCelsius)
                .build()

            val bytes = tempData.toByteArray()
            val parsed = TemperatureData.parseFrom(bytes)

            val actualCelsius = parsed.avgTempDeciCelsius / 10.0
            assertEquals(expectedCelsius, actualCelsius, 0.01, description)
        }
    }

    // ========================================================================
    // CurrentData Serialization Tests
    // ========================================================================

    @Nested
    @DisplayName("CurrentData Serialization")
    inner class CurrentDataTests {

        @ParameterizedTest(name = "{4}")
        @MethodSource("com.star.proto.BatteryManagementTest#currentTestCases")
        @DisplayName("Current data serializes correctly")
        fun testCurrentRoundTrip(testCase: CurrentTestCase) {
            val currentData = CurrentData.newBuilder()
                .setCurrentMa(testCase.currentMa)
                .setAvgCurrentMa(testCase.avgCurrentMa)
                .setVoltageMv(testCase.voltageMv)
                .setPowerMw(testCase.powerMw)
                .build()

            val bytes = currentData.toByteArray()
            val parsed = CurrentData.parseFrom(bytes)

            assertEquals(testCase.currentMa, parsed.currentMa,
                "${testCase.description} - current")
            assertEquals(testCase.avgCurrentMa, parsed.avgCurrentMa,
                "${testCase.description} - avg current")
            assertEquals(testCase.voltageMv, parsed.voltageMv,
                "${testCase.description} - voltage")
            assertEquals(testCase.powerMw, parsed.powerMw,
                "${testCase.description} - power")
        }

        @Test
        @DisplayName("Negative current indicates discharge")
        fun testNegativeCurrentForDischarge() {
            val currentData = CurrentData.newBuilder()
                .setCurrentMa(-5000)  // 5A discharge
                .setAvgCurrentMa(-4800)
                .setVoltageMv(14400)
                .setPowerMw(-72000)   // Negative power = discharge
                .build()

            val bytes = currentData.toByteArray()
            val parsed = CurrentData.parseFrom(bytes)

            assertTrue(parsed.currentMa < 0, "Discharge current should be negative")
            assertTrue(parsed.powerMw < 0, "Discharge power should be negative")
        }
    }

    // ========================================================================
    // StateOfChargeData Serialization Tests
    // ========================================================================

    @Nested
    @DisplayName("StateOfChargeData Serialization")
    inner class SocDataTests {

        @ParameterizedTest(name = "{5}")
        @MethodSource("com.star.proto.BatteryManagementTest#socTestCases")
        @DisplayName("SOC data serializes correctly")
        fun testSocRoundTrip(testCase: SocTestCase) {
            val socData = StateOfChargeData.newBuilder()
                .setRemainingCapacityMah(testCase.remainingMah)
                .setFullCapacityMah(testCase.fullMah)
                .setDesignCapacityMah(5000)  // Assume 5000mAh design
                .setRelativeSocPercent(testCase.relativeSoc)
                .setAbsoluteSocPercent(testCase.absoluteSoc)
                .setCycleCount(testCase.cycleCount)
                .build()

            val bytes = socData.toByteArray()
            val parsed = StateOfChargeData.parseFrom(bytes)

            assertEquals(testCase.remainingMah, parsed.remainingCapacityMah,
                "${testCase.description} - remaining")
            assertEquals(testCase.fullMah, parsed.fullCapacityMah,
                "${testCase.description} - full capacity")
            assertEquals(testCase.relativeSoc, parsed.relativeSocPercent,
                "${testCase.description} - relative SOC")
            assertEquals(testCase.cycleCount, parsed.cycleCount,
                "${testCase.description} - cycles")
        }

        @ParameterizedTest(name = "SOC {0}%")
        @CsvSource(
            "0, Empty",
            "10, Low warning",
            "20, Low",
            "50, Half",
            "80, Good",
            "100, Full"
        )
        @DisplayName("SOC percentage values are valid")
        fun testSocPercentageRange(socPercent: Int, description: String) {
            val socData = StateOfChargeData.newBuilder()
                .setRelativeSocPercent(socPercent)
                .setAbsoluteSocPercent(socPercent)
                .build()

            val bytes = socData.toByteArray()
            val parsed = StateOfChargeData.parseFrom(bytes)

            assertTrue(parsed.relativeSocPercent in 0..100, description)
            assertEquals(socPercent, parsed.relativeSocPercent, description)
        }
    }

    // ========================================================================
    // BatteryStateEnum Tests
    // ========================================================================

    @Nested
    @DisplayName("BatteryStateEnum Tests")
    inner class BatteryStateEnumTests {

        @Test
        @DisplayName("BatteryStateEnum zero value is UNKNOWN")
        fun testEnumZeroValue() {
            assertEquals(0, BatteryStateEnum.BATTERY_STATE_ENUM_UNKNOWN.number,
                "BATTERY_STATE_ENUM_UNKNOWN should have value 0")
        }

        @ParameterizedTest(name = "BatteryStateEnum.{0}")
        @EnumSource(value = BatteryStateEnum::class, names = ["UNRECOGNIZED"], mode = EnumSource.Mode.EXCLUDE)
        @DisplayName("All BatteryStateEnum values serialize correctly")
        fun testBatteryStateEnumSerialization(state: BatteryStateEnum) {
            val status = BatteryStatus.newBuilder()
                .setState(state)
                .build()

            val bytes = status.toByteArray()
            val parsed = BatteryStatus.parseFrom(bytes)

            assertEquals(state, parsed.state, "State $state should survive round-trip")
        }
    }

    // ========================================================================
    // ProtectionThresholds Serialization Tests
    // ========================================================================

    @Nested
    @DisplayName("ProtectionThresholds Serialization")
    inner class ProtectionThresholdsTests {

        @ParameterizedTest(name = "{6}")
        @MethodSource("com.star.proto.BatteryManagementTest#protectionTestCases")
        @DisplayName("Protection thresholds serialize correctly")
        fun testProtectionRoundTrip(testCase: ProtectionTestCase) {
            val protection = ProtectionThresholds.newBuilder()
                .setOvervoltageMv(testCase.ovMv)
                .setUndervoltageMv(testCase.uvMv)
                .setOverchargeMa(testCase.ocMa)
                .setOverdischargeMa(testCase.odMa)
                .setOvertempDeciCelsius(testCase.otDeciC)
                .setUndertempDeciCelsius(testCase.utDeciC)
                .build()

            val bytes = protection.toByteArray()
            val parsed = ProtectionThresholds.parseFrom(bytes)

            assertEquals(testCase.ovMv, parsed.overvoltageMv,
                "${testCase.description} - OV")
            assertEquals(testCase.uvMv, parsed.undervoltageMv,
                "${testCase.description} - UV")
            assertEquals(testCase.ocMa, parsed.overchargeMa,
                "${testCase.description} - OC")
            assertEquals(testCase.odMa, parsed.overdischargeMa,
                "${testCase.description} - OD")
            assertEquals(testCase.otDeciC, parsed.overtempDeciCelsius,
                "${testCase.description} - OT")
            assertEquals(testCase.utDeciC, parsed.undertempDeciCelsius,
                "${testCase.description} - UT")
        }
    }

    // ========================================================================
    // BatteryStatus Serialization Tests
    // ========================================================================

    @Nested
    @DisplayName("BatteryStatus Serialization")
    inner class BatteryStatusTests {

        @ParameterizedTest(name = "Status: charging={0}, discharging={1}, fault={2}")
        @CsvSource(
            "true, false, false, Charging normally",
            "false, true, false, Discharging normally",
            "false, false, false, Idle",
            "true, false, true, Charging with fault",
            "false, true, true, Discharging with fault",
            "false, false, true, Idle with fault"
        )
        @DisplayName("Status flags serialize correctly")
        fun testStatusFlags(
            charging: Boolean,
            discharging: Boolean,
            faultActive: Boolean,
            description: String
        ) {
            val status = BatteryStatus.newBuilder()
                .setCharging(charging)
                .setDischarging(discharging)
                .setFaultActive(faultActive)
                .build()

            val bytes = status.toByteArray()
            val parsed = BatteryStatus.parseFrom(bytes)

            assertEquals(charging, parsed.charging, "$description - charging")
            assertEquals(discharging, parsed.discharging, "$description - discharging")
            assertEquals(faultActive, parsed.faultActive, "$description - fault")
        }

        @Test
        @DisplayName("Safety faults serialize individually")
        fun testSafetyFaults() {
            val faults = SafetyFaults.newBuilder()
                .setCellUndervoltage(true)
                .setCellOvervoltage(false)
                .setOvercurrentCharge(true)
                .setOvercurrentDischarge(false)
                .setOvertempCharge(true)
                .setOvertempDischarge(false)
                .setUndertempCharge(false)
                .setUndertempDischarge(true)
                .build()

            val bytes = faults.toByteArray()
            val parsed = SafetyFaults.parseFrom(bytes)

            assertTrue(parsed.cellUndervoltage, "Cell UV fault")
            assertFalse(parsed.cellOvervoltage, "Cell OV fault")
            assertTrue(parsed.overcurrentCharge, "OC charge fault")
            assertFalse(parsed.overcurrentDischarge, "OC discharge fault")
            assertTrue(parsed.overtempCharge, "OT charge fault")
            assertFalse(parsed.overtempDischarge, "OT discharge fault")
            assertFalse(parsed.undertempCharge, "UT charge fault")
            assertTrue(parsed.undertempDischarge, "UT discharge fault")
        }
    }

    // ========================================================================
    // Complete BatteryState Serialization Tests
    // ========================================================================

    @Nested
    @DisplayName("Complete BatteryState Serialization")
    inner class CompleteBatteryStateTests {

        @Test
        @DisplayName("Complete battery state serializes correctly")
        fun testCompleteBatteryState() {
            val state = BatteryState.newBuilder()
                .setCells(CellData.newBuilder()
                    .addAllCellMv(listOf(3700, 3710, 3720, 3730))
                    .setValidCells(4)
                    .setPackMv(14860)
                    .build())
                .setTemperatures(TemperatureData.newBuilder()
                    .addAllTempDeciCelsius(listOf(250, 255, 260))
                    .setValidSensors(3)
                    .setAvgTempDeciCelsius(255)
                    .build())
                .setCurrent(CurrentData.newBuilder()
                    .setCurrentMa(-2000)
                    .setAvgCurrentMa(-1900)
                    .setVoltageMv(14860)
                    .setPowerMw(-29720)
                    .build())
                .setSoc(StateOfChargeData.newBuilder()
                    .setRemainingCapacityMah(3500)
                    .setFullCapacityMah(5000)
                    .setRelativeSocPercent(70)
                    .setCycleCount(150)
                    .build())
                .setStatus(BatteryStatus.newBuilder()
                    .setDischarging(true)
                    .setState(BatteryStateEnum.BATTERY_STATE_ENUM_DISCHARGING)
                    .build())
                .setTimestampUs(System.currentTimeMillis() * 1000)
                .build()

            val bytes = state.toByteArray()
            assertTrue(bytes.size < 500,
                "Complete battery state should be < 500 bytes, was ${bytes.size}")

            val parsed = BatteryState.parseFrom(bytes)

            assertEquals(4, parsed.cells.validCells, "Cell count")
            assertEquals(14860, parsed.cells.packMv, "Pack voltage")
            assertEquals(3, parsed.temperatures.validSensors, "Temp sensors")
            assertEquals(-2000, parsed.current.currentMa, "Current")
            assertEquals(70, parsed.soc.relativeSocPercent, "SOC")
            assertTrue(parsed.status.discharging, "Discharging flag")
            assertEquals(BatteryStateEnum.BATTERY_STATE_ENUM_DISCHARGING, parsed.status.state, "State enum")
        }

        @ParameterizedTest(name = "Stream {0} states")
        @CsvSource(
            "1, Single state",
            "10, Small batch",
            "100, Medium batch"
        )
        @DisplayName("Battery state streaming works")
        fun testBatteryStateStreaming(count: Int, description: String) {
            val outputStream = java.io.ByteArrayOutputStream()

            repeat(count) { i ->
                val state = BatteryState.newBuilder()
                    .setCells(CellData.newBuilder()
                        .addAllCellMv(listOf(3700 + i, 3710 + i, 3720 + i, 3730 + i))
                        .setValidCells(4)
                        .build())
                    .setTimestampUs(System.currentTimeMillis() * 1000 + i)
                    .build()
                state.writeDelimitedTo(outputStream)
            }

            val inputStream = java.io.ByteArrayInputStream(outputStream.toByteArray())
            var readCount = 0
            while (inputStream.available() > 0) {
                val data = BatteryState.parseDelimitedFrom(inputStream)
                assertNotNull(data, "State $readCount should not be null")
                assertEquals(4, data.cells.validCells, "Cell count at index $readCount")
                readCount++
            }
            assertEquals(count, readCount, "$description: state count mismatch")
        }
    }

    // ========================================================================
    // BmsDeviceInfo Serialization Tests
    // ========================================================================

    @Nested
    @DisplayName("BmsDeviceInfo Serialization")
    inner class DeviceInfoTests {

        @ParameterizedTest(name = "Chemistry: {0}")
        @CsvSource(
            "LION, Standard lithium-ion",
            "LIFEPO4, Lithium iron phosphate",
            "LIPO, Lithium polymer",
            "NIMH, Nickel metal hydride"
        )
        @DisplayName("Device chemistry types serialize correctly")
        fun testChemistryTypes(chemistry: String, description: String) {
            val deviceInfo = BmsDeviceInfo.newBuilder()
                .setDeviceType(0x7850)
                .setFirmwareVersion(100)
                .setHardwareVersion(10)
                .setSerialNumber(12345678)
                .setManufacturer("Texas Instruments")
                .setDeviceName("BQ7850")
                .setChemistry(chemistry)
                .build()

            val bytes = deviceInfo.toByteArray()
            val parsed = BmsDeviceInfo.parseFrom(bytes)

            assertEquals(chemistry, parsed.chemistry, description)
            assertEquals("Texas Instruments", parsed.manufacturer, "Manufacturer")
            assertEquals("BQ7850", parsed.deviceName, "Device name")
            assertEquals(12345678, parsed.serialNumber, "Serial number")
        }

        @Test
        @DisplayName("String fields within nanopb limits")
        fun testStringFieldLimits() {
            // nanopb limit is 32 chars per string
            val maxString = "A".repeat(32)

            val deviceInfo = BmsDeviceInfo.newBuilder()
                .setManufacturer(maxString)
                .setDeviceName(maxString)
                .setChemistry(maxString)
                .build()

            val bytes = deviceInfo.toByteArray()
            val parsed = BmsDeviceInfo.parseFrom(bytes)

            assertEquals(32, parsed.manufacturer.length, "Manufacturer length")
            assertEquals(32, parsed.deviceName.length, "Device name length")
            assertEquals(32, parsed.chemistry.length, "Chemistry length")
        }
    }

    // ========================================================================
    // FET Control Serialization Tests
    // ========================================================================

    @Nested
    @DisplayName("FET Control Serialization")
    inner class FetControlTests {

        @ParameterizedTest(name = "Charge={0}, Discharge={1}")
        @CsvSource(
            "true, true, Both FETs enabled - normal operation",
            "true, false, Charge only - safe storage",
            "false, true, Discharge only - no charging",
            "false, false, Both disabled - emergency stop"
        )
        @DisplayName("FET control request serializes correctly")
        fun testFetControlRequest(
            enableCharge: Boolean,
            enableDischarge: Boolean,
            description: String
        ) {
            val request = ControlFetsRequest.newBuilder()
                .setHeader(RequestHeader.newBuilder()
                    .setRequestId("test-fet-control")
                    .setClientVersion("1.0.0")
                    .build())
                .setEnableChargeFet(enableCharge)
                .setEnableDischargeFet(enableDischarge)
                .build()

            val bytes = request.toByteArray()
            val parsed = ControlFetsRequest.parseFrom(bytes)

            assertEquals(enableCharge, parsed.enableChargeFet, "$description - charge FET")
            assertEquals(enableDischarge, parsed.enableDischargeFet, "$description - discharge FET")
        }

        @Test
        @DisplayName("FET control response echoes state")
        fun testFetControlResponse() {
            val response = ControlFetsResponse.newBuilder()
                .setHeader(ResponseHeader.newBuilder()
                    .setRequestId("test-fet-control")
                    .setStatus(Status.STATUS_OK)
                    .build())
                .setChargeFetEnabled(true)
                .setDischargeFetEnabled(false)
                .build()

            val bytes = response.toByteArray()
            val parsed = ControlFetsResponse.parseFrom(bytes)

            assertEquals(Status.STATUS_OK, parsed.header.status, "Status")
            assertTrue(parsed.chargeFetEnabled, "Charge FET state")
            assertFalse(parsed.dischargeFetEnabled, "Discharge FET state")
        }
    }

    // ========================================================================
    // Cell Balancing Serialization Tests
    // ========================================================================

    @Nested
    @DisplayName("Cell Balancing Serialization")
    inner class CellBalancingTests {

        @ParameterizedTest(name = "Balancing mask: 0x{0}")
        @CsvSource(
            "0001, 1, Cell 1 only",
            "0003, 2, Cells 1-2",
            "000F, 4, Cells 1-4",
            "00FF, 8, Cells 1-8",
            "FFFF, 16, All 16 cells"
        )
        @DisplayName("Cell balancing mask serializes correctly")
        fun testCellBalancingMask(maskHex: String, expectedCells: Int, description: String) {
            val mask = maskHex.toInt(16)
            val request = EnableCellBalancingRequest.newBuilder()
                .setHeader(RequestHeader.newBuilder()
                    .setRequestId("test-balancing")
                    .build())
                .setCellMask(mask)
                .build()

            val bytes = request.toByteArray()
            val parsed = EnableCellBalancingRequest.parseFrom(bytes)

            assertEquals(mask, parsed.cellMask, description)

            // Verify bit count matches expected cells
            val bitCount = Integer.bitCount(parsed.cellMask)
            assertEquals(expectedCells, bitCount, "$description - bit count")
        }

        @Test
        @DisplayName("Balancing status response includes active mask")
        fun testBalancingStatusResponse() {
            val response = GetBalancingStatusResponse.newBuilder()
                .setHeader(ResponseHeader.newBuilder()
                    .setRequestId("test-status")
                    .setStatus(Status.STATUS_OK)
                    .build())
                .setActiveCellMask(0x0007)  // Cells 1, 2, 3 balancing
                .build()

            val bytes = response.toByteArray()
            val parsed = GetBalancingStatusResponse.parseFrom(bytes)

            assertEquals(0x0007, parsed.activeCellMask, "Active mask")
            assertEquals(3, Integer.bitCount(parsed.activeCellMask), "3 cells balancing")
        }
    }
}
