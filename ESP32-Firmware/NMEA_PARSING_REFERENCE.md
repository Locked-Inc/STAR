# NMEA 0183 Sentence Parsing Reference
## GY-GPS6MV2 (NEO-6M) GPS Module

---

## NMEA 0183 PROTOCOL OVERVIEW

### Sentence Structure
```
$<TALKER><FORMATTER>,<data>,<data>,...<data>*<CHECKSUM><CR><LF>

$ = Start delimiter (0x24)
TALKER = Two-character talker ID (e.g., GP = GPS)
FORMATTER = Three-character sentence formatter (e.g., GGA, RMC)
, = Field separator
* = Checksum delimiter
CHECKSUM = Two hexadecimal digits
<CR><LF> = Message terminator (0x0D 0x0A)
```

### Constraints
- Maximum sentence length: 82 characters (including $ and <CR><LF>)
- Typical sentence length: 45-65 characters
- Update rate: 1 Hz (default), 5 Hz (maximum)
- Data format: 8-bit ASCII characters (0x20 to 0x7E printable)
- Invalid data represented as empty field

### Talker Identifiers
| ID | Meaning |
|----|---------|
| GP | GPS (Global Positioning System) |
| GL | GLONASS |
| GN | Combined GPS/GLONASS |
| GB | BeiDou |
| GA | Galileo |

---

## SUPPORTED SENTENCE TYPES

### 1. GGA - Global Positioning System Fix Data

**Purpose**: Position, altitude, fix quality, number of satellites, precision

**Transmission Interval**: Every 1 second (default)

**Format**:
```
$GPGGA,hhmmss.ss,ddmm.mmmm,a,dddmm.mmmm,a,x,xx,x.x,x.x,M,x.x,M,x,xxxx*hh<CR><LF>
```

**Fields**:
```
0  $GPGGA        - Sentence type
1  hhmmss.ss     - UTC Time (hours, minutes, seconds, hundredths)
2  ddmm.mmmm     - Latitude
3  a             - N (North) or S (South)
4  dddmm.mmmm    - Longitude
5  a             - E (East) or W (West)
6  x             - Fix Quality (0=none, 1=GPS, 2=DGPS, 3=PPS, 4=RTK fixed, 5=RTK float)
7  xx            - Number of satellites (00-12)
8  x.x           - Horizontal Dilution of Precision (HDOP)
9  x.x           - Altitude above mean sea level (meters)
10 M             - Altitude unit (M = meters)
11 x.x           - Geoid separation (height of geoid above WGS84 ellipsoid)
12 M             - Geoid separation unit (M = meters)
13 x             - Age of differential data (seconds) - empty if not DGPS
14 xxxx          - Differential reference station ID (0000-4095) - empty if not DGPS
```

**Example**:
```
$GPGGA,123519.00,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47
```

**Parsing** (safe implementation):
```c
typedef struct {
    float latitude;      // Decimal degrees
    float longitude;     // Decimal degrees
    float altitude;      // Meters
    uint8_t fix_quality; // 0-8
    uint8_t satellites;  // 0-12
    float hdop;          // 0.5-99.9
} gga_data_t;

bool parse_gga(const char *sentence, gga_data_t *data)
{
    char buffer[256];
    strncpy(buffer, sentence, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    // Remove checksum
    char *ast = strchr(buffer, '*');
    if (ast) *ast = '\0';

    char *saveptr;
    char *token = strtok_r(buffer, ",", &saveptr);
    int field = 0;

    while (token && field < 15) {
        switch (field) {
            case 1:  // Time - skip for this example
                break;
            case 2:  // Latitude
                {
                    float lat = atof(token);
                    int deg = (int)(lat / 100.0f);
                    float min = lat - (deg * 100.0f);
                    data->latitude = deg + (min / 60.0f);
                }
                break;
            case 3:  // N/S
                if (token[0] == 'S')
                    data->latitude = -data->latitude;
                break;
            case 4:  // Longitude
                {
                    float lon = atof(token);
                    int deg = (int)(lon / 100.0f);
                    float min = lon - (deg * 100.0f);
                    data->longitude = deg + (min / 60.0f);
                }
                break;
            case 5:  // E/W
                if (token[0] == 'W')
                    data->longitude = -data->longitude;
                break;
            case 6:  // Fix quality
                data->fix_quality = (uint8_t)atoi(token);
                break;
            case 7:  // Satellites
                data->satellites = (uint8_t)atoi(token);
                break;
            case 8:  // HDOP
                data->hdop = atof(token);
                break;
            case 9:  // Altitude
                // data->altitude = atof(token);
                break;
            default:
                break;
        }
        token = strtok_r(NULL, ",", &saveptr);
        field++;
    }

    return (data->fix_quality > 0);
}
```

---

### 2. RMC - Recommended Minimum Specific GPS/Transit Data

**Purpose**: Position, speed, course, date (minimum navigation info)

**Transmission Interval**: Every 1 second (default)

**Format**:
```
$GPRMC,hhmmss.ss,A,ddmm.mmmm,a,dddmm.mmmm,a,x.x,x.x,ddmmyy,x.x,a[,m]*hh<CR><LF>
```

**Fields**:
```
0  $GPRMC        - Sentence type
1  hhmmss.ss     - UTC Time
2  A             - Status: A (valid/active), V (void/invalid)
3  ddmm.mmmm     - Latitude
4  a             - N/S
5  dddmm.mmmm    - Longitude
6  a             - E/W
7  x.x           - Speed over ground (knots)
8  x.x           - Track made good (degrees true, 0-359.9)
9  ddmmyy        - Date (day, month, year)
10 x.x           - Magnetic variation (degrees)
11 a             - Magnetic variation direction (E/W)
12 m             - Mode indicator (A=autonomous, D=differential, E=DR, N=not valid)
```

**Example**:
```
$GPRMC,123519.00,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*3C
```

**Parsing**:
```c
typedef struct {
    float latitude;
    float longitude;
    float speed_knots;
    float track_degrees;
    uint8_t day;
    uint8_t month;
    uint16_t year;
    bool valid;
} rmc_data_t;

bool parse_rmc(const char *sentence, rmc_data_t *data)
{
    char buffer[256];
    strncpy(buffer, sentence, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char *ast = strchr(buffer, '*');
    if (ast) *ast = '\0';

    char *saveptr;
    char *token = strtok_r(buffer, ",", &saveptr);
    int field = 0;

    while (token && field < 13) {
        switch (field) {
            case 2:  // Status
                data->valid = (token[0] == 'A');
                break;
            case 3:  // Latitude
                {
                    float lat = atof(token);
                    int deg = (int)(lat / 100.0f);
                    float min = lat - (deg * 100.0f);
                    data->latitude = deg + (min / 60.0f);
                }
                break;
            case 4:  // N/S
                if (token[0] == 'S')
                    data->latitude = -data->latitude;
                break;
            case 5:  // Longitude
                {
                    float lon = atof(token);
                    int deg = (int)(lon / 100.0f);
                    float min = lon - (deg * 100.0f);
                    data->longitude = deg + (min / 60.0f);
                }
                break;
            case 6:  // E/W
                if (token[0] == 'W')
                    data->longitude = -data->longitude;
                break;
            case 7:  // Speed
                data->speed_knots = atof(token);
                break;
            case 8:  // Track
                data->track_degrees = atof(token);
                break;
            case 9:  // Date
                if (strlen(token) == 6) {
                    data->day = ((token[0]-'0')*10) + (token[1]-'0');
                    data->month = ((token[2]-'0')*10) + (token[3]-'0');
                    data->year = ((token[4]-'0')*10) + (token[5]-'0') + 2000;
                }
                break;
            default:
                break;
        }
        token = strtok_r(NULL, ",", &saveptr);
        field++;
    }

    return data->valid;
}
```

---

### 3. GSA - GPS DOP and Active Satellites

**Purpose**: Dilution of Precision, fix type, active satellite PRNs

**Transmission Interval**: Every 1 second (default)

**Format**:
```
$GPGSA,a,x,xx,xx,xx,xx,xx,xx,xx,xx,xx,xx,xx,xx,x.x,x.x,x.x*hh<CR><LF>
```

**Fields**:
```
0  $GPGSA            - Sentence type
1  a                 - Selection mode: M (manual), A (automatic)
2  x                 - Fix type: 1 (none), 2 (2D), 3 (3D)
3-14 xx              - Satellite PRN numbers used in solution (1-32)
15 x.x              - PDOP (Position DOP)
16 x.x              - HDOP (Horizontal DOP)
17 x.x              - VDOP (Vertical DOP)
```

**Example**:
```
$GPGSA,A,3,04,05,,09,12,,,24,,,,,2.5,1.3,2.1*30
```

**Interpretation**:
- Mode A: Automatic mode
- Fix 3: 3D fix
- Satellites: 04, 05, 09, 12, 24 (5 satellites)
- PDOP: 2.5 (good precision)
- HDOP: 1.3 (excellent horizontal precision)
- VDOP: 2.1 (good vertical precision)

---

### 4. GSV - GPS Satellites in View

**Purpose**: List of all visible satellites

**Transmission Interval**: Every 1 second (3 sentences for 12 satellites)

**Format**:
```
$GPGSV,x,x,xx,xx,xx,xxx,xx,xx,xx,xxx,xx,xx,xx,xxx,xx,xx,xx,xxx,xx*hh<CR><LF>
```

**Fields**:
```
0  $GPGSV      - Sentence type
1  x           - Total number of GSV sentences (usually 1-3)
2  x           - Sentence number (1, 2, or 3)
3  xx          - Total number of satellites in view (00-12)
                 [For each satellite, up to 4 per sentence:]
4  xx          - Satellite PRN number (1-32)
5  xx          - Elevation angle (0-90 degrees)
6  xxx         - Azimuth angle (0-359 degrees)
7  xx          - Signal-to-Noise Ratio (0-99 dB) - empty if not tracked
```

**Example**:
```
$GPGSV,2,1,08,01,40,083,46,02,17,308,41,12,07,344,39,14,22,228,45*75
$GPGSV,2,2,08,17,40,208,47,28,16,134,42,19,36,149,42,24,12,273,44*78
```

**Parsing** (to get satellite list):
```c
typedef struct {
    uint8_t prn;
    uint8_t elevation;      // 0-90 degrees
    uint16_t azimuth;       // 0-359 degrees
    uint8_t snr;            // 0-99 dB or 0 if not tracking
} satellite_t;

// Note: GSV requires multiple sentences
// Collect all GSV sentences before processing
```

---

### 5. VTG - Track Made Good and Ground Speed

**Purpose**: Course and speed

**Transmission Interval**: Every 1 second (default)

**Format**:
```
$GPVTG,x.x,T,x.x,M,x.x,N,x.x,K[,a]*hh<CR><LF>
```

**Fields**:
```
0  $GPVTG      - Sentence type
1  x.x         - Track made good (true)
2  T           - True (magnetic variable = 'T')
3  x.x         - Track made good (magnetic)
4  M           - Magnetic
5  x.x         - Speed over ground (knots)
6  N           - Knots
7  x.x         - Speed over ground (km/h)
8  K           - Kilometers per hour
9  a           - Mode indicator (optional, NEO-6M may not provide)
```

**Example**:
```
$GPVTG,054.7,T,034.4,M,005.5,N,010.2,K,A*48
```

---

### 6. GLL - Geographic Position - Latitude/Longitude

**Purpose**: Position and time

**Transmission Interval**: Every 1 second (default)

**Format**:
```
$GPGLL,ddmm.mmmm,a,dddmm.mmmm,a,hhmmss.ss,A[,m]*hh<CR><LF>
```

**Fields**:
```
0  $GPGLL      - Sentence type
1  ddmm.mmmm   - Latitude
2  a           - N/S
3  dddmm.mmmm  - Longitude
4  a           - E/W
5  hhmmss.ss   - UTC Time
6  A           - Status: A (valid), V (void)
7  m           - Mode indicator (optional)
```

**Example**:
```
$GPGLL,4807.038,N,01131.000,E,123519,A,A*5C
```

---

## CHECKSUM VALIDATION

### NMEA Checksum (Simple XOR)

**Definition**: XOR of all characters between $ and *

**Calculation**:
```c
uint8_t nmea_checksum(const char *sentence, size_t length)
{
    uint8_t checksum = 0;

    // Skip the $ character
    for (size_t i = 1; i < length && sentence[i] != '*'; i++) {
        checksum ^= (uint8_t)sentence[i];
    }

    return checksum;
}
```

**Example Calculation**:
```
Sentence: $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,
XOR: G ^ P ^ G ^ G ^ A ^ , ^ 1 ^ 2 ^ 3 ^ 5 ^ 1 ^ 9 ^ ...
    = 0x47 = 71 decimal

Transmitted as: *47 (hexadecimal)
```

**Validation**:
```c
bool nmea_checksum_valid(const char *sentence)
{
    // Find asterisk
    const char *ast = strchr(sentence, '*');
    if (!ast) return false;

    // Extract checksum
    char csum_str[3];
    csum_str[0] = ast[1];
    csum_str[1] = ast[2];
    csum_str[2] = '\0';

    uint8_t provided = (uint8_t)strtol(csum_str, NULL, 16);
    uint8_t calculated = nmea_checksum(sentence, ast - sentence + 1);

    return (provided == calculated);
}
```

---

## COORDINATE FORMAT CONVERSION

### NMEA to Decimal Degrees

**NMEA Format**: DDMM.MMMM (or DDDMM.MMMM for longitude)
- DD or DDD = degrees
- MM.MMMM = minutes.decimal

**Conversion Formula**:
```
Decimal Degrees = Degrees + (Minutes / 60)
```

**Example**:
```
NMEA:    4807.038
Parsing: 48 degrees + 07.038 minutes
Formula: 48 + (07.038 / 60) = 48 + 0.117300 = 48.117300 degrees

NMEA:    01131.000
Parsing: 011 degrees + 31.000 minutes
Formula: 11 + (31.000 / 60) = 11 + 0.516667 = 11.516667 degrees
```

**Code**:
```c
float nmea_to_decimal(float nmea_value)
{
    float degrees = (float)((int)(nmea_value / 100.0f));
    float minutes = nmea_value - (degrees * 100.0f);
    return degrees + (minutes / 60.0f);
}

// Usage
float latitude = nmea_to_decimal(4807.038);    // 48.117300
float longitude = nmea_to_decimal(01131.000);  // 11.516667
```

### Decimal Degrees to NMEA Format

```c
float decimal_to_nmea(float decimal_degrees)
{
    float degrees = (float)((int)fabs(decimal_degrees));
    float minutes = (fabs(decimal_degrees) - degrees) * 60.0f;
    return degrees * 100.0f + minutes;
}

// Usage
float lat_nmea = decimal_to_nmea(48.117300);    // 4807.038
float lon_nmea = decimal_to_nmea(11.516667);    // 01131.000
```

---

## ERROR HANDLING

### Common Parsing Errors

#### 1. Empty Fields
```
$GPGGA,123519,4807.038,N,01131.000,E,,08,0.9,545.4,M,46.9,M,,*47
                                         ^^ Empty fix quality field

Handling:
- Skip empty fields in parsing
- Use default value if required
- Continue parsing next fields
```

#### 2. Missing Fields
```
$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M*47
                                                            ^^ Missing age/station

Handling:
- Detect early line termination
- Consider optional fields
- Validate required field count
```

#### 3. Malformed Data
```
$GPGGA,123519,4807.XYZ,N,...  // Non-numeric latitude

Handling:
- Check atof() return value
- Validate range (lat -90 to +90, lon -180 to +180)
- Reject invalid data
```

#### 4. Overflow in Fixed Fields
```
$GPGGA,999999,4807.038,N,...  // Invalid hour (>23)

Handling:
- Validate time values: hours 0-23, minutes 0-59, seconds 0-59
- Validate date values: day 1-31, month 1-12
```

---

## BEST PRACTICES FOR NMEA PARSING

### 1. Always Validate Checksum First
```c
if (!nmea_checksum_valid(sentence)) {
    ESP_LOGW("GPS", "Invalid checksum");
    return false;  // Reject sentence immediately
}
```

### 2. Validate Sentence Length
```c
size_t len = strlen(sentence);
if (len < 10 || len > 82) {
    ESP_LOGW("GPS", "Invalid sentence length: %zu", len);
    return false;
}
```

### 3. Use Bounded String Operations
```c
// DON'T:
sscanf(sentence, "%[^,]", buffer);  // No size limit!

// DO:
char buffer[256];
sscanf(sentence, "%255[^,]", buffer);  // Size limit specified
```

### 4. Use strtok_r for Thread-Safe Parsing
```c
// Thread-safe version
char work_buffer[256];
strncpy(work_buffer, sentence, sizeof(work_buffer) - 1);

char *saveptr;
char *token = strtok_r(work_buffer, ",", &saveptr);

while (token) {
    // Process field
    token = strtok_r(NULL, ",", &saveptr);
}
```

### 5. Validate Parsed Values
```c
// After parsing latitude
if (latitude < -90.0f || latitude > 90.0f) {
    ESP_LOGW("GPS", "Invalid latitude: %.6f", latitude);
    return false;
}

// After parsing fix quality
if (fix_quality > 8) {
    ESP_LOGW("GPS", "Invalid fix quality: %d", fix_quality);
    return false;
}
```

### 6. Handle Empty Fields
```c
// Empty fields contain no data
if (*token == '\0') {
    // Field is empty - skip or use default
    ESP_LOGD("GPS", "Empty field at position %d", field_num);
    token = strtok_r(NULL, ",", &saveptr);
    continue;
}
```

---

## TROUBLESHOOTING NMEA PARSING

| Issue | Cause | Solution |
|-------|-------|----------|
| Checksum always fails | Wrong checksum calculation | Verify XOR logic, check for off-by-one errors |
| Latitude/longitude inverted | Swapped N/S or E/W fields | Check field order (lat before lon) |
| Decimal conversion wrong | Formula error | Verify: degrees + (minutes/60) |
| Fields shifted | Field count mismatch | Count commas, verify sentence type |
| Empty fields cause errors | No empty field handling | Skip empty fields, check field length |
| Parsing stops early | Unexpected newline in data | Strip \r\n before parsing |
| Integer overflow | Values too large for int | Use uint32_t for satellite PRN, timestamp |

---

## TEST SENTENCES

### Valid Sentences (Real Output)
```
$GPGGA,092751.00,4717.113210,N,00833.915187,E,1,08,0.9,546.589,M,46.9,M,,*47
$GPRMC,092751.00,A,4717.113210,N,00833.915187,E,0.820,188.19,281018,,,A*7C
$GPGSA,A,3,01,02,03,04,05,06,07,08,09,10,11,12,1.5,1.0,1.2*37
$GPGSV,3,1,12,01,87,020,50,02,58,307,49,03,22,197,46,04,17,034,47*7B
$GPVTG,188.19,T,188.19,M,0.820,N,1.519,K*5E
$GPGLL,4717.113210,N,00833.915187,E,092751.00,A,A*5E
```

### Invalid Sentences
```
$GPGGA,092751.00,4717.113210,N,00833.915187,E,1,08,0.9,546.589,M,46.9,M,,*FF  // Wrong checksum
$GPGGA,092751.00,invalid_lat,N,00833.915187,E,1,08,0.9,546.589,M,46.9,M,,*47  // Bad data
GPGGA,092751.00,4717.113210,N,00833.915187,E,1,08,0.9,546.589,M,46.9,M,,*47  // Missing $
$GPGGA,092751.00,4717.113210,N,00833.915187,E,1,08,0.9,546.589,M,46.9,M,,  // Missing checksum
```

---

**Version**: 1.0
**Standard**: NMEA 0183 v2.30 and v3.01
**Module**: u-blox NEO-6M
**Last Updated**: November 2025
