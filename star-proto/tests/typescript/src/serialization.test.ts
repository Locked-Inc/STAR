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
    describe('VelocityCommand - 4 Motors', () => {
        it('should round-trip serialize correctly with 4 motors', () => {
            // Create command with 4 independent motors (MKS units)
            const original: VelocityCommand = {
                motor0VelocityMps: 1.5,
                motor1VelocityMps: 1.5,
                motor2VelocityMps: 1.0,
                motor3VelocityMps: 1.0,
                sequence: 42,
                timestampUs: BigInt(Date.now() * 1000),
            };

            // Serialize to binary
            const bytes = VelocityCommand.toBinary(original);

            // Verify compact encoding
            expect(bytes.length).toBeLessThan(100);

            // Deserialize
            const deserialized = VelocityCommand.fromBinary(bytes);

            // Verify all 4 motor fields
            expect(deserialized.motor0VelocityMps).toBeCloseTo(original.motor0VelocityMps, 3);
            expect(deserialized.motor1VelocityMps).toBeCloseTo(original.motor1VelocityMps, 3);
            expect(deserialized.motor2VelocityMps).toBeCloseTo(original.motor2VelocityMps, 3);
            expect(deserialized.motor3VelocityMps).toBeCloseTo(original.motor3VelocityMps, 3);
            expect(deserialized.sequence).toBe(original.sequence);
        });

        it('should support differential drive mode', () => {
            const leftVel = 2.0;
            const rightVel = 1.5;

            const command: VelocityCommand = {
                motor0VelocityMps: leftVel,  // Left front
                motor1VelocityMps: leftVel,  // Left rear
                motor2VelocityMps: rightVel, // Right front
                motor3VelocityMps: rightVel, // Right rear
                sequence: 1,
                timestampUs: BigInt(0),
            };

            const json = VelocityCommand.toJsonString(command);
            const parsed = VelocityCommand.fromJsonString(json);

            // Verify left motors match
            expect(parsed.motor0VelocityMps).toBeCloseTo(leftVel, 3);
            expect(parsed.motor1VelocityMps).toBeCloseTo(leftVel, 3);

            // Verify right motors match
            expect(parsed.motor2VelocityMps).toBeCloseTo(rightVel, 3);
            expect(parsed.motor3VelocityMps).toBeCloseTo(rightVel, 3);
        });
    });

    describe('EncoderData Streaming - 4 Motors', () => {
        it('should handle streaming message serialization for all 4 motors', () => {
            const messages: EncoderData[] = [];

            // Create multiple encoder data messages (test all 4 motors)
            for (let i = 0; i < 12; i++) {
                messages.push({
                    motorId: i % 4,  // Motor IDs 0, 1, 2, 3
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
                expect(data.motorId).toBe(i % 4);
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
                    // Differential drive mode: all motors at 1.0 m/s (straight forward)
                    motor0VelocityMps: 1.0,  // Left front
                    motor1VelocityMps: 1.0,  // Left rear
                    motor2VelocityMps: 1.0,  // Right front
                    motor3VelocityMps: 1.0,  // Right rear
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
