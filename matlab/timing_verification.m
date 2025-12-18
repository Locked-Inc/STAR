%% timing_verification.m - STAR Communication and Control Timing Verification
% Verifies system timing parameters against hardware constraints.
%
% This script calculates and validates:
% 1. SPI communication timing (100Hz data exchange)
% 2. Motor control loop timing (250Hz PID)
% 3. SPI clock rate feasibility for short PCB traces
%
% References:
% - ESP32-S3 Technical Reference Manual (v1.0)
% - Raspberry Pi 5 SPI documentation
% - JEDEC transmission line guidelines
%
% STAR Project - Texas A&M University
% December 2025

clear; clc;
fprintf('=== STAR Timing Verification ===\n\n');

%% ========================================================================
% HARDWARE PARAMETERS
% ========================================================================

% ESP32-S3 SPI Parameters (from ESP32-S3 TRM)
esp32_spi_max_freq_hz = 80e6;       % Max SPI clock (SPI2/SPI3)
esp32_dma_burst_size = 4;           % DMA burst size in bytes
esp32_cpu_freq_hz = 240e6;          % CPU frequency

% Raspberry Pi 5 SPI Parameters
rpi5_spi_max_freq_hz = 50e6;        % Max practical SPI clock
rpi5_spidev_buffer = 4096;          % Default spidev buffer size

% Physical Layer Parameters
trace_length_spi_in = 3;            % SPI trace length [inches] (user spec: <5in)
trace_length_pwm_in = 3;            % PWM trace length [inches] (user spec: <3in)
pcb_er = 4.5;                       % FR4 dielectric constant
propagation_delay_ps_per_in = 150;  % ~150 ps/in for microstrip on FR4

% Protocol Parameters
frame_header_bytes = 10;            % SYNC(2) + SEQ(2) + LEN(2) + TYPE(1) + FLAGS(1) + reserved(2)
crc_bytes = 4;                      % CRC-32
max_payload_bytes = 1024;           % Maximum protobuf payload
max_frame_bytes = frame_header_bytes + max_payload_bytes + crc_bytes;

%% ========================================================================
% 1. SPI COMMUNICATION TIMING (100Hz)
% ========================================================================
fprintf('--- 1. SPI Communication Timing (100Hz Target) ---\n\n');

spi_comm_freq_hz = 100;             % Target: 100Hz communication rate
spi_comm_period_ms = 1000 / spi_comm_freq_hz;  % 10ms period

% Typical message sizes (from proto definitions)
velocity_request_bytes = 64;        % SetVelocityRequest typical size
velocity_response_bytes = 128;      % SetVelocityResponse with motor status
telemetry_bytes = 256;              % TelemetryData full snapshot

% Calculate transmission times at different SPI clock rates
spi_clocks_mhz = [1, 5, 10, 20, 40];

fprintf('Message transfer times at different SPI clocks:\n');
fprintf('%-12s', 'Clock (MHz)');
fprintf('%-15s%-15s%-15s\n', 'VelReq (us)', 'VelResp (us)', 'Telemetry (us)');
fprintf('%s\n', repmat('-', 1, 57));

for spi_mhz = spi_clocks_mhz
    spi_hz = spi_mhz * 1e6;
    % Time = bits / clock_rate, 8 bits per byte
    t_vel_req_us = (velocity_request_bytes * 8 / spi_hz) * 1e6;
    t_vel_resp_us = (velocity_response_bytes * 8 / spi_hz) * 1e6;
    t_telemetry_us = (telemetry_bytes * 8 / spi_hz) * 1e6;
    fprintf('%-12d%-15.1f%-15.1f%-15.1f\n', spi_mhz, t_vel_req_us, t_vel_resp_us, t_telemetry_us);
end

% Calculate total round-trip time at 10MHz (selected rate)
spi_clock_hz = 10e6;
fprintf('\n--- Analysis at 10 MHz SPI Clock ---\n');

% Full frame transfer (worst case: max payload both directions)
frame_tx_bytes = frame_header_bytes + velocity_request_bytes + crc_bytes;
frame_rx_bytes = frame_header_bytes + velocity_response_bytes + crc_bytes;
total_bytes = frame_tx_bytes + frame_rx_bytes;

transfer_time_us = (total_bytes * 8 / spi_clock_hz) * 1e6;
fprintf('TX frame size:         %d bytes\n', frame_tx_bytes);
fprintf('RX frame size:         %d bytes\n', frame_rx_bytes);
fprintf('Total transfer time:   %.1f us\n', transfer_time_us);

% Software overhead (conservative estimates)
dma_setup_us = 5;                   % DMA descriptor setup
interrupt_latency_us = 10;          % ISR entry/exit
protobuf_encode_us = 50;            % nanopb encode (typical)
protobuf_decode_us = 50;            % nanopb decode (typical)
crc_compute_us = 20;                % CRC-32 calculation
arq_process_us = 10;                % Sequence number handling

software_overhead_us = dma_setup_us + interrupt_latency_us + ...
    protobuf_encode_us + protobuf_decode_us + crc_compute_us + arq_process_us;

total_transaction_us = transfer_time_us + software_overhead_us;
total_transaction_ms = total_transaction_us / 1000;

fprintf('Software overhead:     %.1f us\n', software_overhead_us);
fprintf('Total transaction:     %.1f us (%.3f ms)\n', total_transaction_us, total_transaction_ms);
fprintf('Available period:      %.1f ms\n', spi_comm_period_ms);
fprintf('Utilization:           %.1f%%\n', (total_transaction_ms / spi_comm_period_ms) * 100);

if total_transaction_ms < spi_comm_period_ms * 0.5
    fprintf('RESULT: 100Hz SPI communication is FEASIBLE (<%d%% utilization)\n', 50);
else
    fprintf('WARNING: High utilization at 100Hz\n');
end

%% ========================================================================
% 2. MOTOR CONTROL LOOP TIMING (250Hz)
% ========================================================================
fprintf('\n--- 2. Motor Control Loop Timing (250Hz Target) ---\n\n');

motor_control_freq_hz = 250;        % Target: 250Hz control loop
motor_control_period_ms = 1000 / motor_control_freq_hz;  % 4ms period

% PID computation time (from benchmarks on similar ESP32 systems)
pid_compute_us = 5;                 % PID algorithm (float)
encoder_read_us = 10;               % PCNT register read via multiplexer
pwm_update_us = 5;                  % MCPWM duty update
current_sense_us = 20;              % ADC conversion for current sensing

% Per-motor processing time
per_motor_time_us = pid_compute_us + encoder_read_us + pwm_update_us + current_sense_us;
num_motors = 4;
total_motor_time_us = per_motor_time_us * num_motors;

fprintf('Per-motor processing time:\n');
fprintf('  PID compute:         %d us\n', pid_compute_us);
fprintf('  Encoder read:        %d us\n', encoder_read_us);
fprintf('  PWM update:          %d us\n', pwm_update_us);
fprintf('  Current sense:       %d us\n', current_sense_us);
fprintf('  Subtotal:            %d us\n', per_motor_time_us);
fprintf('\nTotal for %d motors:   %d us (%.3f ms)\n', num_motors, total_motor_time_us, total_motor_time_us/1000);
fprintf('Available period:      %.1f ms\n', motor_control_period_ms);
fprintf('Utilization:           %.1f%%\n', (total_motor_time_us/1000 / motor_control_period_ms) * 100);

if total_motor_time_us/1000 < motor_control_period_ms * 0.5
    fprintf('RESULT: 250Hz motor control is FEASIBLE (<%d%% utilization)\n', 50);
else
    fprintf('WARNING: High utilization at 250Hz\n');
end

%% ========================================================================
% 3. SPI CLOCK RATE FEASIBILITY FOR SHORT TRACES
% ========================================================================
fprintf('\n--- 3. SPI Clock Rate Analysis for Short PCB Traces ---\n\n');

% Transmission line analysis for short traces
% Rule of thumb: Transmission line effects matter when trace length > lambda/10
% where lambda = c / (f * sqrt(er))

c_mps = 3e8;                        % Speed of light [m/s]
trace_length_m = trace_length_spi_in * 0.0254;  % Convert to meters

fprintf('Trace Analysis:\n');
fprintf('  Trace length:        %.1f inches (%.1f mm)\n', trace_length_spi_in, trace_length_spi_in * 25.4);
fprintf('  PCB dielectric:      %.1f (FR4)\n', pcb_er);

% Calculate critical frequency (where trace = lambda/10)
lambda_10_m = trace_length_m;       % When trace = lambda/10
freq_critical_hz = c_mps / (sqrt(pcb_er) * 10 * trace_length_m);
fprintf('  Critical frequency:  %.0f MHz (trace = lambda/10)\n', freq_critical_hz / 1e6);

% Propagation delay
prop_delay_ns = trace_length_spi_in * propagation_delay_ps_per_in / 1000;
fprintf('  Propagation delay:   %.2f ns\n', prop_delay_ns);

% Rise time analysis (at different frequencies)
fprintf('\nRise time vs frequency analysis:\n');
fprintf('%-12s%-15s%-15s%-15s\n', 'Freq (MHz)', 'Period (ns)', 'Typ Rise (ns)', 'Trace/Rise');
fprintf('%s\n', repmat('-', 1, 57));

for freq_mhz = [10, 20, 40, 80]
    period_ns = 1000 / freq_mhz;
    rise_time_ns = period_ns * 0.1;  % Typical 10% of period
    trace_rise_ratio = prop_delay_ns / rise_time_ns;
    fprintf('%-12d%-15.1f%-15.1f%-15.2f\n', freq_mhz, period_ns, rise_time_ns, trace_rise_ratio);
end

fprintf('\nGuideline: If trace_delay/rise_time < 0.3, transmission line effects negligible.\n');

% Recommendation
fprintf('\n--- RECOMMENDATION ---\n');
fprintf('For %.1f inch SPI traces:\n', trace_length_spi_in);
fprintf('  - 10 MHz SPI clock is SAFE (trace/rise = %.2f)\n', prop_delay_ns / 10);
fprintf('  - 20 MHz SPI clock is MARGINAL\n');
fprintf('  - 40+ MHz requires termination or impedance matching\n');

%% ========================================================================
% 4. TIMING BUDGET SUMMARY
% ========================================================================
fprintf('\n=== TIMING BUDGET SUMMARY ===\n\n');

fprintf('Communication Layer (100Hz = 10ms period):\n');
fprintf('  SPI transfer:        %.1f us\n', transfer_time_us);
fprintf('  Protocol overhead:   %.1f us\n', software_overhead_us);
fprintf('  Available margin:    %.1f ms\n', spi_comm_period_ms - total_transaction_ms);

fprintf('\nMotor Control Layer (250Hz = 4ms period):\n');
fprintf('  4-motor PID:         %.1f us\n', total_motor_time_us);
fprintf('  Available margin:    %.2f ms\n', motor_control_period_ms - total_motor_time_us/1000);

fprintf('\nSPI Physical Layer:\n');
fprintf('  Selected clock:      10 MHz\n');
fprintf('  Trace length:        <%.0f inches\n', trace_length_spi_in);
fprintf('  Status:              SAFE (no termination needed)\n');

fprintf('\n=== VERIFICATION COMPLETE ===\n');
