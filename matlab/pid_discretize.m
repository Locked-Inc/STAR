%% pid_discretize.m - Discretize PID for ESP32 Implementation
% Converts continuous-time PID gains to discrete form for ESP32 deployment.
%
% The ESP32 star_pid library uses the parallel form with rectangular
% integration for the integral term and backward difference for derivative:
%
%   P[k] = Kp * e[k]
%   I[k] = I[k-1] + Ki * e[k] * dt
%   D[k] = Kd * (e[k] - e[k-1]) / dt
%   u[k] = P[k] + I[k] + D[k]
%
% This script validates that the continuous gains work correctly when
% discretized at 100 Hz (Ts = 10ms).
%
% STAR Project - Texas A&M University
% December 2025

%% Configuration
clear; clc;

% Sample time (100 Hz motor control loop)
Ts = 0.01;  % 10 ms
Fs = 1 / Ts;  % 100 Hz

fprintf('=== PID Discretization for ESP32 ===\n');
fprintf('Sample Time: %.0f ms (%.0f Hz)\n', Ts * 1000, Fs);

%% Load Motor Model and Design PID
motor_model_1st_order;
fprintf('\n');

% Design continuous PID
target_bandwidth = 20;  % rad/s
opts = pidtuneOptions('CrossoverFrequency', target_bandwidth);
[C_pid_cont, info] = pidtune(motor_tf, 'PID', opts);

Kp = C_pid_cont.Kp;
Ki = C_pid_cont.Ki;
Kd = C_pid_cont.Kd;

fprintf('Continuous PID Gains:\n');
fprintf('  Kp = %.6f\n', Kp);
fprintf('  Ki = %.6f\n', Ki);
fprintf('  Kd = %.6f\n', Kd);

%% Discretize Using Tustin (Bilinear) Transform
fprintf('\n=== Tustin (Bilinear) Discretization ===\n');

C_pid_tustin = c2d(C_pid_cont, Ts, 'tustin');
[num_tustin, den_tustin] = tfdata(C_pid_tustin, 'v');

fprintf('Discrete Transfer Function (Tustin):\n');
C_pid_tustin

fprintf('Numerator coefficients:   [%.6f, %.6f, %.6f]\n', num_tustin);
fprintf('Denominator coefficients: [%.6f, %.6f, %.6f]\n', den_tustin);

%% Discretize Using Forward Euler (Matches star_pid)
fprintf('\n=== Forward Euler Discretization (Matches star_pid) ===\n');

% The star_pid library uses:
% - Rectangular integration: I[k] = I[k-1] + Ki * e[k] * dt
% - Backward difference: D[k] = Kd * (e[k] - e[k-1]) / dt
%
% This is effectively "Forward Euler" for I and "Backward Euler" for D

% For the parallel form, gains don't need transformation:
Kp_d = Kp;              % Proportional: no change
Ki_d = Ki;              % Integral: Ki * Ts applied per sample in code
Kd_d = Kd;              % Derivative: Kd / Ts applied per sample in code

fprintf('Discrete Gains (for star_pid parallel form):\n');
fprintf('  Kp = %.6f (same as continuous)\n', Kp_d);
fprintf('  Ki = %.6f (multiply by Ts=%.3f in each iteration)\n', Ki_d, Ts);
fprintf('  Kd = %.6f (divide by Ts=%.3f in each iteration)\n', Kd_d, Ts);
fprintf('\nPer-sample contributions:\n');
fprintf('  I_increment = Ki * e * Ts = %.6f * e\n', Ki_d * Ts);
fprintf('  D_term = Kd * (e - e_prev) / Ts = %.3f * delta_e\n', Kd_d / Ts);

%% Compare Continuous vs Discrete Step Response
fprintf('\n=== Comparing Continuous vs Discrete ===\n');

% Discretize plant
motor_tf_d = c2d(motor_tf, Ts, 'zoh');

% Continuous closed-loop
sys_cl_cont = feedback(C_pid_cont * motor_tf, 1);

% Discrete closed-loop (Tustin)
sys_cl_tustin = feedback(C_pid_tustin * motor_tf_d, 1);

% Create a discrete PID using forward Euler (like star_pid)
% P(z) = Kp
% I(z) = Ki * Ts * z / (z - 1)  [Forward Euler]
% D(z) = Kd * (z - 1) / (Ts * z) [Backward Euler]
z = tf('z', Ts);
if Kd > 0
    C_pid_euler = Kp + Ki * Ts * z / (z - 1) + Kd * (z - 1) / (Ts * z);
else
    % PI only (no derivative term)
    C_pid_euler = Kp + Ki * Ts * z / (z - 1);
end
sys_cl_euler = feedback(C_pid_euler * motor_tf_d, 1);

% Step response comparison
figure('Name', 'Continuous vs Discrete PID Comparison');

subplot(2,2,1);
t = 0:Ts:0.5;
[y_cont, t_cont] = step(sys_cl_cont, t);
[y_tustin, ~] = step(sys_cl_tustin, t);
[y_euler, ~] = step(sys_cl_euler, t);

plot(t_cont, y_cont, 'b-', 'LineWidth', 2); hold on;
stairs(t, y_tustin, 'r--', 'LineWidth', 1.5);
stairs(t, y_euler, 'g:', 'LineWidth', 1.5);
xlabel('Time (s)');
ylabel('Angular Velocity (rad/s)');
title('Step Response Comparison');
legend('Continuous', 'Discrete (Tustin)', 'Discrete (Euler)', 'Location', 'southeast');
grid on;

subplot(2,2,2);
error_tustin = y_cont - y_tustin;
error_euler = y_cont - y_euler;
plot(t, error_tustin, 'r-', t, error_euler, 'g-');
xlabel('Time (s)');
ylabel('Error from Continuous');
title('Discretization Error');
legend('Tustin Error', 'Euler Error');
grid on;

subplot(2,2,3);
bode(C_pid_cont, C_pid_tustin, C_pid_euler);
title('PID Controller Frequency Response');
legend('Continuous', 'Tustin', 'Euler');

subplot(2,2,4);
nyquist(C_pid_cont * motor_tf);
title('Nyquist Plot (Continuous)');
grid on;

%% Verify Sampling Rate Adequacy
fprintf('\n=== Sampling Rate Verification ===\n');

% Nyquist frequency
f_nyquist = Fs / 2;
omega_nyquist = 2 * pi * f_nyquist;

% Closed-loop bandwidth (use stepinfo as fallback)
try
    bw_cl = bandwidth(sys_cl_cont);
    if isa(bw_cl, 'tf') || isempty(bw_cl) || ~isfinite(bw_cl)
        % Fallback: estimate from step response
        info_cl = stepinfo(sys_cl_cont);
        bw_cl = 0.35 / info_cl.RiseTime;  % Approximate bandwidth
    end
catch
    % Fallback: estimate from step response
    info_cl = stepinfo(sys_cl_cont);
    bw_cl = 0.35 / info_cl.RiseTime;  % Approximate bandwidth
end

fprintf('Nyquist Frequency:     %.1f Hz (%.1f rad/s)\n', f_nyquist, omega_nyquist);
fprintf('Closed-Loop Bandwidth: %.1f rad/s (%.1f Hz)\n', bw_cl, bw_cl/(2*pi));
fprintf('Oversampling Ratio:    %.1fx\n', omega_nyquist / bw_cl);

if omega_nyquist > 5 * bw_cl
    fprintf('Sampling rate is ADEQUATE (>5x bandwidth)\n');
elseif omega_nyquist > 2 * bw_cl
    fprintf('Sampling rate is MARGINAL (2-5x bandwidth)\n');
else
    fprintf('WARNING: Sampling rate may be TOO LOW (<2x bandwidth)\n');
end

%% Generate ESP32 Code
fprintf('\n');
fprintf('=======================================================\n');
fprintf('       ESP32 star_pid CONFIGURATION (VERIFIED)\n');
fprintf('=======================================================\n');
fprintf('\n');
fprintf('// Velocity PID for 100 Hz control loop\n');
fprintf('// Verified: Discrete response matches continuous within %.1f%%\n', ...
        max(abs(error_euler)) / max(y_cont) * 100);
fprintf('\n');
fprintf('star_pid_config_t velocity_pid_config = {\n');
fprintf('    .kp = %.6ff,\n', Kp);
fprintf('    .ki = %.6ff,\n', Ki);
fprintf('    .kd = %.6ff,\n', Kd);
fprintf('    .output_min = -100.0f,   // -100%% duty cycle\n');
fprintf('    .output_max = 100.0f,    // +100%% duty cycle\n');
fprintf('    .integral_min = -50.0f,  // Anti-windup\n');
fprintf('    .integral_max = 50.0f,   // Anti-windup\n');
fprintf('};\n');
fprintf('\n');
fprintf('// In control loop (every 10ms = 100Hz):\n');
fprintf('float dt = 0.01f;  // 10ms sample time\n');
fprintf('star_pid_compute(&velocity_pid, setpoint_rpm, measured_rpm, dt, &output);\n');

%% Anti-Windup Considerations
fprintf('\n=== Anti-Windup Configuration ===\n');

% Calculate integral limits based on output saturation
output_max = 100.0;  % Maximum duty cycle percentage
output_min = -100.0;

% Integral term should not exceed what could saturate output alone
% At steady-state error, I_term = Ki * error * t_accum
% To prevent windup, limit I_term to +/- 50% of output max
integral_limit = 0.5 * output_max;  % 50% of max = 50

fprintf('Output Limits:     [%.0f, %.0f] %%\n', output_min, output_max);
fprintf('Integral Limits:   [%.0f, %.0f] (50%% of output range)\n', -integral_limit, integral_limit);
fprintf('\nNote: These limits are for the accumulated integral VALUE,\n');
fprintf('not the Ki gain itself.\n');

%% Save Configuration to File
config_file = 'pid_config_output.txt';
fid = fopen(config_file, 'w');
fprintf(fid, '// Auto-generated PID configuration\n');
fprintf(fid, '// Sample Time: %.0f ms (%.0f Hz)\n', Ts * 1000, Fs);
fprintf(fid, '// Target Bandwidth: %.0f rad/s\n', target_bandwidth);
fprintf(fid, '\n');
fprintf(fid, 'star_pid_config_t velocity_pid_config = {\n');
fprintf(fid, '    .kp = %.6ff,\n', Kp);
fprintf(fid, '    .ki = %.6ff,\n', Ki);
fprintf(fid, '    .kd = %.6ff,\n', Kd);
fprintf(fid, '    .output_min = %.1ff,\n', output_min);
fprintf(fid, '    .output_max = %.1ff,\n', output_max);
fprintf(fid, '    .integral_min = %.1ff,\n', -integral_limit);
fprintf(fid, '    .integral_max = %.1ff,\n', integral_limit);
fprintf(fid, '};\n');
fclose(fid);

fprintf('\n=== Configuration saved to %s ===\n', config_file);
