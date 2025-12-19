/* lib/star_core/src/star_core.c */

/**
 * @file star_core.c
 * @brief Core module placeholder (interfaces are header-only)
 *
 * This file exists to satisfy PlatformIO's library build requirements.
 * The actual interfaces are defined in headers and are purely abstract.
 */

/**
 * @brief Library version string
 *
 * Static constant following the s_ prefix naming convention for static variables.
 * Version follows semantic versioning: MAJOR.MINOR.PATCH
 */
static const char* const s_version = "1.0.0";

const char* star_core_version(void)
{
  return s_version;
}
