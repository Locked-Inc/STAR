%% cascaded_pid_design.m - Cascaded PID Control Design
% Designs cascaded PID controllers for the STAR robot:
%   Position Loop (Outer) -> Velocity Loop (Middle) -> Current Loop (Inner)
%
% The inner loop must be 5-10x faster than the outer loop for proper
% cascade operation.
%
% Reference: docs/star_control_systems_primer.tex
%
% STAR Project - Texas A&M University
% December 2025

%% Setup
clear; clc;
fprintf('=======================================================\n');
fprintf('    CASCADED PID DESIGN FOR STAR ROBOT\n');
fprintf('=======================================================\n');

%% Load Motor Parameters
motor_params;

%% Control Loop Hierarchy
fprintf('\n=== Control Loop Hierarchy ===\n');
fprintf('Level 1 (Innermost): Current Loop  - Protect motors, fast response\n');
fprintf('Level 2 (Middle):    Velocity Loop - Smooth speed control\n');
fprintf('Level 3 (Outermost): Position Loop - Accurate positioning\n');

% Loop rates (from control systems primer)
Ts_current  = 0.01;   % 100 Hz (same as velocity for simplicity)
Ts_velocity = 0.01;   % 100 Hz
Ts_position = 0.1;    % 10 Hz (from Nav2)

fprintf('\nLoop Rates:\n');
fprintf('  Current Loop:  %.0f Hz (Ts = %.0f ms)\n', 1/Ts_current, Ts_current*1000);
fprintf('  Velocity Loop: %.0f Hz (Ts = %.0f ms)\n', 1/Ts_velocity, Ts_velocity*1000);
fprintf('  Position Loop: %.0f Hz (Ts = %.0f ms)\n', 1/Ts_position, Ts_position*1000);

%% Current Loop Design (Innermost)
fprintf('\n');
fprintf('=======================================================\n');
fprintf('    CURRENT LOOP DESIGN\n');
fprintf('=======================================================\n');

% Current loop plant: I(s)/V(s) = 1 / (L*s + R)
s = tf('s');
G_current = 1 / (L_armature * s + R_armature);

fprintf('Current Loop Plant: I(s)/V(s) = 1 / (L*s + R)\n');
fprintf('  L = %.2f mH, R = %.3f Ohm\n', L_armature*1000, R_armature);

% Electrical time constant
tau_e = L_armature / R_armature;
fprintf('  Electrical time constant: %.3f ms\n', tau_e * 1000);

% Design current controller
% Target bandwidth: 10x velocity loop = ~200 rad/s
bw_current = 200;  % rad/s
opts_current = pidtuneOptions('CrossoverFrequency', bw_current);
[C_current, info_current] = pidtune(G_current, 'PI', opts_current);

fprintf('\nCurrent Loop Controller (PI):\n');
fprintf('  Kp_current = %.4f\n', C_current.Kp);
fprintf('  Ki_current = %.4f\n', C_current.Ki);
fprintf('  Achieved Bandwidth: %.1f rad/s (%.1f Hz)\n', ...
        info_current.CrossoverFrequency, info_current.CrossoverFrequency/(2*pi));
fprintf('  Phase Margin: %.1f degrees\n', info_current.PhaseMargin);

% Closed-loop current dynamics
sys_current_cl = feedback(C_current * G_current, 1);
info_current_step = stepinfo(sys_current_cl);
fprintf('\nCurrent Loop Step Response:\n');
fprintf('  Rise Time:    %.3f ms\n', info_current_step.RiseTime * 1000);
fprintf('  Settling Time: %.3f ms\n', info_current_step.SettlingTime * 1000);

%% Velocity Loop Design (Middle)
fprintf('\n');
fprintf('=======================================================\n');
fprintf('    VELOCITY LOOP DESIGN\n');
fprintf('=======================================================\n');

% Velocity loop sees the closed-loop current dynamics + mechanical system
% Omega(s)/I(s) = Kt / (J*s + b)
G_mech = Kt / (J_total * s + b);

% Cascade: velocity plant = closed-loop current * mechanical
G_velocity = sys_current_cl * G_mech;

fprintf('Velocity Loop Plant: Omega(s)/I_ref(s) = G_current_cl(s) * Kt/(J*s + b)\n');

% Design velocity controller
% Target bandwidth: 10x position loop but < 0.1x current loop
bw_velocity = 20;  % rad/s
opts_velocity = pidtuneOptions('CrossoverFrequency', bw_velocity);
[C_velocity, info_velocity] = pidtune(G_velocity, 'PID', opts_velocity);

fprintf('\nVelocity Loop Controller (PID):\n');
fprintf('  Kp_velocity = %.4f\n', C_velocity.Kp);
fprintf('  Ki_velocity = %.4f\n', C_velocity.Ki);
fprintf('  Kd_velocity = %.4f\n', C_velocity.Kd);
fprintf('  Achieved Bandwidth: %.1f rad/s (%.1f Hz)\n', ...
        info_velocity.CrossoverFrequency, info_velocity.CrossoverFrequency/(2*pi));
fprintf('  Phase Margin: %.1f degrees\n', info_velocity.PhaseMargin);

% Closed-loop velocity dynamics
sys_velocity_cl = feedback(C_velocity * G_velocity, 1);
info_velocity_step = stepinfo(sys_velocity_cl);
fprintf('\nVelocity Loop Step Response:\n');
fprintf('  Rise Time:     %.3f ms\n', info_velocity_step.RiseTime * 1000);
fprintf('  Settling Time: %.3f ms\n', info_velocity_step.SettlingTime * 1000);
fprintf('  Overshoot:     %.1f%%\n', info_velocity_step.Overshoot);

%% Position Loop Design (Outermost)
fprintf('\n');
fprintf('=======================================================\n');
fprintf('    POSITION LOOP DESIGN\n');
fprintf('=======================================================\n');

% Position = integral of velocity
% Position plant = closed-loop velocity * integrator
G_position = sys_velocity_cl / s;

fprintf('Position Loop Plant: Theta(s)/Omega_ref(s) = G_velocity_cl(s) / s\n');

% Design position controller
% Target bandwidth: < 0.1x velocity loop
bw_position = 2;  % rad/s
opts_position = pidtuneOptions('CrossoverFrequency', bw_position);
[C_position, info_position] = pidtune(G_position, 'PID', opts_position);

fprintf('\nPosition Loop Controller (PID):\n');
fprintf('  Kp_position = %.4f\n', C_position.Kp);
fprintf('  Ki_position = %.4f\n', C_position.Ki);
fprintf('  Kd_position = %.4f\n', C_position.Kd);
fprintf('  Achieved Bandwidth: %.1f rad/s (%.1f Hz)\n', ...
        info_position.CrossoverFrequency, info_position.CrossoverFrequency/(2*pi));
fprintf('  Phase Margin: %.1f degrees\n', info_position.PhaseMargin);

% Closed-loop position dynamics
sys_position_cl = feedback(C_position * G_position, 1);
info_position_step = stepinfo(sys_position_cl);
fprintf('\nPosition Loop Step Response:\n');
fprintf('  Rise Time:     %.3f s\n', info_position_step.RiseTime);
fprintf('  Settling Time: %.3f s\n', info_position_step.SettlingTime);
fprintf('  Overshoot:     %.1f%%\n', info_position_step.Overshoot);

%% Bandwidth Separation Verification
fprintf('\n');
fprintf('=======================================================\n');
fprintf('    BANDWIDTH SEPARATION VERIFICATION\n');
fprintf('=======================================================\n');

bw_ratio_cv = info_current.CrossoverFrequency / info_velocity.CrossoverFrequency;
bw_ratio_vp = info_velocity.CrossoverFrequency / info_position.CrossoverFrequency;

fprintf('Current / Velocity bandwidth ratio: %.1fx\n', bw_ratio_cv);
fprintf('Velocity / Position bandwidth ratio: %.1fx\n', bw_ratio_vp);

if bw_ratio_cv >= 5 && bw_ratio_vp >= 5
    fprintf('\nBandwidth separation is ADEQUATE (>= 5x between loops)\n');
elseif bw_ratio_cv >= 3 && bw_ratio_vp >= 3
    fprintf('\nWARNING: Bandwidth separation is MARGINAL (3-5x)\n');
else
    fprintf('\nERROR: Bandwidth separation is INSUFFICIENT (<3x)\n');
    fprintf('Consider reducing outer loop bandwidths\n');
end

%% Plot Cascade Response
figure('Name', 'Cascaded PID Control', 'Position', [100 100 1400 800]);

% Individual loop step responses
subplot(2,3,1);
step(sys_current_cl);
title('Current Loop Step Response');
ylabel('Current (A)');
grid on;

subplot(2,3,2);
step(sys_velocity_cl);
title('Velocity Loop Step Response');
ylabel('Angular Velocity (rad/s)');
grid on;

subplot(2,3,3);
step(sys_position_cl);
title('Position Loop Step Response');
ylabel('Position (rad)');
grid on;

% Bode plots
subplot(2,3,4);
bode(C_current * G_current);
title('Current Loop Open-Loop');
grid on;

subplot(2,3,5);
bode(C_velocity * G_velocity);
title('Velocity Loop Open-Loop');
grid on;

subplot(2,3,6);
bode(C_position * G_position);
title('Position Loop Open-Loop');
grid on;

%% Generate ESP32 Configuration
fprintf('\n');
fprintf('=======================================================\n');
fprintf('    ESP32 star_pid CONFIGURATION\n');
fprintf('=======================================================\n');
fprintf('\n');

fprintf('// Current Loop PID (innermost - 100 Hz)\n');
fprintf('star_pid_config_t current_pid_config = {\n');
fprintf('    .kp = %.6ff,\n', C_current.Kp);
fprintf('    .ki = %.6ff,\n', C_current.Ki);
fprintf('    .kd = 0.0f,  // PI only\n');
fprintf('    .output_min = -100.0f,   // -100%% duty cycle\n');
fprintf('    .output_max = 100.0f,    // +100%% duty cycle\n');
fprintf('    .integral_min = -50.0f,\n');
fprintf('    .integral_max = 50.0f,\n');
fprintf('};\n\n');

fprintf('// Velocity Loop PID (middle - 100 Hz)\n');
fprintf('star_pid_config_t velocity_pid_config = {\n');
fprintf('    .kp = %.6ff,\n', C_velocity.Kp);
fprintf('    .ki = %.6ff,\n', C_velocity.Ki);
fprintf('    .kd = %.6ff,\n', C_velocity.Kd);
fprintf('    .output_min = -%.1ff,   // Max current command (A)\n', I_stall);
fprintf('    .output_max = %.1ff,    // Max current command (A)\n', I_stall);
fprintf('    .integral_min = -%.1ff,\n', I_stall * 0.5);
fprintf('    .integral_max = %.1ff,\n', I_stall * 0.5);
fprintf('};\n\n');

fprintf('// Position Loop PID (outermost - 10 Hz from Nav2)\n');
fprintf('star_pid_config_t position_pid_config = {\n');
fprintf('    .kp = %.6ff,\n', C_position.Kp);
fprintf('    .ki = %.6ff,\n', C_position.Ki);
fprintf('    .kd = %.6ff,\n', C_position.Kd);
fprintf('    .output_min = -%.1ff,   // Max velocity command (rad/s)\n', omega_no_load);
fprintf('    .output_max = %.1ff,    // Max velocity command (rad/s)\n', omega_no_load);
fprintf('    .integral_min = -%.1ff,\n', omega_no_load * 0.5);
fprintf('    .integral_max = %.1ff,\n', omega_no_load * 0.5);
fprintf('};\n');

%% Summary Table
fprintf('\n');
fprintf('=======================================================\n');
fprintf('    CASCADED CONTROL SUMMARY\n');
fprintf('=======================================================\n');
fprintf('\n');
fprintf('Loop       | Kp       | Ki       | Kd       | BW (rad/s) | PM (deg)\n');
fprintf('-----------+----------+----------+----------+------------+---------\n');
fprintf('Current    | %8.4f | %8.4f | %8.4f | %10.1f | %7.1f\n', ...
        C_current.Kp, C_current.Ki, 0, info_current.CrossoverFrequency, info_current.PhaseMargin);
fprintf('Velocity   | %8.4f | %8.4f | %8.4f | %10.1f | %7.1f\n', ...
        C_velocity.Kp, C_velocity.Ki, C_velocity.Kd, info_velocity.CrossoverFrequency, info_velocity.PhaseMargin);
fprintf('Position   | %8.4f | %8.4f | %8.4f | %10.1f | %7.1f\n', ...
        C_position.Kp, C_position.Ki, C_position.Kd, info_position.CrossoverFrequency, info_position.PhaseMargin);

%% Alternative: Velocity-Only Control (Simpler)
fprintf('\n');
fprintf('=======================================================\n');
fprintf('    SIMPLIFIED: VELOCITY LOOP ONLY\n');
fprintf('=======================================================\n');
fprintf('\n');
fprintf('For simpler control without current sensing, use velocity-only:\n\n');

% Direct velocity control (no inner current loop)
G_velocity_direct = Kt / (J_total * R_armature * s + (b * R_armature + Kt * Ke));
[C_vel_direct, info_vel_direct] = pidtune(G_velocity_direct, 'PID', ...
    pidtuneOptions('CrossoverFrequency', 20));

fprintf('// Velocity-Only PID (no current loop)\n');
fprintf('star_pid_config_t velocity_only_config = {\n');
fprintf('    .kp = %.6ff,\n', C_vel_direct.Kp);
fprintf('    .ki = %.6ff,\n', C_vel_direct.Ki);
fprintf('    .kd = %.6ff,\n', C_vel_direct.Kd);
fprintf('    .output_min = -100.0f,\n');
fprintf('    .output_max = 100.0f,\n');
fprintf('    .integral_min = -50.0f,\n');
fprintf('    .integral_max = 50.0f,\n');
fprintf('};\n');
