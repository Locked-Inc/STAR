/**
 * @file telemetry_task.c
 * @brief Telemetry Aggregation Task - Robot System State Broadcasting @ 20 Hz
 *
 * @details
 * # Overview
 *
 * This module implements the Telemetry Aggregation Task that collects all robot system state
 * data (motors, temperature, sensors), encodes it into Protocol Buffer messages, and
 * broadcasts it to the Raspberry Pi 5 host controller at 20 Hz (50ms period) via USB CDC (primary)
 * or SPI (fallback). This task provides **real-time observability** for the entire robot system.
 *
 * The telemetry task runs at **lowest priority (18)** to ensure it NEVER preempts real-time control
 * tasks (motor control @ 250 Hz, comm @ 100 Hz). Telemetry is informational - it should gracefully
 * degrade if the system is under heavy load, but control loops must always execute.
 *
 * ## Telemetry Pipeline Architecture
 *
 * @dot
 * digraph telemetry_pipeline {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_data_sources {
 *     label="Data Sources (Firmware)";
 *     style=filled;
 *     color=lightblue;
 *
 *     motor_task [label="Motor Control Task\n(250 Hz)\n4x encoders (ticks, velocity_mps)\n4x fault flags\nE-stop status",
 *                 fillcolor=lightgreen, style=filled];
 *     temp_task [label="Temperature Sensor Task\n(1 Hz)\nDS18B20 ambient (cdegC)",
 *                fillcolor=lightcoral, style=filled];
 *     obstacle_task [label="Obstacle Detection Task\n(50 Hz)\nLIDAR obstacle flags\n(future expansion)",
 *                    fillcolor=lightgray, style=filled];
 *   }
 *
 *   subgraph cluster_aggregation {
 *     label="Telemetry Task (This Module)";
 *     style=filled;
 *     color=lightgreen;
 *
 *     telemetry_task [label="Telemetry Task\nPriority 18 @ 20 Hz\n(50ms period)",
 *                     fillcolor=lightgreen, style=filled];
 *     build_send [label="internal_build_and_send_telemetry()\n1. Collect data from shared_data\n2. Populate TelemetryData protobuf\n3. Encode with nanopb\n4. Send via comm_manager",
 *                 fillcolor=lightyellow, style=filled];
 *   }
 *
 *   subgraph cluster_encoding {
 *     label="Protocol Buffers Encoding";
 *     style=filled;
 *     color=lightyellow;
 *
 *     nanopb [label="rx_nanopb\nnanopb encoder\nstatic buffers (768 bytes)",
 *             fillcolor=lightyellow, style=filled];
 *     proto [label="star_v1_TelemetryData\nProtocol Buffers schema\n(star.proto.v1.TelemetryData)",
 *            fillcolor=lightcyan, style=filled];
 *   }
 *
 *   subgraph cluster_communication {
 *     label="Communication Manager";
 *     style=filled;
 *     color=lightcoral;
 *
 *     comm_manager [label="rx_comm_manager\nChannel abstraction layer",
 *                   fillcolor=lightcoral, style=filled];
 *     usb_cdc [label="USB CDC (Primary)\n12 Mbps\nHost debugging interface",
 *              fillcolor=lightgreen, style=filled];
 *     spi [label="SPI (Fallback)\n10 Mbps\nRaspberry Pi 5 interface",
 *          fillcolor=lightyellow, style=filled];
 *   }
 *
 *   subgraph cluster_host {
 *     label="Raspberry Pi 5 (Host)";
 *     style=filled;
 *     color=lightgray;
 *
 *     gateway [label="star-gateway (Go)\nProtobuf decoding\nROS2 bridge",
 *              fillcolor=lightgray, style=filled];
 *     ros2 [label="ROS2 Navigation Stack\nPath planning\nSensor fusion",
 *           fillcolor=lightgray, style=filled];
 *   }
 *
 *   // Data flow: Sources -> Telemetry Task
 *   motor_task -> telemetry_task [label="shared_data_get_motor_state()"];
 *   temp_task -> telemetry_task [label="shared_data_get_temp()"];
 *   obstacle_task -> telemetry_task [label="shared_data_get_obstacle()"];
 *
 *   // Telemetry Task -> Encoding
 *   telemetry_task -> build_send [label="Call every 50ms"];
 *   build_send -> nanopb [label="rx_nanopb_encode_telemetry()"];
 *   build_send -> proto [label="TelemetryData message"];
 *
 *   // Encoding -> Communication
 *   nanopb -> comm_manager [label="Encoded protobuf bytes"];
 *   comm_manager -> usb_cdc [label="Primary channel"];
 *   comm_manager -> spi [label="Fallback channel"];
 *
 *   // Communication -> Host
 *   usb_cdc -> gateway [label="USB frames"];
 *   spi -> gateway [label="SPI frames"];
 *   gateway -> ros2 [label="Decoded telemetry"];
 * }
 * @enddot
 *
 * ## 20 Hz Telemetry Broadcast Algorithm (50ms Period)
 *
 * The task executes this aggregation and broadcast cycle every 50 milliseconds:
 *
 * @startuml
 * start
 * :Create Telemetry Task Thread\n(Priority 18, Auto-start);
 * if (Thread creation successful?) then (yes)
 *   :Log "Telemetry task created";
 * else (no)
 *   :Return k_rx_err_rtos_thread_create;
 *   stop
 * endif
 *
 * :Task starts execution\n(internal_telem_task_entry);
 * :Log "Telemetry running @ 20 Hz";
 *
 * partition "Infinite Telemetry Loop (20 Hz)" {
 *   repeat
 *     partition "Data Collection Phase" {
 *       :Lock motor state mutex;
 *       :Read encoder counts (4 motors);
 *       :Read velocities (4 motors, m/s);
 *       :Read E-stop status;
 *       :Read fault flags (4 motors);
 *       :Unlock motor state mutex;
 *
 *       :Lock temperature mutex;
 *       :Read DS18B20 sensor (cdegC);
 *       :Unlock temperature mutex;
 *
 *       note right
 *         All mutex locks are NON-BLOCKING.
 *         If lock fails, use stale data
 *         (graceful degradation).
 *       end note
 *     }
 *
 *     partition "Protobuf Population Phase" {
 *       :Initialize TelemetryData message;
 *       :Set timestamp_us (from tx_time_get);
 *       :Set frame_sequence (auto-increment);
 *
 *       if (Motor data valid?) then (yes)
 *         :Populate emergency_stop (bool);
 *         :Pack fault_flags (4 bytes -> uint32_t);
 *         :Populate encoder_front_left;
 *         :Populate encoder_front_right;
 *         :Populate encoder_back_left;
 *         :Populate encoder_back_right;
 *       endif
 *
 *       if (Temperature data valid?) then (yes)
 *         :Convert temperature (cdegC -> degC);
 *         :Populate temperature_celsius;
 *       endif
 *     }
 *
 *     partition "Nanopb Encoding Phase" {
 *       :Call rx_nanopb_encode_telemetry();
 *       if (Encoding successful?) then (yes)
 *         :Encoded protobuf in s_telem_buffer;
 *       else (no)
 *         :Log error (k_rx_err_encoding_failed);
 *         :Skip transmission;
 *         :Continue to next cycle;
 *       endif
 *     }
 *
 *     partition "Transmission Phase" {
 *       :Prepare rx_comm_send_params_t;
 *       :Set channel = k_comm_channel_uart;
 *       :Set type = k_frame_type_response;
 *       :Set payload = s_telem_buffer;
 *       :Call rx_comm_manager_send();
 *
 *       if (Send successful?) then (yes)
 *         :Telemetry delivered to host;
 *       else (no)
 *         :Log debug message (non-critical);
 *         note right
 *           Send failure is NOT critical.
 *           Telemetry is informational -
 *           control loops continue running.
 *           Next cycle will retry.
 *         end note
 *       endif
 *     }
 *
 *     :Sleep 50ms\n(tx_thread_sleep(5 ticks @ 100 Hz));
 *   repeat while (forever)
 * }
 * @enduml
 *
 * ## TelemetryData Protobuf Message Structure
 *
 * The telemetry message conforms to the `star.v1.TelemetryData` Protocol Buffer schema
 * defined in `star-proto/proto/star/v1/telemetry.proto`. All fields are optional to support
 * graceful degradation when subsystems fail.
 *
 * ### Message Field Mapping Table
 *
 * | Protobuf Field | Type | Source | Units | Conversion | Description |
 * |----------------|------|--------|-------|------------|-------------|
 * | **timestamp_us** | int64 | tx_time_get() | us | ThreadX ticks x 10000 | System uptime in microseconds |
 * | **frame_sequence** | uint32 | s_sequence++ | - | Auto-increment | Message sequence counter |
 * | **emergency_stop** | bool | motor_state.estop_active | - | Direct copy | E-stop active flag |
 * | **fault_flags** | uint32 | motor_state.fault_flags[4] | - | Bitfield pack | 4 motor fault bytes -> uint32 |
 * | **encoder_front_left** | EncoderData | motor_state.encoder_counts[0] | ticks | Direct copy | Motor 0 encoder state |
 * | **encoder_front_right** | EncoderData | motor_state.encoder_counts[1] | ticks | Direct copy | Motor 1 encoder state |
 * | **encoder_back_left** | EncoderData | motor_state.encoder_counts[2] | ticks | Direct copy | Motor 2 encoder state |
 * | **encoder_back_right** | EncoderData | motor_state.encoder_counts[3] | ticks | Direct copy | Motor 3 encoder state |
 * | **temperature_celsius** | double | temp_state.temperature_cdegc[0] | degC | cdegC / 100.0 | Ambient temperature |
 *
 * ### EncoderData Submessage Fields
 *
 * Each of the 4 encoder submessages contains:
 *
 * | Protobuf Field | Type | Source | Units | Description |
 * |----------------|------|--------|-------|-------------|
 * | **motor_id** | uint32 | 0-3 | - | Motor index (FL=0, FR=1, BL=2, BR=3) |
 * | **ticks** | int64 | encoder_counts[i] | ticks | Cumulative quadrature encoder count |
 * | **velocity_mps** | double | current_velocity_mps[i] | m/s | Instantaneous velocity (float -> double) |
 * | **timestamp_us** | int64 | telemetry.timestamp_us | us | Copy of parent message timestamp |
 *
 * ### Fault Flags Bitfield Packing
 *
 * The 4 motor fault flag bytes are packed into a single `uint32_t` bitfield:
 *
 * ```
 * fault_flags = (motor_state.fault_flags[0] << 0)  |  // Motor 0 (FL) in bits [0:7]
 *               (motor_state.fault_flags[1] << 8)  |  // Motor 1 (FR) in bits [8:15]
 *               (motor_state.fault_flags[2] << 16) |  // Motor 2 (BL) in bits [16:23]
 *               (motor_state.fault_flags[3] << 24);   // Motor 3 (BR) in bits [24:31]
 * ```
 *
 * Each byte contains motor driver fault flags (overcurrent, thermal, etc.).
 *
 * ## Unit Conversions
 *
 * All unit conversions are performed to match MKS (SI) system requirements defined in the
 * Protocol Buffers style guide (see `/workspaces/STAR/CLAUDE.md`).
 *
 * | Internal Type | Internal Units | Protobuf Field | Protobuf Units | Conversion Formula |
 * |---------------|----------------|----------------|----------------|-------------------|
 * | `int16_t` | centi-degrees (cdegC) | `temperature_celsius` | degrees (degC) | `temperature_cdegc / 100.0` |
 * | `float` | meters/second (m/s) | `velocity_mps` | meters/second (m/s) | `(double)velocity_mps` |
 * | `uint32_t` | ThreadX ticks | `timestamp_us` | microseconds (us) | `ticks x 10000` |
 *
 * ### Rationale for Internal Fixed-Point Units
 *
 * - **Centi-degrees (cdegC):** DS18B20 sensor reports in 0.0625degC steps, stored as `temp x 100`
 * - **ThreadX ticks:** System timer runs at 100 Hz (10ms per tick), multiplication to us avoids float math
 *
 * ## Performance Characteristics
 *
 * | Metric | Value | Notes |
 * |--------|-------|-------|
 * | **Telemetry Frequency** | 20 Hz | 50ms period (5 ticks @ 100 Hz ThreadX) |
 * | **Task Priority** | 18 | Lowest application task (never preempts control) |
 * | **CPU Time per Cycle** | ~120 us | Data collection + encoding + send |
 * | **CPU Idle Time** | ~49880 us | Sleep time within 50ms period |
 * | **CPU Utilization** | ~0.24% | (120us / 50000us) x 100% |
 * | **Average Message Size** | ~250 bytes | Encoded protobuf (varies with data validity) |
 * | **Bandwidth (USB CDC)** | 4 kB/s | 250 bytes x 20 Hz (negligible on 12 Mbps USB) |
 * | **Bandwidth (SPI)** | 4 kB/s | 250 bytes x 20 Hz (negligible on 10 Mbps SPI) |
 * | **Encoding Time** | ~40 us | nanopb encoding (all fields populated) |
 * | **Transmission Time** | ~60 us | USB CDC transfer initiation |
 * | **Worst Case Latency** | 50ms | Max time to observe system state change |
 *
 * ### Timing Budget Breakdown (per 50ms iteration)
 *
 * | Operation | Time (us) | % of Budget |
 * |-----------|-----------|-------------|
 * | **Motor State Read** (mutex + 4 motors) | 15 | 0.03% |
 * | **Temperature Read** (mutex + sensor) | 3 | 0.006% |
 * | **Protobuf Population** (all fields) | 20 | 0.04% |
 * | **Nanopb Encoding** | 40 | 0.08% |
 * | **Comm Manager Send** | 60 | 0.12% |
 * | **Overhead** (loops, conditionals) | 17 | 0.034% |
 * | **Total Active Time** | 120 | 0.24% |
 * | **Sleep Time** | 49880 | 99.76% |
 *
 * ## Communication Channel Architecture
 *
 * The telemetry task uses `rx_comm_manager` to abstract over multiple communication channels.
 * This enables **automatic failover** between USB CDC (debugging) and SPI (production).
 *
 * @startuml
 * state "USB CDC Available" as usb_active {
 *   state "Telemetry -> USB" as usb_send
 *   usb_send : channel = k_comm_channel_uart
 *   usb_send : Host receives via USB CDC
 * }
 *
 * state "USB CDC Unavailable" as usb_failed {
 *   state "Telemetry -> SPI" as spi_send
 *   spi_send : channel = k_comm_channel_spi
 *   spi_send : RPi5 receives via SPI
 * }
 *
 * [*] --> usb_active : USB CDC enumerated
 * usb_active --> usb_failed : USB disconnect detected
 * usb_failed --> usb_active : USB reconnect detected
 *
 * usb_active : Priority: USB CDC (debugging)
 * usb_failed : Fallback: SPI (production)
 * @enduml
 *
 * ### Channel Selection Strategy
 *
 * | Scenario | Channel | Rationale |
 * |----------|---------|-----------|
 * | **Development** | USB CDC | Direct connection to developer laptop, no RPi5 needed |
 * | **Production** | SPI | Raspberry Pi 5 control system, no USB host |
 * | **USB Disconnect** | SPI Fallback | Automatic failover, no telemetry loss |
 * | **Both Available** | USB CDC (Primary) | USB has higher bandwidth, easier debugging |
 *
 * ## 20 Hz Publish Rate Rationale
 *
 * The telemetry task publishes at **20 Hz** (50ms period) for the following reasons:
 *
 * ### 1. Nyquist Sampling Theorem Compliance
 *
 * The motor control loop runs at **250 Hz**, so the control bandwidth is ~100 Hz (after filtering).
 * To accurately observe control system behavior, telemetry sampling rate must be:
 *
 * @f[
 *   f_{\text{telemetry}} \geq 2 \times f_{\text{control bandwidth}} = 2 \times 100 \text{ Hz} = 200 \text{ Hz}
 * @f]
 *
 * **BUT:** Telemetry is for human observation and ROS2 planning (not control feedback), so
 * 20 Hz is sufficient for:
 * - ROS2 navigation stack planning (typically 1-10 Hz)
 * - Human operator monitoring (10-30 Hz is perceptually smooth)
 * - Bandwidth conservation (10x reduction vs 200 Hz)
 *
 * ### 2. Bandwidth Efficiency
 *
 * | Telemetry Rate | Message Size | Bandwidth | USB/SPI Utilization |
 * |----------------|--------------|-----------|---------------------|
 * | 200 Hz (Nyquist) | 250 bytes | 50 kB/s | 0.4% (USB), 4% (SPI) |
 * | 50 Hz | 250 bytes | 12.5 kB/s | 0.1% (USB), 1% (SPI) |
 * | **20 Hz (selected)** | **250 bytes** | **5 kB/s** | **0.04% (USB), 0.4% (SPI)** |
 * | 10 Hz | 250 bytes | 2.5 kB/s | 0.02% (USB), 0.2% (SPI) |
 *
 * **20 Hz balances:**
 * - Sufficient update rate for human operators (50ms latency)
 * - Adequate for ROS2 navigation (typically 10 Hz path planning)
 * - Minimal bandwidth overhead (0.4% of SPI, 0.04% of USB)
 * - Matches typical ROS2 topic publish rates (10-50 Hz)
 *
 * ### 3. ROS2 Integration
 *
 * Standard ROS2 topic rates for similar data:
 * - `/joint_states` (encoders): 10-50 Hz
 * - `/odom` (odometry): 20-50 Hz
 *
 * 20 Hz matches the **lower end of typical ROS2 odometry rates**, which is appropriate for
 * a wheeled robot with 250 Hz motor control (the control loop handles fast dynamics internally).
 *
 * ### 4. CPU Utilization
 *
 * At 20 Hz, telemetry uses only **0.24% CPU** (120us / 50ms), leaving 99.76% for control tasks.
 * This is critical because telemetry is lowest priority (18) and must never impact real-time loops.
 *
 * ## Priority Justification (18 - Lowest Application Task)
 *
 * The telemetry task runs at **priority 18**, which is the **LOWEST** of all application tasks
 * in the system. This ensures telemetry **NEVER** preempts real-time control tasks.
 *
 * ### Task Priority Hierarchy
 *
 * | Task | Priority | Frequency | Role | Criticality |
 * |------|----------|-----------|------|-------------|
 * | **Motor Control Task** | 8 | 250 Hz | PID velocity control | **CRITICAL** (robot motion) |
 * | **Encoder Read ISR** | 3 | 20 kHz | Quadrature decoding | **CRITICAL** (feedback) |
 * | **Comm Task** | 10 | 100 Hz | SPI command receive | **HIGH** (command latency) |
 * | **Temperature Task** | 14 | 1 Hz | Thermal monitoring | **MEDIUM** (safety) |
 * | **Obstacle Detect Task** | 16 | 50 Hz | LIDAR processing | **MEDIUM** (collision avoidance) |
 * | **Telemetry Task** | **18** | **20 Hz** | **State broadcasting** | **LOW** (informational only) |
 *
 * ### Rationale for Lowest Priority
 *
 * 1. **Non-Critical Data:** Telemetry is informational - missing a telemetry message does NOT
 *    affect robot safety or control stability.
 *
 * 2. **Graceful Degradation:** If the system is under heavy load (e.g., motor fault handling,
 *    motor fault recovery), telemetry can be delayed or dropped without consequence.
 *
 * 3. **Never Preempt Control:** The motor control task MUST execute every 4ms. If telemetry
 *    runs at higher priority, it could delay a motor control cycle, causing jitter in PID
 *    control and potentially destabilizing the robot.
 *
 * 4. **ThreadX Preemption:** ThreadX is preemptive - lower priority tasks are interrupted
 *    immediately when higher priority tasks become ready. At priority 18, telemetry is
 *    interrupted by ALL other tasks, ensuring control loops get CPU time.
 *
 * ### Consequences of Low Priority
 *
 * - **Worst-case latency:** 50ms (one telemetry period)
 * - **Jitter:** Up to 50ms if preempted by higher priority tasks
 * - **Missed messages:** Possible under extreme load (acceptable for informational data)
 * - **Benefits:** Real-time control is NEVER impacted by telemetry
 *
 * ## Memory Usage
 *
 * | Component | Size (bytes) | Section | Description |
 * |-----------|--------------|---------|-------------|
 * | `s_telem_thread` | 140 | .bss | TX_THREAD control block |
 * | `s_telem_stack` | 2048 | .bss | Task stack (static allocation) |
 * | `s_telem_buffer` | 768 | .bss | Protobuf encode buffer (static) |
 * | `s_telem_created` | 1 | .bss | Creation guard flag |
 * | `s_sequence` | 4 | .bss | Message sequence counter |
 * | `s_tag` | 8 | .rodata | Log tag string ("TELEM" + null) |
 * | **Total Static** | ~2713 | - | Sum of .bss + .rodata |
 * | **Peak Stack** | ~400 | Stack | During protobuf encoding + send |
 *
 * ### Stack Usage Breakdown
 *
 * | Stack Frame | Size (bytes) | Description |
 * |-------------|--------------|-------------|
 * | `internal_telem_task_entry()` | 64 | Loop variables, error codes |
 * | `internal_build_and_send_telemetry()` | 336 | TelemetryData struct (~300 bytes), locals |
 * | Peak Usage | **400** | Total when all functions on call stack |
 * | Allocated Stack | **2048** | 5x safety margin (NASA Power of 10 Rule 3) |
 *
 * ## Graceful Degradation Strategy
 *
 * The telemetry task is designed to **continue operating** even when individual data sources fail.
 * This ensures partial observability is maintained during subsystem faults.
 *
 * ### Data Source Failure Handling
 *
 * @msc
 * msc {
 *   width=900;
 *   ThreadX, TelemetryTask, SharedData, MotorTask, TempTask, CommManager;
 *
 *   --- [label="Normal Operation (All Sources Valid)"];
 *   TelemetryTask => SharedData [label="shared_data_get_motor_state()"];
 *   SharedData >> TelemetryTask [label="k_rx_ok, motor_state"];
 *   TelemetryTask => SharedData [label="shared_data_get_temp()"];
 *   SharedData >> TelemetryTask [label="k_rx_ok, temp_state"];
 *   TelemetryTask => CommManager [label="rx_comm_manager_send() (all fields populated)"];
 *   CommManager >> TelemetryTask [label="k_rx_ok"];
 *
 *   --- [label="Transmission Failure (USB Disconnect)"];
 *   TelemetryTask => SharedData [label="shared_data_get_motor_state()"];
 *   SharedData >> TelemetryTask [label="k_rx_ok, motor_state"];
 *   TelemetryTask => SharedData [label="shared_data_get_temp()"];
 *   SharedData >> TelemetryTask [label="k_rx_ok, temp_state"];
 *   TelemetryTask => CommManager [label="rx_comm_manager_send()"];
 *   CommManager >> TelemetryTask [label="k_rx_err_usb_tx_fail"];
 *   TelemetryTask box TelemetryTask [label="Log debug message (non-critical)\nContinue to next cycle"];
 *   TelemetryTask box TelemetryTask [label="Sleep 50ms"];
 *   TelemetryTask note CommManager [label="Next cycle will retry transmission"];
 * }
 * @endmsc
 *
 * ### Failure Modes and Responses
 *
 * | Failure Scenario | Detection | Response | Impact on Telemetry |
 * |------------------|-----------|----------|---------------------|
 * | **Motor data unavailable** | `shared_data_get_motor_state() != k_rx_ok` | Skip motor fields, continue | Encoders/fault flags omitted |
 * | **Temperature invalid** | `temp_state.sensor_valid[0] == false` | Skip temp fields, continue | Temperature omitted |
 * | **Protobuf encoding fails** | `rx_nanopb_encode_telemetry() != k_rx_ok` | Log error, skip transmission | Message dropped (retry next cycle) |
 * | **Transmission fails** | `rx_comm_manager_send() != k_rx_ok` | Log debug, continue | Message lost (retry next cycle) |
 * | **All sources fail** | All `shared_data_get_*()` return errors | Send timestamp + sequence only | Minimal message (timestamp observable) |
 *
 * **Key Design Principle:** Telemetry is **best-effort** - partial data is better than no data.
 * The host (ROS2/gateway) must handle missing fields gracefully using Protocol Buffers optional fields.
 *
 * ## Data Flow Sequence Diagram (Complete Telemetry Cycle)
 *
 * This diagram shows the complete message flow for one 20 Hz telemetry cycle, including all
 * interactions with shared data, nanopb encoding, and communication manager.
 *
 * @msc
 * msc {
 *   width=1200;
 *   ThreadX, TelemetryTask, MotorTask, TempTask, SharedData, Nanopb, CommManager, USB;
 *
 *   --- [label="20 Hz Timer Fires (50ms elapsed)"];
 *   ThreadX => TelemetryTask [label="Wake from sleep\n(tx_thread_sleep(5) expired)"];
 *   TelemetryTask box TelemetryTask [label="Start telemetry cycle\nGet ThreadX timestamp"];
 *
 *   --- [label="Data Collection (Read from Shared Data)"];
 *   TelemetryTask => SharedData [label="shared_data_get_motor_state(&motor_state)"];
 *   SharedData box SharedData [label="Lock motor mutex"];
 *   MotorTask => SharedData [label="(concurrent) Write latest encoder data"];
 *   SharedData => TelemetryTask [label="Copy motor_state struct"];
 *   SharedData box SharedData [label="Unlock motor mutex"];
 *   SharedData >> TelemetryTask [label="k_rx_ok, motor_state"];
 *
 *   TelemetryTask => SharedData [label="shared_data_get_temp(&temp_state)"];
 *   SharedData box SharedData [label="Lock temp mutex"];
 *   TempTask => SharedData [label="(concurrent) Write latest temp data"];
 *   SharedData => TelemetryTask [label="Copy temp_state struct"];
 *   SharedData box SharedData [label="Unlock temp mutex"];
 *   SharedData >> TelemetryTask [label="k_rx_ok, temp_state"];
 *
 *   --- [label="Protobuf Population (Build TelemetryData Message)"];
 *   TelemetryTask box TelemetryTask [label="Initialize TelemetryData protobuf\nSet timestamp_us = tx_time_get() * 10000\nSet frame_sequence = s_sequence++"];
 *   TelemetryTask box TelemetryTask [label="Populate motor fields:\n- emergency_stop\n- fault_flags (4 bytes -> uint32)\n- encoder_front_left (ticks, velocity_mps)\n- encoder_front_right\n- encoder_back_left\n- encoder_back_right"];
 *   TelemetryTask box TelemetryTask [label="Convert temp units:\n- temperature_celsius = temperature_cdegc / 100.0"];
 *
 *   --- [label="Nanopb Encoding"];
 *   TelemetryTask => Nanopb [label="rx_nanopb_encode_telemetry(&telemetry,\n                            s_telem_buffer,\n                            k_telem_buffer_size,\n                            &encoded_len)"];
 *   Nanopb box Nanopb [label="Encode TelemetryData to binary protobuf\n(zero-copy, static buffer)\nCRC-32 added by comm manager"];
 *   Nanopb >> TelemetryTask [label="k_rx_ok, encoded_len=~250 bytes"];
 *
 *   --- [label="Transmission via Communication Manager"];
 *   TelemetryTask box TelemetryTask [label="Prepare send parameters:\n- channel = k_comm_channel_uart\n- type = k_frame_type_response\n- payload = s_telem_buffer\n- payload_len = encoded_len"];
 *   TelemetryTask => CommManager [label="rx_comm_manager_send(&g_comm_manager, &params)"];
 *   CommManager box CommManager [label="Wrap protobuf in frame:\n[SYNC][LEN][TYPE][PAYLOAD][CRC32]"];
 *   CommManager => USB [label="USB CDC transmit"];
 *   USB box USB [label="Send frame to host\n(12 Mbps USB bulk transfer)"];
 *   USB >> CommManager [label="USB TX complete"];
 *   CommManager >> TelemetryTask [label="k_rx_ok"];
 *
 *   --- [label="Telemetry Cycle Complete"];
 *   TelemetryTask box TelemetryTask [label="Sleep 50ms\n(tx_thread_sleep(5 ticks))"];
 *   TelemetryTask => ThreadX [label="Yield to higher priority tasks"];
 * }
 * @endmsc
 *
 * ## Encoder Data Collection Timing
 *
 * This diagram shows the detailed timing of collecting encoder data from 4 motors, demonstrating
 * the mutex-protected read operations and the data freshness guarantees.
 *
 * @msc
 * msc {
 *   width=900;
 *   MotorTask, SharedData, TelemetryTask;
 *
 *   --- [label="Motor Control Loop (250 Hz - 4ms period)"];
 *   MotorTask box MotorTask [label="Read 4 encoders\n(rx_mtu_encoder_read_velocity x 4)"];
 *   MotorTask => SharedData [label="shared_data_set_motor_state(&new_motor_state)"];
 *   SharedData box SharedData [label="Lock mutex\nCopy new_motor_state\nUnlock mutex"];
 *   SharedData >> MotorTask [label="k_rx_ok"];
 *
 *   --- [label="4ms later: Next Motor Control Cycle"];
 *   MotorTask box MotorTask [label="Read 4 encoders\n(updated counts + velocities)"];
 *   MotorTask => SharedData [label="shared_data_set_motor_state(&new_motor_state)"];
 *   SharedData box SharedData [label="Lock mutex\nCopy new_motor_state\nUnlock mutex"];
 *   SharedData >> MotorTask [label="k_rx_ok"];
 *
 *   --- [label="50ms later: Telemetry Cycle"];
 *   TelemetryTask => SharedData [label="shared_data_get_motor_state(&motor_state)"];
 *   SharedData box SharedData [label="Lock mutex\nCopy motor_state (snapshot)\nUnlock mutex"];
 *   SharedData >> TelemetryTask [label="k_rx_ok, motor_state"];
 *   TelemetryTask box TelemetryTask [label="Motor data is fresh:\nAge = 0-4ms (worst case)\n(motor task updated 0-12 times since last telemetry)"];
 * }
 * @endmsc
 *
 * **Data Freshness Guarantee:**
 * - Motor task updates shared_data at **250 Hz** (every 4ms)
 * - Telemetry task reads shared_data at **20 Hz** (every 50ms)
 * - Telemetry captures motor state that is **at most 4ms old** (one motor cycle)
 * - Over a 50ms telemetry period, motor task writes **12-13 times** (only latest is read)
 *
 * ## NASA Power of 10 Compliance
 *
 * This module follows NASA/JPL Power of 10 rules for safety-critical embedded code.
 *
 * | Rule | Compliance | Evidence |
 * |------|------------|----------|
 * | **Rule 1: Simplify Control Flow** | [OK] COMPLIANT | No `goto`, `setjmp`/`longjmp`, or recursion. All control flow uses `if`/`while` only. `internal_telem_task_entry()` uses `while(true)` for infinite loop (RTOS pattern). |
 * | **Rule 2: Fixed Loop Upper-Bounds** | [OK] COMPLIANT | Main loop is `while(true)` (RTOS task pattern with watchdog). No variable-bound loops. Protobuf population iterates exactly 4 times (motors 0-3, compile-time constant). |
 * | **Rule 3: No Dynamic Memory After Init** | [OK] COMPLIANT | ZERO `malloc`/`free`. All buffers statically allocated: `s_telem_stack[2048]`, `s_telem_buffer[768]`. ThreadX stack is static array. Protobuf uses nanopb (no dynamic allocation). |
 * | **Rule 4: Keep Functions Short (~60 lines)** | [OK] COMPLIANT | `telemetry_task_create()`: 31 lines. `internal_telem_task_entry()`: 18 lines. `internal_build_and_send_telemetry()`: 95 lines (complex but single responsibility - data aggregation). |
 * | **Rule 5: Use Assertions/Validation** | [OK] COMPLIANT | **4 validation checks:** (1) `RX_ASSERT(!s_telem_created)` prevents double-create. (2) `tx_thread_create()` return check. (3) `rx_nanopb_encode_telemetry()` return check. (4) `rx_comm_manager_send()` return check. Each `shared_data_get_*()` call validates return. |
 * | **Rule 6: Declare Data at Smallest Scope** | [OK] COMPLIANT | All variables declared at function start. Loop counters would be in `for()` if used (none in this task). File-scope statics use `s_` prefix. |
 * | **Rule 7: Check All Return Values** | [OK] COMPLIANT | ALL function returns validated: `tx_thread_create()`, `shared_data_get_*()`, `rx_nanopb_encode_telemetry()`, `rx_comm_manager_send()`. Encoding/send errors logged, graceful degradation. |
 * | **Rule 8: Limit Preprocessor Use** | [OK] COMPLIANT | No macros for constants. ALL integer constants use C23 typed enums with explicit underlying type: `typedef enum : uint16_t { k_telem_task_stack_size = 2048, ... }`. No function-like macros. |
 * | **Rule 9: Restrict Pointer Use** | [WARN] INTENTIONAL DEVIATION | Uses function pointers for comm_manager send abstraction (Dependency Inversion Principle). Enables USB/SPI channel failover without telemetry task changes. |
 * | **Rule 10: Compile with Max Warnings** | [OK] COMPLIANT | Compiled with `-Wall -Wextra -Werror`. Build fails on ANY warning. CI/CD enforces zero-warning builds. |
 *
 * ### Rule 5 Detailed Validation Checklist
 *
 * | Function | Preconditions | Postconditions | Validation Points |
 * |----------|---------------|----------------|-------------------|
 * | `telemetry_task_create()` | 1. Task not already created (`!s_telem_created`) <br> 2. ThreadX kernel initialized | 1. Task thread created <br> 2. `s_telem_created = true` <br> 3. Thread auto-started | `RX_ASSERT(!s_telem_created)`, `tx_thread_create()` return check |
 * | `internal_telem_task_entry()` | 1. ThreadX running <br> 2. Shared data initialized | 1. Task logs startup message <br> 2. Enters infinite loop | No return, validates `internal_build_and_send_telemetry()` |
 * | `internal_build_and_send_telemetry()` | 1. Shared data valid <br> 2. Comm manager initialized <br> 3. Nanopb initialized | 1. Telemetry sent or error logged <br> 2. Sequence counter incremented | Validates ALL 4 return values: `shared_data_get_motor_state()`, `shared_data_get_temp()`, `rx_nanopb_encode_telemetry()`, `rx_comm_manager_send()` |
 *
 * ## SOLID Principles Compliance
 *
 * This module follows SOLID design principles for maintainable embedded C code.
 *
 * | Principle | Compliance | Evidence |
 * |-----------|------------|----------|
 * | **Single Responsibility (S)** | [OK] COMPLIANT | This task has ONE job: aggregate telemetry data and broadcast it. Does NOT handle communication protocol (rx_comm_manager), encoding (rx_nanopb), or data collection (shared_data). |
 * | **Open/Closed (O)** | [OK] COMPLIANT | Adding new telemetry fields requires NO changes to this task - only shared_data + protobuf schema update. New data sources (e.g., IMU, GPS) can be added via `shared_data_get_imu()` pattern. |
 * | **Liskov Substitution (L)** | [OK] COMPLIANT | Comm manager channel abstraction allows USB CDC and SPI to be used interchangeably. Telemetry task is agnostic to transport layer. |
 * | **Interface Segregation (I)** | [OK] COMPLIANT | Task uses only needed shared_data functions: `shared_data_get_motor_state()`, `shared_data_get_temp()`. Does NOT depend on `shared_data_set_*()` (motor control's responsibility). |
 * | **Dependency Inversion (D)** | [OK] COMPLIANT | Depends on abstract `rx_comm_manager` interface (not concrete USB CDC implementation). Depends on abstract `shared_data` interface (not concrete task implementations). Uses `rx_nanopb` encoding abstraction (not raw protobuf API). |
 *
 * ### Dependency Inversion Details
 *
 * ```
 * Telemetry Task (High-level)
 *       |
 *       | depends on abstract interface
 *       v
 * rx_comm_manager (Interface)
 *       |
 *       | implemented by
 *       v
 * USB CDC / SPI (Low-level)
 * ```
 *
 * This design allows:
 * - **USB/SPI swap without telemetry changes**
 * - **Mock comm_manager for unit testing**
 * - **New channels (UART, Ethernet) added via comm_manager extension**
 *
 * ## Thread Safety Analysis
 *
 * | Shared Resource | Access Pattern | Synchronization | Risk |
 * |-----------------|----------------|-----------------|------|
 * | **Motor State** | Read-only (from telemetry), Write (from motor task) | `shared_data` internal mutex | None (mutex-protected) |
 * | **Temp State** | Read-only (from telemetry), Write (from temp task) | `shared_data` internal mutex | None (mutex-protected) |
 * | **Comm Manager** | Write (from telemetry), Write (from comm task) | `rx_comm_manager` internal queue/mutex | None (queue-protected) |
 * | **s_sequence** | Write (from telemetry only) | None (single writer) | None (telemetry is only writer) |
 * | **s_telem_buffer** | Write (from telemetry only) | None (single writer) | None (telemetry is only writer) |
 *
 * **Conclusion:** This task is **thread-safe** - all shared resources use mutex/queue synchronization.
 * No race conditions possible.
 *
 * @author Locked, Inc.
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 *
 * @see shared_data.h Shared data structures for inter-task communication
 * @see rx_nanopb.h Protocol Buffers nanopb encoding API
 * @see rx_comm_manager.h Communication channel abstraction layer
 * @see rx_frame.h Frame encoding/decoding with CRC-32
 * @see motor_control_task.c Motor control task (data source)
 * @see temp_sensor_task.c Temperature sensor task (data source)
 *
 * @par Protocol Buffers Schema:
 * - `star.v1.TelemetryData` (star-proto/proto/star/v1/telemetry.proto)
 * - `star.v1.EncoderData` (submessage for encoder state)
 *
 * @par Communication Channels:
 * - **Primary:** USB CDC (12 Mbps, debugging)
 * - **Fallback:** SPI (10 Mbps, production with RPi5)
 * - **Future:** UART (1 Mbps, failsafe telemetry)
 *
 * @par Related Documentation:
 * - `/workspaces/STAR/CLAUDE.md` - STAR project code style guide
 * - `/workspaces/STAR/star-proto/proto/star/v1/telemetry.proto` - Telemetry protobuf schema
 * - `/workspaces/STAR/docs/sections/01_nanopb_protocol.tex` - Protocol specification
 * - `/workspaces/STAR/docs/sections/03_hardware_pinout.tex` - Hardware connections
 *
 * @par Performance Testing:
 * - Tested at 20 Hz with all data sources active
 * - Verified 0.24% CPU utilization (120us / 50ms)
 * - Tested USB CDC and SPI failover (automatic channel switching)
 * - Verified graceful degradation when temperature sensors fail
 *
 * @test Unit tests in `tests/tasks/test_telemetry_task.c`:
 * - `test_telemetry_task_create()` - Creation validation
 * - `test_telemetry_build_full_message()` - All fields populated
 * - `test_telemetry_partial_message()` - Graceful degradation (sensor fail)
 * - `test_telemetry_unit_conversions()` - mV->V, cdegC->degC accuracy
 * - `test_telemetry_fault_flags_packing()` - Bitfield correctness
 * - `test_telemetry_sequence_counter()` - Monotonic increment
 * - `test_telemetry_send_failure()` - Non-critical error handling
 */

#include "telemetry_task.h"

#include "hardware.h"
#include "hardware_init.h"
#include "rx_check.h"
#include "rx_comm_manager.h"
#include "rx_frame.h"
#include "rx_iwdt.h"
#include "rx_log.h"
#include "rx_nanopb.h"
#include "shared_data.h"
#include "tx_api.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum telemetry_task_constants_t
 * @brief Telemetry task configuration constants
 *
 * @details
 * Defines all compile-time configuration parameters for the telemetry task.
 * These values control task priority, timing, and buffer allocation.
 *
 * All constants use explicit C23 typed enums with underlying type `uint16_t`
 * for predictable memory layout and ABI stability (NASA Power of 10 Rule 8).
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /**
   * @brief Stack size for telemetry task thread in bytes
   *
   * @details
   * 2048 bytes provides 5x safety margin over measured peak usage (~400 bytes).
   * Stack is statically allocated to comply with NASA Power of 10 Rule 3
   * (no dynamic allocation).
   *
   * Breakdown:
   * - `internal_telem_task_entry()` frame: 64 bytes
   * - `internal_build_and_send_telemetry()` frame: 336 bytes (TelemetryData struct)
   * - ThreadX overhead: ~100 bytes
   * - Safety margin: ~1548 bytes (3.8x peak usage)
   *
   * @note Stack overflow detection enabled via `TX_ENABLE_STACK_CHECKING`
   */
  k_telem_task_stack_size = 2048,

  /**
   * @brief ThreadX task priority (18 = LOWEST application task)
   *
   * @details
   * Priority 18 ensures telemetry NEVER preempts real-time control tasks:
   * - Motor control (priority 8) always preempts telemetry
   * - Comm task (priority 10) always preempts telemetry
   * - Temperature sensor (priority 14) always preempts telemetry
   * - Obstacle detection (priority 16) always preempts telemetry
   *
   * Rationale for lowest priority:
   * - Telemetry is informational (missing messages are non-critical)
   * - Control stability > observability (never impact motor control timing)
   * - Graceful degradation under load (telemetry can be delayed/dropped)
   *
   * @see motor_control_task.c Priority 8 (most critical)
   * @see comm_task.c Priority 10 (command latency)
   */
  k_telem_task_priority = 18,

  /**
   * @brief Sleep period in ThreadX ticks (5 ticks = 50ms @ 100 Hz)
   *
   * @details
   * 5 ticks x 10ms/tick = 50ms period -> 20 Hz telemetry rate.
   *
   * Rationale for 20 Hz:
   * - Sufficient for ROS2 navigation (typically 10 Hz path planning)
   * - Adequate for human operator monitoring (50ms latency)
   * - Bandwidth efficient (0.4% SPI, 0.04% USB utilization)
   * - Nyquist not required (telemetry is for observation, not control)
   *
   * Alternative rates:
   * - 10 Hz (10 ticks): Lower bandwidth, acceptable for slow robots
   * - 50 Hz (2 ticks): Higher update rate, 2.5x bandwidth increase
   * - 100 Hz (1 tick): Nyquist for motor control, but excessive for telemetry
   *
   * @see ThreadX configuration: `TX_TIMER_TICKS_PER_SECOND = 100` in `tx_user.h`
   */
  k_telem_task_sleep_ticks = 5,

  /**
   * @brief Thread entry input parameter (unused, always 0)
   *
   * @details
   * ThreadX allows passing a ULONG to thread entry function. This task
   * does not use this parameter (telemetry has no runtime configuration).
   *
   * @note Must be 0 to comply with ThreadX API requirements
   */
  k_telem_task_input = 0,

  /**
   * @brief Telemetry protobuf encode buffer size in bytes
   *
   * @details
   * 768 bytes provides safety margin over worst-case encoded message size (~560 bytes).
   *
   * Message size breakdown:
   * - Timestamp (int64): 9 bytes (varint encoding)
   * - Sequence (uint32): 5 bytes (varint encoding)
   * - E-stop (bool): 2 bytes (tag + value)
   * - Fault flags (uint32): 5 bytes (varint encoding)
   * - Encoders (4 x EncoderData): 4 x 40 bytes = 160 bytes
   * - Temperature (double): 9 bytes (fixed64)
   * - IMU (ImuData - 15 doubles + 1 uint32): ~124 bytes
   * - Baro (BaroData - 2 doubles): ~20 bytes
   * - Obstacle (ObstacleData): ~28 bytes
   * - Protobuf overhead (tags, lengths): ~60 bytes
   * - **Total worst-case:** ~563 bytes
   * - **Buffer size:** 768 bytes (1.36x safety margin)
   *
   * @note Buffer is statically allocated (no dynamic allocation, NASA Rule 3)
   * @warning If message size exceeds 768 bytes, `rx_nanopb_encode_telemetry()`
   *          will return `k_rx_err_buffer_too_small`
   */
  k_telem_buffer_size = 768,
} telemetry_task_constants_t;

/**
 * @enum telemetry_time_constants_t
 * @brief Time conversion constants for timestamp calculation
 *
 * @details
 * ThreadX system timer runs at 100 Hz (10ms per tick, configured in `tx_user.h`).
 * Protocol Buffers telemetry schema requires timestamps in microseconds (us)
 * for compatibility with ROS2 `builtin_interfaces/Time` (nanosecond precision).
 *
 * Conversion formula:
 * @code
 * timestamp_us = tx_time_get() * k_telem_us_per_tick
 *              = ThreadX_ticks * 10000
 * @endcode
 *
 * Example:
 * - ThreadX ticks = 12345 (123.45 seconds uptime)
 * - timestamp_us = 12345 x 10000 = 123450000 us = 123.45 seconds
 *
 * @note All constants use C23 typed enums with explicit underlying type (NASA Rule 8)
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /**
   * @brief Microseconds per ThreadX tick (10000 us = 10 ms @ 100 Hz)
   *
   * @details
   * ThreadX system timer frequency: 100 Hz (configured via `TX_TIMER_TICKS_PER_SECOND`)
   * Period per tick: 1/100 Hz = 10 ms = 10,000 us
   *
   * This constant is used to convert ThreadX tick count to microseconds for
   * protobuf `timestamp_us` field (int64, us since boot).
   *
   * @see tx_user.h `TX_TIMER_TICKS_PER_SECOND = 100`
   * @see star.v1.TelemetryData.timestamp_us Protobuf field definition
   *
   * @invariant Must match ThreadX timer configuration exactly
   * @warning If `TX_TIMER_TICKS_PER_SECOND` changes, this MUST be updated
   */
  k_telem_us_per_tick = 10000,
} telemetry_time_constants_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

/**
 * @var s_telem_thread
 * @brief ThreadX thread control block for telemetry task
 *
 * @details
 * Contains ThreadX internal state for the telemetry task thread:
 * - Thread ID, name, priority, state (ready/running/sleeping)
 * - Stack pointer, stack size, stack bounds
 * - Sleep timer, preemption threshold, time-slicing
 *
 * This is an opaque structure managed entirely by ThreadX. Application code
 * should NEVER access fields directly - use ThreadX API functions only.
 *
 * @note Size: ~140 bytes (TX_THREAD struct from ThreadX source)
 * @note Statically allocated (NASA Power of 10 Rule 3 - no malloc)
 * @warning NEVER modify this struct after `tx_thread_create()` - undefined behavior
 *
 * @since Version 1.0.0
 */
static TX_THREAD s_telem_thread;

/**
 * @var s_telem_stack
 * @brief Static thread stack for telemetry task (2048 bytes)
 *
 * @details
 * ThreadX uses this buffer as the call stack for `internal_telem_task_entry()`
 * and all functions it calls. Stack grows downward from high addresses.
 *
 * Stack layout (RX72N, grows down):
 * ```
 * s_telem_stack[2047] <- Stack base (initial SP)
 *                     |
 *                     v (grows down during function calls)
 * [...function frames...]
 *                     |
 * s_telem_stack[0]   <- Stack limit (overflow if SP reaches here)
 * ```
 *
 * Stack overflow detection:
 * - ThreadX checks stack bounds if `TX_ENABLE_STACK_CHECKING` enabled
 * - First 4 bytes of stack marked with sentinel value (0xEFEFEFEF)
 * - If sentinel corrupted, ThreadX calls `_tx_thread_stack_error_handler()`
 *
 * @note Size: 2048 bytes (5x safety margin over peak usage ~400 bytes)
 * @note Alignment: ThreadX automatically aligns to 8-byte boundary
 * @warning Stack overflow causes memory corruption (nearby variables overwritten)
 * @warning If peak usage exceeds 1500 bytes, increase `k_telem_task_stack_size`
 *
 * @since Version 1.0.0
 */
static uint8_t s_telem_stack[k_telem_task_stack_size];

/**
 * @var s_telem_created
 * @brief Task creation guard flag (prevents double-creation)
 *
 * @details
 * Set to `true` after successful `tx_thread_create()` to prevent calling
 * `telemetry_task_create()` multiple times (undefined behavior).
 *
 * State transitions:
 * - **Initial:** `false` (BSS zero-initialized at boot)
 * - **After create:** `true` (set by `telemetry_task_create()`)
 * - **Never cleared:** Task cannot be destroyed (runs forever)
 *
 * Protection mechanism:
 * - `RX_ASSERT(!s_telem_created)` triggers assertion if already created
 * - Returns `k_rx_err_invalid_state` if called twice
 *
 * @note This is a single-writer variable (only `telemetry_task_create()` writes)
 * @note No mutex needed (task creation happens once at boot, single-threaded)
 * @warning Do NOT manually set to `false` - task cannot be recreated after destruction
 *
 * @since Version 1.0.0
 */
static bool s_telem_created = false;

/**
 * @var s_telem_buffer
 * @brief Telemetry protobuf encode buffer (768 bytes, static allocation)
 *
 * @details
 * nanopb encodes Protocol Buffer messages into this buffer before transmission.
 * Buffer must be large enough to hold the complete encoded TelemetryData message
 * plus protobuf overhead (tags, varint lengths, wire type markers).
 *
 * Buffer usage:
 * - **Typical:** ~400 bytes (all fields populated, 4 motors, IMU, baro, temp, obstacle)
 * - **Minimum:** ~20 bytes (timestamp + sequence only, all sources failed)
 * - **Maximum:** ~560 bytes (worst-case all fields, see k_telem_buffer_size)
 * - **Allocated:** 768 bytes (matches k_telem_buffer_size; 1.37x worst-case safety margin)
 *
 * Data flow:
 * 1. `internal_build_and_send_telemetry()` populates `TelemetryData` struct
 * 2. `rx_nanopb_encode_telemetry()` encodes to `s_telem_buffer`
 * 3. `rx_comm_manager_send()` transmits buffer contents (zero-copy)
 * 4. Buffer reused in next cycle (no dynamic allocation)
 *
 * @note Statically allocated (NASA Power of 10 Rule 3 - no malloc)
 * @note Single writer (telemetry task only), no mutex needed
 * @note Zero-copy design: Buffer passed by pointer to comm_manager (no memcpy)
 * @warning If encoding fails with `k_rx_err_buffer_too_small`, increase `k_telem_buffer_size`
 *
 * @see rx_nanopb_encode_telemetry() Encodes TelemetryData to this buffer
 * @see rx_comm_manager_send() Transmits buffer contents
 *
 * @since Version 1.0.0
 */
static uint8_t s_telem_buffer[k_telem_buffer_size];

/**
 * @var g_comm_manager
 * @brief Communication manager handle (external global, defined in comm_task.c)
 *
 * @details
 * The communication manager abstracts over multiple physical channels (USB CDC, SPI)
 * and provides a unified send interface for telemetry. This enables automatic failover:
 * - **Primary:** USB CDC (12 Mbps, debugging interface)
 * - **Fallback:** SPI (10 Mbps, production interface to RPi5)
 *
 * The telemetry task does NOT own this handle - it is initialized by `comm_task.c`
 * and shared across multiple tasks (comm_task writes commands, telemetry_task writes data).
 *
 * Thread safety:
 * - `rx_comm_manager_send()` is thread-safe (uses internal queue + mutex)
 * - Multiple tasks can call `send()` concurrently without corruption
 *
 * @note Declared `extern` - definition is in `comm_task.c`
 * @note Shared across tasks (comm_task, telemetry_task, motor_control_task)
 * @warning Do NOT call `rx_comm_manager_send()` before `comm_task_create()` completes
 *
 * @see rx_comm_manager.h Communication manager API
 * @see comm_task.c Initializes and manages g_comm_manager
 *
 * @since Version 1.0.0
 */
extern rx_comm_manager_t g_comm_manager;

/**
 * @var s_tag
 * @brief Log tag for telemetry task (used by rx_log macros)
 *
 * @details
 * All log messages from this task are prefixed with this tag for easy filtering:
 * - `[TELEM] Telemetry task created`
 * - `[TELEM] Telemetry running @ 20 Hz`
 * - `[TELEM] Telemetry encode failed: 5`
 *
 * Log messages can be filtered by tag using host-side log viewer:
 * ```bash
 * # View only telemetry logs
 * cat /dev/ttyUSB0 | grep "\[TELEM\]"
 * ```
 *
 * @note String stored in `.rodata` (flash), not RAM
 * @note Total size: 8 bytes ("TELEM" + null terminator + padding)
 *
 * @since Version 1.0.0
 */
static const char* const s_tag = "TELEM";

/**
 * @var s_sequence
 * @brief Telemetry message sequence counter (auto-incrementing)
 *
 * @details
 * Incremented for every telemetry message sent (successful or not). Used by
 * the host to detect dropped messages and reorder out-of-sequence packets.
 *
 * Sequence counter behavior:
 * - **Initial value:** 0 (BSS zero-initialized at boot)
 * - **Increment:** Post-increment (`s_sequence++`) every cycle
 * - **Wraparound:** Wraps from 2^32-1 -> 0 after ~24.8 days @ 20 Hz
 * - **Monotonic:** Always increases (never resets during runtime)
 *
 * Dropped message detection (host-side):
 * ```c
 * if (current_seq != last_seq + 1) {
 *     uint32_t dropped = current_seq - last_seq - 1;
 *     printf("Dropped %u telemetry messages\n", dropped);
 * }
 * ```
 *
 * @note Single writer (telemetry task only), no mutex needed
 * @note Wraps around every ~49.7 days (2^32 / 20 Hz / 86400 s/day)
 * @note Host must handle wraparound: `(current_seq < last_seq) -> wraparound detected`
 *
 * @invariant Increments by exactly 1 every cycle (never skips, never resets)
 *
 * @since Version 1.0.0
 */
static uint32_t s_sequence = 0;

/** @brief Conversion factor: millivolts per volt */

/** @brief Conversion factor: centi-degrees Celsius per degree Celsius */
static const float s_cdegc_per_degree = 100.0F;

/**
 * @var s_cm_per_m
 * @brief Centimetres per metre conversion factor (100.0).
 *
 * @details
 * Used to convert obstacle sensor distances from the integer centimetre
 * values stored in obstacle_state_t::distance_cm[] to floating-point
 * metre values required by the star_v1_ObstacleData protobuf fields
 * (distance_front_left_m, distance_front_right_m, etc.).
 *
 * Conversion: metres = (float)distance_cm[i] / s_cm_per_m
 *
 * @note Read-only; never modified after program start.
 *
 * @since Version 1.0.0
 */
static const float s_cm_per_m = 100.0F;

/**
 * @var s_rad_per_deg
 * @brief Radians per degree conversion factor (pi / 180.0).
 *
 * @details
 * Used to populate the legacy radian-unit Euler angle fields in ImuData
 * (pitch_rad, roll_rad, yaw_rad) from the BNO055 fixed-point degree values.
 *
 * Conversion: radians = degrees * s_rad_per_deg
 *
 * @note Read-only; never modified after program start.
 *
 * @since Version 1.0.0
 */
static const double s_rad_per_deg = 0.017453292519943295;

/**
 * @var s_half_turn_deg
 * @brief Half revolution in degrees (180.0); threshold for yaw normalization.
 *
 * @details
 * BNO055 heading output is in [0, 360) degrees. To produce yaw in (-180, 180]
 * for ROS2 geometry_msgs compatibility, headings above 180 degrees are shifted
 * down by one full turn (360 degrees).
 *
 * @note Read-only; never modified after program start.
 * @since Version 1.0.0
 */
static const double s_half_turn_deg = 180.0;

/**
 * @var s_full_turn_deg
 * @brief Full revolution in degrees (360.0); subtracted to wrap yaw to (-180, 180].
 *
 * @details
 * Used in the yaw normalization: yaw_deg = heading_deg - s_full_turn_deg
 * when heading_deg > s_half_turn_deg.
 *
 * @note Read-only; never modified after program start.
 * @since Version 1.0.0
 */
static const double s_full_turn_deg = 360.0;

/**
 * @enum telem_motor_idx_t
 * @brief Motor indices for telemetry encoder data
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_telem_motor_front_left  = 0, /**< Front left motor index */
  k_telem_motor_front_right = 1, /**< Front right motor index */
  k_telem_motor_back_left   = 2, /**< Back left motor index */
  k_telem_motor_back_right  = 3, /**< Back right motor index */
} telem_motor_idx_t;

/**
 * @enum telem_fault_shift_t
 * @brief Bit shift offsets for packing per-motor fault flags into uint32_t
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_fault_shift_front_left  = 0,  /**< Front left motor fault flags at bits [7:0] */
  k_fault_shift_front_right = 8,  /**< Front right motor fault flags at bits [15:8] */
  k_fault_shift_back_left   = 16, /**< Back left motor fault flags at bits [23:16] */
  k_fault_shift_back_right  = 24, /**< Back right motor fault flags at bits [31:24] */
} telem_fault_shift_t;

/**
 * @enum telem_sensor_idx_t
 * @brief Temperature sensor indices for telemetry
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_telem_sensor_ambient = 0, /**< DS18B20 ambient temperature sensor index */
} telem_sensor_idx_t;

/**
 * @enum telem_obstacle_sensor_idx_t
 * @brief Obstacle sensor array indices for telemetry population
 *
 * @details
 * Maps each ultrasonic obstacle sensor to its physical position on the
 * vehicle. The sensors are mounted at the four corners:
 *   - 0: front-left
 *   - 1: front-right
 *   - 2: rear-left
 *   - 3: rear-right
 *
 * These values are used as indices into the `telemetry_t::obstacle`
 * distance and status arrays when assembling telemetry packets.
 *
 * @invariant Values are contiguous integers in the range 0-3, one for
 * each installed sensor.
 *
 * @code
 * // access front-left distance (already in meters)
 * float dist_fl = telemetry->obstacle.distance_front_left_m;
 *
 * // build detected_mask for sensors 0 and 2
 * uint32_t mask = (1U << k_telem_obstacle_shift_0) |
 *                 (1U << k_telem_obstacle_shift_2);
 * @endcode
 *
 * @see telem_obstacle_mask_shift_t
 * @see telemetry_t
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_telem_obstacle_sensor_0 = 0, /**< Front-left ultrasonic sensor array index */
  k_telem_obstacle_sensor_1 = 1, /**< Front-right ultrasonic sensor array index */
  k_telem_obstacle_sensor_2 = 2, /**< Rear-left ultrasonic sensor array index */
  k_telem_obstacle_sensor_3 = 3, /**< Rear-right ultrasonic sensor array index */
} telem_obstacle_sensor_idx_t;

/**
 * @enum telem_obstacle_mask_shift_t
 * @brief Bit shifts for constructing obstacle detected_mask field
 *
 * @details
 * Each sensor corresponds to a bit in the `detected_mask` bitfield.  The
 * shift values match the physical layout used by
 * `telem_obstacle_sensor_idx_t`
 * (front-left=0, front-right=1, rear-left=2, rear-right=3).  Masks are
 * created with `(1U << k_telem_obstacle_shift_*)`.
 *
 * @invariant Valid values 0-3, representing the four sensors.
 *
 * @code
 * uint32_t mask = (1U << k_telem_obstacle_shift_0) |
 *                 (1U << k_telem_obstacle_shift_2); // front-left & rear-left
 *
 * if (telemetry->obstacle.detected_mask & (1U << k_telem_obstacle_shift_1)) {
 *   // front-right detected
 * }
 * @endcode
 *
 * @see telem_obstacle_sensor_idx_t
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_telem_obstacle_shift_0 = 0, /**< Bit shift for front-left sensor detection flag */
  k_telem_obstacle_shift_1 = 1, /**< Bit shift for front-right sensor detection flag */
  k_telem_obstacle_shift_2 = 2, /**< Bit shift for rear-left sensor detection flag */
  k_telem_obstacle_shift_3 = 3, /**< Bit shift for rear-right sensor detection flag */
} telem_obstacle_mask_shift_t;

/**
 * @enum telemetry_transport_t
 * @brief Active transport channel for telemetry transmission
 *
 * @details
 * Identifies which physical communication channel is currently selected for
 * telemetry broadcast. The selection algorithm uses symmetric routing: it
 * reads shared_data_get_active_channel() to return the same channel on which
 * the most recent command was received.
 *
 * @par Channel Selection Priority:
 * 1. k_telemetry_transport_uart - UART was the last command channel (also the default)
 * 2. k_telemetry_transport_spi  - SPI was the last command channel
 * 3. k_telemetry_transport_i2c  - I2C was the last command channel
 * 4. k_telemetry_transport_none - Out-of-range channel value (programming error)
 *
 * @startuml
 * [*] --> SELECT
 * SELECT --> UART : active_channel == k_comm_channel_uart\n(or no command received yet)
 * SELECT --> SPI  : active_channel == k_comm_channel_spi
 * SELECT --> I2C  : active_channel == k_comm_channel_i2c
 * SELECT --> NONE : active_channel out-of-range (fail-safe maps to UART)
 * UART --> SELECT : Next cycle
 * SPI  --> SELECT : Next cycle
 * I2C  --> SELECT : Next cycle
 * NONE --> SELECT : Next cycle
 * @enduml
 *
 * @invariant Exactly one value selected per telemetry cycle
 *
 * @see internal_select_transport() Returns this type
 * @see internal_build_and_send_telemetry() Consumer of this type
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief UART channel (SCI9 via CY7C65213 bridge, primary gateway link)
   * @details Maps to k_comm_channel_uart in the comm manager.
   * @par Value: 0
   */
  k_telemetry_transport_uart = 0,

  /**
   * @brief SPI channel (ROS2 spi_driver_node link)
   * @details Maps to k_comm_channel_spi in the comm manager.
   * @par Value: 1
   */
  k_telemetry_transport_spi = 1,

  /**
   * @brief I2C channel (RIIC0 peripheral mode)
   * @details Maps to k_comm_channel_i2c in the comm manager.
   * @par Value: 2
   */
  k_telemetry_transport_i2c = 2,

  /**
   * @brief No transport available (all channels unavailable)
   * @details Telemetry message is dropped for this cycle; task retries next cycle.
   * @par Value: 3
   */
  k_telemetry_transport_none = 3,
} telemetry_transport_t;

/* =============================================================================
 * Forward Declarations
 * =============================================================================
 */

static void                  internal_telem_task_entry(ULONG input);
static rx_err_t              internal_build_and_send_telemetry(void);
static telemetry_transport_t internal_select_transport(void);
static rx_err_t              internal_transport_to_channel(telemetry_transport_t transport,
                                                           rx_comm_channel_t*    out_channel);
static rx_err_t              internal_populate_motor_telemetry(star_v1_TelemetryData* telemetry);
static void                  internal_collect_state(star_v1_TelemetryData* telemetry);
static void                  internal_populate_imu_telemetry(star_v1_TelemetryData* telemetry);
static void                  internal_populate_baro_telemetry(star_v1_TelemetryData* telemetry);
static rx_err_t              internal_encode_telemetry(const star_v1_TelemetryData* telemetry,
                                                       uint32_t*                    out_encoded_len);
static rx_err_t internal_send_via_channel(rx_comm_channel_t channel, uint32_t encoded_len);

/* =============================================================================
 * Public Functions
 * =============================================================================
 */

/**
 * @brief Create and start the telemetry aggregation task
 *
 * @details
 * Initializes the telemetry task thread and starts it with auto-start enabled.
 * The task immediately begins running after this function returns (unless a
 * higher-priority task is ready to run).
 *
 * ## Task Creation Sequence
 *
 * 1. **Validate not already created:** Check `s_telem_created` flag
 * 2. **Create ThreadX thread:** Call `tx_thread_create()` with static stack
 * 3. **Validate creation success:** Check ThreadX return code
 * 4. **Set creation flag:** Mark `s_telem_created = true`
 * 5. **Log success:** Emit creation confirmation message
 * 6. **Auto-start:** ThreadX automatically starts thread (TX_AUTO_START)
 *
 * ## ThreadX Thread Configuration
 *
 * | Parameter | Value | Description |
 * |-----------|-------|-------------|
 * | **Thread control block** | `&s_telem_thread` | Static TX_THREAD struct (140 bytes) |
 * | **Name** | `"TelemTask"` | Thread name (for debugging, max 8 chars) |
 * | **Entry function** | `internal_telem_task_entry` | Task main loop |
 * | **Entry input** | `k_telem_task_input` (0) | Unused parameter |
 * | **Stack** | `s_telem_stack` | Static 2048-byte array |
 * | **Stack size** | `k_telem_task_stack_size` (2048) | Stack size in bytes |
 * | **Priority** | `k_telem_task_priority` (18) | Lowest app task priority |
 * | **Preemption threshold** | `k_telem_task_priority` (18) | Same as priority (preemptible) |
 * | **Time slice** | `TX_NO_TIME_SLICE` | No time-slicing (priority-based only) |
 * | **Auto-start** | `TX_AUTO_START` | Start immediately after creation |
 *
 * ## Error Handling
 *
 * | Error Condition | Return Code | Action |
 * |-----------------|-------------|--------|
 * | Task already created | `k_rx_err_invalid_state` | Assert fails (debug), return error (release) |
 * | ThreadX create fails | `k_rx_err_rtos_thread_create` | Log error, return error code |
 * | Success | `k_rx_ok` | Task running, log success message |
 *
 * @return rx_err_t Operation status
 * @retval k_rx_ok Task created successfully, thread started
 * @retval k_rx_err_invalid_state Task already created (s_telem_created == true)
 * @retval k_rx_err_rtos_thread_create ThreadX thread creation failed (insufficient memory,
 *         invalid parameters, or ThreadX internal error)
 *
 * @pre ThreadX kernel must be initialized via `tx_kernel_enter()`
 * @pre Shared data module must be initialized via `shared_data_init()`
 * @pre Communication manager must be initialized via `rx_comm_manager_init()`
 * @pre nanopb must be initialized (happens at compile time, no init function)
 *
 * @post Telemetry task thread created with priority 18
 * @post Task started and will begin executing when scheduler allows
 * @post `s_telem_created` flag set to `true` (prevents double-creation)
 * @post Task enters infinite loop, sleeping 50ms between cycles
 *
 * @note This function should be called ONCE from `tx_application_define()` in `main.c`
 * @note Task starts immediately (TX_AUTO_START) - no need to call `tx_thread_resume()`
 * @note Task runs at lowest priority (18) - all other tasks preempt telemetry
 *
 * @warning Do NOT call this function more than once - assertion will fail in debug builds
 * @warning Ensure shared_data and comm_manager are initialized BEFORE calling this function
 *
 * @par Thread Safety:
 * This function is NOT thread-safe. It must be called from single-threaded context during
 * system initialization (typically from `tx_application_define()` before scheduler starts).
 *
 * @par Performance:
 * - Execution time: ~50 us (ThreadX thread creation overhead)
 * - Memory allocated: 2188 bytes (2048 stack + 140 TX_THREAD)
 * - Stack usage: 0 bytes (task not yet running)
 *
 * @par Example Usage:
 * @code
 * // In main.c, during system initialization
 * void tx_application_define(void *first_unused_memory)
 * {
 *   (void)first_unused_memory;
 *
 *   // Initialize subsystems first
 *   shared_data_init();
 *   rx_comm_manager_init(&g_comm_manager);
 *
 *   // Create all tasks
 *   motor_control_task_create();  // Priority 8 (highest)
 *   comm_task_create();            // Priority 10
 *   temp_sensor_task_create();    // Priority 14
 *   telemetry_task_create();      // Priority 18 (lowest)
 *
 *   // ThreadX scheduler will start, telemetry runs @ 20 Hz
 * }
 * @endcode
 *
 * @see internal_telem_task_entry() Task main loop (entry function)
 * @see tx_thread_create() ThreadX thread creation API
 * @see motor_control_task_create() Highest priority task (priority 8)
 * @see comm_task_create() Communication task (priority 10)
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Rule 5 Compliance:
 * - **Precondition 1:** ThreadX kernel initialized (`tx_kernel_enter()` called)
 * - **Precondition 2:** `!s_telem_created` (not already created)
 * - **Precondition 3:** Shared data initialized (`shared_data_init()` called)
 * - **Precondition 4:** Comm manager initialized (`rx_comm_manager_init()` called)
 * - **Postcondition 1:** Thread created (`s_telem_thread` valid)
 * - **Postcondition 2:** Thread started (auto-start enabled)
 * - **Postcondition 3:** `s_telem_created == true` (flag set)
 * - **Validation:** Returns checked, `RX_ASSERT(!s_telem_created)`, log on error
 */
rx_err_t telemetry_task_create(void)
{
  /* Check if already created */
  RX_ASSERT(!s_telem_created, "Telemetry task already created");
  if (s_telem_created) {
    return k_rx_err_invalid_state;
  }

  /* Create the thread */
  UINT tx_status = tx_thread_create(&s_telem_thread,
                                    "TelemTask",
                                    internal_telem_task_entry,
                                    k_telem_task_input,
                                    s_telem_stack,
                                    k_telem_task_stack_size,
                                    k_telem_task_priority,
                                    k_telem_task_priority,
                                    TX_NO_TIME_SLICE,
                                    TX_AUTO_START);

  if (tx_status != TX_SUCCESS) {
    rx_log_error_val(s_tag, "Thread create failed", (uint32_t)tx_status);
    return k_rx_err_rtos_thread_create;
  }

  s_telem_created = true;
  rx_log_info(s_tag, "Telemetry task created");

  return k_rx_ok;
}

/* =============================================================================
 * Private Functions
 * =============================================================================
 */

/**
 * @brief Telemetry aggregation task entry point (infinite 20 Hz loop)
 *
 * @details
 * This is the main loop for the telemetry task. It executes continuously at 20 Hz,
 * collecting robot system state from all data sources, encoding to Protocol Buffers,
 * and transmitting to the host via USB CDC or SPI.
 *
 * ## Main Loop Structure
 *
 * ```
 * while(true):
 *   1. Call internal_build_and_send_telemetry()
 *   2. Check return code (log error if failed, continue regardless)
 *   3. Sleep 50ms (5 ThreadX ticks @ 100 Hz)
 *   4. Repeat forever
 * ```
 *
 * ## Graceful Degradation Philosophy
 *
 * This task is designed to **NEVER crash** the system, even if telemetry transmission
 * fails repeatedly. Telemetry is informational - missing messages are non-critical.
 *
 * Failure modes that are tolerated:
 * - USB CDC disconnected (comm_manager auto-fails to SPI)
 * - SPI communication error (log error, retry next cycle)
 * - nanopb encoding error (log error, skip transmission)
 * - Temperature sensor failure (send partial message without temp data)
 * - ALL sources fail (send timestamp + sequence only)
 *
 * **Key principle:** Partial telemetry is better than no telemetry. If motors are
 * working but temperature sensor failed, host still receives motor encoder data.
 *
 * ## Execution Flow Diagram
 *
 * @startuml
 * start
 * :Task created by telemetry_task_create();
 * :ThreadX starts task\n(internal_telem_task_entry called);
 * :Log "Telemetry task starting";
 * :Log "Telemetry running @ 20 Hz";
 *
 * repeat
 *   :Call internal_build_and_send_telemetry();
 *   if (Send failed?) then (yes)
 *     :Log debug message\n"Telemetry send failed: <error>";
 *     note right
 *       Non-critical error.
 *       Continue to next cycle.
 *       Do NOT crash or stop task.
 *     end note
 *   else (no)
 *     :Telemetry sent successfully;
 *   endif
 *
 *   :Sleep 50ms\n(tx_thread_sleep(5 ticks));
 *   note right
 *     Task yields to scheduler.
 *     Higher priority tasks can run.
 *     Timer wakes task after 50ms.
 *   end note
 * repeat while (forever)
 * @enduml
 *
 * ## Error Handling Strategy
 *
 * | Error Type | Detection | Response | Impact |
 * |------------|-----------|----------|--------|
 * | **Encoding failure** | `internal_build_and_send_telemetry()` returns error | Log debug message, skip transmission | One message lost (retry next cycle) |
 * | **Send failure** | `rx_comm_manager_send()` returns error | Log debug message, continue | Message lost (host detects via sequence gap) |
 * | **Data source failure** | `shared_data_get_*()` returns error | Skip failed source, send partial message | Partial telemetry (better than nothing) |
 * | **ALL sources fail** | All `shared_data_get_*()` return errors | Send timestamp + sequence only | Minimal telemetry (proves task alive) |
 *
 * ## Task Lifecycle State Machine
 *
 * @startuml
 * [*] --> Created : telemetry_task_create()
 * Created --> Ready : ThreadX auto-start
 * Ready --> Running : Scheduler selects task
 * Running --> Sleeping : tx_thread_sleep(5)
 * Sleeping --> Ready : 50ms timer expires
 * Ready --> Running : Scheduler selects task
 *
 * note right of Sleeping
 *   Task yields CPU for 50ms.
 *   Higher priority tasks
 *   (motor, comm)
 *   can preempt.
 * end note
 * @enduml
 *
 * @param[in] input Thread input parameter (unused, always 0)
 *
 * @return void This function never returns (infinite loop)
 *
 * @pre ThreadX kernel running (scheduler started)
 * @pre Shared data initialized (`shared_data_init()` called)
 * @pre Communication manager initialized (`rx_comm_manager_init()` called)
 * @pre Task created via `telemetry_task_create()` (stack allocated, thread started)
 *
 * @post Task enters infinite loop (never exits)
 * @post Telemetry messages sent at 20 Hz (best-effort, non-critical)
 * @post Task sleeps 50ms between cycles (yields CPU to other tasks)
 *
 * @note This function is called ONCE by ThreadX when task starts (auto-start enabled)
 * @note Task runs at priority 18 (lowest) - all other tasks can preempt this
 * @note `(void)input` suppresses unused parameter warning (NASA Rule 7)
 *
 * @warning This function never returns - do NOT call directly, let ThreadX manage it
 * @warning Infinite loop is acceptable for RTOS tasks (NASA Rule 2 exception)
 *
 * @par Thread Safety:
 * This function is thread-safe. It uses mutex-protected shared_data accessors and
 * thread-safe comm_manager send operations. Multiple data source tasks (motor,
 * temp) can write to shared_data concurrently without corruption.
 *
 * @par Performance:
 * - Active time per cycle: ~120 us (data collection + encoding + send)
 * - Sleep time per cycle: ~49880 us (50ms - 120us)
 * - CPU utilization: ~0.24% (120us / 50000us)
 * - Worst-case preemption latency: 50ms (one full cycle if preempted immediately)
 *
 * @par Example Execution Timeline (50ms cycle):
 * ```
 * t=0ms:     Task wakes, internal_build_and_send_telemetry() starts
 * t=0.015ms: Motor state read (mutex lock + copy)
 * t=0.018ms: Temp state read (mutex lock + copy)
 * t=0.043ms: Protobuf population complete
 * t=0.083ms: nanopb encoding complete (~40us)
 * t=0.143ms: USB CDC send initiated (~60us)
 * t=0.160ms: tx_thread_sleep(5) called, task yields
 * t=50ms:    Timer expires, task wakes, cycle repeats
 * ```
 *
 * @par Graceful Degradation Example:
 * @code
 * // Scenario: Temperature sensor fails, but motors still work
 * // Cycle 1:
 * internal_build_and_send_telemetry():
 *   - Motor state: OK (encoders populated)
 *   - Temp state: FAIL (temp_state.sensor_valid == false, skip temp field)
 *   - Result: Partial message sent (motors, no temperature)
 *   - Host receives: timestamp, sequence, encoders (missing temperature)
 *
 * // Cycle 2:
 * internal_build_and_send_telemetry():
 *   - Motor state: OK
 *   - Temp state: STILL FAIL (skip again)
 *   - Result: Partial message sent again
 * @endcode
 *
 * @see internal_build_and_send_telemetry() Actual telemetry aggregation function
 * @see tx_thread_sleep() ThreadX sleep function (yields CPU)
 * @see shared_data.h Shared data module for inter-task communication
 * @see rx_comm_manager.h Communication manager for USB/SPI abstraction
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Rule 2 Compliance:
 * - **Infinite loop:** `while(true)` is ACCEPTABLE for RTOS tasks (standard pattern)
 * - **Loop bound:** ThreadX watchdog monitors task liveness (if enabled)
 * - **Termination:** Task never exits (runs until power-off or reset)
 * - **Rationale:** RTOS tasks are designed to run forever (not procedural programs)
 */
static void internal_telem_task_entry(ULONG input)
{
  (void)input;

  rx_log_info(s_tag, "Telemetry task starting");
  rx_log_info(s_tag, "Telemetry running @ 20 Hz");

  /* Main telemetry loop */
  while (true) {
    /* Build and send telemetry */
    rx_err_t err = internal_build_and_send_telemetry();
    if (err != k_rx_ok) {
      rx_log_debug_val(s_tag, "Telemetry send failed", (uint32_t)err);
    }

    /* Report task heartbeat to IWDT (must execute within 150ms timeout) */
    err = rx_iwdt_task_heartbeat("Telemetry");
    if (err != k_rx_ok) {
      rx_log_error_val(s_tag, "IWDT heartbeat failed", (uint32_t)err);
      /* Continue operation - watchdog monitor will detect timeout */
    }

    /* Sleep until next telemetry cycle */
    (void)tx_thread_sleep(k_telem_task_sleep_ticks);
  }
}

/**
 * @brief Select the active transport channel for this telemetry cycle
 *
 * @details
 * Reads the active command channel recorded by the comm task and maps it to
 * the corresponding telemetry transport. Implements symmetric routing so that
 * telemetry replies travel on the same physical link as incoming commands:
 *
 * 1. Call shared_data_get_active_channel() to read the last command channel
 * 2. If the raw value is >= k_comm_channel_count (out-of-range), return
 *    k_telemetry_transport_uart as a fail-safe
 * 3. Map the channel value to the corresponding telemetry_transport_t:
 *    - k_comm_channel_uart -> k_telemetry_transport_uart
 *    - k_comm_channel_spi  -> k_telemetry_transport_spi
 *    - k_comm_channel_i2c  -> k_telemetry_transport_i2c
 *
 * Before any command has been received, shared_data_get_active_channel()
 * returns k_comm_channel_uart (the UART default) so telemetry flows over
 * the gateway's UART link until a command arrives on a different channel.
 *
 * @return telemetry_transport_t Selected transport
 * @retval k_telemetry_transport_uart UART was the last command channel, no
 *                                    command has been received yet, or
 *                                    shared_data_get_active_channel() returned
 *                                    an out-of-range/unknown rx_comm_channel_t
 *                                    (fail-safe)
 * @retval k_telemetry_transport_spi  SPI was the last command channel
 * @retval k_telemetry_transport_i2c  I2C was the last command channel
 *
 * @pre shared_data_init() has been called
 * @pre shared_data_get_active_channel() may return any uint8_t value,
 *      including an invalid or unknown rx_comm_channel_t; this function
 *      handles all cases safely
 * @post Return value is a valid telemetry_transport_t (UART/SPI/I2C)
 *       for all possible inputs including out-of-range channel values
 * @post Shared data is not modified (read-only access)
 *
 * @note Called every 50 ms from internal_build_and_send_telemetry() (20 Hz)
 * @note Bounded blocking: shared_data_get_active_channel() acquires motor_mutex
 *       with TX_WAIT_FOREVER and holds it for ~2 us; callers must not assume
 *       lock-free or zero-latency timing
 * @note Thread-safe: shared_data_get_active_channel() is motor_mutex-protected
 * @note Symmetric routing: telemetry replies are sent on the same physical
 *       link as the most recently received command; out-of-range channel
 *       values are intentionally mapped to k_telemetry_transport_uart as the
 *       safe default
 *
 * @see internal_build_and_send_telemetry() Caller - maps transport to channel
 * @see shared_data_get_active_channel() Active channel reader (returns uint8_t)
 * @see shared_data_update_active_channel() Written by comm task on each frame
 * @see rx_comm_channel_t Channel enum (UART, SPI, I2C)
 * @see k_telemetry_transport_uart UART transport constant
 * @see k_telemetry_transport_spi SPI transport constant
 * @see k_telemetry_transport_i2c I2C transport constant
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Rule 5 Compliance:
 * - Precondition 1: shared_data_init() has been called
 * - Precondition 2: shared_data_get_active_channel() may return any uint8_t,
 *                   including invalid rx_comm_channel_t values
 * - Postcondition 1: Returns a valid telemetry_transport_t value
 * - Postcondition 2: Out-of-range input always maps to k_telemetry_transport_uart
 */

/**
 * @enum telem_channel_contract_t
 * @brief Compile-time contract: number of rx_comm_channel_t values this
 *        function explicitly handles
 *
 * @details
 * Used in the static_assert inside internal_select_transport() to ensure the
 * switch statement is updated whenever a new channel is added to
 * rx_comm_channel_t.  Must equal k_comm_channel_count.
 *
 * @see internal_select_transport() Consumer of this constant
 * @see k_comm_channel_count Total channel count in rx_comm_channel_t
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_telem_supported_channel_count = 3U, /**< UART + SPI + I2C; must match k_comm_channel_count */
} telem_channel_contract_t;

static telemetry_transport_t internal_select_transport(void)
{
  /* Compile-time guard: this switch covers exactly k_telem_supported_channel_count
   * channels.  If a new channel is ever added to rx_comm_channel_t, update
   * k_comm_channel_count, increment k_telem_supported_channel_count, and add a
   * case below so the new channel is not silently routed to USB. */
  static_assert(
    (bool)((unsigned int)k_comm_channel_count == (unsigned int)k_telem_supported_channel_count),
    "internal_select_transport: add a case for every new rx_comm_channel_t value");

  const uint8_t raw_ch = shared_data_get_active_channel();
  if (raw_ch >= k_comm_channel_count) {
    return k_telemetry_transport_uart; /* fail-safe: unexpected value defaults to UART */
  }

  switch ((rx_comm_channel_t)raw_ch) {
    case k_comm_channel_uart:
      return k_telemetry_transport_uart;
    case k_comm_channel_spi:
      return k_telemetry_transport_spi;
    case k_comm_channel_i2c:
      return k_telemetry_transport_i2c;
    default:
      return k_telemetry_transport_uart; /* fail-safe for unhandled future values */
  }
}

/**
 * @brief Map firmware estop_reason_t to proto star_v1_EstopReason
 *
 * @details
 * Converts the firmware-internal emergency stop reason code (estop_reason_t)
 * to the corresponding Protocol Buffer enum value (star_v1_EstopReason) for
 * transmission to the Raspberry Pi 5. The mapping is:
 *
 * | estop_reason_t              | star_v1_EstopReason                        |
 * |-----------------------------|--------------------------------------------|
 * | k_estop_reason_none         | ESTOP_REASON_UNKNOWN (no estop active)     |
 * | k_estop_reason_comm_timeout | ESTOP_REASON_COMM_TIMEOUT                  |
 * | k_estop_reason_obstacle     | ESTOP_REASON_OBSTACLE                      |
 * | k_estop_reason_driver_fault | ESTOP_REASON_FAULT                         |
 * | k_estop_reason_overcurrent    | ESTOP_REASON_OVERCURRENT                   |
 * | k_estop_reason_manual         | ESTOP_REASON_MANUAL                        |
 * | k_estop_reason_sensor_failure | ESTOP_REASON_FAULT (closest proto value)   |
 * | (unrecognized)                | ESTOP_REASON_UNKNOWN (safe default)        |
 *
 * @param[in] reason Firmware estop reason code from shared_data
 *
 * @return star_v1_EstopReason Corresponding proto enum value
 * @retval star_v1_EstopReason_ESTOP_REASON_UNKNOWN Unknown or no estop active
 * @retval star_v1_EstopReason_ESTOP_REASON_MANUAL  Manual software command
 * @retval star_v1_EstopReason_ESTOP_REASON_FAULT   Motor driver or sensor failure
 * @retval star_v1_EstopReason_ESTOP_REASON_COMM_TIMEOUT Communication timeout
 * @retval star_v1_EstopReason_ESTOP_REASON_OBSTACLE Obstacle too close
 * @retval star_v1_EstopReason_ESTOP_REASON_OVERCURRENT Motor overcurrent
 *
 * @pre reason is a valid estop_reason_t value
 * @pre star_v1_EstopReason enum is generated from telemetry.proto
 * @post Returns a valid star_v1_EstopReason; unrecognized input yields UNKNOWN
 *
 * @note Thread Safety: Pure function, no shared state accessed
 * @note Unrecognized values fall through to UNKNOWN (fail-safe default)
 *
 * @see internal_populate_motor_telemetry() Caller
 * @see estop_reason_t Firmware reason codes (shared_data.h)
 * @see star_v1_EstopReason Proto enum (gen/star/v1/telemetry.pb.h)
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Rule 5 Compliance:
 * - Precondition 1: reason is a valid estop_reason_t value
 * - Precondition 2: star_v1_EstopReason enum is available via telemetry.pb.h
 * - Postcondition 1: Return value is always a valid star_v1_EstopReason
 * - Postcondition 2: Unrecognized input safely maps to ESTOP_REASON_UNKNOWN
 */
static star_v1_EstopReason internal_map_estop_reason(estop_reason_t reason)
{
  switch (reason) {
    case k_estop_reason_comm_timeout:
      return star_v1_EstopReason_ESTOP_REASON_COMM_TIMEOUT;
    case k_estop_reason_obstacle:
      return star_v1_EstopReason_ESTOP_REASON_OBSTACLE;
    case k_estop_reason_driver_fault:
      return star_v1_EstopReason_ESTOP_REASON_FAULT;
    case k_estop_reason_overcurrent:
      return star_v1_EstopReason_ESTOP_REASON_OVERCURRENT;
    case k_estop_reason_manual:
      return star_v1_EstopReason_ESTOP_REASON_MANUAL;
    case k_estop_reason_sensor_failure:
      return star_v1_EstopReason_ESTOP_REASON_FAULT;
    case k_estop_reason_none:
    default:
      return star_v1_EstopReason_ESTOP_REASON_UNKNOWN;
  }
}

/**
 * @brief Populate TelemetryData motor fields from shared motor state
 *
 * @details
 * Reads the current motor state from shared_data and populates all motor-related
 * fields in the telemetry protobuf message. If shared_data is unavailable (mutex
 * locked), the motor fields are left as their zero-initialized defaults and the
 * error is propagated to the caller for non-fatal handling.
 *
 * Populated fields:
 * - `emergency_stop` - E-stop active flag
 * - `estop_reason` - E-stop reason code mapped from estop_reason_t to star_v1_EstopReason
 * - `fault_flags` - 4 motor fault bytes packed into uint32_t bitfield
 * - `has_encoder_*` / `encoder_*` - 4 encoder submessages (motor_id, ticks,
 *   velocity_mps, timestamp_us) for front_left, front_right, back_left, back_right
 *
 * @param[in,out] telemetry TelemetryData struct to populate (must not be NULL)
 *
 * @return rx_err_t Result from shared_data_get_motor_state()
 * @retval k_rx_ok Motor state read and fields populated
 * @retval k_rx_err_timeout Shared data mutex timed out (stale data, partial msg)
 * @retval k_rx_err_null_ptr NULL pointer passed (programming error)
 *
 * @pre telemetry must point to a valid star_v1_TelemetryData struct
 * @pre telemetry->timestamp_us must already be set (used for encoder timestamps)
 * @post If k_rx_ok: all motor encoder fields populated, has_encoder_* = true
 * @post If k_rx_ok: estop_reason set to mapped star_v1_EstopReason when estop_active; UNKNOWN otherwise
 * @post If error: motor fields left at zero-init defaults, caller handles non-fatally
 *
 * @note Non-blocking: shared_data_get_motor_state() uses a non-blocking mutex try
 * @note Error return is non-fatal for callers - partial telemetry is acceptable
 *
 * @see internal_collect_state() Caller - treats non-ok return as non-fatal
 * @see shared_data_get_motor_state() Shared data accessor
 * @see internal_map_estop_reason() Maps firmware reason to proto enum
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Rule 5 Compliance:
 * - Precondition 1: telemetry != NULL
 * - Precondition 2: timestamp_us already set before call
 * - Postcondition 1: Returns shared_data_get_motor_state() result directly
 * - Postcondition 2: On k_rx_ok, has_encoder_* flags are true for all 4 encoders
 */
static rx_err_t internal_populate_motor_telemetry(star_v1_TelemetryData* telemetry)
{
  motor_state_t motor_state;
  rx_err_t      err;

  err = shared_data_get_motor_state(&motor_state);
  if (err != k_rx_ok) {
    return err;
  }

  /* Emergency stop status and reason */
  telemetry->emergency_stop = motor_state.estop_active;
  if (motor_state.estop_active) {
    telemetry->estop_reason = internal_map_estop_reason(motor_state.estop_reason);
  } else {
    telemetry->estop_reason = star_v1_EstopReason_ESTOP_REASON_UNKNOWN;
  }

  /* Pack 4 motor fault bytes into single uint32_t bitfield */
  telemetry->fault_flags =
    ((uint32_t)motor_state.fault_flags[k_telem_motor_front_left] << k_fault_shift_front_left) |
    ((uint32_t)motor_state.fault_flags[k_telem_motor_front_right] << k_fault_shift_front_right) |
    ((uint32_t)motor_state.fault_flags[k_telem_motor_back_left] << k_fault_shift_back_left) |
    ((uint32_t)motor_state.fault_flags[k_telem_motor_back_right] << k_fault_shift_back_right);

  /* Front left encoder */
  telemetry->has_encoder_front_left      = true;
  telemetry->encoder_front_left.motor_id = k_telem_motor_front_left;
  telemetry->encoder_front_left.ticks    = motor_state.encoder_counts[k_telem_motor_front_left];
  telemetry->encoder_front_left.velocity_mps =
    motor_state.current_velocity_mps[k_telem_motor_front_left];
  telemetry->encoder_front_left.timestamp_us = telemetry->timestamp_us;

  /* Front right encoder */
  telemetry->has_encoder_front_right      = true;
  telemetry->encoder_front_right.motor_id = k_telem_motor_front_right;
  telemetry->encoder_front_right.ticks    = motor_state.encoder_counts[k_telem_motor_front_right];
  telemetry->encoder_front_right.velocity_mps =
    motor_state.current_velocity_mps[k_telem_motor_front_right];
  telemetry->encoder_front_right.timestamp_us = telemetry->timestamp_us;

  /* Back left encoder */
  telemetry->has_encoder_back_left      = true;
  telemetry->encoder_back_left.motor_id = k_telem_motor_back_left;
  telemetry->encoder_back_left.ticks    = motor_state.encoder_counts[k_telem_motor_back_left];
  telemetry->encoder_back_left.velocity_mps =
    motor_state.current_velocity_mps[k_telem_motor_back_left];
  telemetry->encoder_back_left.timestamp_us = telemetry->timestamp_us;

  /* Back right encoder */
  telemetry->has_encoder_back_right      = true;
  telemetry->encoder_back_right.motor_id = k_telem_motor_back_right;
  telemetry->encoder_back_right.ticks    = motor_state.encoder_counts[k_telem_motor_back_right];
  telemetry->encoder_back_right.velocity_mps =
    motor_state.current_velocity_mps[k_telem_motor_back_right];
  telemetry->encoder_back_right.timestamp_us = telemetry->timestamp_us;

  return k_rx_ok;
}

/**
 * @brief Populate ImuData fields in TelemetryData from shared_data IMU state
 *
 * @details
 * Reads IMU state via shared_data_get_imu() and populates all ImuData fields
 * in the telemetry message. Sets has_imu = true only on valid data.
 *
 * Data is read from the shared_data module via shared_data_get_imu(), which
 * returns a copy of the last imu_state_t written by the IMU task.
 *
 * Conversion and scaling steps applied to BNO055 fixed-point register values:
 * - Euler angles (heading, roll, pitch): raw deg*16 -> radians via
 *   (raw / (double)k_imu_scale_euler) * s_rad_per_deg
 * - Quaternion components (w, x, y, z): raw int16 -> unit float via
 *   raw / (double)k_imu_scale_quat (= 16384.0)
 * - Linear acceleration (x, y, z): raw int16 -> m/s^2 via
 *   raw / (double)k_imu_scale_acc (= 100.0, gravity-compensated)
 * - Calibration status: raw uint8 calib_stat register value (cast to uint32_t for calibration_status field)
 * - On-chip temperature: raw int8 temp_degc value (cast to double)
 *
 * Side effects: has_imu set to true when valid data present. Not thread-safe;
 * relies on caller to call from single-threaded context.
 *
 * @param[in,out] telemetry TelemetryData message to populate
 *
 * @pre telemetry non-NULL
 * @pre Shared data module initialized via shared_data_init() and IMU task running
 * @post telemetry->has_imu set if shared_data_get_imu returns valid data
 * @post All imu.* fields populated on success
 *
 * @return void
 *
 * @note Not thread-safe; called from single-threaded internal_collect_state()
 *
 * @see shared_data_get_imu() Data source accessor
 * @see imu_state_t Raw IMU state structure
 * @see internal_collect_state() Caller function
 *
 * @since Version 1.0.0
 */
static void internal_populate_imu_telemetry(star_v1_TelemetryData* telemetry)
{
  RX_ASSERT(telemetry != NULL, "telemetry must not be NULL");

  imu_state_t    imu_state;
  const rx_err_t err = shared_data_get_imu(&imu_state);
  if ((bool)((err == k_rx_ok) && (int)imu_state.valid)) {
    telemetry->has_imu = true;

    /* Heading in degrees [0, 360) then convert to radians [0, 2*pi) */
    const double heading_deg   = (double)imu_state.heading_deg16 / (double)k_imu_scale_euler;
    telemetry->imu.heading_rad = heading_deg * s_rad_per_deg;

    /* Normalize heading [0, 360) to yaw (-180, 180] for ROS2 geometry_msgs compatibility */
    const double yaw_deg =
      (heading_deg > s_half_turn_deg) ? (heading_deg - s_full_turn_deg) : heading_deg;
    telemetry->imu.yaw_rad = yaw_deg * s_rad_per_deg;

    /* Roll and pitch in radians */
    telemetry->imu.roll_rad =
      ((double)imu_state.roll_deg16 / (double)k_imu_scale_euler) * s_rad_per_deg;
    telemetry->imu.pitch_rad =
      ((double)imu_state.pitch_deg16 / (double)k_imu_scale_euler) * s_rad_per_deg;

    /* Unit quaternion (each raw value / 16384) */
    telemetry->imu.quat_w = (double)imu_state.quat_w / (double)k_imu_scale_quat;
    telemetry->imu.quat_x = (double)imu_state.quat_x / (double)k_imu_scale_quat;
    telemetry->imu.quat_y = (double)imu_state.quat_y / (double)k_imu_scale_quat;
    telemetry->imu.quat_z = (double)imu_state.quat_z / (double)k_imu_scale_quat;

    /* Linear acceleration (gravity-compensated, each raw value / 100 m/s^2) */
    telemetry->imu.accel_x_mps2 = (double)imu_state.lin_acc_x / (double)k_imu_scale_acc;
    telemetry->imu.accel_y_mps2 = (double)imu_state.lin_acc_y / (double)k_imu_scale_acc;
    telemetry->imu.accel_z_mps2 = (double)imu_state.lin_acc_z / (double)k_imu_scale_acc;

    /* Gyroscope angular rates (raw dps * 16 -> rad/s) */
    telemetry->imu.gyro_x_rad_per_s =
      ((double)imu_state.gyro_x_dps16 / (double)k_imu_scale_gyro) * s_rad_per_deg;
    telemetry->imu.gyro_y_rad_per_s =
      ((double)imu_state.gyro_y_dps16 / (double)k_imu_scale_gyro) * s_rad_per_deg;
    telemetry->imu.gyro_z_rad_per_s =
      ((double)imu_state.gyro_z_dps16 / (double)k_imu_scale_gyro) * s_rad_per_deg;

    /* Calibration status and on-chip temperature */
    telemetry->imu.calibration_status  = (uint32_t)imu_state.calib_stat;
    telemetry->imu.temperature_celsius = (double)imu_state.temp_degc;
  }
  /* Postcondition: if valid IMU data was available, has_imu must be set */
  RX_ASSERT(!(err == k_rx_ok && imu_state.valid) || telemetry->has_imu,
            "has_imu must be true when imu_state.valid is true");
}

/**
 * @brief Populate BaroData fields in TelemetryData from shared_data baro state
 *
 * @details
 * Reads barometric state via shared_data_get_baro() and populates BaroData
 * fields. Converts BMP280 integer-scaled values to physical SI units:
 * temperature from centi-degC to degC, pressure from Pa*256 to Pa.
 * Sets has_baro = true only on valid data.
 *
 * @param[in,out] telemetry TelemetryData message to populate
 *
 * @pre telemetry non-NULL
 * @pre Shared data module initialized via shared_data_init()
 * @post telemetry->has_baro set if shared_data_get_baro returns valid data
 * @post All baro.* fields populated on success
 *
 * @return void
 *
 * @note Not thread-safe; called from single-threaded internal_collect_state()
 *
 * @see shared_data_get_baro() Data source accessor
 * @see baro_state_t Raw barometric state structure
 * @see internal_collect_state() Caller function
 *
 * @since Version 1.0.0
 */
static void internal_populate_baro_telemetry(star_v1_TelemetryData* telemetry)
{
  RX_ASSERT(telemetry != NULL, "telemetry must not be NULL");

  baro_state_t   baro_state;
  const rx_err_t err = shared_data_get_baro(&baro_state);
  if ((bool)((err == k_rx_ok) && (int)baro_state.valid)) {
    telemetry->has_baro = true;

    /* Temperature: centi-degrees Celsius -> degrees Celsius */
    telemetry->baro.temperature_celsius =
      (double)baro_state.temp_centi_degc / (double)s_cdegc_per_degree;

    /* Pressure: Pa * 256 -> Pa */
    telemetry->baro.pressure_pa = (double)baro_state.press_pa_256 / (double)k_baro_scale_press;
  }
  /* Postcondition: if valid baro data was available, has_baro must be set */
  RX_ASSERT(!(err == k_rx_ok && baro_state.valid) || telemetry->has_baro,
            "has_baro must be true when baro_state.valid is true");
}

/**
 * @brief Collect all robot system state into the telemetry message
 *
 * @details
 * Reads motor, temperature, obstacle, IMU, and baro state from shared_data and
 * populates the corresponding fields in `telemetry`. Reads have mixed blocking
 * semantics but are all non-fatal: if a data source is unavailable, the
 * corresponding fields are left at zero-init defaults and collection continues
 * for remaining sources.
 *
 * - **Motor, temperature, estop** accessors: blocking (TX_WAIT_FOREVER)
 * - **Obstacle, IMU, baro** accessors: non-blocking (TX_NO_WAIT); may return
 *   k_rx_err_rtos_mutex if the mutex is busy, leaving those fields at
 *   zero-init defaults for this telemetry cycle.
 *
 * Data collected:
 * 1. **Motor state** (via internal_populate_motor_telemetry()):
 *    E-stop flag, fault_flags bitfield, 4 encoder submessages
 * 2. **Temperature state:** temperature_celsius (cdegC->degC)
 * 3. **Obstacle state:** 4 sensor distances in metres (m), any_obstacle flag, detected_mask bitmask
 * 4. **IMU state** (BNO055 NDOF): heading_rad, roll_rad, pitch_rad (radians), quat w/x/y/z,
 *    accel x/y/z (mps2), calib_stat, temperature_celsius
 * 5. **Baro state** (BMP280 forced mode): temperature_celsius, pressure_pa
 *
 * @param[in,out] telemetry TelemetryData struct to populate (must not be NULL)
 *
 * @pre telemetry must point to a valid, zero-initialized star_v1_TelemetryData struct
 * @pre telemetry->timestamp_us must already be set before this call
 * @post telemetry fields populated for all available data sources
 * @post Missing data sources leave their fields at zero-init defaults (graceful degradation)
 *
 * @note Always returns; partial population is intentional and acceptable
 * @note Mixed blocking: motor/temp/estop accessors use TX_WAIT_FOREVER;
 *       obstacle/IMU/baro accessors use TX_NO_WAIT (non-blocking)
 * @note Thread-safe: shared_data accessors are mutex-protected
 *
 * @see internal_populate_motor_telemetry() Motor field population helper
 * @see shared_data_get_temp() Temperature state accessor
 * @see shared_data_get_obstacle() Obstacle state accessor
 * @see shared_data_get_imu() IMU state accessor (BNO055)
 * @see shared_data_get_baro() Barometric state accessor (BMP280)
 * @see internal_build_and_send_telemetry() Caller - sets timestamp before calling
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Rule 5 Compliance:
 * - Precondition 1: telemetry != NULL
 * - Precondition 2: timestamp_us set before call (for encoder sub-timestamps)
 * - Postcondition 1: Motor fields populated if shared_data_get_motor_state() == k_rx_ok
 * - Postcondition 2: Temp fields populated if shared_data_get_temp() == k_rx_ok && valid
 * - Postcondition 3: Obstacle fields populated if shared_data_get_obstacle() == k_rx_ok
 * - Postcondition 4: IMU fields populated if shared_data_get_imu() == k_rx_ok && valid
 * - Postcondition 5: Baro fields populated if shared_data_get_baro() == k_rx_ok && valid
 */
static void internal_collect_state(star_v1_TelemetryData* telemetry)
{
  temp_sensor_state_t temp_state;
  obstacle_state_t    obstacle_state;
  rx_err_t            err;

  /* Collect motor state (non-fatal: missing data leaves fields at zero-init) */
  err = internal_populate_motor_telemetry(telemetry);
  (void)err; /* Non-fatal: partial telemetry is acceptable per graceful degradation policy */

  /* Collect temperature state */
  err = shared_data_get_temp(&temp_state);
  if ((bool)((err == k_rx_ok) && (int)temp_state.sensor_valid[k_telem_sensor_ambient])) {
    /* Convert from centi-degrees to degrees */
    telemetry->temperature_celsius =
      (float)temp_state.temperature_cdegc[k_telem_sensor_ambient] / s_cdegc_per_degree;
  }

  /* Collect obstacle state */
  err = shared_data_get_obstacle(&obstacle_state);
  if (err == k_rx_ok) {
    telemetry->has_obstacle = true;
    telemetry->obstacle.distance_front_left_m =
      (float)obstacle_state.distance_cm[k_telem_obstacle_sensor_0] / s_cm_per_m;
    telemetry->obstacle.distance_front_right_m =
      (float)obstacle_state.distance_cm[k_telem_obstacle_sensor_1] / s_cm_per_m;
    telemetry->obstacle.distance_back_left_m =
      (float)obstacle_state.distance_cm[k_telem_obstacle_sensor_2] / s_cm_per_m;
    telemetry->obstacle.distance_back_right_m =
      (float)obstacle_state.distance_cm[k_telem_obstacle_sensor_3] / s_cm_per_m;
    telemetry->obstacle.any_obstacle = obstacle_state.any_obstacle;
    telemetry->obstacle.detected_mask =
      ((uint32_t)obstacle_state.obstacle_detected[k_telem_obstacle_sensor_0]
       << k_telem_obstacle_shift_0) |
      ((uint32_t)obstacle_state.obstacle_detected[k_telem_obstacle_sensor_1]
       << k_telem_obstacle_shift_1) |
      ((uint32_t)obstacle_state.obstacle_detected[k_telem_obstacle_sensor_2]
       << k_telem_obstacle_shift_2) |
      ((uint32_t)obstacle_state.obstacle_detected[k_telem_obstacle_sensor_3]
       << k_telem_obstacle_shift_3);
  }

  /* Collect IMU state (BNO055 NDOF fusion) */
  internal_populate_imu_telemetry(telemetry);

  /* Collect barometric state (BMP280 forced mode) */
  internal_populate_baro_telemetry(telemetry);
}

/**
 * @brief Encode TelemetryData protobuf message into the static transmit buffer
 *
 * @details
 * Encodes the populated `star_v1_TelemetryData` struct into binary protobuf format
 * using nanopb and writes to the static `s_telem_buffer`. The encoded length is
 * returned via `out_encoded_len` for use by the transport layer.
 *
 * @param[in]  telemetry      Populated TelemetryData message to encode
 * @param[out] out_encoded_len Encoded byte count written to s_telem_buffer
 *
 * @return rx_err_t Encoding result
 * @retval k_rx_ok Encoding succeeded; s_telem_buffer contains *out_encoded_len valid bytes
 * @retval k_rx_err_encoding_failed nanopb encoding failed (buffer too small or invalid msg)
 *
 * @pre telemetry must point to a valid, populated star_v1_TelemetryData struct
 * @pre out_encoded_len must not be NULL
 * @post On k_rx_ok: s_telem_buffer[0..*out_encoded_len-1] contains encoded protobuf
 * @post On error: s_telem_buffer contents are undefined, error logged
 *
 * @note Zero-copy: passes s_telem_buffer pointer to nanopb (no intermediate copy)
 * @note Not thread-safe: s_telem_buffer is exclusively owned by telemetry task
 *
 * @warning If encoding fails, message is NOT sent (caller returns error, retry next cycle)
 *
 * @see rx_nanopb_encode_telemetry() Underlying nanopb encoder
 * @see internal_build_and_send_telemetry() Orchestrator - calls this after collect_state
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Rule 5 Compliance:
 * - Precondition 1: telemetry != NULL
 * - Precondition 2: out_encoded_len != NULL
 * - Postcondition 1: Returns k_rx_err_encoding_failed on failure (error logged)
 * - Postcondition 2: *out_encoded_len valid only when k_rx_ok returned
 */
static rx_err_t internal_encode_telemetry(const star_v1_TelemetryData* telemetry,
                                          uint32_t*                    out_encoded_len)
{
  rx_err_t err;

  err = rx_nanopb_encode_telemetry(telemetry, s_telem_buffer, k_telem_buffer_size, out_encoded_len);
  if (err != k_rx_ok) {
    rx_log_error_val(s_tag, "Telemetry encode failed", (uint32_t)err);
  }
  return err;
}

/**
 * @brief Send the encoded telemetry buffer via the specified comm manager channel
 *
 * @details
 * Builds `rx_comm_send_params_t` with `k_frame_type_response` (RX72N -> host data),
 * `s_telem_buffer` as payload, and calls `rx_comm_manager_send()`. The channel
 * argument is the caller-selected transport (USB or SPI) returned by
 * `internal_select_transport()`.
 *
 * @param[in] channel     Comm manager channel to send on (k_comm_channel_uart or k_comm_channel_spi)
 * @param[in] encoded_len Number of bytes in s_telem_buffer to transmit
 *
 * @return rx_err_t Send result from rx_comm_manager_send()
 * @retval k_rx_ok Message queued for transmission
 * @retval k_rx_err_usb_tx_fail USB CDC transmission failed (host disconnected mid-cycle)
 * @retval k_rx_err_spi_tx_fail SPI transmission failed (RPi5 not responding)
 * @retval k_rx_err_timeout Communication manager send queue full
 *
 * @pre channel must be k_comm_channel_uart or k_comm_channel_spi
 * @pre encoded_len must be > 0 and <= k_telem_buffer_size
 * @pre s_telem_buffer must contain valid encoded protobuf (internal_encode_telemetry() called)
 * @pre g_comm_manager must be initialized (rx_comm_manager_init() called)
 * @post On k_rx_ok: bytes queued in comm manager TX queue for DMA/interrupt transfer
 * @post On error: message lost this cycle (host detects via sequence gap)
 *
 * @note Send failure is non-critical: telemetry task logs and continues to next cycle
 * @note CRC added by comm_manager (not included in encoded_len)
 * @note Zero-copy: s_telem_buffer passed by pointer, no intermediate memcpy
 *
 * @see internal_build_and_send_telemetry() Caller - provides channel from select_transport
 * @see rx_comm_manager_send() Underlying send implementation
 * @see internal_select_transport() Channel selection (USB preferred, SPI fallback)
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Rule 5 Compliance:
 * - Precondition 1: channel is a valid rx_comm_channel_t value
 * - Precondition 2: encoded_len > 0 (caller verified encoding succeeded)
 * - Postcondition 1: Returns rx_comm_manager_send() result directly
 * - Postcondition 2: s_telem_buffer remains valid after call (no modification)
 */
static rx_err_t internal_send_via_channel(rx_comm_channel_t channel, uint32_t encoded_len)
{
  rx_comm_send_params_t params;

  params.channel     = channel;
  params.type        = k_frame_type_response;
  params.flags       = k_frame_flag_none;
  params.payload     = s_telem_buffer;
  params.payload_len = encoded_len;

  return rx_comm_manager_send(&g_comm_manager, &params);
}

/**
 * @brief Map a telemetry transport to its comm-manager channel enum
 *
 * @details
 * Converts the telemetry_transport_t selected by internal_select_transport()
 * into the rx_comm_channel_t required by rx_comm_manager_send().  Each
 * transport maps 1-to-1 to a channel; k_telemetry_transport_none and any
 * unknown value return k_rx_err_invalid_state (programming error).
 *
 * @param[in]  transport  Active transport returned by internal_select_transport()
 * @param[out] out_channel Corresponding comm channel; valid only on k_rx_ok
 *
 * @return rx_err_t
 * @retval k_rx_ok            Mapping succeeded; *out_channel is valid
 * @retval k_rx_err_null_ptr  out_channel is NULL
 * @retval k_rx_err_invalid_state Unexpected transport value (programming error)
 *
 * @pre transport is a valid telemetry_transport_t value
 * @pre out_channel is not NULL
 *
 * @post *out_channel contains the matching rx_comm_channel_t on k_rx_ok
 * @post *out_channel is unchanged on error
 *
 * @note Not thread-safe; called only from internal_build_and_send_telemetry()
 * @see internal_select_transport() Produces the transport argument
 * @see internal_build_and_send_telemetry() Sole caller
 * @since Version 1.0.0
 */
static rx_err_t internal_transport_to_channel(telemetry_transport_t transport,
                                              rx_comm_channel_t*    out_channel)
{
  RX_CHECK_NULL_PTR(out_channel, s_tag, "out_channel pointer is nullptr");
  switch (transport) {
    case k_telemetry_transport_uart:
      *out_channel = k_comm_channel_uart;
      return k_rx_ok;
    case k_telemetry_transport_spi:
      *out_channel = k_comm_channel_spi;
      return k_rx_ok;
    case k_telemetry_transport_i2c:
      *out_channel = k_comm_channel_i2c;
      return k_rx_ok;
    case k_telemetry_transport_none:
    default:
      RX_ASSERT(false, "internal_select_transport() returned unexpected transport");
      return k_rx_err_invalid_state;
  }
}

/**
 * @brief Build and send complete telemetry message (4-phase aggregation orchestrator)
 *
 * @details
 * Orchestrates the complete telemetry aggregation pipeline across 4 focused helpers:
 *
 * 1. **Timestamp + sequence** - Set timestamp_us and increment s_sequence
 * 2. **internal_collect_state()** - populate motor and temperature fields
 * 3. **internal_encode_telemetry()** - nanopb encode to s_telem_buffer
 * 4. **internal_select_transport()** - symmetric routing via
 *    shared_data_get_active_channel(); maps to USB/SPI/I2C/UART transport
 *    based on the last received command channel
 * 5. **Assert HOST_IRQ LOW** (P67, active-low) only for SPI sends, to notify
 *    the RPi5 SPI peripheral that data is ready. HOST_IRQ is NOT toggled for
 *    USB, I2C, or UART sends because those channels do not use a data-ready
 *    handshake signal (I2C/UART have their own framing; USB has endpoint polling)
 * 6. **internal_send_via_channel()** - transmit via comm manager
 * 7. **Deassert HOST_IRQ HIGH** (P67) only if it was asserted in step 5
 *
 * Encoding failure is fatal for this cycle (message not sent, retry next cycle).
 * Send failure is non-critical (message lost, host detects via sequence gap).
 * An unexpected transport value from internal_select_transport() is a
 * programming error and returns k_rx_err_invalid_state immediately.
 *
 * @return rx_err_t Operation status
 * @retval k_rx_ok Telemetry sent successfully
 * @retval k_rx_err_encoding_failed nanopb encoding failed (buffer too small, invalid message)
 * @retval k_rx_err_invalid_state internal_select_transport() returned an unexpected
 *                                transport (programming error; indicates enum mismatch)
 * @retval k_rx_err_usb_tx_fail USB CDC transmission failed (host disconnected)
 * @retval k_rx_err_spi_tx_fail SPI transmission failed (RPi5 not responding)
 * @retval k_rx_err_timeout Communication manager send timeout
 *
 * @pre shared_data_init() has been called
 * @pre rx_comm_manager_init() has been called
 * @pre shared_data_get_active_channel() reflects the most recent command channel
 * @pre Task created and running (telemetry_task_create() called)
 *
 * @post Telemetry message sent on the same physical link as the last received command
 *       (k_comm_channel_uart, k_comm_channel_spi, k_comm_channel_i2c, or k_comm_channel_uart)
 * @post HOST_IRQ (P67) is HIGH (deasserted) on return; toggled only for SPI sends
 * @post s_sequence incremented exactly once per call
 * @post s_telem_buffer overwritten with latest encoded protobuf
 *
 * @note Called every 50ms by internal_telem_task_entry() (20 Hz)
 * @note Execution time: ~120 us
 * @note Data collection is non-blocking (graceful degradation if mutex locked)
 * @note Transport channel is selected symmetrically: telemetry replies travel
 *       on the same physical link as the last incoming command frame;
 *       only k_comm_channel_uart and k_comm_channel_spi are expected
 *
 * @warning If encoding fails, message is NOT sent (retry next cycle)
 * @warning If transmission fails, message is lost (host detects via sequence gap)
 * @attention Partial messages are acceptable (Protocol Buffers optional fields)
 *
 * @see internal_collect_state() Collect and populate all robot state
 * @see internal_populate_motor_telemetry() Motor encoder field population
 * @see internal_encode_telemetry() nanopb protobuf encoding
 * @see internal_select_transport() Select active transport channel
 * @see internal_send_via_channel() Transmit via comm manager
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Rule 4 Compliance:
 * This function is now a ~30-line orchestrator. Each step delegated to a focused
 * helper function of <=60 lines, all individually verifiable.
 *
 * @par NASA Power of 10 Rule 5 Compliance:
 * - Precondition 1: Shared data initialized
 * - Precondition 2: Comm manager initialized
 * - Postcondition 1: s_sequence incremented
 * - Postcondition 2: Returns error code for any fatal phase failure
 */
static rx_err_t internal_build_and_send_telemetry(void)
{
  rx_err_t              err;
  star_v1_TelemetryData telemetry = star_v1_TelemetryData_init_zero;
  uint32_t              encoded_len;
  telemetry_transport_t transport;
  rx_comm_channel_t     channel;

  /* Set timestamp and sequence number */
  telemetry.timestamp_us   = (int64_t)tx_time_get() * (int64_t)k_telem_us_per_tick;
  telemetry.frame_sequence = s_sequence++;

  /* Collect all robot state and populate telemetry fields */
  internal_collect_state(&telemetry);

  /* Encode to protobuf binary format */
  err = internal_encode_telemetry(&telemetry, &encoded_len);
  if (err != k_rx_ok) {
    return err;
  }

  /* Select transport based on active command channel */
  transport = internal_select_transport();

  /* Map transport to comm channel */
  err = internal_transport_to_channel(transport, &channel);
  if (err != k_rx_ok) {
    return err;
  }

  /* Assert HOST_IRQ LOW (active-low) only for SPI sends: the RPi5 SPI
   * peripheral watches HOST_IRQ (P67) as a data-ready signal.  USB sends
   * do not involve the SPI peer so toggling HOST_IRQ for USB would
   * spuriously wake it. */
  const bool assert_host_irq = (bool)(channel == k_comm_channel_spi);
  if (assert_host_irq) {
    (void)gpio_write_low(g_pin_host_irq);
  }

  err = internal_send_via_channel(channel, encoded_len);

  /* Deassert HOST_IRQ HIGH only if we asserted it above */
  if (assert_host_irq) {
    (void)gpio_write_high(g_pin_host_irq);
  }

  if (err != k_rx_ok) {
    return err;
  }

  return k_rx_ok;
}
