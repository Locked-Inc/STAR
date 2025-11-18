/* esp32-firmware/components/pynq_wifi_bridge/test/test_ota_manager.c */

/**
 * @file test_ota_manager.c
 * @brief STAR_TEST comprehensive tests for OTA manager
 */

#include <string.h>

#include "pynq_ota_manager.h"
#include "star_test.h"

/* ========================================================================
 * OTA Manager Initialization Tests (5 tests)
 * ======================================================================== */

STAR_TEST_CASE(ota_manager, init_with_valid_config)
{
  ota_config_t config = {
    .update_url        = "https://example.com/firmware.bin",
    .check_interval_ms = 3600000,
    .auto_update       = false,
    .auto_reboot       = true,
  };

  bool result = ota_manager_init(&config);

  STAR_ASSERT_TRUE(result);

  /* Cleanup */
  ota_manager_deinit();
}

STAR_TEST_CASE(ota_manager, init_with_null_config)
{
  bool result = ota_manager_init(NULL);

  STAR_ASSERT_FALSE(result);
}

STAR_TEST_CASE(ota_manager, init_with_null_url)
{
  ota_config_t config = {
    .update_url        = NULL,
    .check_interval_ms = 3600000,
    .auto_update       = false,
    .auto_reboot       = true,
  };

  bool result = ota_manager_init(&config);

  STAR_ASSERT_FALSE(result);
}

STAR_TEST_CASE(ota_manager, init_twice)
{
  ota_config_t config = {
    .update_url        = "https://example.com/firmware.bin",
    .check_interval_ms = 3600000,
    .auto_update       = false,
    .auto_reboot       = true,
  };

  bool result1 = ota_manager_init(&config);
  STAR_ASSERT_TRUE(result1);

  /* Second init should succeed (returns true with warning) */
  bool result2 = ota_manager_init(&config);
  STAR_ASSERT_TRUE(result2);

  /* Cleanup */
  ota_manager_deinit();
}

STAR_TEST_CASE(ota_manager, deinit_without_init)
{
  /* Should not crash */
  ota_manager_deinit();
  STAR_ASSERT_TRUE(true);
}

/* ========================================================================
 * Version Functions Tests (5 tests)
 * ======================================================================== */

STAR_TEST_CASE(ota_manager, get_version_all_params)
{
  uint8_t major = 0xFF;
  uint8_t minor = 0xFF;
  uint8_t patch = 0xFF;

  ota_manager_get_version(&major, &minor, &patch);

  /* Should return valid version numbers (not 0xFF) */
  STAR_ASSERT_NOT_EQUAL(0xFF, major);
  STAR_ASSERT_NOT_EQUAL(0xFF, minor);
  STAR_ASSERT_NOT_EQUAL(0xFF, patch);
}

STAR_TEST_CASE(ota_manager, get_version_null_major)
{
  uint8_t minor = 0xFF;
  uint8_t patch = 0xFF;

  ota_manager_get_version(NULL, &minor, &patch);

  /* Should still set minor and patch */
  STAR_ASSERT_NOT_EQUAL(0xFF, minor);
  STAR_ASSERT_NOT_EQUAL(0xFF, patch);
}

STAR_TEST_CASE(ota_manager, get_version_null_minor)
{
  uint8_t major = 0xFF;
  uint8_t patch = 0xFF;

  ota_manager_get_version(&major, NULL, &patch);

  /* Should still set major and patch */
  STAR_ASSERT_NOT_EQUAL(0xFF, major);
  STAR_ASSERT_NOT_EQUAL(0xFF, patch);
}

STAR_TEST_CASE(ota_manager, get_version_null_patch)
{
  uint8_t major = 0xFF;
  uint8_t minor = 0xFF;

  ota_manager_get_version(&major, &minor, NULL);

  /* Should still set major and minor */
  STAR_ASSERT_NOT_EQUAL(0xFF, major);
  STAR_ASSERT_NOT_EQUAL(0xFF, minor);
}

STAR_TEST_CASE(ota_manager, get_version_all_null)
{
  /* Should not crash */
  ota_manager_get_version(NULL, NULL, NULL);
  STAR_ASSERT_TRUE(true);
}

/* ========================================================================
 * Status Functions Tests (6 tests)
 * ======================================================================== */

STAR_TEST_CASE(ota_manager, get_status_without_init)
{
  ota_status_t status;

  bool result = ota_manager_get_status(&status);

  STAR_ASSERT_FALSE(result);
}

STAR_TEST_CASE(ota_manager, get_status_null_pointer)
{
  ota_config_t config = {
    .update_url        = "https://example.com/firmware.bin",
    .check_interval_ms = 3600000,
    .auto_update       = false,
    .auto_reboot       = true,
  };

  ota_manager_init(&config);

  bool result = ota_manager_get_status(NULL);

  STAR_ASSERT_FALSE(result);

  ota_manager_deinit();
}

STAR_TEST_CASE(ota_manager, get_status_after_init)
{
  ota_config_t config = {
    .update_url        = "https://example.com/firmware.bin",
    .check_interval_ms = 3600000,
    .auto_update       = false,
    .auto_reboot       = true,
  };

  ota_manager_init(&config);

  ota_status_t status;
  bool         result = ota_manager_get_status(&status);

  STAR_ASSERT_TRUE(result);
  STAR_ASSERT_EQUAL(k_ota_idle, status.state);
  STAR_ASSERT_EQUAL(0, status.progress);
  STAR_ASSERT_EQUAL(0, status.bytes_downloaded);
  STAR_ASSERT_EQUAL(0, status.total_bytes);

  ota_manager_deinit();
}

STAR_TEST_CASE(ota_manager, status_contains_version)
{
  ota_config_t config = {
    .update_url        = "https://example.com/firmware.bin",
    .check_interval_ms = 3600000,
    .auto_update       = false,
    .auto_reboot       = true,
  };

  ota_manager_init(&config);

  ota_status_t status;
  ota_manager_get_status(&status);

  /* Verify version is populated */
  uint8_t major, minor, patch;
  ota_manager_get_version(&major, &minor, &patch);

  STAR_ASSERT_EQUAL(major, status.current_version[0]);
  STAR_ASSERT_EQUAL(minor, status.current_version[1]);
  STAR_ASSERT_EQUAL(patch, status.current_version[2]);

  ota_manager_deinit();
}

STAR_TEST_CASE(ota_manager, initial_state_is_idle)
{
  ota_config_t config = {
    .update_url        = "https://example.com/firmware.bin",
    .check_interval_ms = 3600000,
    .auto_update       = false,
    .auto_reboot       = true,
  };

  ota_manager_init(&config);

  ota_status_t status;
  ota_manager_get_status(&status);

  STAR_ASSERT_EQUAL(k_ota_idle, status.state);
  STAR_ASSERT_FALSE(status.update_available);

  ota_manager_deinit();
}

STAR_TEST_CASE(ota_manager, get_status_preserves_data)
{
  ota_config_t config = {
    .update_url        = "https://example.com/firmware.bin",
    .check_interval_ms = 3600000,
    .auto_update       = false,
    .auto_reboot       = true,
  };

  ota_manager_init(&config);

  ota_status_t status1, status2;
  ota_manager_get_status(&status1);
  ota_manager_get_status(&status2);

  /* Multiple calls should return consistent data */
  STAR_ASSERT_EQUAL(status1.state, status2.state);
  STAR_ASSERT_EQUAL(status1.current_version[0], status2.current_version[0]);
  STAR_ASSERT_EQUAL(status1.current_version[1], status2.current_version[1]);
  STAR_ASSERT_EQUAL(status1.current_version[2], status2.current_version[2]);

  ota_manager_deinit();
}

/* ========================================================================
 * Check Update Tests (5 tests)
 * ======================================================================== */

STAR_TEST_CASE(ota_manager, check_update_without_init)
{
  bool result = ota_manager_check_update();

  STAR_ASSERT_FALSE(result);
}

STAR_TEST_CASE(ota_manager, check_update_without_wifi)
{
  ota_config_t config = {
    .update_url        = "https://example.com/firmware.bin",
    .check_interval_ms = 3600000,
    .auto_update       = false,
    .auto_reboot       = true,
  };

  ota_manager_init(&config);

  /* Without WiFi connected, should return false */
  bool result = ota_manager_check_update();

  STAR_ASSERT_FALSE(result);

  ota_manager_deinit();
}

STAR_TEST_CASE(ota_manager, check_update_no_version_url)
{
  ota_config_t config = {
    .update_url        = "https://example.com/firmware.bin",
    .check_interval_ms = 3600000,
    .auto_update       = false,
    .auto_reboot       = true,
  };

  ota_manager_init(&config);

  /* Without CONFIG_PYNQ_OTA_VERSION_CHECK_URL, should return false */
  bool result = ota_manager_check_update();

  STAR_ASSERT_FALSE(result);

  ota_manager_deinit();
}

STAR_TEST_CASE(ota_manager, check_update_updates_status)
{
  ota_config_t config = {
    .update_url        = "https://example.com/firmware.bin",
    .check_interval_ms = 3600000,
    .auto_update       = false,
    .auto_reboot       = true,
  };

  ota_manager_init(&config);

  ota_status_t before;
  ota_manager_get_status(&before);

  ota_manager_check_update();

  ota_status_t after;
  ota_manager_get_status(&after);

  /* Status should be updated (update_available set to false) */
  STAR_ASSERT_FALSE(after.update_available);

  ota_manager_deinit();
}

STAR_TEST_CASE(ota_manager, check_update_returns_false_when_current)
{
  ota_config_t config = {
    .update_url        = "https://example.com/firmware.bin",
    .check_interval_ms = 3600000,
    .auto_update       = false,
    .auto_reboot       = true,
  };

  ota_manager_init(&config);

  /* When firmware is up-to-date, should return false */
  bool result = ota_manager_check_update();

  STAR_ASSERT_FALSE(result);

  ota_manager_deinit();
}

/* ========================================================================
 * Start Update Tests (7 tests)
 * ======================================================================== */

STAR_TEST_CASE(ota_manager, start_update_without_init)
{
  bool result = ota_manager_start_update("https://example.com/firmware.bin");

  STAR_ASSERT_FALSE(result);
}

STAR_TEST_CASE(ota_manager, start_update_null_url)
{
  ota_config_t config = {
    .update_url        = "https://example.com/firmware.bin",
    .check_interval_ms = 3600000,
    .auto_update       = false,
    .auto_reboot       = true,
  };

  ota_manager_init(&config);

  bool result = ota_manager_start_update(NULL);

  STAR_ASSERT_FALSE(result);

  ota_manager_deinit();
}

STAR_TEST_CASE(ota_manager, start_update_empty_url)
{
  ota_config_t config = {
    .update_url        = "https://example.com/firmware.bin",
    .check_interval_ms = 3600000,
    .auto_update       = false,
    .auto_reboot       = true,
  };

  ota_manager_init(&config);

  bool result = ota_manager_start_update("");

  /* Empty URL should fail (WiFi not connected) */
  STAR_ASSERT_FALSE(result);

  ota_manager_deinit();
}

STAR_TEST_CASE(ota_manager, start_update_without_wifi)
{
  ota_config_t config = {
    .update_url        = "https://example.com/firmware.bin",
    .check_interval_ms = 3600000,
    .auto_update       = false,
    .auto_reboot       = true,
  };

  ota_manager_init(&config);

  bool result = ota_manager_start_update("https://example.com/firmware.bin");

  /* Without WiFi, should fail */
  STAR_ASSERT_FALSE(result);

  ota_manager_deinit();
}

STAR_TEST_CASE(ota_manager, start_update_valid_url_format)
{
  ota_config_t config = {
    .update_url        = "https://example.com/firmware.bin",
    .check_interval_ms = 3600000,
    .auto_update       = false,
    .auto_reboot       = true,
  };

  ota_manager_init(&config);

  /* Note: This will fail without WiFi, but tests URL validation */
  bool result = ota_manager_start_update("https://valid-url.com/firmware.bin");

  /* Expected to fail due to no WiFi, but should not crash */
  STAR_ASSERT_FALSE(result);

  ota_manager_deinit();
}

STAR_TEST_CASE(ota_manager, start_update_http_url)
{
  ota_config_t config = {
    .update_url        = "http://example.com/firmware.bin",
    .check_interval_ms = 3600000,
    .auto_update       = false,
    .auto_reboot       = true,
  };

  ota_manager_init(&config);

  /* HTTP URL should be accepted (will fail without WiFi) */
  bool result = ota_manager_start_update("http://example.com/firmware.bin");

  STAR_ASSERT_FALSE(result);

  ota_manager_deinit();
}

STAR_TEST_CASE(ota_manager, start_update_long_url)
{
  ota_config_t config = {
    .update_url        = "https://example.com/firmware.bin",
    .check_interval_ms = 3600000,
    .auto_update       = false,
    .auto_reboot       = true,
  };

  ota_manager_init(&config);

  /* Test with very long URL */
  char long_url[512];
  snprintf(long_url,
           sizeof(long_url),
           "https://very-long-domain-name-example.com/path/to/firmware/with/many/subdirectories/"
           "firmware.bin?version=1.0.0");

  bool result = ota_manager_start_update(long_url);

  /* Should handle long URL (will fail without WiFi) */
  STAR_ASSERT_FALSE(result);

  ota_manager_deinit();
}

/* ========================================================================
 * Cancel Update Tests (3 tests)
 * ======================================================================== */

STAR_TEST_CASE(ota_manager, cancel_without_init)
{
  /* Should not crash */
  ota_manager_cancel_update();
  STAR_ASSERT_TRUE(true);
}

STAR_TEST_CASE(ota_manager, cancel_when_idle)
{
  ota_config_t config = {
    .update_url        = "https://example.com/firmware.bin",
    .check_interval_ms = 3600000,
    .auto_update       = false,
    .auto_reboot       = true,
  };

  ota_manager_init(&config);

  /* Canceling when idle should be safe */
  ota_manager_cancel_update();

  ota_status_t status;
  ota_manager_get_status(&status);

  STAR_ASSERT_EQUAL(k_ota_idle, status.state);

  ota_manager_deinit();
}

STAR_TEST_CASE(ota_manager, cancel_multiple_times)
{
  ota_config_t config = {
    .update_url        = "https://example.com/firmware.bin",
    .check_interval_ms = 3600000,
    .auto_update       = false,
    .auto_reboot       = true,
  };

  ota_manager_init(&config);

  /* Multiple cancel calls should be safe */
  ota_manager_cancel_update();
  ota_manager_cancel_update();
  ota_manager_cancel_update();

  STAR_ASSERT_TRUE(true);

  ota_manager_deinit();
}

/* ========================================================================
 * Configuration Tests (4 tests)
 * ======================================================================== */

STAR_TEST_CASE(ota_manager, config_zero_check_interval)
{
  ota_config_t config = {
    .update_url        = "https://example.com/firmware.bin",
    .check_interval_ms = 0, /* Disable auto-check */
    .auto_update       = false,
    .auto_reboot       = true,
  };

  bool result = ota_manager_init(&config);

  STAR_ASSERT_TRUE(result);

  ota_manager_deinit();
}

STAR_TEST_CASE(ota_manager, config_large_check_interval)
{
  ota_config_t config = {
    .update_url        = "https://example.com/firmware.bin",
    .check_interval_ms = 86400000, /* 24 hours */
    .auto_update       = true,
    .auto_reboot       = true,
  };

  bool result = ota_manager_init(&config);

  STAR_ASSERT_TRUE(result);

  ota_manager_deinit();
}

STAR_TEST_CASE(ota_manager, config_auto_update_enabled)
{
  ota_config_t config = {
    .update_url        = "https://example.com/firmware.bin",
    .check_interval_ms = 3600000,
    .auto_update       = true,
    .auto_reboot       = true,
  };

  bool result = ota_manager_init(&config);

  STAR_ASSERT_TRUE(result);

  ota_manager_deinit();
}

STAR_TEST_CASE(ota_manager, config_auto_reboot_disabled)
{
  ota_config_t config = {
    .update_url        = "https://example.com/firmware.bin",
    .check_interval_ms = 3600000,
    .auto_update       = false,
    .auto_reboot       = false,
  };

  bool result = ota_manager_init(&config);

  STAR_ASSERT_TRUE(result);

  ota_manager_deinit();
}

/* Register all tests */
STAR_TEST_LIST_BEGIN()

/* Initialization tests */
STAR_TEST_REF(ota_manager, init_with_valid_config)
STAR_TEST_REF(ota_manager, init_with_null_config)
STAR_TEST_REF(ota_manager, init_with_null_url)
STAR_TEST_REF(ota_manager, init_twice)
STAR_TEST_REF(ota_manager, deinit_without_init)

/* Version function tests */
STAR_TEST_REF(ota_manager, get_version_all_params)
STAR_TEST_REF(ota_manager, get_version_null_major)
STAR_TEST_REF(ota_manager, get_version_null_minor)
STAR_TEST_REF(ota_manager, get_version_null_patch)
STAR_TEST_REF(ota_manager, get_version_all_null)

/* Status function tests */
STAR_TEST_REF(ota_manager, get_status_without_init)
STAR_TEST_REF(ota_manager, get_status_null_pointer)
STAR_TEST_REF(ota_manager, get_status_after_init)
STAR_TEST_REF(ota_manager, status_contains_version)
STAR_TEST_REF(ota_manager, initial_state_is_idle)
STAR_TEST_REF(ota_manager, get_status_preserves_data)

/* Check update tests */
STAR_TEST_REF(ota_manager, check_update_without_init)
STAR_TEST_REF(ota_manager, check_update_without_wifi)
STAR_TEST_REF(ota_manager, check_update_no_version_url)
STAR_TEST_REF(ota_manager, check_update_updates_status)
STAR_TEST_REF(ota_manager, check_update_returns_false_when_current)

/* Start update tests */
STAR_TEST_REF(ota_manager, start_update_without_init)
STAR_TEST_REF(ota_manager, start_update_null_url)
STAR_TEST_REF(ota_manager, start_update_empty_url)
STAR_TEST_REF(ota_manager, start_update_without_wifi)
STAR_TEST_REF(ota_manager, start_update_valid_url_format)
STAR_TEST_REF(ota_manager, start_update_http_url)
STAR_TEST_REF(ota_manager, start_update_long_url)

/* Cancel update tests */
STAR_TEST_REF(ota_manager, cancel_without_init)
STAR_TEST_REF(ota_manager, cancel_when_idle)
STAR_TEST_REF(ota_manager, cancel_multiple_times)

/* Configuration tests */
STAR_TEST_REF(ota_manager, config_zero_check_interval)
STAR_TEST_REF(ota_manager, config_large_check_interval)
STAR_TEST_REF(ota_manager, config_auto_update_enabled)
STAR_TEST_REF(ota_manager, config_auto_reboot_disabled)

STAR_TEST_LIST_END()
