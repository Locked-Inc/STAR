%% sync_robustness_sweep.m - Sync PID Gain Robustness Sweep
%
% Sweeps motor degradation level (20%..80% of nominal gain) and EMA alpha
% (0.1..0.5) and reports the worst-case steady-state sync error.
%
% Output: heatmap of max sync error (RPM) vs. (degrade%, alpha).
%
% Run sync_pid_sim.m first to confirm the design is working, then run this
% to verify the gain margins are acceptable.
%
% STAR Project
% Copyright (c) 2026 Locked Inc.

clear; clc;
fprintf('=== Sync PID Robustness Sweep ===\n\n');

%% Motor model + controller (same as sync_pid_sim.m)
K         = 3.665;
tau       = 0.075;
Ts        = 0.004;
rpm2rads  = 2 * pi / 60;
rads2rpm  = 60 / (2 * pi);
v_cmd_rads = 210.0 * rpm2rads;

s        = tf('s');
motor_tf = K / (tau * s + 1);
opts     = pidtuneOptions('CrossoverFrequency', 20, 'PhaseMargin', 65);
C_pi     = pidtune(motor_tf, 'PI', opts);
C_d      = c2d(C_pi, Ts, 'tustin');
[num, ~] = tfdata(C_d, 'v');
Q0 = num(1); Q1 = num(2);

%% Sweep parameters
degrade_levels = 0.20 : 0.10 : 0.80;   % fraction of nominal gain
alpha_levels   = 0.10 : 0.10 : 0.50;   % EMA alpha

T_sim  = 10.0;
N      = round(T_sim / Ts);
t_vec  = (0:N-1) * Ts;
t_deg  = 5.0;
fault_thresh_frac  = 0.05;
fault_ticks_thresh = round(0.5 / Ts);
NUM_MOTORS = 4;

n_deg = length(degrade_levels);
n_alp = length(alpha_levels);
err_map = zeros(n_deg, n_alp);  % max steady-state sync error [RPM]

fprintf('Running %d x %d = %d simulations...\n', n_deg, n_alp, n_deg * n_alp);

for id = 1:n_deg
    for ia = 1:n_alp
        deg_fac   = degrade_levels(id);
        ema_alpha = alpha_levels(ia);

        v_plant  = zeros(NUM_MOTORS, N);
        u_pid    = zeros(NUM_MOTORS, 1);
        e_prev   = zeros(NUM_MOTORS, 1);
        v_ema    = zeros(NUM_MOTORS, 1);
        fault_cnt = zeros(NUM_MOTORS, 1);
        faulted  = false(NUM_MOTORS, 1);
        v_sync_log = zeros(1, N);

        for k = 2:N
            t = (k-1) * Ts;
            K_eff = ones(NUM_MOTORS, 1) * K;
            if t >= t_deg; K_eff(3) = K * deg_fac; end

            for m = 1:NUM_MOTORS
                v_ema(m) = ema_alpha * v_plant(m, k-1) + (1-ema_alpha) * v_ema(m);
            end
            for m = 1:NUM_MOTORS
                if v_ema(m) < fault_thresh_frac * v_cmd_rads
                    fault_cnt(m) = fault_cnt(m) + 1;
                else
                    fault_cnt(m) = 0; faulted(m) = false;
                end
                if fault_cnt(m) > fault_ticks_thresh; faulted(m) = true; end
            end

            v_valid = v_ema;
            v_valid(faulted) = Inf;
            if all(isinf(v_valid)); v_sync = v_cmd_rads;
            else; v_sync = min(v_valid); end
            v_sync_log(k) = v_sync;

            sp = min(v_cmd_rads, v_sync) * ones(NUM_MOTORS, 1);

            for m = 1:NUM_MOTORS
                e     = sp(m) - v_plant(m, k-1);
                du    = Q0 * e + Q1 * e_prev(m);
                u_new = max(-1.0, min(1.0, u_pid(m) + du));
                u_pid(m) = u_new; e_prev(m) = e;
                dv = (K_eff(m) * u_new - v_plant(m, k-1)) / tau;
                v_plant(m, k) = max(0, v_plant(m, k-1) + dv * Ts);
            end
        end

        % Steady-state sync error (last 2 seconds)
        idx_ss  = t_vec >= 8.0;
        ss_data = v_plant(:, idx_ss) * rads2rpm;
        v_ss    = mean(ss_data, 2);
        sync_err = max(abs(v_ss - min(v_ss)));
        err_map(id, ia) = sync_err;

        fprintf('  degrade=%.0f%%  alpha=%.1f  sync_err=%.2f RPM\n', ...
                deg_fac*100, ema_alpha, sync_err);
    end
end

%% Heatmap
figure('Name', 'Sync PID Robustness Sweep', 'Position', [100 100 800 600], 'Color', 'w');

imagesc(alpha_levels, degrade_levels * 100, err_map);
colorbar;
colormap(flipud(hot));
clim([0 20]);

xlabel('EMA \alpha');
ylabel('Motor 2 Degradation (% of nominal)');
title(sprintf('Max Steady-State Sync Error (RPM)\nRed = > 20 RPM, target < 5 RPM'));

xticks(alpha_levels);
yticks(degrade_levels * 100);

% Overlay pass/fail contour at 5 RPM
hold on;
contour(alpha_levels, degrade_levels * 100, err_map, [5 5], 'g-', 'LineWidth', 2);
text(alpha_levels(end)-0.05, degrade_levels(1)*100+3, '5 RPM threshold', ...
     'Color', 'g', 'FontWeight', 'bold', 'HorizontalAlignment', 'right');

out_path = fullfile(fileparts(mfilename('fullpath')), '..', 'figures');
if ~exist(out_path, 'dir'); mkdir(out_path); end
saveas(gcf, fullfile(out_path, 'sync_robustness_sweep.png'));

fprintf('\nWorst-case sync error: %.2f RPM\n', max(err_map(:)));
fprintf('Best-case sync error:  %.2f RPM\n', min(err_map(:)));
pass_frac = sum(err_map(:) < 5.0) / numel(err_map) * 100;
fprintf('Fraction of sweep passing (< 5 RPM): %.0f%%\n', pass_frac);
fprintf('\nFigure saved: figures/sync_robustness_sweep.png\n');
