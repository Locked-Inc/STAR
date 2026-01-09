// telemetry.test.ts - TypeScript Telemetry Tests
// Verifies RX72N-specific telemetry schema serialization.
//
// STAR Project - Texas A&M University
// January 2026

import { describe, it, expect } from 'vitest';

// Import generated classes (will be available after buf generate)
// import { TelemetryData, EncoderData } from '@gen/star/v1/telemetry';
// import { GetTelemetryResponse, StreamTelemetryRequest } from '@gen/star/v1/telemetry';
// import { RequestHeader, ResponseHeader, Status } from '@gen/star/v1/common';

describe('Telemetry Tests', () => {

    describe('Test Framework', () => {
        it('should have working test framework', () => {
            // Placeholder test - verifies Vitest is operational
            expect(true).toBe(true);
        });
    });

    // TODO: Uncomment after buf generate produces TypeScript code
    /*
    describe('TelemetryData - RX72N Encoder Fields', () => {
        it('should support individual encoder fields (encoder_0..3)', () => {
            const telem: TelemetryData = {
                timestampUs: BigInt(1000000),
                encoder0: {
                    motorId: 0,
                    ticks: BigInt(1000),
                    velocityMps: 1.5,
                    timestampUs: BigInt(1000000),
                },
                encoder1: {
                    motorId: 1,
                    ticks: BigInt(1050),
                    velocityMps: 1.5,
                    timestampUs: BigInt(1000000),
                },
                encoder2: {
                    motorId: 2,
                    ticks: BigInt(900),
                    velocityMps: 1.0,
                    timestampUs: BigInt(1000000),
                },
                encoder3: {
                    motorId: 3,
                    ticks: BigInt(950),
                    velocityMps: 1.0,
                    timestampUs: BigInt(1000000),
                },
                emergencyStop: false,
                faultFlags: 0,
            };

            // Serialize
            const bytes = TelemetryData.toBinary(telem);

            // Verify reasonable size for embedded systems
            expect(bytes.length).toBeLessThan(256);

            // Deserialize
            const decoded = TelemetryData.fromBinary(bytes);

            // Verify all 4 encoder fields
            expect(decoded.encoder0?.motorId).toBe(0);
            expect(decoded.encoder1?.motorId).toBe(1);
            expect(decoded.encoder2?.motorId).toBe(2);
            expect(decoded.encoder3?.motorId).toBe(3);

            // Verify encoder data
            expect(Number(decoded.encoder0?.ticks)).toBe(1000);
            expect(decoded.encoder0?.velocityMps).toBeCloseTo(1.5, 3);
        });
    });

    describe('TelemetryData - RX72N Battery Fields', () => {
        it('should support battery telemetry from BQ4050', () => {
            const telem: TelemetryData = {
                batteryVoltageV: 12.6,
                batterySocPercent: 85,
                batteryPercent: 85.0, // Legacy field
                timestampUs: BigInt(1000000),
            };

            const bytes = TelemetryData.toBinary(telem);
            const decoded = TelemetryData.fromBinary(bytes);

            // Verify battery fields
            expect(decoded.batteryVoltageV).toBeCloseTo(12.6, 1);
            expect(decoded.batterySocPercent).toBe(85);
        });
    });

    describe('TelemetryData - RX72N Safety Fields', () => {
        it('should support emergency stop and fault flags', () => {
            const testCases = [
                { name: 'normal operation', emergencyStop: false, faultFlags: 0 },
                { name: 'emergency stop active', emergencyStop: true, faultFlags: 0 },
                { name: 'motor 0 fault', emergencyStop: false, faultFlags: 0x01 },
                { name: 'motor 1 fault', emergencyStop: false, faultFlags: 0x02 },
                { name: 'motor 2 fault', emergencyStop: false, faultFlags: 0x04 },
                { name: 'motor 3 fault', emergencyStop: false, faultFlags: 0x08 },
                { name: 'all motors fault', emergencyStop: false, faultFlags: 0x0F },
                { name: 'E-STOP with faults', emergencyStop: true, faultFlags: 0x0F },
            ];

            testCases.forEach(tc => {
                const telem: TelemetryData = {
                    emergencyStop: tc.emergencyStop,
                    faultFlags: tc.faultFlags,
                    timestampUs: BigInt(1000000),
                };

                const bytes = TelemetryData.toBinary(telem);
                const decoded = TelemetryData.fromBinary(bytes);

                expect(decoded.emergencyStop).toBe(tc.emergencyStop);
                expect(decoded.faultFlags).toBe(tc.faultFlags);
            });
        });
    });

    describe('TelemetryData - RX72N Temperature Field', () => {
        it('should support DS18B20 temperature monitoring', () => {
            const telem: TelemetryData = {
                temperatureCelsius: 45.5,
                timestampUs: BigInt(1000000),
            };

            const bytes = TelemetryData.toBinary(telem);
            const decoded = TelemetryData.fromBinary(bytes);

            expect(decoded.temperatureCelsius).toBeCloseTo(45.5, 1);
        });
    });

    describe('TelemetryData - Complete Message', () => {
        it('should support full telemetry with all RX72N fields', () => {
            const telem: TelemetryData = {
                // Standard fields
                timestamp: { seconds: BigInt(Date.now() / 1000), nanos: 0 },
                batteryPercent: 85.0,
                wifiSignalDbm: -45,
                cpuUsagePercent: 25.5,
                temperatureCelsius: 45.0,
                motorLoadPercent: 60.0,

                // RX72N-specific fields
                timestampUs: BigInt(1000000),
                encoder0: {
                    motorId: 0,
                    ticks: BigInt(1000),
                    velocityMps: 1.5,
                    timestampUs: BigInt(1000000),
                },
                encoder1: {
                    motorId: 1,
                    ticks: BigInt(1050),
                    velocityMps: 1.5,
                    timestampUs: BigInt(1000000),
                },
                encoder2: {
                    motorId: 2,
                    ticks: BigInt(900),
                    velocityMps: 1.0,
                    timestampUs: BigInt(1000000),
                },
                encoder3: {
                    motorId: 3,
                    ticks: BigInt(950),
                    velocityMps: 1.0,
                    timestampUs: BigInt(1000000),
                },
                emergencyStop: false,
                faultFlags: 0,
                batteryVoltageV: 12.6,
                batterySocPercent: 85,
            };

            // Serialize
            const bytes = TelemetryData.toBinary(telem);

            // Verify reasonable size
            expect(bytes.length).toBeLessThan(512);

            // Deserialize
            const decoded = TelemetryData.fromBinary(bytes);

            // Verify critical fields
            expect(decoded.emergencyStop).toBe(false);
            expect(decoded.encoder0).toBeDefined();
            expect(decoded.encoder0?.velocityMps).toBeCloseTo(1.5, 3);
            expect(decoded.batteryVoltageV).toBeCloseTo(12.6, 1);
        });
    });

    describe('TelemetryData - Streaming Simulation', () => {
        it('should handle high-frequency telemetry streaming (100 Hz)', () => {
            const numMessages = 100;

            for (let i = 0; i < numMessages; i++) {
                const telem: TelemetryData = {
                    timestampUs: BigInt(i * 10000), // 10ms intervals (100 Hz)
                    encoder0: {
                        motorId: 0,
                        ticks: BigInt(i * 10),
                        velocityMps: 1.5,
                        timestampUs: BigInt(i * 10000),
                    },
                    encoder1: {
                        motorId: 1,
                        ticks: BigInt(i * 10),
                        velocityMps: 1.5,
                        timestampUs: BigInt(i * 10000),
                    },
                    encoder2: {
                        motorId: 2,
                        ticks: BigInt(i * 8),
                        velocityMps: 1.0,
                        timestampUs: BigInt(i * 10000),
                    },
                    encoder3: {
                        motorId: 3,
                        ticks: BigInt(i * 8),
                        velocityMps: 1.0,
                        timestampUs: BigInt(i * 10000),
                    },
                    emergencyStop: false,
                    faultFlags: 0,
                    batteryVoltageV: 12.6,
                    batterySocPercent: 85,
                };

                const bytes = TelemetryData.toBinary(telem);

                // Verify consistent encoding size
                if (i === 0) {
                    console.log(`Telemetry message size: ${bytes.length} bytes`);
                }
                expect(bytes.length).toBeLessThan(256);

                // Spot check decoding every 10th message
                if (i % 10 === 0) {
                    const decoded = TelemetryData.fromBinary(bytes);
                    expect(Number(decoded.timestampUs)).toBe(i * 10000);
                }
            }
        });
    });

    describe('GetTelemetryResponse', () => {
        it('should support telemetry service response', () => {
            const resp: GetTelemetryResponse = {
                header: {
                    requestId: 'telem-request-1',
                    status: Status.STATUS_OK,
                    errorMessage: '',
                    latencyUs: BigInt(0),
                },
                telemetry: {
                    timestampUs: BigInt(1000000),
                    encoder0: {
                        motorId: 0,
                        ticks: BigInt(1000),
                        velocityMps: 1.5,
                        timestampUs: BigInt(1000000),
                    },
                    emergencyStop: false,
                    faultFlags: 0,
                    batteryVoltageV: 12.6,
                    batterySocPercent: 85,
                },
            };

            const bytes = GetTelemetryResponse.toBinary(resp);
            const decoded = GetTelemetryResponse.fromBinary(bytes);

            expect(decoded.header?.requestId).toBe('telem-request-1');
            expect(decoded.telemetry).toBeDefined();
            expect(decoded.telemetry?.batteryVoltageV).toBeCloseTo(12.6, 1);
        });
    });

    describe('StreamTelemetryRequest', () => {
        it('should support streaming request configuration', () => {
            const req: StreamTelemetryRequest = {
                header: {
                    requestId: 'stream-request-1',
                    clientVersion: '1.0.0',
                },
                rateHz: 100, // 100 Hz streaming
                fields: ['encoders', 'battery', 'emergency_stop'],
            };

            const bytes = StreamTelemetryRequest.toBinary(req);
            const decoded = StreamTelemetryRequest.fromBinary(bytes);

            expect(decoded.rateHz).toBe(100);
            expect(decoded.fields).toHaveLength(3);
        });
    });
    */
});
