%% sync_pid_sim.m - 4-Motor Synchronization PID Simulation
%
% Simulates the STAR robot's 4-motor velocity control + synchronization
% layer under a motor degradation scenario.
%
% Motor model (from CLAUDE.md, identified):
%   G(s) = 3.665 / (0.075*s + 1)   [rad/s / duty]
%
% Synchronization strategy: minimum-velocity tracking with EMA filter.
%   v_sync = min(v_ema[0..3]) over non-faulted motors
%   sp[i]  = min(v_commanded, v_sync)
%
% Scenario:
%   t = 0 s   : all 4 motors commanded at 210 RPM
%   t = 2 s   : all converged (verify individual PID)
%   t = 5 s   : motor 2 plant gain degraded to 50% (worn motor)
%   t = 5..10 : sync layer drags all motors down to match motor 2
%
% Success criteria:
%   - Individual settling: all motors reach 210 RPM within 500 ms of t=0
%   - Sync settling: all motors reach motor 2's degraded speed within 500 ms
%   - Steady-state sync error: max|v_i - v_sync| < 5 RPM after settling
%
% Requires: motor_model_1st_order.m, pid_design_velocity.m
%
% STAR Project
% Copyright (c) 2026 Locked Inc.

clear; clc;
fprintf('=== STAR 4-Motor Sync PID Simulation ===\n\n');

%% 1. Motor model (1st order, from CLAUDE.md)
K     = 3.665;    % rad/s per unit duty [-1, +1]
tau   = 0.075;    % mechanical time constant [s]

% Convert commanded RPM to rad/s: omega = rpm * 2*pi/60
rpm2rads = 2 * pi / 60;
rads2rpm = 60 / (2 * pi);

%% 2. PI controller design via pidtune (builds on pid_design_velocity.m)
s        = tf('s');
motor_tf = K / (tau * s + 1);

opts   = pidtuneOptions('CrossoverFrequency', 20, 'PhaseMargin', 65);
C_pi   = pidtune(motor_tf, 'PI', opts);
Kp_pi  = C_pi.Kp;
Ki_pi  = C_pi.Ki;

fprintf('PI gains (pidtune, 20 rad/s bandwidth):\n');
fprintf('  Kp = %.6f\n', Kp_pi);
fprintf('  Ki = %.6f\n\n', Ki_pi);

%% 3. Discretize at Ts = 4 ms (250 Hz) via Tustin
Ts  = 0.004;
C_d = c2d(C_pi, Ts, 'tustin');

% Velocity-form coefficients:  u[k] = u[k-1] + Q0*e[k] + Q1*e[k-1]
[num, den] = tfdata(C_d, 'v');
Q0 =  num(1);
Q1 =  num(2);
% den(1) = 1, den(2) = -1 for integrator in velocity form

fprintf('Discrete velocity-form (Ts = %.0f ms):\n', Ts*1000);
fprintf('  Q0 = %.6f\n', Q0);
fprintf('  Q1 = %.6f\n\n', Q1);

%% 4. Simulation parameters
T_sim       = 10.0;       % total simulation time [s]
N           = round(T_sim / Ts);
t_vec       = (0:N-1) * Ts;

v_cmd_rpm   = 210.0;      % commanded speed for all motors [RPM]
v_cmd_rads  = v_cmd_rpm * rpm2rads;

t_degrade   = 5.0;        % time at which motor 2 degrades [s]
degrade_fac = 0.50;       % motor 2 plant gain drops to 50%

% EMA filter coefficient for sync layer (matches firmware rx_sync.c)
ema_alpha   = 0.30;       % alpha = 0.3 -> ~3 Hz cutoff at 250 Hz

% Fault exclusion: if v < threshold for > fault_ticks samples, exclude from min
fault_thresh_frac = 0.05;          % 5% of commanded speed
fault_ticks_thresh = round(0.5 / Ts);  % 500 ms

%% 5. State variables
NUM_MOTORS = 4;

% Motor plant states (continuous motor simulated as Euler 1st order)
v_plant   = zeros(NUM_MOTORS, N);  % plant velocity [rad/s]

% PID states
u_pid     = zeros(NUM_MOTORS, 1);  % previous control output (velocity form)
e_prev    = zeros(NUM_MOTORS, 1);  % previous error

% Sync layer states
v_ema     = zeros(NUM_MOTORS, 1);  % EMA-filtered velocities
v_sync_log = zeros(1, N);
sp_log    = zeros(NUM_MOTORS, N);  % effective setpoints [rad/s]
fault_cnt = zeros(NUM_MOTORS, 1);
faulted   = false(NUM_MOTORS, 1);

% Output logs
u_log     = zeros(NUM_MOTORS, N);
v_rpm_log = zeros(NUM_MOTORS, N);

%% 6. Discrete simulation loop
for k = 2:N
    t = (k - 1) * Ts;

    % Motor 2 plant gain degraded after t_degrade
    K_eff = ones(NUM_MOTORS, 1) * K;
    if t >= t_degrade
        K_eff(3) = K * degrade_fac;  % motor index 3 = motor 2 (0-based in plan)
    end

    % --- Sync layer ---
    % Update EMA for each motor
    for m = 1:NUM_MOTORS
        v_ema(m) = ema_alpha * v_plant(m, k-1) + (1 - ema_alpha) * v_ema(m);
    end

    % Fault detection
    for m = 1:NUM_MOTORS
        if v_ema(m) < fault_thresh_frac * v_cmd_rads
            fault_cnt(m) = fault_cnt(m) + 1;
        else
            fault_cnt(m) = 0;
            faulted(m)   = false;
        end
        if fault_cnt(m) > fault_ticks_thresh
            faulted(m) = true;
        end
    end

    % Compute v_sync = min of non-faulted motors
    v_valid = v_ema;
    v_valid(faulted) = Inf;
    if all(isinf(v_valid))
        v_sync = v_cmd_rads;  % all faulted: fallback to command
    else
        v_sync = min(v_valid);
    end
    v_sync_log(k) = v_sync;

    % Effective setpoint per motor
    sp = min(v_cmd_rads, v_sync) * ones(NUM_MOTORS, 1);
    sp_log(:, k) = sp;

    % --- Per-motor PI (velocity form) ---
    for m = 1:NUM_MOTORS
        e     = sp(m) - v_plant(m, k-1);
        du    = Q0 * e + Q1 * e_prev(m);
        u_new = u_pid(m) + du;
        u_new = max(-1.0, min(1.0, u_new));  % duty clamp [-1, +1]

        u_pid(m)    = u_new;
        e_prev(m)   = e;
        u_log(m, k) = u_new;

        % Plant: Euler integration of  tau * dv/dt + v = K_eff * u
        dv = (K_eff(m) * u_new - v_plant(m, k-1)) / tau;
        v_plant(m, k) = v_plant(m, k-1) + dv * Ts;
        v_plant(m, k) = max(0, v_plant(m, k));  % clamp: no negative velocity
    end

    v_rpm_log(:, k) = v_plant(:, k) * rads2rpm;
end

%% 7. Performance metrics
fprintf('=== Performance Metrics ===\n\n');

% Individual settling (t = 0 to 2s)
idx_settle = t_vec >= 0 & t_vec <= 2.0;
settled_at_2s = v_rpm_log(:, idx_settle);
fprintf('At t=2s (before degradation):\n');
for m = 1:NUM_MOTORS
    v_end = mean(settled_at_2s(m, end-5:end));
    fprintf('  Motor %d: %.1f RPM  (target %.0f RPM, error %.1f RPM)\n', ...
            m-1, v_end, v_cmd_rpm, v_cmd_rpm - v_end);
end

% Sync settling (t = 5s to 6s)
idx_sync  = t_vec >= 5.5 & t_vec <= 6.0;
synced    = v_rpm_log(:, idx_sync);
v_sync_ss = mean(v_sync_log(idx_sync));
fprintf('\nAt t=5.5..6s (0.5s after degradation):\n');
fprintf('  v_sync = %.1f RPM\n', v_sync_ss);
for m = 1:NUM_MOTORS
    v_ss = mean(synced(m, :));
    fprintf('  Motor %d: %.1f RPM  sync_error = %.1f RPM\n', ...
            m-1, v_ss, abs(v_ss - v_sync_ss));
end

% Steady-state sync error (t = 8..10s)
idx_ss   = t_vec >= 8.0;
ss_data  = v_rpm_log(:, idx_ss);
v_ss_vec = mean(ss_data, 2);
sync_err = max(abs(v_ss_vec - min(v_ss_vec)));
fprintf('\nSteady-state (t=8..10s):\n');
fprintf('  max sync error = %.2f RPM  ', sync_err);
fprintf('%s\n', sync_err < 5.0 ? '[PASS]' : '[FAIL - target < 5 RPM]');

%% 8. Plots
colors = lines(NUM_MOTORS);
motor_names = {'Motor 0 (FL)', 'Motor 1 (FR)', 'Motor 2 (RL) [degrades]', 'Motor 3 (RR)'};

figure('Name', 'STAR 4-Motor Sync PID Simulation', ...
       'Position', [50 50 1400 900], 'Color', 'w');

% --- Subplot 1: velocity traces ---
ax1 = subplot(3, 2, [1 2]);
hold on;
for m = 1:NUM_MOTORS
    plot(t_vec, v_rpm_log(m, :), 'Color', colors(m, :), ...
         'LineWidth', 1.5, 'DisplayName', motor_names{m});
end
plot(t_vec, v_sync_log * rads2rpm, 'k--', 'LineWidth', 2, 'DisplayName', 'v\_sync');
xline(t_degrade, 'r:', 'LineWidth', 1.5, 'DisplayName', 'Motor 2 degrades');
yline(v_cmd_rpm, 'b:', 'Alpha', 0.4, 'DisplayName', 'Command');
xlabel('Time (s)'); ylabel('Speed (RPM)');
title('Motor Velocities + Sync Reference');
legend('Location', 'southwest'); grid on;
xlim([0 T_sim]); ylim([0 v_cmd_rpm * 1.1]);

% --- Subplot 2: sync error ---
ax2 = subplot(3, 2, 3);
v_sync_rpm = v_sync_log * rads2rpm;
hold on;
for m = 1:NUM_MOTORS
    sync_err_trace = abs(v_rpm_log(m, :) - v_sync_rpm);
    plot(t_vec, sync_err_trace, 'Color', colors(m, :), 'LineWidth', 1.2, ...
         'DisplayName', sprintf('Motor %d', m-1));
end
yline(5.0, 'r--', '5 RPM threshold', 'LineWidth', 1.2);
xline(t_degrade, 'r:', 'LineWidth', 1.5);
xlabel('Time (s)'); ylabel('|v_i - v_{sync}| (RPM)');
title('Per-Motor Sync Error');
legend('Location', 'northeast'); grid on;
xlim([0 T_sim]); ylim([0 30]);

% --- Subplot 3: control outputs ---
ax3 = subplot(3, 2, 4);
hold on;
for m = 1:NUM_MOTORS
    plot(t_vec, u_log(m, :), 'Color', colors(m, :), 'LineWidth', 1.2, ...
         'DisplayName', sprintf('Motor %d', m-1));
end
xline(t_degrade, 'r:', 'LineWidth', 1.5);
xlabel('Time (s)'); ylabel('Duty Cycle [-1, +1]');
title('Control Outputs (Duty Cycle)');
legend('Location', 'northeast'); grid on;
xlim([0 T_sim]); ylim([-0.1 1.1]);

% --- Subplot 4: setpoints ---
ax4 = subplot(3, 2, 5);
hold on;
for m = 1:NUM_MOTORS
    plot(t_vec, sp_log(m, :) * rads2rpm, 'Color', colors(m, :), 'LineWidth', 1.2, ...
         'DisplayName', sprintf('Motor %d', m-1));
end
xline(t_degrade, 'r:', 'LineWidth', 1.5);
xlabel('Time (s)'); ylabel('Setpoint (RPM)');
title('Effective Setpoints (after sync layer)');
legend('Location', 'southwest'); grid on;
xlim([0 T_sim]); ylim([0 v_cmd_rpm * 1.1]);

% --- Subplot 5: zoom on degradation event ---
ax5 = subplot(3, 2, 6);
zoom_range = t_vec >= 4.5 & t_vec <= 7.0;
hold on;
for m = 1:NUM_MOTORS
    plot(t_vec(zoom_range), v_rpm_log(m, zoom_range), 'Color', colors(m, :), ...
         'LineWidth', 2.0, 'DisplayName', motor_names{m});
end
plot(t_vec(zoom_range), v_sync_rpm(zoom_range), 'k--', 'LineWidth', 2, ...
     'DisplayName', 'v\_sync');
xline(t_degrade, 'r:', 'LineWidth', 1.5);
xlabel('Time (s)'); ylabel('Speed (RPM)');
title('Zoom: Degradation Event (t=4.5..7s)');
legend('Location', 'southwest'); grid on;

linkaxes([ax1 ax2 ax3 ax4], 'x');

% Save figure
out_path = fullfile(fileparts(mfilename('fullpath')), '..', 'figures');
if ~exist(out_path, 'dir'); mkdir(out_path); end
saveas(gcf, fullfile(out_path, 'sync_pid_sim.png'));
fprintf('\nFigure saved: figures/sync_pid_sim.png\n');

%% 9. Export gains for firmware
fprintf('\n=== Firmware Gains (rx_pid/pid_gains_auto.h) ===\n');
fprintf('#define PID_KP  %.6ff\n', Kp_pi);
fprintf('#define PID_KI  %.6ff\n', Ki_pi);
fprintf('#define PID_Q0  %.6ff\n', Q0);
fprintf('#define PID_Q1  %.6ff\n', Q1);
fprintf('#define PID_TS  %.6ff\n', Ts);
fprintf('#define SYNC_EMA_ALPHA  %.2ff\n', ema_alpha);
