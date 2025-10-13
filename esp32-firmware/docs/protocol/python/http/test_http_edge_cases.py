#!/usr/bin/env python3
# esp32-firmware/docs/protocol/python/http/test_http_edge_cases.py

"""HTTP edge case and stress tests"""

import serial
import time
import sys
import struct

class HTTPEdgeCaseTester:
    """Tests HTTP edge cases and error conditions"""

    CMD_HTTP_GET = 0x20
    CMD_HTTP_POST = 0x21
    STATUS_OK = 0x00
    STATUS_HTTP_FAILED = 0x20

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

    def read_response(self, timeout=10, max_bytes=8192):
        """Read response packet"""
        start_time = time.time()
        while time.time() - start_time < timeout:
            if self.uart.in_waiting >= 5:
                time.sleep(0.2)  # Wait for full response
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

    def test_invalid_urls(self):
        """Test HTTP with various invalid URLs"""
        print("\n" + "="*60)
        print("INVALID URL TESTS")
        print("="*60)

        invalid_urls = [
            ("", "Empty URL"),
            ("not-a-url", "Invalid format"),
            ("ftp://example.com", "Wrong protocol"),
            ("http://", "Missing host"),
            ("http://nonexistent-domain-12345.com", "Non-existent domain"),
            ("http://httpbin.org:99999", "Invalid port"),
            ("http://256.256.256.256", "Invalid IP"),
        ]

        for url, description in invalid_urls:
            print(f"\n[TEST] {description}: '{url}'")
            url_bytes = url.encode('utf-8')[:255] + b'\x00'
            url_bytes = url_bytes.ljust(256, b'\x00')

            self.send_packet(self.CMD_HTTP_GET, url_bytes)
            response = self.read_response()

            # Should fail or return error status
            if len(response) >= 5:
                if response[4] == self.STATUS_HTTP_FAILED:
                    self.log_test(f"Invalid URL: {description}", True, "Correctly rejected")
                elif response[4] == self.STATUS_OK and url == "":
                    # Empty URL might be handled as error or just fail silently
                    self.log_test(f"Invalid URL: {description}", True, "Handled gracefully")
                else:
                    self.log_test(f"Invalid URL: {description}", False, f"Unexpected status: 0x{response[4]:02X}")
            else:
                self.log_test(f"Invalid URL: {description}", False, "No response")

            time.sleep(0.5)

    def test_http_status_codes(self):
        """Test various HTTP status codes"""
        print("\n" + "="*60)
        print("HTTP STATUS CODE TESTS")
        print("="*60)

        status_codes = [
            (200, "OK"),
            (201, "Created"),
            (204, "No Content"),
            (301, "Moved Permanently"),
            (302, "Found"),
            (400, "Bad Request"),
            (401, "Unauthorized"),
            (403, "Forbidden"),
            (404, "Not Found"),
            (500, "Internal Server Error"),
            (502, "Bad Gateway"),
            (503, "Service Unavailable"),
        ]

        for code, description in status_codes:
            print(f"\n[TEST] HTTP {code} - {description}")
            url = f"http://httpbin.org/status/{code}"
            url_bytes = url.encode('utf-8')[:255] + b'\x00'
            url_bytes = url_bytes.ljust(256, b'\x00')

            self.send_packet(self.CMD_HTTP_GET, url_bytes)
            response = self.read_response()

            if len(response) >= 5:
                if response[4] == self.STATUS_OK:
                    self.log_test(f"HTTP {code}", True, "Request completed")
                else:
                    self.log_test(f"HTTP {code}", False, f"Status: 0x{response[4]:02X}")
            else:
                self.log_test(f"HTTP {code}", False, "No response")

            time.sleep(0.5)

    def test_large_response(self):
        """Test HTTP with large response"""
        print("\n" + "="*60)
        print("LARGE RESPONSE TESTS")
        print("="*60)

        # Test with progressively larger responses
        sizes = [1024, 2048, 4096, 8192]

        for size in sizes:
            print(f"\n[TEST] Response size: {size} bytes")
            url = f"http://httpbin.org/bytes/{size}"
            url_bytes = url.encode('utf-8')[:255] + b'\x00'
            url_bytes = url_bytes.ljust(256, b'\x00')

            self.send_packet(self.CMD_HTTP_GET, url_bytes)
            response = self.read_response(timeout=15, max_bytes=16384)

            if len(response) >= 7 and response[4] == self.STATUS_OK:
                data_len = response[5] | (response[6] << 8)
                # Allow for some overhead (headers, etc.)
                if data_len >= size * 0.8:  # At least 80% of expected size
                    self.log_test(f"Large response {size}B", True, f"Received {data_len} bytes")
                else:
                    self.log_test(f"Large response {size}B", False, f"Only received {data_len} bytes")
            else:
                self.log_test(f"Large response {size}B", False, "Request failed")

            time.sleep(1)

    def test_special_characters(self):
        """Test URLs with special characters"""
        print("\n" + "="*60)
        print("SPECIAL CHARACTER TESTS")
        print("="*60)

        # Test URL with query parameters
        print("\n[TEST] URL with query parameters")
        url = "http://httpbin.org/get?param1=value1&param2=value2&test=123"
        url_bytes = url.encode('utf-8')[:255] + b'\x00'
        url_bytes = url_bytes.ljust(256, b'\x00')

        self.send_packet(self.CMD_HTTP_GET, url_bytes)
        response = self.read_response()
        passed = len(response) >= 5 and response[4] == self.STATUS_OK
        self.log_test("URL with query params", passed)

        time.sleep(0.5)

        # Test URL with fragment
        print("\n[TEST] URL with fragment")
        url = "http://httpbin.org/get#fragment"
        url_bytes = url.encode('utf-8')[:255] + b'\x00'
        url_bytes = url_bytes.ljust(256, b'\x00')

        self.send_packet(self.CMD_HTTP_GET, url_bytes)
        response = self.read_response()
        passed = len(response) >= 5 and response[4] == self.STATUS_OK
        self.log_test("URL with fragment", passed)

    def test_post_edge_cases(self):
        """Test HTTP POST edge cases"""
        print("\n" + "="*60)
        print("POST EDGE CASE TESTS")
        print("="*60)

        # Test POST with empty body
        print("\n[TEST] POST with empty body")
        url = "http://httpbin.org/post"
        post_data = ""

        url_bytes = url.encode('utf-8')
        data_bytes = post_data.encode('utf-8')
        payload = struct.pack('<HH', len(url_bytes), len(data_bytes)) + url_bytes + data_bytes

        self.send_packet(self.CMD_HTTP_POST, payload)
        response = self.read_response()
        passed = len(response) >= 5 and response[4] == self.STATUS_OK
        self.log_test("POST with empty body", passed)

        time.sleep(0.5)

        # Test POST with large body
        print("\n[TEST] POST with large body (1KB)")
        large_data = '{"data":"' + ('x' * 900) + '"}'

        url_bytes = url.encode('utf-8')
        data_bytes = large_data.encode('utf-8')
        payload = struct.pack('<HH', len(url_bytes), len(data_bytes)) + url_bytes + data_bytes

        self.send_packet(self.CMD_HTTP_POST, payload)
        response = self.read_response(timeout=15)
        passed = len(response) >= 5 and response[4] == self.STATUS_OK
        self.log_test("POST with large body", passed, f"Sent {len(data_bytes)} bytes")

        time.sleep(0.5)

        # Test POST with special characters
        print("\n[TEST] POST with special characters")
        special_data = '{"text":"Hello\\nWorld\\t\\"Quotes\\"","unicode":"\\u0041"}'

        url_bytes = url.encode('utf-8')
        data_bytes = special_data.encode('utf-8')
        payload = struct.pack('<HH', len(url_bytes), len(data_bytes)) + url_bytes + data_bytes

        self.send_packet(self.CMD_HTTP_POST, payload)
        response = self.read_response()
        passed = len(response) >= 5 and response[4] == self.STATUS_OK
        self.log_test("POST with special chars", passed)

    def test_timeout_scenarios(self):
        """Test HTTP timeout scenarios"""
        print("\n" + "="*60)
        print("TIMEOUT TESTS")
        print("="*60)

        # Test with delay endpoint
        print("\n[TEST] Delayed response (5 seconds)")
        url = "http://httpbin.org/delay/5"
        url_bytes = url.encode('utf-8')[:255] + b'\x00'
        url_bytes = url_bytes.ljust(256, b'\x00')

        start = time.time()
        self.send_packet(self.CMD_HTTP_GET, url_bytes)
        response = self.read_response(timeout=20)
        elapsed = time.time() - start

        if len(response) >= 5:
            if response[4] == self.STATUS_OK:
                self.log_test("Delayed response", True, f"Completed in {elapsed:.1f}s")
            else:
                self.log_test("Delayed response", False, f"Failed after {elapsed:.1f}s")
        else:
            self.log_test("Delayed response", False, "Timeout")

    def run_all_tests(self):
        """Run all edge case tests"""
        print("="*60)
        print("HTTP EDGE CASE & STRESS TEST SUITE")
        print("="*60)

        self.test_invalid_urls()
        self.test_http_status_codes()
        self.test_large_response()
        self.test_special_characters()
        self.test_post_edge_cases()
        self.test_timeout_scenarios()

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
        print("Usage: python test_http_edge_cases.py <serial_port>")
        print("Example: python test_http_edge_cases.py /dev/ttyUSB0")
        print("\nThis tests HTTP edge cases and error conditions.")
        print("Note: ESP32 must be connected to WiFi for these tests.")
        sys.exit(1)

    tester = HTTPEdgeCaseTester(sys.argv[1])
    success = tester.run_all_tests()
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
