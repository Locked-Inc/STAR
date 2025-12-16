%% motor_model_2nd_order.m - Second-Order DC Motor Model
% Creates a more detailed second-order transfer function including
% both electrical and mechanical dynamics.
%
% Full DC Motor Model (Velocity):
%   Omega(s)/V(s) = Kt / (J*L*s^2 + (J*R + b*L)*s + (b*R + Kt*Ke))
%
% Simplified (L << R, common for small DC motors):
%   Omega(s)/V(s) = Kt / (J*R*s + (b*R + Kt*Ke))
%
% STAR Project - Texas A&M University
% December 2025

%% Load Motor Parameters
motor_params;

%% Second-Order Model (Full Form)
fprintf('\n=== Second-Order DC Motor Model (Full) ===\n');

% Coefficients for: J*L*s^2 + (J*R + b*L)*s + (b*R + Kt*Ke)
a2 = J_total * L_armature;                    % s^2 coefficient
a1 = J_total * R_armature + b * L_armature;   % s^1 coefficient
a0 = b * R_armature + Kt * Ke;                % s^0 coefficient

% Numerator is just Kt
num_full = Kt;
den_full = [a2 a1 a0];

s = tf('s');
motor_tf_full = tf(num_full, den_full);

fprintf('Full Second-Order Transfer Function:\n');
motor_tf_full

% Natural frequency and damping ratio
omega_n = sqrt(a0 / a2);
zeta = a1 / (2 * sqrt(a0 * a2));

fprintf('Natural Frequency:  %.1f rad/s (%.1f Hz)\n', omega_n, omega_n/(2*pi));
fprintf('Damping Ratio:      %.3f\n', zeta);

if zeta < 1
    fprintf('System Type:        Underdamped\n');
elseif zeta == 1
    fprintf('System Type:        Critically Damped\n');
else
    fprintf('System Type:        Overdamped\n');
end

%% Simplified Model (Neglecting Inductance)
fprintf('\n=== Simplified Model (L -> 0) ===\n');

% When L << R, the model reduces to first-order
% Omega(s)/V(s) = Kt / (J*R*s + (b*R + Kt*Ke))
% Which is: K / (tau*s + 1) where K = Kt/(b*R + Kt*Ke) and tau = J*R/(b*R + Kt*Ke)

K_simplified = Kt / (b * R_armature + Kt * Ke);
tau_simplified = J_total * R_armature / (b * R_armature + Kt * Ke);

motor_tf_simplified = K_simplified / (tau_simplified * s + 1);

fprintf('Simplified (ignoring inductance):\n');
motor_tf_simplified
fprintf('DC Gain:        %.3f rad/s/V\n', K_simplified);
fprintf('Time Constant:  %.1f ms\n', tau_simplified * 1000);

%% Current to Velocity Transfer Function
fprintf('\n=== Current-to-Velocity Model ===\n');
% Omega(s)/I(s) = Kt / (J*s + b)
% This is useful for the velocity loop in cascaded control

motor_tf_current = Kt / (J_total * s + b);
fprintf('Omega(s)/I(s):\n');
motor_tf_current

%% Voltage to Current Transfer Function
fprintf('\n=== Voltage-to-Current Model ===\n');
% I(s)/V(s) = 1 / (L*s + R + Ke*Kt/(J*s + b))
% Simplified when Ke*Ke/(J*s + b) << R:
% I(s)/V(s) ≈ 1 / (L*s + R)

motor_tf_electrical = 1 / (L_armature * s + R_armature);
fprintf('I(s)/V(s) (electrical only):\n');
motor_tf_electrical

%% Compare Full vs Simplified Models
figure('Name', 'Second-Order Motor Model Comparison');

subplot(2,2,1);
step(motor_tf_full, motor_tf_simplified);
title('Step Response Comparison');
ylabel('Angular Velocity (rad/s)');
legend('Full 2nd Order', 'Simplified 1st Order', 'Location', 'southeast');
grid on;

subplot(2,2,2);
bode(motor_tf_full, motor_tf_simplified);
title('Frequency Response Comparison');
legend('Full 2nd Order', 'Simplified 1st Order');
grid on;

subplot(2,2,3);
impulse(motor_tf_full, motor_tf_simplified);
title('Impulse Response Comparison');
ylabel('Angular Velocity (rad/s)');
legend('Full 2nd Order', 'Simplified 1st Order', 'Location', 'northeast');
grid on;

subplot(2,2,4);
pzmap(motor_tf_full);
title('Pole-Zero Map (Full Model)');
grid on;

%% Display Poles and Zeros
fprintf('\n=== Poles and Zeros ===\n');
[poles_full, ~] = pzmap(motor_tf_full);
fprintf('Full Model Poles:\n');
for i = 1:length(poles_full)
    if isreal(poles_full(i))
        fprintf('  p%d = %.1f rad/s (tau = %.3f ms)\n', i, poles_full(i), -1000/poles_full(i));
    else
        fprintf('  p%d = %.1f + %.1fj rad/s\n', i, real(poles_full(i)), imag(poles_full(i)));
    end
end

%% Time Constants Summary
fprintf('\n=== Time Constants Summary ===\n');
fprintf('Electrical (tau_e = L/R):     %.3f ms (very fast)\n', tau_e * 1000);
fprintf('Mechanical (simplified):      %.1f ms\n', tau_simplified * 1000);
fprintf('\nSince tau_e << tau_m, the first-order approximation is usually sufficient.\n');

%% Save Models to Workspace
fprintf('\n=== Models Saved to Workspace ===\n');
fprintf('motor_tf_full       - Full 2nd order model\n');
fprintf('motor_tf_simplified - 1st order approximation (L->0)\n');
fprintf('motor_tf_current    - Current-to-velocity model (for cascaded control)\n');
fprintf('motor_tf_electrical - Voltage-to-current model\n');

%% Block Diagram Representation
fprintf('\n=== State-Space Model ===\n');
% State variables: x = [omega; i_a]
% States: angular velocity and armature current
% Input: voltage
% Output: angular velocity

A = [-b/J_total, Kt/J_total;
     -Ke/L_armature, -R_armature/L_armature];
B = [0; 1/L_armature];
C = [1, 0];  % Output is omega
D = 0;

motor_ss = ss(A, B, C, D);
fprintf('State-Space Matrices:\n');
fprintf('States: x = [omega; i_armature]\n');
fprintf('A = [%.3f, %.3f; %.3f, %.3f]\n', A(1,1), A(1,2), A(2,1), A(2,2));
fprintf('B = [%.3f; %.3f]\n', B(1), B(2));
fprintf('C = [%.0f, %.0f]\n', C(1), C(2));
fprintf('D = %.0f\n', D);
