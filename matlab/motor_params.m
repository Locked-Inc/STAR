%% motor_params.m - STAR Robot Motor Parameters
% DFRobot FIT0520 DC Gear Motor Specifications
% Source: Locked_Inc_FTP/sections/15_implementation_electrical.tex
%
% USAGE:
%   Run this script to load motor parameters into workspace:
%   >> motor_params
%
% STAR Project - Texas A&M University
% December 2025

%% Motor Electrical Specifications (from FTP Table: Motor and Driver Specifications)
V_rated = 6;                    % Rated voltage [V]
n_no_load = 210;                % No-load speed [RPM]
I_no_load = 0.13;               % No-load current [A] (estimated)
I_stall = 3.2;                  % Stall current [A]
T_stall_kgcm = 10;              % Stall torque [kg*cm]

%% Motor Mechanical Specifications
gear_ratio = 34.02;             % Gear reduction ratio (34:1 nominal)
encoder_ppr = 341.2;            % Pulses per revolution (after gearbox)

%% Unit Conversions
T_stall_Nm = T_stall_kgcm * 0.0981;  % Stall torque [N*m] = 0.981 N*m

%% Derived Electrical Parameters
R_armature = V_rated / I_stall;      % Armature resistance [Ohm] = 1.875 Ohm
% Note: Armature inductance is typically small for brushed DC motors
% Estimate L ~ 0.5 mH (to be measured)
L_armature = 0.5e-3;                 % Armature inductance [H] (estimated)

%% Derived Motor Constants
% Torque constant: Kt = T_stall / I_stall
Kt = T_stall_Nm / I_stall;           % Torque constant [N*m/A] = 0.307 N*m/A

% Back-EMF constant: Ke = (V - I*R) / omega at no-load
omega_no_load = n_no_load * 2 * pi / 60;  % No-load speed [rad/s] = 22 rad/s
Ke = (V_rated - I_no_load * R_armature) / omega_no_load;  % Back-EMF constant [V/(rad/s)]

%% Derived Mechanical Parameters (Estimates - To Be Measured)
% Motor rotor inertia (before gearbox)
J_motor = 1e-6;                 % Motor rotor inertia [kg*m^2] (estimated)

% Reflected inertia at output shaft
J_load = 5e-5;                  % Load inertia [kg*m^2] (estimated for wheel)
J_total = J_motor * gear_ratio^2 + J_load;  % Total reflected inertia [kg*m^2]

% Viscous friction coefficient
b = 1e-4;                       % Viscous friction [N*m*s/rad] (estimated)

%% Time Constants
tau_e = L_armature / R_armature;     % Electrical time constant [s] = 0.27 ms
tau_m = J_total * R_armature / (Ke * Kt + b * R_armature);  % Mechanical time constant [s]

%% Control System Parameters
Ts = 0.01;                      % Sample time [s] (100 Hz motor control loop)
encoder_counts_per_sample = 48; % Expected counts per sample at max speed

%% Velocity Calculation
% At 100 Hz with encoder_ppr counts/rev:
% velocity_rpm = (delta_counts / encoder_ppr) * (60 / Ts)
% Example: 48 counts in 10ms -> (48/341.2) * 6000 = 844 RPM

%% Physical Dimensions (from FTP)
wheel_diameter_mm = 144;        % Wheel diameter [mm]
wheel_radius_m = wheel_diameter_mm / 2000;  % Wheel radius [m]
max_linear_speed = omega_no_load * wheel_radius_m;  % Max speed [m/s] = 1.58 m/s

%% Display Summary
fprintf('=== STAR Motor Parameters (DFRobot FIT0520) ===\n');
fprintf('\n--- Electrical ---\n');
fprintf('Rated Voltage:     %.1f V\n', V_rated);
fprintf('Stall Current:     %.1f A\n', I_stall);
fprintf('Armature R:        %.3f Ohm\n', R_armature);
fprintf('Armature L:        %.2f mH (estimated)\n', L_armature * 1000);
fprintf('\n--- Mechanical ---\n');
fprintf('No-load Speed:     %d RPM (%.1f rad/s)\n', n_no_load, omega_no_load);
fprintf('Stall Torque:      %.2f kg*cm (%.3f N*m)\n', T_stall_kgcm, T_stall_Nm);
fprintf('Gear Ratio:        %.2f:1\n', gear_ratio);
fprintf('Encoder PPR:       %.1f counts/rev\n', encoder_ppr);
fprintf('\n--- Motor Constants ---\n');
fprintf('Torque Constant:   %.4f N*m/A\n', Kt);
fprintf('Back-EMF Constant: %.4f V/(rad/s)\n', Ke);
fprintf('\n--- Time Constants ---\n');
fprintf('Electrical tau_e:  %.3f ms\n', tau_e * 1000);
fprintf('Mechanical tau_m:  %.3f ms (estimated)\n', tau_m * 1000);
fprintf('\n--- Control ---\n');
fprintf('Sample Time:       %.0f ms (%.0f Hz)\n', Ts * 1000, 1/Ts);
fprintf('Max Wheel Speed:   %.2f m/s\n', max_linear_speed);
fprintf('============================================\n');
