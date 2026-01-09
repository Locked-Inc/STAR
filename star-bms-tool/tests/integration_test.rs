// Integration tests for BMS Tool
// Tests full CLI + Mock Device communication flow
// STAR Project - Texas A&M University

use std::process::{Child, Command, Stdio};
use std::thread;
use std::time::Duration;

/// Helper to start the mock device on a PTY
fn start_mock_device(pty_path: &str) -> Child {
    Command::new("./target/release/mock_device")
        .arg(pty_path)
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .spawn()
        .expect("Failed to start mock device")
}

/// Helper to run a CLI command and return stdout
fn run_cli_command(port: &str, args: &[&str]) -> String {
    let output = Command::new("./target/release/app")
        .arg("--port")
        .arg(port)
        .args(args)
        .output()
        .expect("Failed to run CLI command");

    String::from_utf8_lossy(&output.stdout).to_string()
}

/// Helper to cleanup processes
fn cleanup_processes(mut mock: Child, mut socat: Child, mock_pty: &str, client_pty: &str) {
    mock.kill().ok();
    mock.wait().ok();
    socat.kill().ok();
    socat.wait().ok();
    std::fs::remove_file(mock_pty).ok();
    std::fs::remove_file(client_pty).ok();
}

#[test]
#[ignore] // Requires socat and mock device binary
fn test_integration_with_mock_device_telemetry() {
    // This test requires:
    // 1. Mock device binary built
    // 2. socat installed
    // 3. PTY pair created

    let mock_pty = "/tmp/bms_mock_test";
    let client_pty = "/tmp/bms_client_test";

    // Start socat in background to create PTY pair
    let socat = Command::new("socat")
        .args([
            "-d",
            "-d",
            &format!("pty,raw,echo=0,link={}", mock_pty),
            &format!("pty,raw,echo=0,link={}", client_pty),
        ])
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .spawn()
        .expect("Failed to start socat - is it installed?");

    thread::sleep(Duration::from_millis(500)); // Let PTY pair initialize

    // Start mock device
    let mock = start_mock_device(mock_pty);

    thread::sleep(Duration::from_millis(500)); // Let mock device initialize

    // Run telemetry command
    let output = run_cli_command(client_pty, &["telemetry"]);

    // Verify output contains expected data
    assert!(output.contains("Voltage:"));
    assert!(output.contains("14.8")); // 14800 mV = 14.8 V
    assert!(output.contains("Current:"));
    assert!(output.contains("-1.5")); // -1500 mA = -1.5 A
    assert!(output.contains("SOC:"));
    assert!(output.contains("75%"));

    // Cleanup
    cleanup_processes(mock, socat, mock_pty, client_pty);
}

#[test]
#[ignore] // Requires socat and mock device binary
fn test_integration_with_mock_device_cell_voltages() {
    let mock_pty = "/tmp/bms_mock_test2";
    let client_pty = "/tmp/bms_client_test2";

    let socat = Command::new("socat")
        .args([
            "-d",
            "-d",
            &format!("pty,raw,echo=0,link={}", mock_pty),
            &format!("pty,raw,echo=0,link={}", client_pty),
        ])
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .spawn()
        .expect("Failed to start socat");

    thread::sleep(Duration::from_millis(500));

    let mock = start_mock_device(mock_pty);

    thread::sleep(Duration::from_millis(500));

    // Run cell-voltages command
    let output = run_cli_command(client_pty, &["cell-voltages", "--num-cells", "4"]);

    // Verify output contains cell voltage data
    assert!(output.contains("Cell"));
    assert!(output.contains("3.7")); // 3700 mV = 3.7 V

    cleanup_processes(mock, socat, mock_pty, client_pty);
}

#[test]
#[ignore] // Requires socat and mock device binary
fn test_integration_with_mock_device_info() {
    let mock_pty = "/tmp/bms_mock_test3";
    let client_pty = "/tmp/bms_client_test3";

    let socat = Command::new("socat")
        .args([
            "-d",
            "-d",
            &format!("pty,raw,echo=0,link={}", mock_pty),
            &format!("pty,raw,echo=0,link={}", client_pty),
        ])
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .spawn()
        .expect("Failed to start socat");

    thread::sleep(Duration::from_millis(500));

    let mock = start_mock_device(mock_pty);

    thread::sleep(Duration::from_millis(500));

    // Run device-info command
    let output = run_cli_command(client_pty, &["device-info"]);

    // Verify output contains device info
    assert!(output.contains("Texas Instruments"));
    assert!(output.contains("BQ78350"));
    assert!(output.contains("LION"));

    cleanup_processes(mock, socat, mock_pty, client_pty);
}

#[test]
#[ignore] // Requires socat and mock device binary
fn test_integration_register_read_write() {
    let mock_pty = "/tmp/bms_mock_test4";
    let client_pty = "/tmp/bms_client_test4";

    let socat = Command::new("socat")
        .args([
            "-d",
            "-d",
            &format!("pty,raw,echo=0,link={}", mock_pty),
            &format!("pty,raw,echo=0,link={}", client_pty),
        ])
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .spawn()
        .expect("Failed to start socat");

    thread::sleep(Duration::from_millis(500));

    let mock = start_mock_device(mock_pty);

    thread::sleep(Duration::from_millis(500));

    // Test register read
    let output = run_cli_command(client_pty, &["read-register", "0x00"]);
    assert!(output.contains("Address: 0x00"));
    assert!(output.contains("Data:"));

    // Test register write
    let output = run_cli_command(client_pty, &["write-register", "0x10", "0x42"]);
    assert!(output.contains("success"));

    cleanup_processes(mock, socat, mock_pty, client_pty);
}
