%% pid_design_velocity.m - Velocity Loop PID Design
% Designs a PID controller for motor velocity control using MATLAB's
% automatic tuning tools (pidtune) and interactive tuner (pidTuner).
%
% The gains are compatible with the ESP32 star_pid library parallel form:
%   u = Kp*e + Ki*integral(e) + Kd*d(e)/dt
%
% USAGE:
%   >> pid_design_velocity        % Auto-tune and display results
%   >> pidTuner(motor_tf, 'PID')  % Interactive tuning
%
% STAR Project - Texas A&M University
% December 2025

%% Load Motor Model
motor_model_1st_order;

fprintf('\n');
fprintf('=======================================================\n');
fprintf('       VELOCITY LOOP PID DESIGN FOR STAR ROBOT\n');
fprintf('=======================================================\n');

%% Design Specifications
fprintf('\n=== Design Specifications ===\n');

% Target specifications for velocity control
target_bandwidth = 20;        % rad/s - desired closed-loop bandwidth
target_phase_margin = 60;     % degrees - for stability margin
target_settling_time = 0.1;   % seconds - 100ms settling

fprintf('Target Bandwidth:      %.0f rad/s (%.1f Hz)\n', target_bandwidth, target_bandwidth/(2*pi));
fprintf('Target Phase Margin:   %.0f degrees\n', target_phase_margin);
fprintf('Target Settling Time:  %.0f ms\n', target_settling_time * 1000);

%% Automatic PID Tuning - Default
fprintf('\n=== Auto-Tuned PID (Default) ===\n');

% Use MATLAB's pidtune for automatic tuning
[C_pid_default, info_default] = pidtune(motor_tf, 'PID');

fprintf('Default Auto-Tune Results:\n');
fprintf('  Kp = %.4f\n', C_pid_default.Kp);
fprintf('  Ki = %.4f\n', C_pid_default.Ki);
fprintf('  Kd = %.4f\n', C_pid_default.Kd);
fprintf('  Achieved Bandwidth: %.1f rad/s\n', info_default.CrossoverFrequency);
fprintf('  Phase Margin:       %.1f degrees\n', info_default.PhaseMargin);

%% Automatic PID Tuning - With Bandwidth Target
fprintf('\n=== Auto-Tuned PID (Bandwidth = %.0f rad/s) ===\n', target_bandwidth);

opts = pidtuneOptions('CrossoverFrequency', target_bandwidth, ...
                      'PhaseMargin', target_phase_margin);
[C_pid_bw, info_bw] = pidtune(motor_tf, 'PID', opts);

fprintf('Bandwidth-Targeted Results:\n');
fprintf('  Kp = %.4f\n', C_pid_bw.Kp);
fprintf('  Ki = %.4f\n', C_pid_bw.Ki);
fprintf('  Kd = %.4f\n', C_pid_bw.Kd);
fprintf('  Achieved Bandwidth: %.1f rad/s\n', info_bw.CrossoverFrequency);
fprintf('  Phase Margin:       %.1f degrees\n', info_bw.PhaseMargin);

%% Automatic PI Tuning (No Derivative)
fprintf('\n=== Auto-Tuned PI (No Derivative) ===\n');

[C_pi, info_pi] = pidtune(motor_tf, 'PI', opts);

fprintf('PI Controller Results:\n');
fprintf('  Kp = %.4f\n', C_pi.Kp);
fprintf('  Ki = %.4f\n', C_pi.Ki);
fprintf('  Kd = 0 (PI only)\n');
fprintf('  Achieved Bandwidth: %.1f rad/s\n', info_pi.CrossoverFrequency);
fprintf('  Phase Margin:       %.1f degrees\n', info_pi.PhaseMargin);

%% Select Best Controller
% Use bandwidth-targeted PID as default
C_pid = C_pid_bw;
Kp = C_pid.Kp;
Ki = C_pid.Ki;
Kd = C_pid.Kd;

fprintf('\n=== SELECTED CONTROLLER ===\n');
fprintf('Using bandwidth-targeted PID:\n');
fprintf('  Kp = %.4f\n', Kp);
fprintf('  Ki = %.4f\n', Ki);
fprintf('  Kd = %.4f\n', Kd);

%% Closed-Loop Analysis
fprintf('\n=== Closed-Loop Analysis ===\n');

% Create closed-loop system
sys_cl = feedback(C_pid * motor_tf, 1);

% Step response info
info_cl = stepinfo(sys_cl);
fprintf('Step Response Metrics:\n');
fprintf('  Rise Time:      %.3f s\n', info_cl.RiseTime);
fprintf('  Settling Time:  %.3f s\n', info_cl.SettlingTime);
fprintf('  Overshoot:      %.1f%%\n', info_cl.Overshoot);
fprintf('  Peak:           %.3f rad/s (for 1 rad/s step)\n', info_cl.Peak);

% Stability margins
[Gm, Pm, Wcg, Wcp] = margin(C_pid * motor_tf);
fprintf('\nStability Margins:\n');
fprintf('  Gain Margin:    %.1f dB at %.1f rad/s\n', 20*log10(Gm), Wcg);
fprintf('  Phase Margin:   %.1f degrees at %.1f rad/s\n', Pm, Wcp);

%% Sensitivity Analysis
fprintf('\n=== Sensitivity Analysis ===\n');

S = feedback(1, C_pid * motor_tf);          % Sensitivity
T = feedback(C_pid * motor_tf, 1);          % Complementary sensitivity
SG = feedback(motor_tf, C_pid * motor_tf);  % Load disturbance

% Peak sensitivities
[Ms, wMs] = getPeakGain(S);
[Mt, wMt] = getPeakGain(T);

fprintf('Peak Sensitivity:  Ms = %.2f (%.1f dB) at %.1f rad/s\n', Ms, 20*log10(Ms), wMs);
fprintf('Peak Comp. Sens.:  Mt = %.2f (%.1f dB) at %.1f rad/s\n', Mt, 20*log10(Mt), wMt);

if Ms < 2
    fprintf('Sensitivity OK (Ms < 2)\n');
else
    fprintf('WARNING: High sensitivity (Ms >= 2). Consider reducing bandwidth.\n');
end

%% Plot Results
figure('Name', 'Velocity PID Design Results', 'Position', [100 100 1200 800]);

% Step Response Comparison
subplot(2,3,1);
step(sys_cl, feedback(C_pi * motor_tf, 1), feedback(C_pid_default * motor_tf, 1));
title('Closed-Loop Step Response');
ylabel('Angular Velocity (rad/s)');
legend('PID (BW target)', 'PI only', 'PID (default)', 'Location', 'southeast');
grid on;

% Open-Loop Bode
subplot(2,3,2);
margin(C_pid * motor_tf);
title('Open-Loop Frequency Response');

% Sensitivity Functions
subplot(2,3,3);
bodemag(S, T, SG);
title('Sensitivity Functions');
legend('S (sensitivity)', 'T (comp. sens.)', 'SG (dist. reject.)', 'Location', 'southwest');
grid on;

% Nichols Chart
subplot(2,3,4);
nichols(C_pid * motor_tf);
ngrid;
title('Nichols Chart');

% Root Locus
subplot(2,3,5);
rlocus(motor_tf);
title('Root Locus (Plant Only)');
grid on;

% Closed-Loop Poles
subplot(2,3,6);
pzmap(sys_cl);
title('Closed-Loop Pole-Zero Map');
grid on;

%% Format for ESP32 star_pid Library
fprintf('\n');
fprintf('=======================================================\n');
fprintf('       ESP32 star_pid CONFIGURATION\n');
fprintf('=======================================================\n');
fprintf('\n');
fprintf('// Copy this to your ESP32 initialization code:\n');
fprintf('star_pid_config_t velocity_pid_config = {\n');
fprintf('    .kp = %.6ff,\n', Kp);
fprintf('    .ki = %.6ff,\n', Ki);
fprintf('    .kd = %.6ff,\n', Kd);
fprintf('    .output_min = -100.0f,   // -100%% duty cycle\n');
fprintf('    .output_max = 100.0f,    // +100%% duty cycle\n');
fprintf('    .integral_min = -50.0f,  // Anti-windup\n');
fprintf('    .integral_max = 50.0f,   // Anti-windup\n');
fprintf('};\n');
fprintf('star_pid_init(&velocity_pid, &velocity_pid_config);\n');

%% Interactive Tuning Option
fprintf('\n=== Interactive Tuning ===\n');
fprintf('For interactive tuning, run:\n');
fprintf('  >> pidTuner(motor_tf, ''PID'')\n');
fprintf('\nThis opens a GUI to adjust gains and see real-time response.\n');

%% Save to Workspace
fprintf('\n=== Saved to Workspace ===\n');
fprintf('C_pid      - PID controller object\n');
fprintf('Kp, Ki, Kd - Individual gains\n');
fprintf('sys_cl     - Closed-loop transfer function\n');
