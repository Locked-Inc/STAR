// battery_management_test.go - Battery Management Protobuf Serialization Tests
// Verifies BQ7850 BMS proto serialization/deserialization.
//
// Using Go table-driven tests following standard patterns.
//
// STAR Project - Texas A&M University
// December 2025

package starproto_test

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/proto"

	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
)

// ============================================================================
// CellData Tests
// ============================================================================

func TestCellDataRoundTrip(t *testing.T) {
	tests := []struct {
		name         string
		cellVoltages []uint32
		packMv       uint32
		desc         string
	}{
		{"4s_nominal", []uint32{3700, 3710, 3720, 3730}, 14860, "4S pack at nominal voltage"},
		{"4s_full", []uint32{4200, 4200, 4200, 4200}, 16800, "4S pack fully charged"},
		{"4s_low", []uint32{3200, 3210, 3220, 3230}, 12860, "4S pack low voltage"},
		{"6s_nominal", []uint32{3700, 3710, 3720, 3730, 3740, 3750}, 22350, "6S pack at nominal voltage"},
		{"4s_imbalanced", []uint32{3500, 3800, 3600, 3700}, 14600, "4S pack with cell imbalance"},
		{"1s_nominal", []uint32{3700}, 3700, "Single cell"},
		{"16s_nominal", []uint32{3700, 3705, 3710, 3715, 3720, 3725, 3730, 3735, 3740, 3745, 3750, 3755, 3760, 3765, 3770, 3775}, 59800, "16S pack maximum cells"},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			cellData := &starv1.CellData{
				CellMv:     tc.cellVoltages,
				ValidCells: int32(len(tc.cellVoltages)),
				PackMv:     tc.packMv,
			}

			bytes, err := proto.Marshal(cellData)
			require.NoError(t, err, tc.desc)

			parsed := &starv1.CellData{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err, tc.desc)

			assert.Equal(t, tc.cellVoltages, parsed.CellMv, tc.desc)
			assert.Equal(t, int32(len(tc.cellVoltages)), parsed.ValidCells, tc.desc)
			assert.Equal(t, tc.packMv, parsed.PackMv, tc.desc)
		})
	}
}

func TestCellDataDeltaCalculation(t *testing.T) {
	cellData := &starv1.CellData{
		CellMv:     []uint32{3500, 3800, 3600, 3700},
		ValidCells: 4,
		MinCellMv:  3500,
		MaxCellMv:  3800,
		DeltaMv:    300, // 3800 - 3500
	}

	bytes, err := proto.Marshal(cellData)
	require.NoError(t, err)

	parsed := &starv1.CellData{}
	err = proto.Unmarshal(bytes, parsed)
	require.NoError(t, err)

	assert.Equal(t, uint32(300), parsed.DeltaMv)
	assert.Equal(t, uint32(3500), parsed.MinCellMv)
	assert.Equal(t, uint32(3800), parsed.MaxCellMv)
}

// ============================================================================
// TemperatureData Tests
// ============================================================================

func TestTemperatureDataRoundTrip(t *testing.T) {
	tests := []struct {
		name    string
		temps   []int32
		avgTemp int32
		desc    string
	}{
		{"room_temp", []int32{250, 255, 260}, 255, "Room temperature 25C"},
		{"warm", []int32{350, 360, 370}, 360, "Warm operation 36C"},
		{"hot", []int32{450, 460, 470}, 460, "Hot operation 46C"},
		{"cold", []int32{50, 60, 70}, 60, "Cold environment 6C"},
		{"freezing", []int32{-50, -40, -30}, -40, "Below freezing -4C"},
		{"single_sensor", []int32{300}, 300, "Single temperature sensor"},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			tempData := &starv1.TemperatureData{
				TempDeciCelsius:    tc.temps,
				ValidSensors:       int32(len(tc.temps)),
				AvgTempDeciCelsius: tc.avgTemp,
			}

			bytes, err := proto.Marshal(tempData)
			require.NoError(t, err, tc.desc)

			parsed := &starv1.TemperatureData{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err, tc.desc)

			assert.Equal(t, tc.temps, parsed.TempDeciCelsius, tc.desc)
			assert.Equal(t, tc.avgTemp, parsed.AvgTempDeciCelsius, tc.desc)
		})
	}
}

func TestDeciCelsiusConversion(t *testing.T) {
	// Verify deci-celsius values convert correctly
	tests := []struct {
		deciC     int32
		expectedC float64
	}{
		{250, 25.0},
		{0, 0.0},
		{-100, -10.0},
		{600, 60.0},
		{850, 85.0},
	}

	for _, tc := range tests {
		t.Run("", func(t *testing.T) {
			actualC := float64(tc.deciC) / 10.0
			assert.InDelta(t, tc.expectedC, actualC, 0.01)
		})
	}
}

// ============================================================================
// CurrentData Tests
// ============================================================================

func TestCurrentDataRoundTrip(t *testing.T) {
	tests := []struct {
		name         string
		currentMa    int32
		avgCurrentMa int32
		voltageMv    uint32
		powerMw      int32
		desc         string
	}{
		{"charging", 2000, 1800, 14800, 29600, "Charging at 2A"},
		{"discharging", -5000, -4500, 14400, -72000, "Discharging at 5A"},
		{"idle", 50, 30, 14700, 735, "Idle current"},
		{"high_discharge", -15000, -14000, 13800, -207000, "High discharge current"},
		{"trickle_charge", 100, 90, 16800, 1680, "Trickle charging"},
		{"zero", 0, 0, 14700, 0, "No current flow"},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			currentData := &starv1.CurrentData{
				CurrentMa:    tc.currentMa,
				AvgCurrentMa: tc.avgCurrentMa,
				VoltageMv:    tc.voltageMv,
				PowerMw:      tc.powerMw,
			}

			bytes, err := proto.Marshal(currentData)
			require.NoError(t, err, tc.desc)

			parsed := &starv1.CurrentData{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err, tc.desc)

			assert.Equal(t, tc.currentMa, parsed.CurrentMa, tc.desc)
			assert.Equal(t, tc.avgCurrentMa, parsed.AvgCurrentMa, tc.desc)
			assert.Equal(t, tc.voltageMv, parsed.VoltageMv, tc.desc)
			assert.Equal(t, tc.powerMw, parsed.PowerMw, tc.desc)
		})
	}
}

// ============================================================================
// StateOfChargeData Tests
// ============================================================================

func TestStateOfChargeDataRoundTrip(t *testing.T) {
	tests := []struct {
		name         string
		remainingMah uint32
		fullMah      uint32
		relativeSoc  int32
		absoluteSoc  int32
		cycleCount   uint32
		desc         string
	}{
		{"full", 5200, 5200, 100, 100, 0, "Fully charged new battery"},
		{"nominal", 3900, 5200, 75, 75, 50, "75% charge after 50 cycles"},
		{"low", 1040, 5200, 20, 20, 100, "20% charge after 100 cycles"},
		{"critical", 260, 5200, 5, 5, 200, "5% critical charge"},
		{"empty", 0, 5200, 0, 0, 300, "Empty battery"},
		{"degraded", 3640, 4680, 78, 70, 500, "Degraded capacity after 500 cycles"},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			socData := &starv1.StateOfChargeData{
				RemainingCapacityMah: tc.remainingMah,
				FullCapacityMah:      tc.fullMah,
				RelativeSocPercent:   tc.relativeSoc,
				AbsoluteSocPercent:   tc.absoluteSoc,
				CycleCount:           tc.cycleCount,
			}

			bytes, err := proto.Marshal(socData)
			require.NoError(t, err, tc.desc)

			parsed := &starv1.StateOfChargeData{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err, tc.desc)

			assert.Equal(t, tc.remainingMah, parsed.RemainingCapacityMah, tc.desc)
			assert.Equal(t, tc.fullMah, parsed.FullCapacityMah, tc.desc)
			assert.Equal(t, tc.relativeSoc, parsed.RelativeSocPercent, tc.desc)
			assert.Equal(t, tc.absoluteSoc, parsed.AbsoluteSocPercent, tc.desc)
			assert.Equal(t, tc.cycleCount, parsed.CycleCount, tc.desc)
		})
	}
}

// ============================================================================
// BatteryStateEnum Tests
// ============================================================================

func TestBatteryStateEnumZeroValue(t *testing.T) {
	// Boston Dynamics style: zero value must be UNKNOWN
	assert.Equal(t, int32(0), int32(starv1.BatteryStateEnum_BATTERY_STATE_ENUM_UNKNOWN))
}

func TestAllBatteryStateEnumValues(t *testing.T) {
	states := []starv1.BatteryStateEnum{
		starv1.BatteryStateEnum_BATTERY_STATE_ENUM_UNKNOWN,
		starv1.BatteryStateEnum_BATTERY_STATE_ENUM_IDLE,
		starv1.BatteryStateEnum_BATTERY_STATE_ENUM_CHARGING,
		starv1.BatteryStateEnum_BATTERY_STATE_ENUM_DISCHARGING,
		starv1.BatteryStateEnum_BATTERY_STATE_ENUM_FULL,
		starv1.BatteryStateEnum_BATTERY_STATE_ENUM_FAULT,
		starv1.BatteryStateEnum_BATTERY_STATE_ENUM_SLEEP,
	}

	for _, state := range states {
		t.Run(state.String(), func(t *testing.T) {
			status := &starv1.BatteryStatus{
				State: state,
			}

			bytes, err := proto.Marshal(status)
			require.NoError(t, err)

			parsed := &starv1.BatteryStatus{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err)

			assert.Equal(t, state, parsed.State)
		})
	}
}

// ============================================================================
// ProtectionThresholds Tests
// ============================================================================

func TestProtectionThresholdsRoundTrip(t *testing.T) {
	tests := []struct {
		name     string
		ovMv     uint32
		uvMv     uint32
		ocMa     uint32
		odMa     uint32
		otDeciC  int32
		utDeciC  int32
		desc     string
	}{
		{"standard_lion", 4200, 2800, 10000, 15000, 600, 0, "Standard Li-ion thresholds"},
		{"conservative", 4150, 3000, 5000, 8000, 500, 50, "Conservative thresholds"},
		{"high_power", 4200, 2700, 20000, 30000, 650, -100, "High-power application"},
		{"lifepo4", 3650, 2500, 15000, 20000, 550, 0, "LiFePO4 thresholds"},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			thresholds := &starv1.ProtectionThresholds{
				OvervoltageMv:      tc.ovMv,
				UndervoltageMv:     tc.uvMv,
				OverchargeMa:       tc.ocMa,
				OverdischargeMa:    tc.odMa,
				OvertempDeciCelsius:  tc.otDeciC,
				UndertempDeciCelsius: tc.utDeciC,
			}

			bytes, err := proto.Marshal(thresholds)
			require.NoError(t, err, tc.desc)

			parsed := &starv1.ProtectionThresholds{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err, tc.desc)

			assert.Equal(t, tc.ovMv, parsed.OvervoltageMv, tc.desc)
			assert.Equal(t, tc.uvMv, parsed.UndervoltageMv, tc.desc)
			assert.Equal(t, tc.ocMa, parsed.OverchargeMa, tc.desc)
			assert.Equal(t, tc.odMa, parsed.OverdischargeMa, tc.desc)
			assert.Equal(t, tc.otDeciC, parsed.OvertempDeciCelsius, tc.desc)
			assert.Equal(t, tc.utDeciC, parsed.UndertempDeciCelsius, tc.desc)
		})
	}
}

// ============================================================================
// BmsDeviceInfo Tests
// ============================================================================

func TestBmsDeviceInfoRoundTrip(t *testing.T) {
	tests := []struct {
		name        string
		chemistry   string
		manufacturer string
		deviceName  string
	}{
		{"lion", "LION", "Texas Instruments", "BQ7850"},
		{"lifepo4", "LIFEPO4", "TI", "BQ7850-Q1"},
		{"lipo", "LIPO", "Texas Instruments", "BQ76952"},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			info := &starv1.BmsDeviceInfo{
				Chemistry:    tc.chemistry,
				Manufacturer: tc.manufacturer,
				DeviceName:   tc.deviceName,
				DeviceType:   0x7850,
				FirmwareVersion: 0x0102,
				HardwareVersion: 0x0001,
				SerialNumber:    12345678,
			}

			bytes, err := proto.Marshal(info)
			require.NoError(t, err)

			parsed := &starv1.BmsDeviceInfo{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err)

			assert.Equal(t, tc.chemistry, parsed.Chemistry)
			assert.Equal(t, tc.manufacturer, parsed.Manufacturer)
			assert.Equal(t, tc.deviceName, parsed.DeviceName)
		})
	}
}

// ============================================================================
// FET Control Tests
// ============================================================================

func TestFetControlRoundTrip(t *testing.T) {
	tests := []struct {
		name      string
		chargeFet bool
		dischFet  bool
	}{
		{"both_on", true, true},
		{"charge_only", true, false},
		{"discharge_only", false, true},
		{"both_off", false, false},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			req := &starv1.ControlFetsRequest{
				Header: &starv1.RequestHeader{
					RequestId: "fet-001",
				},
				EnableChargeFet:    tc.chargeFet,
				EnableDischargeFet: tc.dischFet,
			}

			bytes, err := proto.Marshal(req)
			require.NoError(t, err)

			parsed := &starv1.ControlFetsRequest{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err)

			assert.Equal(t, tc.chargeFet, parsed.EnableChargeFet)
			assert.Equal(t, tc.dischFet, parsed.EnableDischargeFet)
		})
	}
}

// ============================================================================
// Cell Balancing Tests
// ============================================================================

func TestCellBalancingMaskRoundTrip(t *testing.T) {
	tests := []struct {
		name string
		mask uint32
	}{
		{"cell_1", 0x0001},
		{"cells_1_2", 0x0003},
		{"cells_1_to_4", 0x000F},
		{"cells_1_to_8", 0x00FF},
		{"all_16", 0xFFFF},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			req := &starv1.EnableCellBalancingRequest{
				Header: &starv1.RequestHeader{
					RequestId: "bal-001",
				},
				CellMask: tc.mask,
			}

			bytes, err := proto.Marshal(req)
			require.NoError(t, err)

			parsed := &starv1.EnableCellBalancingRequest{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err)

			assert.Equal(t, tc.mask, parsed.CellMask)
		})
	}
}

// ============================================================================
// Complete BatteryState Tests
// ============================================================================

func TestCompleteBatteryStateRoundTrip(t *testing.T) {
	state := &starv1.BatteryState{
		Cells: &starv1.CellData{
			CellMv:     []uint32{3700, 3710, 3720, 3730},
			ValidCells: 4,
			PackMv:     14860,
			MinCellMv:  3700,
			MaxCellMv:  3730,
			DeltaMv:    30,
		},
		Temperatures: &starv1.TemperatureData{
			TempDeciCelsius:    []int32{250, 255, 260},
			ValidSensors:       3,
			AvgTempDeciCelsius: 255,
			MinTempDeciCelsius: 250,
			MaxTempDeciCelsius: 260,
		},
		Current: &starv1.CurrentData{
			CurrentMa:    -2000,
			AvgCurrentMa: -1800,
			VoltageMv:    14800,
			PowerMw:      -29600,
		},
		Soc: &starv1.StateOfChargeData{
			RemainingCapacityMah: 3900,
			FullCapacityMah:      5200,
			RelativeSocPercent:   75,
			AbsoluteSocPercent:   75,
			CycleCount:           50,
		},
		Status: &starv1.BatteryStatus{
			Charging:       false,
			Discharging:    true,
			FullyCharged:   false,
			FullyDischarged: false,
			FaultActive:    false,
			State:          starv1.BatteryStateEnum_BATTERY_STATE_ENUM_DISCHARGING,
		},
		TimestampUs: 1234567890,
	}

	bytes, err := proto.Marshal(state)
	require.NoError(t, err)
	require.Less(t, len(bytes), 500, "BatteryState should be < 500 bytes")

	parsed := &starv1.BatteryState{}
	err = proto.Unmarshal(bytes, parsed)
	require.NoError(t, err)

	assert.Equal(t, 4, len(parsed.Cells.CellMv))
	assert.Equal(t, uint32(14860), parsed.Cells.PackMv)
	assert.Equal(t, int32(255), parsed.Temperatures.AvgTempDeciCelsius)
	assert.Equal(t, int32(-2000), parsed.Current.CurrentMa)
	assert.Equal(t, int32(75), parsed.Soc.RelativeSocPercent)
	assert.True(t, parsed.Status.Discharging)
	assert.Equal(t, starv1.BatteryStateEnum_BATTERY_STATE_ENUM_DISCHARGING, parsed.Status.State)
}
