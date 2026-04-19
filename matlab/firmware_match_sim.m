%% firmware_match_sim.m - Firmware-Mirror Simulation
%
% Mirrors the rx_sync.c and rx_pid.c firmware logic exactly in MATLAB using
% the same integer-safe arithmetic and clamping.  If this simulation
% converges but the actual firmware does not, the fault is downstream of the
% control law (timing, peripheral configuration, encoder wiring).
%
% Key firmware constants mirrored here:
%   Ts         = 0.004 s   (250 Hz)
%   DUTY_CLAMP = 1.0       ([-1, +1] duty range)
%   EMA_ALPHA  = 0.30
%   Q0, Q1     from pid_gains_auto.h (run sync_pid_sim.m to generate)
%
% Scenario (Phase B CSV):
%   t = 0..5 s   : all motors at 210 RPM
%   t = 5..10 s  : motor 2 at 60% of nominal gain
%
% Expected output:
%   All motors track 210 RPM initially.
%   After t=5s, all motors converge to ~motor 2's degraded steady state
%   within 500 ms, with sync error < 5 RPM in steady state.
%
% STAR Project
% Copyright (c) 2026 Locked Inc.

clear; clc;
fprintf('=== Firmware-Mirror Simulation ===\n');
fprintf('(Mirrors rx_sync.c + rx_pid.c exactly)\n\n');

%% 1. Gains -- copy from sync_pid_sim.m output or pid_gains_auto.h
%    These MUST match what is compiled into the firmware.
PID_KP = 0.286000;   % placeholder -- replace with sync_pid_sim.m output
PID_KI = 8.010000;   % placeholder -- replace with sync_pid_sim.m output
PID_TS = 0.004000;
PID_Q0 = 0.0;        % will be computed below from Kp/Ki
PID_Q1 = 0.0;

% Compute velocity-form coefficients (same math as rx_pid.c discretisation)
% Forward Euler: Q0 = Kp + Ki*Ts/2, Q1 = -(Kp - Ki*Ts/2)  (Tustin)
PID_Q0 = PID_KP + PID_KI * PID_TS / 2.0;
PID_Q1 = -(PID_KP - PID_KI * PID_TS / 2.0);
fprintf('Q0 = %.6f  Q1 = %.6f\n\n', PID_Q0, PID_Q1);

SYNC_EMA_ALPHA     = 0.30;
DUTY_CLAMP         = 1.0;
FAULT_THRESH_FRAC  = 0.05;
FAULT_TICKS_THRESH = round(0.5 / PID_TS);   % 500 ms

%% 2. Motor plant
K   = 3.665;   % rad/s per duty unit
tau = 0.075;   % s

rpm2rads   = 2 * pi / 60;
rads2rpm   = 60 / (2 * pi);
v_cmd_rpm  = 210.0;
v_cmd_rads = v_cmd_rpm * rpm2rads;

T_sim      = 10.0;
N          = round(T_sim / PID_TS);
t_vec      = (0:N-1) * PID_TS;
NUM_MOTORS = 4;
t_degrade  = 5.0;
degrade_fac = 0.60;   % Phase B: motor 2 at 60%

%% 3. State (single-precision floats -- firmware uses float32)
v_plant   = single(zeros(NUM_MOTORS, N));
u_out     = single(zeros(NUM_MOTORS, 1));   % u[k-1] (velocity form accumulated)
e_prev    = single(zeros(NUM_MOTORS, 1));
v_ema     = single(zeros(NUM_MOTORS, 1));
fault_cnt = zeros(NUM_MOTORS, 1, 'int32');
faulted   = false(NUM_MOTORS, 1);

v_sync_log = single(zeros(1, N));
sp_log     = single(zeros(NUM_MOTORS, N));
u_log      = single(zeros(NUM_MOTORS, N));

%% 4. Simulation loop (mirrors ThreadX 250 Hz task)
for k = 2:N
    t = single((k-1) * PID_TS);

    %--- rx_sync_update() ---
    for m = 1:NUM_MOTORS
        v_ema(m) = single(SYNC_EMA_ALPHA) * v_plant(m, k-1) ...
                 + single(1.0 - SYNC_EMA_ALPHA) * v_ema(m);
    end
    for m = 1:NUM_MOTORS
        if double(v_ema(m)) < FAULT_THRESH_FRAC * double(v_cmd_rads)
            fault_cnt(m) = fault_cnt(m) + int32(1);
        else
            fault_cnt(m) = int32(0);
            faulted(m)   = false;
        end
        if fault_cnt(m) > int32(FAULT_TICKS_THRESH)
            faulted(m) = true;
        end
    end

    v_valid = double(v_ema);
    v_valid(faulted) = Inf;
    if all(isinf(v_valid)); v_sync = single(v_cmd_rads);
    else; v_sync = single(min(v_valid)); end
    v_sync_log(k) = v_sync;

    %--- rx_sync_get_setpoint() ---
    sp = single(min(double(v_cmd_rads), double(v_sync))) * ones(NUM_MOTORS, 1, 'single');
    sp_log(:, k) = sp;

    %--- rx_pid_compute() x4 ---
    K_eff = single(K) * ones(NUM_MOTORS, 1, 'single');
    if double(t) >= t_degrade
        K_eff(3) = single(K * degrade_fac);
    end

    for m = 1:NUM_MOTORS
        e     = sp(m) - v_plant(m, k-1);
        du    = single(PID_Q0) * e + single(PID_Q1) * e_prev(m);
        u_new = u_out(m) + du;
        % rx_pid clamp
        u_new = single(max(-single(DUTY_CLAMP), min(single(DUTY_CLAMP), u_new)));
        u_out(m)  = u_new;
        e_prev(m) = e;
        u_log(m, k) = u_new;

        % rx_motor_set_duty -> plant
        dv = (K_eff(m) * u_new - v_plant(m, k-1)) / single(tau);
        v_plant(m, k) = single(max(single(0), v_plant(m, k-1) + dv * single(PID_TS)));
    end
end

v_rpm_log = double(v_plant) * rads2rpm;

%% 5. Metrics
fprintf('=== Firmware-Mirror Metrics ===\n');

idx_before = t_vec >= 4.0 & t_vec < 5.0;
fprintf('Before degradation (t=4..5s):\n');
for m = 1:NUM_MOTORS
    v_ss = mean(v_rpm_log(m, idx_before));
    fprintf('  Motor %d: %.2f RPM\n', m-1, v_ss);
end

idx_after = t_vec >= 8.0;
ss = v_rpm_log(:, idx_after);
v_ss_vec = mean(ss, 2);
sync_err_ss = max(abs(v_ss_vec - min(v_ss_vec)));
fprintf('\nSteady-state after degradation (t=8..10s):\n');
for m = 1:NUM_MOTORS
    fprintf('  Motor %d: %.2f RPM\n', m-1, v_ss_vec(m));
end
fprintf('Max sync error: %.2f RPM  %s\n', sync_err_ss, ...
        sync_err_ss < 5.0 ? '[PASS]' : '[FAIL]');

%% 6. Plots
colors = lines(NUM_MOTORS);
motor_names = {'Motor 0', 'Motor 1', 'Motor 2 (degrades @t=5s)', 'Motor 3'};

figure('Name', 'Firmware-Mirror Simulation (Phase B)', ...
       'Position', [100 50 1200 700], 'Color', 'w');

subplot(2, 1, 1);
hold on;
for m = 1:NUM_MOTORS
    plot(t_vec, v_rpm_log(m, :), 'Color', colors(m,:), 'LineWidth', 1.5, ...
         'DisplayName', motor_names{m});
end
plot(t_vec, double(v_sync_log) * rads2rpm, 'k--', 'LineWidth', 2, ...
     'DisplayName', 'v\_sync');
xline(t_degrade, 'r:', 'LineWidth', 1.5);
yline(v_cmd_rpm, 'b:', 'Alpha', 0.4);
legend('Location', 'southwest'); grid on;
xlabel('Time (s)'); ylabel('Speed (RPM)');
title('Firmware-Mirror: Motor Velocities (Phase B CSV scenario)');
xlim([0 T_sim]);

subplot(2, 1, 2);
hold on;
for m = 1:NUM_MOTORS
    plot(t_vec, double(u_log(m, :)), 'Color', colors(m,:), 'LineWidth', 1.2, ...
         'DisplayName', motor_names{m});
end
xline(t_degrade, 'r:', 'LineWidth', 1.5);
legend('Location', 'northeast'); grid on;
xlabel('Time (s)'); ylabel('Duty Cycle');
title('Control Outputs');
xlim([0 T_sim]);

out_path = fullfile(fileparts(mfilename('fullpath')), '..', 'figures');
if ~exist(out_path, 'dir'); mkdir(out_path); end
saveas(gcf, fullfile(out_path, 'firmware_match_sim.png'));
fprintf('\nFigure saved: figures/firmware_match_sim.png\n');
fprintf('\nIf this passes but firmware does not: fault is in peripheral/timing, not algorithm.\n');
