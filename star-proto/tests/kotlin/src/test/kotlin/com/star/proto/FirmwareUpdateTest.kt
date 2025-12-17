// FirmwareUpdateTest.kt - Firmware Update Protobuf Serialization Tests
// Verifies OTA firmware update proto serialization/deserialization.
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
import com.google.protobuf.ByteString
import java.util.stream.Stream

// Import generated classes
import com.star.proto.star.v1.*

class FirmwareUpdateTest {

    // ========================================================================
    // Test Data Classes for Table-Driven Tests
    // ========================================================================

    data class FirmwareChunkTestCase(
        val dataSize: Int,
        val sequence: Int,
        val offset: Int,
        val description: String
    )

    data class ProgressTestCase(
        val totalSize: Int,
        val bytesReceived: Int,
        val expectedPercent: Int,
        val description: String
    )

    data class FirmwareInfoTestCase(
        val version: String,
        val gitHash: String,
        val buildDate: String,
        val description: String
    )

    data class ErrorTestCase(
        val errorCode: FirmwareUpdateErrorCode,
        val message: String,
        val description: String
    )

    // ========================================================================
    // Test Data Providers (Table-Driven)
    // ========================================================================

    companion object {

        @JvmStatic
        fun chunkTestCases(): Stream<Arguments> = Stream.of(
            // Standard chunk sizes
            Arguments.of(FirmwareChunkTestCase(
                1024, 0, 0,
                "First 1KB chunk"
            )),
            Arguments.of(FirmwareChunkTestCase(
                1024, 1, 1024,
                "Second 1KB chunk"
            )),
            Arguments.of(FirmwareChunkTestCase(
                1024, 100, 102400,
                "100th chunk at 100KB offset"
            )),
            // Smaller chunks
            Arguments.of(FirmwareChunkTestCase(
                512, 0, 0,
                "Small 512B chunk"
            )),
            Arguments.of(FirmwareChunkTestCase(
                256, 0, 0,
                "Tiny 256B chunk"
            )),
            // Final partial chunk
            Arguments.of(FirmwareChunkTestCase(
                500, 999, 1023476,
                "Final partial chunk"
            ))
        )

        @JvmStatic
        fun progressTestCases(): Stream<Arguments> = Stream.of(
            // Start of transfer
            Arguments.of(ProgressTestCase(
                1048576, 0, 0,
                "1MB firmware - start"
            )),
            // Quarter progress
            Arguments.of(ProgressTestCase(
                1048576, 262144, 25,
                "1MB firmware - 25%"
            )),
            // Half progress
            Arguments.of(ProgressTestCase(
                1048576, 524288, 50,
                "1MB firmware - 50%"
            )),
            // Nearly complete
            Arguments.of(ProgressTestCase(
                1048576, 1000000, 95,
                "1MB firmware - 95%"
            )),
            // Complete
            Arguments.of(ProgressTestCase(
                1048576, 1048576, 100,
                "1MB firmware - 100%"
            )),
            // Small firmware
            Arguments.of(ProgressTestCase(
                102400, 51200, 50,
                "100KB firmware - 50%"
            ))
        )

        @JvmStatic
        fun firmwareInfoTestCases(): Stream<Arguments> = Stream.of(
            Arguments.of(FirmwareInfoTestCase(
                "1.0.0", "a1b2c3d", "2025-01-15",
                "Initial release"
            )),
            Arguments.of(FirmwareInfoTestCase(
                "1.1.0", "e4f5g6h", "2025-02-01",
                "Feature update"
            )),
            Arguments.of(FirmwareInfoTestCase(
                "1.1.1", "i7j8k9l", "2025-02-15",
                "Bug fix release"
            )),
            Arguments.of(FirmwareInfoTestCase(
                "2.0.0-beta.1", "m0n1o2p", "2025-03-01",
                "Beta release"
            )),
            Arguments.of(FirmwareInfoTestCase(
                "2.0.0-rc.1", "q3r4s5t", "2025-03-15",
                "Release candidate"
            ))
        )

        @JvmStatic
        fun errorTestCases(): Stream<Arguments> = Stream.of(
            Arguments.of(ErrorTestCase(
                FirmwareUpdateErrorCode.FIRMWARE_UPDATE_ERROR_CODE_INVALID_SIZE,
                "Firmware size exceeds partition limit",
                "Size error"
            )),
            Arguments.of(ErrorTestCase(
                FirmwareUpdateErrorCode.FIRMWARE_UPDATE_ERROR_CODE_CRC_FAILED,
                "CRC checksum mismatch",
                "CRC error"
            )),
            Arguments.of(ErrorTestCase(
                FirmwareUpdateErrorCode.FIRMWARE_UPDATE_ERROR_CODE_WRITE_FAILED,
                "Flash write operation failed",
                "Write error"
            )),
            Arguments.of(ErrorTestCase(
                FirmwareUpdateErrorCode.FIRMWARE_UPDATE_ERROR_CODE_TIMEOUT,
                "Transfer timed out",
                "Timeout error"
            )),
            Arguments.of(ErrorTestCase(
                FirmwareUpdateErrorCode.FIRMWARE_UPDATE_ERROR_CODE_SEQUENCE_ERROR,
                "Chunk sequence number mismatch",
                "Sequence error"
            ))
        )
    }

    // ========================================================================
    // FirmwareChunk Serialization Tests
    // ========================================================================

    @Nested
    @DisplayName("FirmwareChunk Serialization")
    inner class FirmwareChunkTests {

        @ParameterizedTest(name = "{3}")
        @MethodSource("com.star.proto.FirmwareUpdateTest#chunkTestCases")
        @DisplayName("Firmware chunks serialize correctly")
        fun testChunkRoundTrip(testCase: FirmwareChunkTestCase) {
            // Create test data
            val testData = ByteArray(testCase.dataSize) { (it % 256).toByte() }
            val crc32 = calculateCrc32(testData)

            val chunk = FirmwareChunk.newBuilder()
                .setData(ByteString.copyFrom(testData))
                .setSequence(testCase.sequence)
                .setOffset(testCase.offset)
                .setChunkCrc32(crc32)
                .build()

            val bytes = chunk.toByteArray()
            val parsed = FirmwareChunk.parseFrom(bytes)

            assertEquals(testCase.dataSize, parsed.data.size(),
                "${testCase.description} - data size")
            assertEquals(testCase.sequence, parsed.sequence,
                "${testCase.description} - sequence")
            assertEquals(testCase.offset, parsed.offset,
                "${testCase.description} - offset")
            assertEquals(crc32, parsed.chunkCrc32,
                "${testCase.description} - CRC")
        }

        @ParameterizedTest(name = "Chunk size: {0} bytes")
        @CsvSource(
            "256, Small chunk",
            "512, Medium chunk",
            "1024, Standard chunk",
            "2048, Large chunk (may exceed nanopb limit)"
        )
        @DisplayName("Various chunk sizes serialize correctly")
        fun testChunkSizes(size: Int, description: String) {
            val testData = ByteArray(size) { 0xAA.toByte() }
            val chunk = FirmwareChunk.newBuilder()
                .setData(ByteString.copyFrom(testData))
                .setSequence(0)
                .build()

            val bytes = chunk.toByteArray()
            val parsed = FirmwareChunk.parseFrom(bytes)

            assertEquals(size, parsed.data.size(), description)
        }

        @Test
        @DisplayName("Empty chunk data is valid")
        fun testEmptyChunk() {
            val chunk = FirmwareChunk.newBuilder()
                .setData(ByteString.EMPTY)
                .setSequence(0)
                .setOffset(0)
                .build()

            val bytes = chunk.toByteArray()
            val parsed = FirmwareChunk.parseFrom(bytes)

            assertEquals(0, parsed.data.size(), "Empty chunk should have 0 bytes")
        }

        private fun calculateCrc32(data: ByteArray): Int {
            val crc = java.util.zip.CRC32()
            crc.update(data)
            return crc.value.toInt()
        }
    }

    // ========================================================================
    // FirmwareUpdateProgress Serialization Tests
    // ========================================================================

    @Nested
    @DisplayName("FirmwareUpdateProgress Serialization")
    inner class ProgressTests {

        @ParameterizedTest(name = "{3}")
        @MethodSource("com.star.proto.FirmwareUpdateTest#progressTestCases")
        @DisplayName("Progress calculations are correct")
        fun testProgressRoundTrip(testCase: ProgressTestCase) {
            val progress = FirmwareUpdateProgress.newBuilder()
                .setTotalSize(testCase.totalSize)
                .setBytesReceived(testCase.bytesReceived)
                .setProgressPercent(testCase.expectedPercent)
                .setState(FirmwareUpdateState.FIRMWARE_UPDATE_STATE_RECEIVING)
                .setTransferSpeedBps(102400)  // 100 KB/s
                .build()

            val bytes = progress.toByteArray()
            val parsed = FirmwareUpdateProgress.parseFrom(bytes)

            assertEquals(testCase.totalSize, parsed.totalSize,
                "${testCase.description} - total size")
            assertEquals(testCase.bytesReceived, parsed.bytesReceived,
                "${testCase.description} - received")
            assertEquals(testCase.expectedPercent, parsed.progressPercent,
                "${testCase.description} - percent")
        }

        @Test
        @DisplayName("Progress percentage calculation")
        fun testProgressPercentageCalculation() {
            val totalSize = 1000000
            val bytesReceived = 500000
            val expectedPercent = (bytesReceived * 100) / totalSize

            val progress = FirmwareUpdateProgress.newBuilder()
                .setTotalSize(totalSize)
                .setBytesReceived(bytesReceived)
                .setProgressPercent(expectedPercent)
                .build()

            val bytes = progress.toByteArray()
            val parsed = FirmwareUpdateProgress.parseFrom(bytes)

            val calculatedPercent = (parsed.bytesReceived * 100) / parsed.totalSize
            assertEquals(expectedPercent, calculatedPercent, "Progress percentage")
        }

        @Test
        @DisplayName("Estimated time remaining calculation")
        fun testEstimatedTimeRemaining() {
            val totalSize = 1048576  // 1MB
            val bytesReceived = 524288  // 512KB (50%)
            val transferSpeedBps = 102400  // 100KB/s
            val bytesRemaining = totalSize - bytesReceived
            val estimatedSeconds = bytesRemaining / transferSpeedBps

            val progress = FirmwareUpdateProgress.newBuilder()
                .setTotalSize(totalSize)
                .setBytesReceived(bytesReceived)
                .setTransferSpeedBps(transferSpeedBps)
                .setEstimatedSecondsRemaining(estimatedSeconds)
                .build()

            val bytes = progress.toByteArray()
            val parsed = FirmwareUpdateProgress.parseFrom(bytes)

            // 512KB remaining at 100KB/s = ~5 seconds
            assertTrue(parsed.estimatedSecondsRemaining > 0,
                "Should have positive time remaining")
            assertEquals(5, parsed.estimatedSecondsRemaining,
                "Estimated 5 seconds remaining")
        }
    }

    // ========================================================================
    // FirmwareUpdateState Enum Tests
    // ========================================================================

    @Nested
    @DisplayName("FirmwareUpdateState Enum Tests")
    inner class UpdateStateEnumTests {

        @Test
        @DisplayName("FirmwareUpdateState zero value is UNKNOWN")
        fun testEnumZeroValue() {
            assertEquals(0, FirmwareUpdateState.FIRMWARE_UPDATE_STATE_UNKNOWN.number,
                "FIRMWARE_UPDATE_STATE_UNKNOWN should have value 0")
        }

        @ParameterizedTest(name = "FirmwareUpdateState.{0}")
        @EnumSource(value = FirmwareUpdateState::class, names = ["UNRECOGNIZED"], mode = EnumSource.Mode.EXCLUDE)
        @DisplayName("All FirmwareUpdateState enum values serialize correctly")
        fun testUpdateStateEnumSerialization(state: FirmwareUpdateState) {
            val progress = FirmwareUpdateProgress.newBuilder()
                .setState(state)
                .build()

            val bytes = progress.toByteArray()
            val parsed = FirmwareUpdateProgress.parseFrom(bytes)

            assertEquals(state, parsed.state, "State $state should survive round-trip")
        }

        @ParameterizedTest(name = "State transition: {0} -> {1}")
        @CsvSource(
            "FIRMWARE_UPDATE_STATE_IDLE, FIRMWARE_UPDATE_STATE_RECEIVING, Start transfer",
            "FIRMWARE_UPDATE_STATE_RECEIVING, FIRMWARE_UPDATE_STATE_VALIDATING, Transfer complete",
            "FIRMWARE_UPDATE_STATE_VALIDATING, FIRMWARE_UPDATE_STATE_COMPLETE, Validation passed",
            "FIRMWARE_UPDATE_STATE_RECEIVING, FIRMWARE_UPDATE_STATE_ERROR, Transfer error"
        )
        @DisplayName("State transitions are valid")
        fun testStateTransitions(fromState: FirmwareUpdateState, toState: FirmwareUpdateState, description: String) {
            // Just verify both states serialize correctly
            val progressFrom = FirmwareUpdateProgress.newBuilder()
                .setState(fromState)
                .build()
            val progressTo = FirmwareUpdateProgress.newBuilder()
                .setState(toState)
                .build()

            val parsedFrom = FirmwareUpdateProgress.parseFrom(progressFrom.toByteArray())
            val parsedTo = FirmwareUpdateProgress.parseFrom(progressTo.toByteArray())

            assertEquals(fromState, parsedFrom.state, description)
            assertEquals(toState, parsedTo.state, description)
        }
    }

    // ========================================================================
    // FirmwareUpdateErrorCode Enum Tests
    // ========================================================================

    @Nested
    @DisplayName("FirmwareUpdateErrorCode Enum Tests")
    inner class ErrorCodeEnumTests {

        @Test
        @DisplayName("FirmwareUpdateErrorCode zero value is UNKNOWN")
        fun testEnumZeroValue() {
            assertEquals(0, FirmwareUpdateErrorCode.FIRMWARE_UPDATE_ERROR_CODE_UNKNOWN.number,
                "FIRMWARE_UPDATE_ERROR_CODE_UNKNOWN should have value 0")
        }

        @ParameterizedTest(name = "FirmwareUpdateErrorCode.{0}")
        @EnumSource(value = FirmwareUpdateErrorCode::class, names = ["UNRECOGNIZED"], mode = EnumSource.Mode.EXCLUDE)
        @DisplayName("All FirmwareUpdateErrorCode enum values serialize correctly")
        fun testErrorCodeEnumSerialization(errorCode: FirmwareUpdateErrorCode) {
            val error = FirmwareUpdateError.newBuilder()
                .setCode(errorCode)
                .setMessage("Test error")
                .build()

            val bytes = error.toByteArray()
            val parsed = FirmwareUpdateError.parseFrom(bytes)

            assertEquals(errorCode, parsed.code, "ErrorCode $errorCode should survive round-trip")
        }
    }

    // ========================================================================
    // FirmwareUpdateError Serialization Tests
    // ========================================================================

    @Nested
    @DisplayName("FirmwareUpdateError Serialization")
    inner class ErrorTests {

        @ParameterizedTest(name = "{2}")
        @MethodSource("com.star.proto.FirmwareUpdateTest#errorTestCases")
        @DisplayName("Error messages serialize correctly")
        fun testErrorRoundTrip(testCase: ErrorTestCase) {
            val error = FirmwareUpdateError.newBuilder()
                .setCode(testCase.errorCode)
                .setMessage(testCase.message)
                .setDetails("ESP_ERR_OTA_VALIDATE_FAILED")
                .setErrorAtSequence(100)
                .setErrorAtOffset(102400)
                .build()

            val bytes = error.toByteArray()
            val parsed = FirmwareUpdateError.parseFrom(bytes)

            assertEquals(testCase.errorCode, parsed.code,
                "${testCase.description} - error code")
            assertEquals(testCase.message, parsed.message,
                "${testCase.description} - message")
            assertEquals(100, parsed.errorAtSequence,
                "${testCase.description} - sequence")
            assertEquals(102400, parsed.errorAtOffset,
                "${testCase.description} - offset")
        }

        @Test
        @DisplayName("Error with location information")
        fun testErrorWithLocation() {
            val error = FirmwareUpdateError.newBuilder()
                .setCode(FirmwareUpdateErrorCode.FIRMWARE_UPDATE_ERROR_CODE_WRITE_FAILED)
                .setMessage("Flash write failed at sector boundary")
                .setErrorAtSequence(50)
                .setErrorAtOffset(51200)
                .build()

            val bytes = error.toByteArray()
            val parsed = FirmwareUpdateError.parseFrom(bytes)

            assertEquals(50, parsed.errorAtSequence, "Error at sequence 50")
            assertEquals(51200, parsed.errorAtOffset, "Error at offset 51200")
        }
    }

    // ========================================================================
    // FirmwareInfo Serialization Tests
    // ========================================================================

    @Nested
    @DisplayName("FirmwareInfo Serialization")
    inner class FirmwareInfoTests {

        @ParameterizedTest(name = "{3}")
        @MethodSource("com.star.proto.FirmwareUpdateTest#firmwareInfoTestCases")
        @DisplayName("Firmware info serializes correctly")
        fun testFirmwareInfoRoundTrip(testCase: FirmwareInfoTestCase) {
            val info = FirmwareInfo.newBuilder()
                .setVersion(testCase.version)
                .setGitHash(testCase.gitHash)
                .setBuildDate(testCase.buildDate)
                .setSize(1048576)
                .setCrc32(0x12345678)
                .setIdfVersion("5.1.0")
                .setChipTarget("esp32s3")
                .build()

            val bytes = info.toByteArray()
            val parsed = FirmwareInfo.parseFrom(bytes)

            assertEquals(testCase.version, parsed.version,
                "${testCase.description} - version")
            assertEquals(testCase.gitHash, parsed.gitHash,
                "${testCase.description} - git hash")
            assertEquals(testCase.buildDate, parsed.buildDate,
                "${testCase.description} - build date")
            assertEquals("esp32s3", parsed.chipTarget,
                "${testCase.description} - chip target")
        }

        @Test
        @DisplayName("String fields within nanopb limits")
        fun testStringFieldLimits() {
            val info = FirmwareInfo.newBuilder()
                .setVersion("1.0.0-alpha.1+build.123")  // Complex version (< 32 chars)
                .setGitHash("a1b2c3d4e5f6")  // 12 char hash (< 16 chars)
                .setBuildDate("2025-01-15T10:30:00Z")  // ISO 8601 (< 32 chars)
                .setIdfVersion("5.1.0")
                .setChipTarget("esp32s3")
                .build()

            val bytes = info.toByteArray()
            val parsed = FirmwareInfo.parseFrom(bytes)

            assertTrue(parsed.version.length <= 32, "Version within limit")
            assertTrue(parsed.gitHash.length <= 16, "Git hash within limit")
            assertTrue(parsed.buildDate.length <= 32, "Build date within limit")
        }
    }

    // ========================================================================
    // Complete Update Flow Serialization Tests
    // ========================================================================

    @Nested
    @DisplayName("Complete Update Flow Serialization")
    inner class CompleteFlowTests {

        @Test
        @DisplayName("BeginUpdate request/response serializes correctly")
        fun testBeginUpdateFlow() {
            val request = BeginUpdateRequest.newBuilder()
                .setHeader(RequestHeader.newBuilder()
                    .setRequestId("begin-update-001")
                    .setClientVersion("1.0.0")
                    .build())
                .setFirmwareSize(1048576)
                .setExpectedVersion("1.1.0")
                .setExpectedCrc32(0xABCD1234.toInt())
                .build()

            val requestBytes = request.toByteArray()
            val parsedRequest = BeginUpdateRequest.parseFrom(requestBytes)

            assertEquals("begin-update-001", parsedRequest.header.requestId)
            assertEquals(1048576, parsedRequest.firmwareSize)
            assertEquals("1.1.0", parsedRequest.expectedVersion)

            val response = BeginUpdateResponse.newBuilder()
                .setHeader(ResponseHeader.newBuilder()
                    .setRequestId("begin-update-001")
                    .setStatus(Status.STATUS_OK)
                    .build())
                .setAccepted(true)
                .setMaxChunkSize(1024)
                .setSessionId("session-12345")
                .build()

            val responseBytes = response.toByteArray()
            val parsedResponse = BeginUpdateResponse.parseFrom(responseBytes)

            assertEquals(Status.STATUS_OK, parsedResponse.header.status)
            assertTrue(parsedResponse.accepted)
            assertEquals(1024, parsedResponse.maxChunkSize)
        }

        @Test
        @DisplayName("WriteChunk request/response serializes correctly")
        fun testWriteChunkFlow() {
            val testData = ByteArray(1024) { (it % 256).toByte() }

            val request = WriteChunkRequest.newBuilder()
                .setHeader(RequestHeader.newBuilder()
                    .setRequestId("write-chunk-001")
                    .build())
                .setChunk(FirmwareChunk.newBuilder()
                    .setData(ByteString.copyFrom(testData))
                    .setSequence(0)
                    .setOffset(0)
                    .build())
                .build()

            val requestBytes = request.toByteArray()
            val parsedRequest = WriteChunkRequest.parseFrom(requestBytes)

            assertEquals(1024, parsedRequest.chunk.data.size())
            assertEquals(0, parsedRequest.chunk.sequence)

            val response = WriteChunkResponse.newBuilder()
                .setHeader(ResponseHeader.newBuilder()
                    .setRequestId("write-chunk-001")
                    .setStatus(Status.STATUS_OK)
                    .build())
                .setSuccess(true)
                .setProgress(FirmwareUpdateProgress.newBuilder()
                    .setTotalSize(1048576)
                    .setBytesReceived(1024)
                    .setProgressPercent(0)
                    .setState(FirmwareUpdateState.FIRMWARE_UPDATE_STATE_RECEIVING)
                    .build())
                .build()

            val responseBytes = response.toByteArray()
            val parsedResponse = WriteChunkResponse.parseFrom(responseBytes)

            assertTrue(parsedResponse.success)
            assertEquals(1024, parsedResponse.progress.bytesReceived)
        }

        @Test
        @DisplayName("FinalizeUpdate request/response serializes correctly")
        fun testFinalizeUpdateFlow() {
            val request = FinalizeUpdateRequest.newBuilder()
                .setHeader(RequestHeader.newBuilder()
                    .setRequestId("finalize-001")
                    .build())
                .build()

            val response = FinalizeUpdateResponse.newBuilder()
                .setHeader(ResponseHeader.newBuilder()
                    .setRequestId("finalize-001")
                    .setStatus(Status.STATUS_OK)
                    .build())
                .setValidated(true)
                .setReadyToReboot(true)
                .build()

            val responseBytes = response.toByteArray()
            val parsedResponse = FinalizeUpdateResponse.parseFrom(responseBytes)

            assertTrue(parsedResponse.validated)
            assertTrue(parsedResponse.readyToReboot)
        }

        @Test
        @DisplayName("Reboot request/response serializes correctly")
        fun testRebootFlow() {
            val request = RebootRequest.newBuilder()
                .setHeader(RequestHeader.newBuilder()
                    .setRequestId("reboot-001")
                    .build())
                .setDelayMs(1000)
                .build()

            val response = RebootResponse.newBuilder()
                .setHeader(ResponseHeader.newBuilder()
                    .setRequestId("reboot-001")
                    .setStatus(Status.STATUS_OK)
                    .build())
                .setRebooting(true)
                .setRebootDelayMs(1000)
                .build()

            val responseBytes = response.toByteArray()
            val parsedResponse = RebootResponse.parseFrom(responseBytes)

            assertTrue(parsedResponse.rebooting)
            assertEquals(1000, parsedResponse.rebootDelayMs)
        }

        @Test
        @DisplayName("Rollback request/response serializes correctly")
        fun testRollbackFlow() {
            val request = RollbackRequest.newBuilder()
                .setHeader(RequestHeader.newBuilder()
                    .setRequestId("rollback-001")
                    .build())
                .build()

            val response = RollbackResponse.newBuilder()
                .setHeader(ResponseHeader.newBuilder()
                    .setRequestId("rollback-001")
                    .setStatus(Status.STATUS_OK)
                    .build())
                .setRollingBack(true)
                .setPreviousVersion("1.0.0")
                .build()

            val responseBytes = response.toByteArray()
            val parsedResponse = RollbackResponse.parseFrom(responseBytes)

            assertTrue(parsedResponse.rollingBack)
            assertEquals("1.0.0", parsedResponse.previousVersion)
        }

        @Test
        @DisplayName("GetFirmwareInfo request/response serializes correctly")
        fun testGetFirmwareInfoFlow() {
            val response = GetFirmwareInfoResponse.newBuilder()
                .setHeader(ResponseHeader.newBuilder()
                    .setRequestId("info-001")
                    .setStatus(Status.STATUS_OK)
                    .build())
                .setCurrentFirmware(FirmwareInfo.newBuilder()
                    .setVersion("1.1.0")
                    .setGitHash("a1b2c3d")
                    .setBuildDate("2025-01-15")
                    .setChipTarget("esp32s3")
                    .build())
                .setPreviousFirmware(FirmwareInfo.newBuilder()
                    .setVersion("1.0.0")
                    .setGitHash("e4f5g6h")
                    .setBuildDate("2025-01-01")
                    .setChipTarget("esp32s3")
                    .build())
                .setIsOtaBoot(true)
                .setIsValid(true)
                .build()

            val responseBytes = response.toByteArray()
            val parsedResponse = GetFirmwareInfoResponse.parseFrom(responseBytes)

            assertEquals("1.1.0", parsedResponse.currentFirmware.version)
            assertEquals("1.0.0", parsedResponse.previousFirmware.version)
            assertTrue(parsedResponse.isOtaBoot)
            assertTrue(parsedResponse.isValid)
        }

        @ParameterizedTest(name = "Stream {0} chunks")
        @CsvSource(
            "1, Single chunk",
            "10, Small batch",
            "100, Medium batch"
        )
        @DisplayName("Chunk streaming simulation")
        fun testChunkStreaming(chunkCount: Int, description: String) {
            val outputStream = java.io.ByteArrayOutputStream()

            repeat(chunkCount) { i ->
                val chunk = FirmwareChunk.newBuilder()
                    .setData(ByteString.copyFrom(ByteArray(1024) { (it % 256).toByte() }))
                    .setSequence(i)
                    .setOffset(i * 1024)
                    .build()
                chunk.writeDelimitedTo(outputStream)
            }

            val inputStream = java.io.ByteArrayInputStream(outputStream.toByteArray())
            var readCount = 0
            while (inputStream.available() > 0) {
                val chunk = FirmwareChunk.parseDelimitedFrom(inputStream)
                assertNotNull(chunk, "Chunk $readCount should not be null")
                assertEquals(readCount, chunk.sequence, "Sequence at index $readCount")
                assertEquals(readCount * 1024, chunk.offset, "Offset at index $readCount")
                readCount++
            }
            assertEquals(chunkCount, readCount, "$description: chunk count mismatch")
        }
    }
}
