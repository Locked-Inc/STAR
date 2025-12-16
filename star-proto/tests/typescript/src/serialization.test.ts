// serialization.test.ts - TypeScript Protobuf Serialization Tests
// Verifies that generated TypeScript code serializes/deserializes correctly.
//
// STAR Project - Texas A&M University
// December 2025

import { describe, it, expect } from 'vitest';

// Import generated classes (will be available after buf generate)
// import { VelocityCommand } from '@gen/star/v1/motor_control';
// import { TelemetryData, ImuData } from '@gen/star/v1/telemetry';
// import { Status, ConnectionStatus, RobotMode } from '@gen/star/v1/common';

describe('Protobuf Serialization Tests', () => {

    describe('Test Framework', () => {
        it('should have working test framework', () => {
            // Placeholder test - verifies Vitest is operational
            expect(true).toBe(true);
        });
    });

    // TODO: Uncomment after buf generate produces TypeScript code
    /*
    describe('VelocityCommand', () => {
        it('should round-trip serialize correctly', () => {
            // Create command with MKS units
            const original: VelocityCommand = {
                leftVelocityMps: 1.5,
                rightVelocityMps: -0.5,
                sequence: 42,
                timestampUs: BigInt(Date.now() * 1000),
            };

            // Serialize to binary
            const bytes = VelocityCommand.toBinary(original);

            // Verify compact encoding
            expect(bytes.length).toBeLessThan(100);

            // Deserialize
            const deserialized = VelocityCommand.fromBinary(bytes);

            // Verify fields
            expect(deserialized.leftVelocityMps).toBeCloseTo(original.leftVelocityMps, 3);
            expect(deserialized.rightVelocityMps).toBeCloseTo(original.rightVelocityMps, 3);
            expect(deserialized.sequence).toBe(original.sequence);
        });

        it('should serialize to JSON correctly', () => {
            const command: VelocityCommand = {
                leftVelocityMps: 2.0,
                rightVelocityMps: 2.0,
                sequence: 1,
                timestampUs: BigInt(0),
            };

            const json = VelocityCommand.toJsonString(command);
            const parsed = VelocityCommand.fromJsonString(json);

            expect(parsed.leftVelocityMps).toBeCloseTo(2.0, 3);
        });
    });

    describe('EncoderData Streaming', () => {
        it('should handle streaming message serialization', () => {
            const messages: EncoderData[] = [];

            // Create multiple encoder data messages
            for (let i = 0; i < 10; i++) {
                messages.push({
                    motorId: i % 2,
                    ticks: BigInt(i * 1000),
                    velocityMps: i * 0.1,
                    timestampUs: BigInt(Date.now() * 1000),
                });
            }

            // Serialize each message
            const serialized = messages.map(m => EncoderData.toBinary(m));

            // Deserialize and verify
            serialized.forEach((bytes, i) => {
                const data = EncoderData.fromBinary(bytes);
                expect(data.motorId).toBe(i % 2);
                expect(Number(data.ticks)).toBe(i * 1000);
            });
        });
    });

    describe('Enum Values', () => {
        it('should have UNKNOWN as zero value (Boston Dynamics style)', () => {
            expect(Status.STATUS_UNKNOWN).toBe(0);
            expect(ConnectionStatus.CONNECTION_STATUS_UNKNOWN).toBe(0);
            expect(RobotMode.ROBOT_MODE_UNKNOWN).toBe(0);
        });

        it('should preserve enum values through serialization', () => {
            const response = {
                requestId: 'enum-test',
                status: Status.STATUS_OK,
                errorMessage: '',
                latencyUs: BigInt(0),
            };

            const bytes = ResponseHeader.toBinary(response);
            const parsed = ResponseHeader.fromBinary(bytes);

            expect(parsed.status).toBe(Status.STATUS_OK);
        });
    });

    describe('Unit Conventions', () => {
        it('should use MKS units for all measurements', () => {
            const telemetry: TelemetryData = {
                imu: {
                    pitchRad: Math.PI / 4,       // radians
                    rollRad: 0,
                    yawRad: Math.PI,
                    accelXMps2: 0,
                    accelYMps2: 0,
                    accelZMps2: 9.81,           // m/s^2
                    gyroXRadPerS: 0.1,          // rad/s
                    gyroYRadPerS: 0,
                    gyroZRadPerS: 0,
                },
                batteryPercent: 75.0,
                temperatureCelsius: 35.0,
            };

            const bytes = TelemetryData.toBinary(telemetry);
            const parsed = TelemetryData.fromBinary(bytes);

            // Verify MKS units preserved
            expect(parsed.imu?.accelZMps2).toBeCloseTo(9.81, 2);
            expect(parsed.imu?.pitchRad).toBeCloseTo(Math.PI / 4, 4);
        });
    });

    describe('RequestHeader/ResponseHeader Pattern', () => {
        it('should support header pattern for all requests', () => {
            const requestId = `test-${Date.now()}`;

            const request: SetVelocityRequest = {
                header: {
                    requestId: requestId,
                    clientVersion: '1.0.0',
                },
                command: {
                    leftVelocityMps: 1.0,
                    rightVelocityMps: 1.0,
                    sequence: 1,
                    timestampUs: BigInt(Date.now() * 1000),
                },
            };

            const bytes = SetVelocityRequest.toBinary(request);
            const parsed = SetVelocityRequest.fromBinary(bytes);

            expect(parsed.header?.requestId).toBe(requestId);
        });
    });
    */
});
