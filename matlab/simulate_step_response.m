%% simulate_step_response.m - Motor Control Simulation
% Simulates the closed-loop step response with PID control and
% analyzes performance metrics.
%
% Features:
% - Step response simulation
% - Disturbance rejection analysis
% - Setpoint tracking
% - Control effort visualization
% - Performance metrics comparison
%
% STAR Project - Texas A&M University
% December 2025

%% Setup
clear; clc;
fprintf('=== STAR Motor Control Simulation ===\n');

%% Load Motor Model and PID
motor_model_1st_order;
pid_design_velocity;

fprintf('\n=== Starting Simulation ===\n');

%% Simulation Parameters
Ts = 0.01;           % 10 ms sample time (100 Hz)
T_sim = 2.0;         % 2 second simulation
t = 0:Ts:T_sim;
N = length(t);

% Setpoint profile: step from 0 to 100 RPM at t=0.2s
setpoint_rpm = zeros(size(t));
setpoint_rpm(t >= 0.2) = 100;  % 100 RPM step

% Convert to rad/s for internal calculations
setpoint_rads = setpoint_rpm * (2*pi/60);

fprintf('Simulation Duration: %.1f s\n', T_sim);
fprintf('Sample Time:         %.0f ms\n', Ts * 1000);
fprintf('Setpoint:            %.0f RPM step at t=0.2s\n', 100);

%% Discretize Plant and Controller
motor_tf_d = c2d(motor_tf, Ts, 'zoh');
C_pid_d = c2d(C_pid, Ts, 'tustin');

% Create closed-loop system
sys_cl_d = feedback(C_pid_d * motor_tf_d, 1);

%% Simulate Step Response
fprintf('\n--- Running Simulation ---\n');

% Use lsim for discrete simulation
[velocity_rads, ~] = lsim(sys_cl_d, setpoint_rads, t);
velocity_rpm = velocity_rads * (60/(2*pi));

% Calculate control effort
% u = C * e, where e = setpoint - velocity
error_rads = setpoint_rads' - velocity_rads;
[control_output, ~] = lsim(C_pid_d, error_rads, t);

%% Add Disturbance Simulation
fprintf('--- Adding Disturbance ---\n');

% Disturbance: output disturbance at t=1.0s (models load torque effect on velocity)
% Magnitude represents velocity deviation in rad/s if uncontrolled
disturbance = zeros(size(t));
disturbance(t >= 1.0 & t < 1.5) = 5;  % 5 rad/s (~48 RPM) disturbance

% Simulate with disturbance using sensitivity function S = 1/(1+GC)
% Output disturbance rejection: y_dist = S * d
sys_sensitivity = feedback(1, C_pid_d * motor_tf_d);  % S = 1/(1+GC)
[dist_response, ~] = lsim(sys_sensitivity, disturbance', t);
velocity_with_dist = velocity_rpm + dist_response * (60/(2*pi));

%% Calculate Performance Metrics
fprintf('\n=== Performance Metrics ===\n');

% Find step response after t=0.2s
idx_step = find(t >= 0.2, 1);
step_response = velocity_rpm(idx_step:end);
step_time = t(idx_step:end) - t(idx_step);

% Final value
final_value = setpoint_rpm(end);

% Rise time (10% to 90%)
idx_10 = find(step_response >= 0.1 * final_value, 1);
idx_90 = find(step_response >= 0.9 * final_value, 1);
if ~isempty(idx_10) && ~isempty(idx_90)
    rise_time = step_time(idx_90) - step_time(idx_10);
else
    rise_time = NaN;
end

% Settling time (within 2%)
settling_band = 0.02 * final_value;
idx_settled = find(abs(step_response - final_value) <= settling_band, 1);
if ~isempty(idx_settled)
    settling_time = step_time(idx_settled);
else
    settling_time = NaN;
end

% Overshoot
peak_value = max(step_response);
overshoot = (peak_value - final_value) / final_value * 100;
if overshoot < 0
    overshoot = 0;
end

% Steady-state error
ss_error = abs(final_value - mean(step_response(end-10:end)));

fprintf('Rise Time (10-90%%):  %.3f s\n', rise_time);
fprintf('Settling Time (2%%):  %.3f s\n', settling_time);
fprintf('Overshoot:           %.1f%%\n', overshoot);
fprintf('Peak Value:          %.1f RPM\n', peak_value);
fprintf('Steady-State Error:  %.2f RPM\n', ss_error);

%% Disturbance Rejection Metrics
fprintf('\n=== Disturbance Rejection ===\n');

idx_dist = find(t >= 1.0, 1);
dist_deviation = velocity_with_dist(idx_dist:end) - velocity_rpm(idx_dist:end);
max_dist_deviation = max(abs(dist_deviation));
dist_recovery_idx = find(abs(dist_deviation(50:end)) < 2, 1) + 50;  % Within 2 RPM
if ~isempty(dist_recovery_idx)
    dist_recovery_time = (dist_recovery_idx - 1) * Ts;
else
    dist_recovery_time = NaN;
end

fprintf('Max Disturbance Deviation: %.1f RPM\n', max_dist_deviation);
fprintf('Disturbance Recovery Time: %.3f s\n', dist_recovery_time);

%% Plot Results
figure('Name', 'Motor Control Simulation', 'Position', [100 100 1400 900]);

% Velocity Response
subplot(2,3,1);
plot(t, velocity_rpm, 'b-', 'LineWidth', 2); hold on;
plot(t, setpoint_rpm, 'r--', 'LineWidth', 1.5);
xlabel('Time (s)');
ylabel('Velocity (RPM)');
title('Velocity Step Response');
legend('Actual', 'Setpoint', 'Location', 'southeast');
grid on;
ylim([-10 120]);

% Velocity with Disturbance
subplot(2,3,2);
plot(t, velocity_with_dist, 'b-', 'LineWidth', 2); hold on;
plot(t, setpoint_rpm, 'r--', 'LineWidth', 1.5);
area(t, disturbance * 0.5, 'FaceColor', [1 0.8 0.8], 'EdgeColor', 'none', 'FaceAlpha', 0.5);
xlabel('Time (s)');
ylabel('Velocity (RPM)');
title('Response with Load Disturbance');
legend('Actual', 'Setpoint', 'Disturbance', 'Location', 'southeast');
grid on;
ylim([-10 150]);

% Control Effort
subplot(2,3,3);
plot(t, control_output, 'g-', 'LineWidth', 1.5); hold on;
yline(100, 'r--', 'Saturation');
yline(-100, 'r--');
xlabel('Time (s)');
ylabel('Control Output (%)');
title('Control Effort (PWM Duty Cycle)');
grid on;
ylim([-120 120]);

% Error Signal
subplot(2,3,4);
error_rpm = setpoint_rpm' - velocity_rpm;
plot(t, error_rpm, 'm-', 'LineWidth', 1.5);
xlabel('Time (s)');
ylabel('Error (RPM)');
title('Tracking Error');
grid on;

% Zoomed Step Response
subplot(2,3,5);
idx_zoom = t >= 0.15 & t <= 0.6;
plot(t(idx_zoom), velocity_rpm(idx_zoom), 'b-', 'LineWidth', 2); hold on;
plot(t(idx_zoom), setpoint_rpm(idx_zoom), 'r--', 'LineWidth', 1.5);
yline(final_value * 0.9, 'g:', '90%');
yline(final_value * 1.02, 'c:', '+2%');
yline(final_value * 0.98, 'c:', '-2%');
xlabel('Time (s)');
ylabel('Velocity (RPM)');
title('Step Response (Zoomed)');
legend('Actual', 'Setpoint', 'Location', 'southeast');
grid on;

% Performance Summary
subplot(2,3,6);
axis off;
text(0.1, 0.9, 'Performance Summary', 'FontSize', 14, 'FontWeight', 'bold');
text(0.1, 0.75, sprintf('Rise Time: %.0f ms', rise_time * 1000), 'FontSize', 12);
text(0.1, 0.60, sprintf('Settling Time: %.0f ms', settling_time * 1000), 'FontSize', 12);
text(0.1, 0.45, sprintf('Overshoot: %.1f%%', overshoot), 'FontSize', 12);
text(0.1, 0.30, sprintf('SS Error: %.2f RPM', ss_error), 'FontSize', 12);
text(0.1, 0.15, sprintf('Max Dist. Dev: %.1f RPM', max_dist_deviation), 'FontSize', 12);

% PID Gains
text(0.55, 0.9, 'PID Gains', 'FontSize', 14, 'FontWeight', 'bold');
text(0.55, 0.75, sprintf('Kp = %.4f', Kp), 'FontSize', 12);
text(0.55, 0.60, sprintf('Ki = %.4f', Ki), 'FontSize', 12);
text(0.55, 0.45, sprintf('Kd = %.4f', Kd), 'FontSize', 12);
text(0.55, 0.30, sprintf('Sample Rate: %.0f Hz', 1/Ts), 'FontSize', 12);

%% Compare Different Tuning Strategies
fprintf('\n=== Comparing Tuning Strategies ===\n');

figure('Name', 'Tuning Strategy Comparison', 'Position', [200 200 1000 600]);

% Conservative tuning (lower bandwidth)
opts_cons = pidtuneOptions('CrossoverFrequency', 10);
[C_cons, ~] = pidtune(motor_tf, 'PID', opts_cons);
sys_cons = feedback(c2d(C_cons, Ts, 'tustin') * motor_tf_d, 1);
[y_cons, ~] = lsim(sys_cons, setpoint_rads, t);

% Aggressive tuning (higher bandwidth)
opts_aggr = pidtuneOptions('CrossoverFrequency', 30);
[C_aggr, ~] = pidtune(motor_tf, 'PID', opts_aggr);
sys_aggr = feedback(c2d(C_aggr, Ts, 'tustin') * motor_tf_d, 1);
[y_aggr, ~] = lsim(sys_aggr, setpoint_rads, t);

subplot(1,2,1);
plot(t, y_cons * (60/(2*pi)), 'b-', 'LineWidth', 1.5); hold on;
plot(t, velocity_rpm, 'g-', 'LineWidth', 1.5);
plot(t, y_aggr * (60/(2*pi)), 'r-', 'LineWidth', 1.5);
plot(t, setpoint_rpm, 'k--', 'LineWidth', 1);
xlabel('Time (s)');
ylabel('Velocity (RPM)');
title('Tuning Strategy Comparison');
legend('Conservative (BW=10)', 'Default (BW=20)', 'Aggressive (BW=30)', 'Setpoint', 'Location', 'southeast');
grid on;

subplot(1,2,2);
info_cons = stepinfo(sys_cons);
info_default = stepinfo(sys_cl_d);
info_aggr = stepinfo(sys_aggr);

bar_data = [info_cons.RiseTime*1000, info_default.RiseTime*1000, info_aggr.RiseTime*1000;
            info_cons.SettlingTime*1000, info_default.SettlingTime*1000, info_aggr.SettlingTime*1000;
            info_cons.Overshoot, info_default.Overshoot, info_aggr.Overshoot];
bar(bar_data');
set(gca, 'XTickLabel', {'Conservative', 'Default', 'Aggressive'});
legend('Rise Time (ms)', 'Settling Time (ms)', 'Overshoot (%)', 'Location', 'northwest');
title('Performance Comparison');
ylabel('Value');
grid on;

fprintf('Conservative: Rise=%.0fms, Settle=%.0fms, OS=%.1f%%\n', ...
    info_cons.RiseTime*1000, info_cons.SettlingTime*1000, info_cons.Overshoot);
fprintf('Default:      Rise=%.0fms, Settle=%.0fms, OS=%.1f%%\n', ...
    info_default.RiseTime*1000, info_default.SettlingTime*1000, info_default.Overshoot);
fprintf('Aggressive:   Rise=%.0fms, Settle=%.0fms, OS=%.1f%%\n', ...
    info_aggr.RiseTime*1000, info_aggr.SettlingTime*1000, info_aggr.Overshoot);

%% Save Simulation Data
fprintf('\n=== Saved to Workspace ===\n');
fprintf('t             - Time vector\n');
fprintf('velocity_rpm  - Simulated velocity response\n');
fprintf('setpoint_rpm  - Setpoint profile\n');
fprintf('control_output - Control effort\n');
fprintf('error_rpm     - Tracking error\n');
