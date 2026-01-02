#!/usr/bin/env python3
"""
Generate STAR RX72N Firmware State Machine Diagram

This script generates a visual state machine showing task interactions
and state transitions in the RX72N motor controller firmware.

Requirements:
    pip install graphviz

Output:
    system_state_machine.png - Visual state machine diagram
"""

from graphviz import Digraph

def create_state_machine():
    """Create the STAR firmware state machine diagram"""

    dot = Digraph(comment='STAR RX72N System State Machine',
                  format='png',
                  engine='dot')

    # Graph attributes for better layout
    dot.attr(rankdir='TB', splines='ortho', nodesep='0.8', ranksep='1.2')
    dot.attr('node', shape='box', style='rounded,filled', fillcolor='lightblue',
             fontname='Arial', fontsize='11')
    dot.attr('edge', fontname='Arial', fontsize='10')

    # ==========================================================================
    # System Initialization States
    # ==========================================================================

    with dot.subgraph(name='cluster_init') as init:
        init.attr(label='System Initialization', style='dashed', color='gray')
        init.node('power_on', 'Power On', fillcolor='lightgreen')
        init.node('hw_init', 'Hardware Init\n(Clocks, Peripherals)', fillcolor='lightgreen')
        init.node('threadx_init', 'ThreadX Init', fillcolor='lightgreen')
        init.node('task_create', 'Create Tasks', fillcolor='lightgreen')
        init.node('scheduler_start', 'Start Scheduler', fillcolor='lightgreen')

    # Initialization flow
    dot.edge('power_on', 'hw_init', label='main()')
    dot.edge('hw_init', 'threadx_init', label='tx_kernel_enter()')
    dot.edge('threadx_init', 'task_create', label='tx_application_define()')
    dot.edge('task_create', 'scheduler_start', label='tasks created')

    # ==========================================================================
    # Motor Control Task States
    # ==========================================================================

    with dot.subgraph(name='cluster_motor') as motor:
        motor.attr(label='Motor Control Task (250 Hz, Priority: HIGHEST)',
                   style='filled', fillcolor='#E8F5E9', color='darkgreen')
        motor.node('motor_idle', 'Idle\n(Awaiting Commands)', fillcolor='#C8E6C9')
        motor.node('motor_running', 'Running PID\n(PWM + Encoder)', fillcolor='#A5D6A7')
        motor.node('motor_emergency_stop', 'Emergency Stop\n(<4ms latency)', fillcolor='#FF8A80')
        motor.node('motor_fault', 'Fault\n(Driver Error)', fillcolor='#FF5252')

    # Motor state transitions
    motor.edge('motor_idle', 'motor_running', label='velocity cmd')
    motor.edge('motor_running', 'motor_idle', label='zero velocity')
    motor.edge('motor_running', 'motor_emergency_stop', label='DWA STOP\n/ E-STOP button')
    motor.edge('motor_running', 'motor_fault', label='driver nFAULT')
    motor.edge('motor_emergency_stop', 'motor_idle', label='fault cleared')
    motor.edge('motor_fault', 'motor_idle', label='reset')


    # ==========================================================================
    # Sensor Task States
    # ==========================================================================

    with dot.subgraph(name='cluster_sensor') as sensor:
        sensor.attr(label='Sensor Task (50 Hz, Priority: MEDIUM)',
                    style='filled', fillcolor='#FFF3E0', color='darkorange')
        sensor.node('sensor_init', 'Initializing\n(4x HC-SR04)', fillcolor='#FFE0B2')
        sensor.node('sensor_reading', 'Reading Sensors\n(4 sonar distances)', fillcolor='#FFCC80')
        sensor.node('sensor_threshold_check', 'Threshold Check\n(< 20cm?)', fillcolor='#FFB74D')
        sensor.node('sensor_stop_set', 'Set STOP Flag\n(obstacle detected)', fillcolor='#FF8A80')
        sensor.node('sensor_clear', 'Clear STOP Flag\n(all clear)', fillcolor='#A5D6A7')

    # Sensor state transitions
    sensor.edge('sensor_init', 'sensor_reading', label='initialized')
    sensor.edge('sensor_reading', 'sensor_threshold_check', label='distances acquired')
    sensor.edge('sensor_threshold_check', 'sensor_stop_set', label='obstacle < 20cm')
    sensor.edge('sensor_threshold_check', 'sensor_clear', label='all clear')
    sensor.edge('sensor_stop_set', 'sensor_reading', label='50 Hz loop')
    sensor.edge('sensor_clear', 'sensor_reading', label='50 Hz loop')

    # ==========================================================================
    # Communication Task States (SPI or USB)
    # ==========================================================================

    with dot.subgraph(name='cluster_comm') as comm:
        comm.attr(label='Communication Task (Priority: LOW)\n[SPI OR USB - Compile Time]',
                  style='filled', fillcolor='#F3E5F5', color='purple')
        comm.node('comm_init', 'Initializing\n(SPI/USB)', fillcolor='#E1BEE7')
        comm.node('comm_rx_waiting', 'RX Waiting\n(for commands)', fillcolor='#CE93D8')
        comm.node('comm_processing', 'Processing Cmd\n(protobuf decode)', fillcolor='#BA68C8')
        comm.node('comm_tx_telemetry', 'TX Telemetry\n(encoder, status)', fillcolor='#AB47BC')

    # Communication state transitions
    comm.edge('comm_init', 'comm_rx_waiting', label='peripheral ready')
    comm.edge('comm_rx_waiting', 'comm_processing', label='frame received')
    comm.edge('comm_processing', 'comm_rx_waiting', label='cmd executed')
    comm.edge('comm_rx_waiting', 'comm_tx_telemetry', label='periodic (100 Hz)')
    comm.edge('comm_tx_telemetry', 'comm_rx_waiting', label='telemetry sent')

    # ==========================================================================
    # Cross-Task Interactions (Critical Paths)
    # ==========================================================================

    # Scheduler starts all tasks
    dot.edge('scheduler_start', 'motor_idle', label='task starts', style='dashed', color='gray')
    dot.edge('scheduler_start', 'sensor_init', label='task starts', style='dashed', color='gray')
    dot.edge('scheduler_start', 'comm_init', label='task starts', style='dashed', color='gray')

    # Sensor → Motor Control (STOP flag)
    dot.edge('sensor_stop_set', 'motor_emergency_stop',
             label='STOP flag\n(obstacle detected)',
             color='red', penwidth='2.0', style='bold')

    # Communication → Motor Control (velocity commands)
    dot.edge('comm_processing', 'motor_running',
             label='velocity cmd\n(from RPi5)',
             color='purple', penwidth='1.5', style='dashed')

    # Motor Control → Communication (telemetry feedback)
    dot.edge('motor_running', 'comm_tx_telemetry',
             label='encoder counts\n(shared state)',
             color='blue', penwidth='1.5', style='dashed')

    return dot


def main():
    """Generate and save the state machine diagram"""

    print("Generating STAR RX72N System State Machine Diagram...")

    dot = create_state_machine()

    # Save to file
    output_file = 'system_state_machine'
    dot.render(output_file, cleanup=True)

    print(f"✓ Diagram saved to {output_file}.png")
    print("\nKey Interactions:")
    print("  • Sensor → Motor Control: STOP flag when obstacle detected (red, bold)")
    print("  • Communication → Motor: Velocity commands from RPi5 (purple)")
    print("  • Motor → Communication: Encoder telemetry (blue)")


if __name__ == '__main__':
    main()
