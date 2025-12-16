%% motor_model_1st_order.m - First-Order DC Motor Model
% Creates a first-order transfer function approximation for velocity control
%
% For velocity control of geared DC motors, a first-order model is often
% sufficient because the electrical time constant (tau_e ~ 0.3ms) is much
% faster than the mechanical time constant (tau_m ~ 50-100ms).
%
% Transfer Function: Omega(s)/V(s) = K / (tau*s + 1)
%
% USAGE:
%   >> motor_model_1st_order
%   >> step(motor_tf)  % View step response
%
% STAR Project - Texas A&M University
% December 2025

%% Load Motor Parameters
motor_params;

%% First-Order Model Parameters
% DC Gain: K = omega_no_load / V_rated [rad/s per volt]
K_dc = omega_no_load / V_rated;

% Time Constant: tau [seconds]
% Use mechanical time constant (electrical is negligible)
% IMPORTANT: This should be measured from actual motor step response!
% Initial estimate of 50ms based on typical geared DC motor values (literature: 30-100ms)
tau_est = 0.05;  % Estimated 50ms - MEASURE THIS ON REAL MOTOR

fprintf('\n=== First-Order Motor Model ===\n');
fprintf('DC Gain K:        %.3f (rad/s)/V\n', K_dc);
fprintf('Time Constant:    %.0f ms (ESTIMATED - measure on hardware!)\n', tau_est * 1000);

%% Create Transfer Function
s = tf('s');
motor_tf = K_dc / (tau_est * s + 1);

fprintf('\nTransfer Function: Omega(s)/V(s)\n');
motor_tf

%% Frequency Domain Analysis
fprintf('\n--- Frequency Domain Characteristics ---\n');

% Bandwidth (frequency where gain drops to -3dB)
bandwidth = 1 / tau_est;  % rad/s
fprintf('Bandwidth:        %.1f rad/s (%.1f Hz)\n', bandwidth, bandwidth/(2*pi));

% DC Gain in dB
dc_gain_db = 20 * log10(K_dc);
fprintf('DC Gain:          %.2f dB\n', dc_gain_db);

%% Time Domain Analysis
fprintf('\n--- Time Domain Characteristics ---\n');

% Step response metrics
info = stepinfo(motor_tf);
fprintf('Rise Time:        %.3f s\n', info.RiseTime);
fprintf('Settling Time:    %.3f s\n', info.SettlingTime);
fprintf('Peak Value:       %.3f rad/s (for 1V step)\n', info.Peak);

%% Plot Step Response
figure('Name', 'First-Order Motor Model - Step Response');

subplot(2,1,1);
step(motor_tf);
title('Motor Velocity Response to 1V Step Input');
ylabel('Angular Velocity (rad/s)');
xlabel('Time (s)');
grid on;

% Add annotations
hold on;
yline(K_dc, 'r--', 'Steady State', 'LabelHorizontalAlignment', 'left');
xline(tau_est, 'g--', ['\tau = ' num2str(tau_est*1000) 'ms']);
xline(info.SettlingTime, 'm--', ['T_s = ' num2str(info.SettlingTime*1000, '%.0f') 'ms']);
hold off;

subplot(2,1,2);
bode(motor_tf);
title('Motor Frequency Response');
grid on;

%% RPM Response (for practical use)
figure('Name', 'First-Order Motor Model - RPM Response');

% Convert to RPM units
K_dc_rpm = n_no_load / V_rated;  % RPM per volt
motor_tf_rpm = K_dc_rpm / (tau_est * s + 1);

step(motor_tf_rpm);
title('Motor Speed Response to 1V Step Input');
ylabel('Speed (RPM)');
xlabel('Time (s)');
grid on;

hold on;
yline(K_dc_rpm, 'r--', ['Steady State = ' num2str(K_dc_rpm, '%.0f') ' RPM/V']);
hold off;

%% Save Model to Workspace
fprintf('\n=== Model Saved to Workspace ===\n');
fprintf('motor_tf     - Transfer function Omega(s)/V(s) [rad/s / V]\n');
fprintf('motor_tf_rpm - Transfer function Speed(s)/V(s) [RPM / V]\n');
fprintf('K_dc         - DC gain [rad/s / V]\n');
fprintf('tau_est      - Time constant [s] (MEASURE THIS!)\n');

%% Instructions for Measuring Time Constant
fprintf('\n=== HOW TO MEASURE TIME CONSTANT ===\n');
fprintf('1. Apply a step voltage (e.g., 3V) to motor\n');
fprintf('2. Log encoder counts at 100Hz\n');
fprintf('3. Calculate velocity in RPM\n');
fprintf('4. Find time to reach 63.2%% of final velocity\n');
fprintf('5. Update tau_est in this script\n');
fprintf('6. Or use system_id_template.m for automated fitting\n');
