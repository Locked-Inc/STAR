#!/usr/bin/env python3
# esp32-firmware/docs/protocol/python/test_all_commands.py

"""Comprehensive integration test suite for all ESP32 commands"""

import serial
import time
import sys
import struct

class ProtocolTester:
    """ESP32 Protocol Tester"""

    # Command IDs
    CMD_PING = 0x01
    CMD_RESET = 0x02
    CMD_GET_VERSION = 0x03
    CMD_WIFI_CONNECT = 0x10
    CMD_WIFI_DISCONNECT = 0x11
    CMD_WIFI_STATUS = 0x12
    CMD_WIFI_SCAN = 0x13
    CMD_HTTP_GET = 0x20
    CMD_HTTP_POST = 0x21
    CMD_TCP_CONNECT = 0x22
    CMD_TCP_SEND = 0x23
    CMD_TCP_CLOSE = 0x24
    CMD_OTA_CHECK_UPDATE = 0x28
    CMD_OTA_START_UPDATE = 0x29
    CMD_OTA_GET_STATUS = 0x2A

    # Status codes
    STATUS_OK = 0x00
    STATUS_ERROR = 0x01
    STATUS_WIFI_FAILED = 0x10
    STATUS_HTTP_FAILED = 0x20
    STATUS_OTA_FAILED = 0x30

    def __init__(self, port):
        self.uart = serial.Serial(port, 115200, timeout=15)
        time.sleep(0.1)
        self.test_results = []

    def send_packet(self, cmd, payload=b''):
        """Send a protocol packet"""
        payload_len = len(payload)
        packet = bytes([0xA5, cmd, payload_len & 0xFF, (payload_len >> 8) & 0xFF]) + payload
        self.uart.reset_input_buffer()
        self.uart.reset_output_buffer()
        self.uart.write(packet)

    def read_response(self, timeout=5, max_bytes=4096):
        """Read response packet"""
        start_time = time.time()
        while time.time() - start_time < timeout:
            if self.uart.in_waiting >= 5:
                time.sleep(0.1)  # Wait for full response
                return self.uart.read(max_bytes)
        return b''

    def log_test(self, name, passed, details=""):
        """Log test result"""
        status = "[OK] PASSED" if passed else "[FAIL] FAILED"
        result = f"  {status}: {name}"
        if details:
            result += f" - {details}"
        print(result)
        self.test_results.append((name, passed))

    def test_system_commands(self):
        """Test all system commands"""
        print("\n" + "="*60)
        print("SYSTEM COMMANDS")
        print("="*60)

        # Test PING
        print("\n[1] CMD_PING")
        test_data = b"INTEGRATION_TEST"
        self.send_packet(self.CMD_PING, test_data)
        response = self.read_response()
        passed = len(response) >= 7 and response[7:7+len(test_data)] == test_data
        self.log_test("PING with echo data", passed)

        time.sleep(0.3)

        # Test GET_VERSION
        print("\n[2] CMD_GET_VERSION")
        self.send_packet(self.CMD_GET_VERSION)
        response = self.read_response()
        if len(response) >= 10:
            version = f"{response[7]}.{response[8]}.{response[9]}"
            self.log_test("GET_VERSION", True, f"v{version}")
        else:
            self.log_test("GET_VERSION", False)

    def test_wifi_commands(self):
        """Test all WiFi commands"""
        print("\n" + "="*60)
        print("WIFI COMMANDS")
        print("="*60)

        # Test WIFI_STATUS
        print("\n[1] CMD_WIFI_STATUS")
        self.send_packet(self.CMD_WIFI_STATUS)
        response = self.read_response()
        if len(response) >= 13:
            status = response[7]
            status_names = {0: 'DISCONNECTED', 1: 'CONNECTING', 2: 'CONNECTED', 3: 'FAILED'}
            self.log_test("WIFI_STATUS", True, status_names.get(status, 'UNKNOWN'))
            self.wifi_connected = (status == 2)
        else:
            self.log_test("WIFI_STATUS", False)
            self.wifi_connected = False

    def test_http_commands(self):
        """Test all HTTP commands"""
        print("\n" + "="*60)
        print("HTTP COMMANDS")
        print("="*60)

        if not self.wifi_connected:
            print("  [SKIP] SKIPPED: WiFi not connected")
            return

        # Test HTTP GET
        print("\n[1] CMD_HTTP_GET")
        url = "http://httpbin.org/status/200"
        url_bytes = url.encode('utf-8')[:255] + b'\x00'
        url_bytes = url_bytes.ljust(256, b'\x00')

        self.send_packet(self.CMD_HTTP_GET, url_bytes)
        response = self.read_response(timeout=10)
        passed = len(response) >= 5 and response[4] == self.STATUS_OK
        if passed and len(response) >= 7:
            data_len = response[5] | (response[6] << 8)
            self.log_test("HTTP_GET", True, f"{data_len} bytes received")
        else:
            self.log_test("HTTP_GET", passed)

        time.sleep(0.5)

        # Test HTTP POST
        print("\n[2] CMD_HTTP_POST")
        url = "http://httpbin.org/post"
        post_data = '{"test":"integration","id":12345}'

        url_bytes = url.encode('utf-8')
        data_bytes = post_data.encode('utf-8')

        payload = struct.pack('<HH', len(url_bytes), len(data_bytes)) + url_bytes + data_bytes

        self.send_packet(self.CMD_HTTP_POST, payload)
        response = self.read_response(timeout=10)
        passed = len(response) >= 5 and response[4] == self.STATUS_OK
        if passed and len(response) >= 7:
            data_len = response[5] | (response[6] << 8)
            self.log_test("HTTP_POST", True, f"{data_len} bytes received")
        else:
            self.log_test("HTTP_POST", passed)

    def test_tcp_commands(self):
        """Test all TCP commands"""
        print("\n" + "="*60)
        print("TCP COMMANDS")
        print("="*60)

        if not self.wifi_connected:
            print("  [SKIP] SKIPPED: WiFi not connected")
            return

        # Test TCP_CONNECT
        print("\n[1] CMD_TCP_CONNECT")
        host = "httpbin.org"
        port = 80

        host_bytes = host.encode('utf-8')[:127] + b'\x00'
        host_bytes = host_bytes.ljust(128, b'\x00')
        payload = host_bytes + struct.pack('<H', port)

        self.send_packet(self.CMD_TCP_CONNECT, payload)
        response = self.read_response(timeout=5)

        if len(response) >= 8 and response[4] == self.STATUS_OK:
            socket_id = response[7]
            self.log_test("TCP_CONNECT", True, f"Socket ID {socket_id}")

            # Test TCP_SEND
            print("\n[2] CMD_TCP_SEND")
            http_request = "GET /get HTTP/1.1\r\nHost: httpbin.org\r\nConnection: close\r\n\r\n"
            request_bytes = http_request.encode('utf-8')
            payload = struct.pack('<BH', socket_id, len(request_bytes)) + request_bytes

            self.send_packet(self.CMD_TCP_SEND, payload)
            response = self.read_response(timeout=5)

            if len(response) >= 5 and response[4] == self.STATUS_OK:
                data_len = response[5] | (response[6] << 8) if len(response) >= 7 else 0
                self.log_test("TCP_SEND", True, f"{data_len} bytes received")
            else:
                self.log_test("TCP_SEND", False)

            time.sleep(0.5)

            # Test TCP_CLOSE
            print("\n[3] CMD_TCP_CLOSE")
            payload = struct.pack('<B', socket_id)
            self.send_packet(self.CMD_TCP_CLOSE, payload)
            response = self.read_response()
            passed = len(response) >= 5 and response[4] == self.STATUS_OK
            self.log_test("TCP_CLOSE", passed)
        else:
            self.log_test("TCP_CONNECT", False)
            self.log_test("TCP_SEND", False, "skipped")
            self.log_test("TCP_CLOSE", False, "skipped")

    def test_ota_commands(self):
        """Test all OTA commands"""
        print("\n" + "="*60)
        print("OTA COMMANDS")
        print("="*60)

        # Test OTA_CHECK_UPDATE
        print("\n[1] CMD_OTA_CHECK_UPDATE")
        self.send_packet(self.CMD_OTA_CHECK_UPDATE)
        response = self.read_response(timeout=10)

        if len(response) >= 14 and response[4] == self.STATUS_OK:
            update_available = response[7]
            current_version = f"{response[8]}.{response[9]}.{response[10]}"
            new_version = f"{response[11]}.{response[12]}.{response[13]}"
            details = f"Current: {current_version}, New: {new_version}, Available: {'Yes' if update_available else 'No'}"
            self.log_test("OTA_CHECK_UPDATE", True, details)
        else:
            self.log_test("OTA_CHECK_UPDATE", len(response) >= 5 and response[4] == self.STATUS_OK)

        time.sleep(0.5)

        # Test OTA_GET_STATUS
        print("\n[2] CMD_OTA_GET_STATUS")
        self.send_packet(self.CMD_OTA_GET_STATUS)
        response = self.read_response()

        if len(response) >= 17 and response[4] == self.STATUS_OK:
            state = response[7]
            progress = response[8]
            state_names = {0: 'IDLE', 1: 'CHECKING', 2: 'DOWNLOADING', 3: 'VERIFYING',
                          4: 'INSTALLING', 5: 'COMPLETE', 6: 'FAILED'}
            details = f"State: {state_names.get(state, 'UNKNOWN')}, Progress: {progress}%"
            if len(response) > 17 and response[17] != 0:
                details += f", Error: 0x{response[17]:02X}"
            self.log_test("OTA_GET_STATUS", True, details)
        else:
            self.log_test("OTA_GET_STATUS", False)

    def run_all_tests(self):
        """Run complete test suite"""
        print("="*60)
        print("ESP32 COMPREHENSIVE INTEGRATION TEST SUITE")
        print("="*60)

        self.wifi_connected = False

        self.test_system_commands()
        self.test_wifi_commands()
        self.test_http_commands()
        self.test_tcp_commands()
        self.test_ota_commands()

        # Summary
        print("\n" + "="*60)
        print("TEST SUMMARY")
        print("="*60)

        total = len(self.test_results)
        passed = sum(1 for _, p in self.test_results if p)
        failed = total - passed

        print(f"\nTotal tests: {total}")
        print(f"Passed: {passed} [OK]")
        print(f"Failed: {failed} [FAIL]")
        print(f"Success rate: {100*passed//total if total > 0 else 0}%")

        if failed > 0:
            print("\nFailed tests:")
            for name, p in self.test_results:
                if not p:
                    print(f"  - {name}")

        print("\n" + "="*60)

        self.uart.close()

        return failed == 0

def main():
    if len(sys.argv) < 2:
        print("Usage: python test_all_commands.py <serial_port>")
        print("Example: python test_all_commands.py /dev/ttyUSB0")
        print("\nThis will run a comprehensive test of all protocol commands.")
        print("Note: ESP32 should be connected to WiFi for full testing.")
        sys.exit(1)

    tester = ProtocolTester(sys.argv[1])
    success = tester.run_all_tests()

    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
