%% system_id_template.m - System Identification from Motor Data
% Template for identifying motor transfer function from experimental data.
%
% WORKFLOW:
% 1. Collect step response data from ESP32 (encoder counts at 100 Hz)
% 2. Import data into MATLAB
% 3. Fit transfer function model
% 4. Validate model
% 5. Export parameters to motor_params.m
%
% STAR Project - Texas A&M University
% December 2025

%% Configuration
clear; clc;
fprintf('=== Motor System Identification ===\n');

%% Step 1: Data Collection Instructions
fprintf('\n=== STEP 1: DATA COLLECTION ===\n');
fprintf('Run the following test on the ESP32:\n\n');
fprintf('1. Start with motor at rest\n');
fprintf('2. Apply step voltage (e.g., 50%% duty cycle = 3V)\n');
fprintf('3. Log encoder counts at 100 Hz for 2 seconds\n');
fprintf('4. Save as CSV: [time_ms, encoder_counts]\n');
fprintf('\nESP32 code example:\n');
fprintf('-------------------------------------------\n');
fprintf('// In motor control task (100 Hz)\n');
fprintf('static uint32_t start_time = 0;\n');
fprintf('static bool logging = false;\n');
fprintf('\n');
fprintf('if (button_pressed && !logging) {\n');
fprintf('    start_time = esp_timer_get_time() / 1000;\n');
fprintf('    logging = true;\n');
fprintf('    star_motor_set_duty(&motor, 50.0f);  // 50%% step\n');
fprintf('}\n');
fprintf('\n');
fprintf('if (logging) {\n');
fprintf('    uint32_t t = esp_timer_get_time() / 1000 - start_time;\n');
fprintf('    int32_t counts;\n');
fprintf('    star_encoder_get_count(&encoder, &counts);\n');
fprintf('    printf("%%u,%%d\\n", t, counts);\n');
fprintf('    if (t > 2000) logging = false;\n');
fprintf('}\n');
fprintf('-------------------------------------------\n');

%% Step 2: Load Experimental Data
fprintf('\n=== STEP 2: LOAD DATA ===\n');

% Check if data file exists
data_file = 'motor_step_data.csv';
if exist(data_file, 'file')
    fprintf('Loading data from %s...\n', data_file);
    data = readmatrix(data_file);
    t_exp = data(:,1) / 1000;  % Convert ms to seconds
    counts_exp = data(:,2);
    use_experimental = true;
else
    fprintf('No data file found (%s)\n', data_file);
    fprintf('Using SIMULATED data for demonstration...\n\n');
    use_experimental = false;

    % Generate simulated step response data
    motor_params;

    % True plant (unknown to identification)
    K_true = omega_no_load / V_rated;  % DC gain
    tau_true = 0.05;  % 50ms time constant
    G_true = tf(K_true, [tau_true 1]);

    % Simulate step response at 50% duty = 3V
    V_step = 3.0;  % 50% of 6V
    Ts = 0.01;
    t_exp = (0:Ts:2)';

    % Add measurement noise
    [y_ideal, ~] = step(G_true * V_step, t_exp);
    noise = 0.5 * randn(size(y_ideal));  % 0.5 rad/s noise
    omega_exp = y_ideal + noise;

    % Convert to encoder counts
    counts_per_rad = encoder_ppr / (2 * pi);
    counts_exp = cumtrapz(t_exp, omega_exp) * counts_per_rad;
end

%% Step 3: Process Data
fprintf('\n=== STEP 3: PROCESS DATA ===\n');

% Load motor parameters for unit conversion
motor_params;

% Calculate velocity from encoder counts
Ts = mean(diff(t_exp));
fprintf('Detected sample time: %.1f ms\n', Ts * 1000);

% Velocity = d(counts)/dt converted to rad/s
velocity_counts_per_s = [0; diff(counts_exp)] / Ts;
velocity_rads = velocity_counts_per_s / encoder_ppr * (2 * pi);
velocity_rpm = velocity_rads * (60 / (2*pi));

% Filter velocity (5-point moving average)
velocity_rads_filt = movmean(velocity_rads, 5);
velocity_rpm_filt = movmean(velocity_rpm, 5);

fprintf('Max velocity: %.1f RPM\n', max(velocity_rpm_filt));
fprintf('Final velocity: %.1f RPM\n', mean(velocity_rpm_filt(end-10:end)));

%% Step 4: Fit First-Order Model
fprintf('\n=== STEP 4: FIT FIRST-ORDER MODEL ===\n');

% Input: step voltage (assume 50% duty = 3V)
V_input = 3.0;
u_exp = V_input * ones(size(t_exp));

% Create iddata object
data_id = iddata(velocity_rads_filt, u_exp, Ts);

% Estimate first-order transfer function
% G(s) = K / (tau*s + 1)
sys_id_1 = tfest(data_id, 1, 0);  % 1 pole, 0 zeros

fprintf('Identified First-Order Model:\n');
sys_id_1

% Extract parameters
[num_id, den_id] = tfdata(sys_id_1, 'v');
K_id = num_id(end) / den_id(end);  % DC gain
tau_id = den_id(1) / den_id(2);     % Time constant

fprintf('DC Gain K:       %.4f rad/s/V\n', K_id);
fprintf('Time Constant:   %.1f ms\n', tau_id * 1000);

%% Step 5: Fit Second-Order Model
fprintf('\n=== STEP 5: FIT SECOND-ORDER MODEL ===\n');

sys_id_2 = tfest(data_id, 2, 0);  % 2 poles, 0 zeros
fprintf('Identified Second-Order Model:\n');
sys_id_2

%% Step 6: Model Validation
fprintf('\n=== STEP 6: MODEL VALIDATION ===\n');

% Simulate identified models
[y_id_1, t_sim] = step(sys_id_1 * V_input, t_exp);
[y_id_2, ~] = step(sys_id_2 * V_input, t_exp);

% Calculate fit percentage
fit_1 = 100 * (1 - norm(velocity_rads_filt - y_id_1) / norm(velocity_rads_filt - mean(velocity_rads_filt)));
fit_2 = 100 * (1 - norm(velocity_rads_filt - y_id_2) / norm(velocity_rads_filt - mean(velocity_rads_filt)));

fprintf('First-Order Model Fit:  %.1f%%\n', fit_1);
fprintf('Second-Order Model Fit: %.1f%%\n', fit_2);

if fit_1 > 85
    fprintf('First-order model is adequate for PID design.\n');
else
    fprintf('Consider using second-order model for better accuracy.\n');
end

%% Step 7: Residual Analysis
fprintf('\n=== STEP 7: RESIDUAL ANALYSIS ===\n');

residual_1 = velocity_rads_filt - y_id_1;
residual_2 = velocity_rads_filt - y_id_2;

fprintf('First-Order Residual:\n');
fprintf('  Mean:    %.4f rad/s\n', mean(residual_1));
fprintf('  Std Dev: %.4f rad/s\n', std(residual_1));
fprintf('  Max:     %.4f rad/s\n', max(abs(residual_1)));

fprintf('\nSecond-Order Residual:\n');
fprintf('  Mean:    %.4f rad/s\n', mean(residual_2));
fprintf('  Std Dev: %.4f rad/s\n', std(residual_2));
fprintf('  Max:     %.4f rad/s\n', max(abs(residual_2)));

%% Visualization
figure('Name', 'System Identification Results', 'Position', [100 100 1200 800]);

% Raw data and velocity
subplot(2,3,1);
plot(t_exp, counts_exp, 'b-');
xlabel('Time (s)');
ylabel('Encoder Counts');
title('Raw Encoder Data');
grid on;

subplot(2,3,2);
plot(t_exp, velocity_rpm, 'b-', 'LineWidth', 0.5); hold on;
plot(t_exp, velocity_rpm_filt, 'r-', 'LineWidth', 1.5);
xlabel('Time (s)');
ylabel('Velocity (RPM)');
title('Calculated Velocity');
legend('Raw', 'Filtered', 'Location', 'southeast');
grid on;

% Model comparison
subplot(2,3,3);
plot(t_exp, velocity_rads_filt, 'b-', 'LineWidth', 1.5); hold on;
plot(t_exp, y_id_1, 'r--', 'LineWidth', 1.5);
plot(t_exp, y_id_2, 'g:', 'LineWidth', 1.5);
xlabel('Time (s)');
ylabel('Velocity (rad/s)');
title('Model Comparison');
legend('Measured', sprintf('1st Order (%.1f%%)', fit_1), ...
       sprintf('2nd Order (%.1f%%)', fit_2), 'Location', 'southeast');
grid on;

% Residuals
subplot(2,3,4);
plot(t_exp, residual_1, 'r-', t_exp, residual_2, 'g-');
xlabel('Time (s)');
ylabel('Residual (rad/s)');
title('Model Residuals');
legend('1st Order', '2nd Order');
grid on;

% Bode comparison
subplot(2,3,5);
bode(sys_id_1, sys_id_2);
title('Frequency Response');
legend('1st Order', '2nd Order');
grid on;

% Pole-zero map
subplot(2,3,6);
pzmap(sys_id_1, sys_id_2);
title('Pole-Zero Map');
legend('1st Order', '2nd Order');
grid on;

%% Step 8: Export Parameters
fprintf('\n=== STEP 8: EXPORT PARAMETERS ===\n');
fprintf('\nUpdate motor_params.m with:\n\n');
fprintf('K_dc = %.4f;     %% DC gain [rad/s per volt]\n', K_id);
fprintf('tau_est = %.4f;  %% Time constant [s]\n', tau_id);
fprintf('\n');

% Also save to file
fid = fopen('identified_params.txt', 'w');
fprintf(fid, '%% Identified Motor Parameters\n');
fprintf(fid, '%% Generated: %s\n\n', datestr(now));
fprintf(fid, 'K_dc = %.6f;     %% DC gain [rad/s per volt]\n', K_id);
fprintf(fid, 'tau_est = %.6f;  %% Time constant [s]\n', tau_id);
fprintf(fid, '\n%% First-order transfer function:\n');
fprintf(fid, '%% G(s) = %.4f / (%.4f*s + 1)\n', K_id, tau_id);
fclose(fid);
fprintf('Parameters saved to identified_params.txt\n');

%% Step 9: Design PID with Identified Model
fprintf('\n=== STEP 9: PID DESIGN WITH IDENTIFIED MODEL ===\n');

% Use identified model for PID tuning
target_bw = 20;  % rad/s
opts = pidtuneOptions('CrossoverFrequency', target_bw);
[C_pid_id, info_id] = pidtune(sys_id_1, 'PID', opts);

fprintf('PID Gains (from identified model):\n');
fprintf('  Kp = %.6f\n', C_pid_id.Kp);
fprintf('  Ki = %.6f\n', C_pid_id.Ki);
fprintf('  Kd = %.6f\n', C_pid_id.Kd);
fprintf('  Bandwidth: %.1f rad/s\n', info_id.CrossoverFrequency);
fprintf('  Phase Margin: %.1f degrees\n', info_id.PhaseMargin);

%% Summary
fprintf('\n');
fprintf('=======================================================\n');
fprintf('    SYSTEM IDENTIFICATION COMPLETE\n');
fprintf('=======================================================\n');
fprintf('\n');
fprintf('Identified Model: G(s) = %.4f / (%.4fs + 1)\n', K_id, tau_id);
fprintf('Model Fit: %.1f%%\n', fit_1);
fprintf('\n');
fprintf('Recommended PID (for star_pid):\n');
fprintf('  Kp = %.6f\n', C_pid_id.Kp);
fprintf('  Ki = %.6f\n', C_pid_id.Ki);
fprintf('  Kd = %.6f\n', C_pid_id.Kd);
