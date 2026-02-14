# Frame Drop Baseline Metrics

**Purpose:** Establish performance baselines for the STAR system's frame drop diagnostics to validate Virtual RX72N simulator behavior and set acceptance criteria for integration tests.

**Collection System:**
- Virtual RX72N simulator
- Gateway (Go)
- ROS2 gateway bridge
- Diagnostic monitoring at `/diagnostics` topic

---

## Baseline Collection Methodology

### Test Scenarios

1. **Idle Baseline** - No commands, passive telemetry only
   - No teleop commands published
   - Telemetry flowing at 10 Hz (expected)
   - Duration: 30-60 minutes

2. **Active Control Baseline** - Continuous command stream
   - Teleop commands at 50 Hz
   - Telemetry at 10 Hz
   - Duration: 30-60 minutes

3. **Stress Test** - High frequency with multiple subscribers
   - Teleop commands at 100 Hz
   - Multiple topic echo subscribers (load testing)
   - Duration: 15-30 minutes

### Data Collection

Run baseline collection script:
```bash
cd star-ros2/scripts

# Idle baseline (30 minutes)
./collect_baseline_metrics.sh 30 idle

# Active control baseline (30 minutes)
# In separate terminal: ros2 topic pub -r 50 /cmd_vel geometry_msgs/msg/Twist "linear: {x: 0.5}"
./collect_baseline_metrics.sh 30 active_control

# Stress test (15 minutes)
# In separate terminals:
#   ros2 topic echo /odom/unfiltered
#   ros2 topic echo /robot_status
#   ros2 topic pub -r 100 /cmd_vel geometry_msgs/msg/Twist "linear: {x: 1.0}"
./collect_baseline_metrics.sh 15 stress_test
```

---

## Baseline Results

### Idle Baseline (No Commands)

**Collection Date:** [TBD - Run script to populate]

**Telemetry Statistics:**
- Expected rate: 10 Hz
- Total frames: [TBD]
- Frames dropped: [TBD]
- Drop rate: [TBD]%

**Teleop Statistics:**
- Expected rate: 0 Hz (no commands)
- Total frames: [TBD]
- Frames dropped: [TBD]
- Drop rate: N/A

**Conclusion:** [TBD after collection]
- ✅ System idle performance meets expectations
- ⚠️ Observed issues: [describe any anomalies]
- ❌ Performance below acceptable threshold

---

### Active Control Baseline (50 Hz Commands)

**Collection Date:** [TBD - Run script to populate]

**Teleop Statistics:**
- Expected rate: 50 Hz
- Total frames: [TBD]
- Frames dropped: [TBD]
- Drop rate: [TBD]%

**Telemetry Statistics:**
- Expected rate: 10 Hz
- Total frames: [TBD]
- Frames dropped: [TBD]
- Drop rate: [TBD]%

**Conclusion:** [TBD after collection]
- ✅ Normal operation performance acceptable
- ⚠️ Observed issues: [describe any anomalies]
- ❌ Performance below acceptable threshold

---

### Stress Test (100 Hz Commands + Multiple Subscribers)

**Collection Date:** [TBD - Run script to populate]

**Teleop Statistics:**
- Expected rate: 100 Hz
- Total frames: [TBD]
- Frames dropped: [TBD]
- Drop rate: [TBD]%

**Telemetry Statistics:**
- Expected rate: 10 Hz
- Total frames: [TBD]
- Frames dropped: [TBD]
- Drop rate: [TBD]%

**System Load:**
- CPU usage: [TBD]
- Memory usage: [TBD]
- Additional subscribers: 2 (odom/unfiltered, robot_status)

**Conclusion:** [TBD after collection]
- ✅ Stress test performance acceptable
- ⚠️ Observed issues: [describe any anomalies]
- ❌ Performance below acceptable threshold

---

## Performance Acceptance Criteria

Based on these baselines, integration tests should validate:

### Normal Operation (Active Control Scenario)
- ✅ **Teleop drop rate < 1.0%** - Critical for real-time control
- ✅ **Telemetry drop rate < 5.0%** - Acceptable for monitoring

### Stress Conditions (Stress Test Scenario)
- ✅ **Telemetry drop rate < 10.0%** - Acceptable under load
- ⚠️ **Any drop rate > 10%** - Indicates system issues requiring investigation

### System Health Indicators
- ✅ No process crashes during extended runs
- ✅ No memory leaks (stable memory usage over time)
- ✅ Consistent frame timing (no large jitter)
- ✅ Graceful degradation under load (no complete failures)

---

## Analysis and Recommendations

### Expected Results

**Virtual RX72N Simulator:**
- Should produce consistent telemetry at 10 Hz
- Should handle command bursts without data loss
- Should demonstrate realistic timing behavior

**Gateway:**
- HARQ protocol should prevent frame loss via retransmissions
- FEC should correct occasional bit errors
- Sequence tracking should accurately detect any drops

**ROS2 Bridge:**
- gRPC calls should complete reliably
- Diagnostics should publish accurate statistics
- No frames should be lost in ROS2 layer

### Performance Targets

| Metric | Target | Rationale |
|--------|--------|-----------|
| Idle telemetry drop rate | < 1% | Virtual simulator should be reliable with no load |
| Active teleop drop rate | < 1% | Critical for safe robot control |
| Active telemetry drop rate | < 5% | Monitoring data can tolerate minor loss |
| Stress telemetry drop rate | < 10% | Acceptable degradation under extreme load |

### Troubleshooting Guide

**If drop rates exceed thresholds:**

1. **Check Virtual RX72N performance**
   - Review `virtual_rx72n.log` for errors
   - Verify socket communication is stable
   - Check CPU usage during collection

2. **Check Gateway HARQ**
   - Review `gateway.log` for retransmission counts
   - Verify FEC is decoding successfully
   - Check for timeout errors

3. **Check ROS2 Bridge**
   - Review `ros2_bridge.log` for gRPC errors
   - Verify callback timing (should be < 100ms)
   - Check for thread contention issues

4. **System-level issues**
   - Verify adequate CPU/memory resources
   - Check for competing processes
   - Review kernel scheduling parameters

---

## Next Steps

After establishing baselines:

1. **Week 4-5:** Implement foundational integration tests using these metrics as acceptance criteria
2. **Week 6:** Add integration tests to CI/CD pipeline
3. **Post-MVP:** Expand test coverage and advanced diagnostics

### Integration Test Strategy

With observability in place, integration tests can:
- ✅ Validate performance, not just correctness
- ✅ Fast debugging when tests fail (check diagnostics first)
- ✅ Confidence that Virtual RX72N behaves realistically
- ✅ Data-driven performance optimization

### Future Enhancements

- **Real-time graphing** of drop rates during collection
- **Automatic comparison** against previous baselines
- **Alerting** when drop rates exceed thresholds
- **Performance regression testing** in CI/CD
- **Hardware baseline collection** when RX72N firmware is available

---

## Appendix: Raw Data Location

Baseline data is stored in `star-ros2/baselines/` directory:

```
star-ros2/baselines/
├── idle_YYYYMMDD_HHMMSS/
│   ├── virtual_rx72n.log
│   ├── gateway.log
│   ├── ros2_bridge.log
│   ├── diagnostics.log
│   └── SUMMARY.txt
├── active_control_YYYYMMDD_HHMMSS/
│   └── ...
└── stress_test_YYYYMMDD_HHMMSS/
    └── ...
```

Each baseline run creates a timestamped directory with all logs and a summary file.

---

**Last Updated:** 2026-01-22
**Status:** Template ready for baseline collection
