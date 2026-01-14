// motor_control.test.ts - TypeScript Motor Control Tests
// Verifies 4-motor control schema serialization and validation.
//
// STAR Project - Texas A&M University
// January 2026

import { describe, it, expect } from 'vitest';

// Import generated classes (will be available after buf generate)
// import { VelocityCommand, EncoderData, MotorStatus, MotorState } from '@gen/star/v1/motor_control';
// import { SetVelocityRequest, SetVelocityResponse } from '@gen/star/v1/motor_control';
// import { RequestHeader, ResponseHeader, Status } from '@gen/star/v1/common';

describe('Motor Control Tests', () => {

    describe('Test Framework', () => {
        it('should have working test framework', () => {
            // Placeholder test - verifies Vitest is operational
            expect(true).toBe(true);
        });
    });

    // TODO: Uncomment after buf generate produces TypeScript code
    /*
    describe('VelocityCommand - 4 Motors', () => {
        it('should support 4 independent motors', () => {
            const cmd: VelocityCommand = {
                motor0VelocityMps: 1.5,
                motor1VelocityMps: 1.5,
                motor2VelocityMps: 1.0,
                motor3VelocityMps: 1.0,
                sequence: 42,
                timestampUs: BigInt(1000000),
            };

            // Serialize
            const bytes = VelocityCommand.toBinary(cmd);

            // Verify compact encoding (4 motors should still be small)
            expect(bytes.length).toBeLessThan(100);

            // Deserialize
            const decoded = VelocityCommand.fromBinary(bytes);

            // Verify all 4 motor fields
            expect(decoded.motor0VelocityMps).toBeCloseTo(1.5, 3);
            expect(decoded.motor1VelocityMps).toBeCloseTo(1.5, 3);
            expect(decoded.motor2VelocityMps).toBeCloseTo(1.0, 3);
            expect(decoded.motor3VelocityMps).toBeCloseTo(1.0, 3);
            expect(decoded.sequence).toBe(42);
        });

        it('should support differential drive mode', () => {
            const leftVel = 1.5;
            const rightVel = 1.0;

            const cmd: VelocityCommand = {
                motor0VelocityMps: leftVel,  // Left front
                motor1VelocityMps: leftVel,  // Left rear
                motor2VelocityMps: rightVel, // Right front
                motor3VelocityMps: rightVel, // Right rear
                sequence: 1,
                timestampUs: BigInt(0),
            };

            const bytes = VelocityCommand.toBinary(cmd);
            const decoded = VelocityCommand.fromBinary(bytes);

            // Verify left motors match (0 & 1)
            expect(decoded.motor0VelocityMps).toBeCloseTo(leftVel, 3);
            expect(decoded.motor1VelocityMps).toBeCloseTo(leftVel, 3);

            // Verify right motors match (2 & 3)
            expect(decoded.motor2VelocityMps).toBeCloseTo(rightVel, 3);
            expect(decoded.motor3VelocityMps).toBeCloseTo(rightVel, 3);
        });

        it('should validate velocity range (±2.0 m/s)', () => {
            const testCases = [
                { name: 'zero', vel: 0.0, valid: true },
                { name: 'max forward', vel: 2.0, valid: true },
                { name: 'max reverse', vel: -2.0, valid: true },
                { name: 'within range', vel: 1.5, valid: true },
                { name: 'exceeds max', vel: 2.5, valid: false },
                { name: 'exceeds min', vel: -2.5, valid: false },
            ];

            testCases.forEach(tc => {
                const cmd: VelocityCommand = {
                    motor0VelocityMps: tc.vel,
                    motor1VelocityMps: tc.vel,
                    motor2VelocityMps: tc.vel,
                    motor3VelocityMps: tc.vel,
                    sequence: 0,
                    timestampUs: BigInt(0),
                };

                const bytes = VelocityCommand.toBinary(cmd);
                const decoded = VelocityCommand.fromBinary(bytes);

                // Protocol buffers will serialize any value, but application
                // should validate and clamp to ±2.0 m/s range
                if (tc.valid) {
                    expect(decoded.motor0VelocityMps).toBeCloseTo(tc.vel, 3);
                }
            });
        });

        it('should serialize to JSON correctly', () => {
            const cmd: VelocityCommand = {
                motor0VelocityMps: 1.5,
                motor1VelocityMps: 1.5,
                motor2VelocityMps: 1.0,
                motor3VelocityMps: 1.0,
                sequence: 42,
                timestampUs: BigInt(1000000),
            };

            const json = VelocityCommand.toJsonString(cmd);
            const parsed = VelocityCommand.fromJsonString(json);

            expect(parsed.motor0VelocityMps).toBeCloseTo(1.5, 3);
            expect(parsed.motor1VelocityMps).toBeCloseTo(1.5, 3);
        });
    });

    describe('EncoderData - 4 Motors', () => {
        it('should support motor IDs 0-3', () => {
            const testCases = [
                { motorId: 0, ticks: 1000, velocityMps: 1.5 },
                { motorId: 1, ticks: 2000, velocityMps: 1.2 },
                { motorId: 2, ticks: 1500, velocityMps: 1.0 },
                { motorId: 3, ticks: 1800, velocityMps: 0.8 },
            ];

            testCases.forEach(tc => {
                const encoder: EncoderData = {
                    motorId: tc.motorId,
                    ticks: BigInt(tc.ticks),
                    velocityMps: tc.velocityMps,
                    timestampUs: BigInt(1000000),
                };

                const bytes = EncoderData.toBinary(encoder);
                const decoded = EncoderData.fromBinary(bytes);

                expect(decoded.motorId).toBe(tc.motorId);
                expect(Number(decoded.ticks)).toBe(tc.ticks);
                expect(decoded.velocityMps).toBeCloseTo(tc.velocityMps, 3);
            });
        });
    });

    describe('MotorStatus - 4 Motors', () => {
        it('should support motor IDs 0-3', () => {
            for (let motorId = 0; motorId < 4; motorId++) {
                const status: MotorStatus = {
                    motorId: motorId,
                    dutyCyclePercent: 75.5,
                    velocityMps: 1.5,
                    targetVelocityMps: 1.5,
                    temperatureCelsius: 45.0,
                    currentMa: 2500.0,
                    faultFlags: 0,
                    state: MotorState.MOTOR_STATE_RUNNING,
                };

                const bytes = MotorStatus.toBinary(status);
                const decoded = MotorStatus.fromBinary(bytes);

                expect(decoded.motorId).toBe(motorId);
                expect(decoded.dutyCyclePercent).toBeCloseTo(75.5, 1);
                expect(decoded.state).toBe(MotorState.MOTOR_STATE_RUNNING);
            }
        });
    });

    describe('SetVelocityRequest/Response', () => {
        it('should complete request/response roundtrip', () => {
            const req: SetVelocityRequest = {
                header: {
                    requestId: 'test-request-123',
                    clientVersion: '1.0.0',
                },
                command: {
                    motor0VelocityMps: 1.5,
                    motor1VelocityMps: 1.5,
                    motor2VelocityMps: 1.0,
                    motor3VelocityMps: 1.0,
                    sequence: 1,
                    timestampUs: BigInt(1000000),
                },
            };

            const reqBytes = SetVelocityRequest.toBinary(req);
            const parsedReq = SetVelocityRequest.fromBinary(reqBytes);

            expect(parsedReq.header?.requestId).toBe('test-request-123');
            expect(parsedReq.command?.motor0VelocityMps).toBeCloseTo(1.5, 3);

            // Response
            const resp: SetVelocityResponse = {
                header: {
                    requestId: 'test-request-123',
                    status: Status.STATUS_OK,
                    errorMessage: '',
                    latencyUs: BigInt(0),
                },
            };

            const respBytes = SetVelocityResponse.toBinary(resp);
            const parsedResp = SetVelocityResponse.fromBinary(respBytes);

            expect(parsedResp.header?.requestId).toBe('test-request-123');
            expect(parsedResp.header?.status).toBe(Status.STATUS_OK);
        });
    });
    */
});
