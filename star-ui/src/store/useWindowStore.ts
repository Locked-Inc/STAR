import { create } from 'zustand';
import { persist, createJSONStorage } from 'zustand/middleware';
export interface Layout {
    i: string;
    x: number;
    y: number;
    w: number;
    h: number;
    minW?: number;
    minH?: number;
    maxW?: number;
    maxH?: number;
    static?: boolean;
}
export type Layouts = { [P: string]: Layout[] };

// 24-column base grid defaults — 10 panels
export const PRESETS = {
    DEFAULT: {
        lg: [
            // Row 1: Movement | Odometry | Camera | Battery
            { i: 'movement', x: 0, y: 0, w: 6, h: 9, minW: 4, minH: 5 },
            { i: 'odometry', x: 6, y: 0, w: 6, h: 9, minW: 4, minH: 5 },
            { i: 'camera', x: 12, y: 0, w: 6, h: 9, minW: 4, minH: 5 },
            { i: 'battery', x: 18, y: 0, w: 6, h: 5, minW: 4, minH: 3 },
            { i: 'health', x: 18, y: 5, w: 6, h: 4, minW: 4, minH: 3 },
            // Row 2: Motors | LiDAR | IMU | Packet | Alerts
            { i: 'motors', x: 0, y: 9, w: 6, h: 9, minW: 4, minH: 4 },
            { i: 'lidar', x: 6, y: 9, w: 6, h: 9, minW: 4, minH: 5 },
            { i: 'imu', x: 12, y: 9, w: 6, h: 9, minW: 4, minH: 5 },
            { i: 'packet', x: 18, y: 9, w: 6, h: 5, minW: 4, minH: 3 },
            { i: 'alerts', x: 18, y: 14, w: 6, h: 4, minW: 4, minH: 3 },
        ]
    },
    FOCUS: {
        lg: [
            // Large center panels for navigation/control
            { i: 'camera', x: 0, y: 0, w: 12, h: 12, minW: 6, minH: 6 },
            { i: 'odometry', x: 0, y: 12, w: 6, h: 6, minW: 4, minH: 4 },
            { i: 'lidar', x: 6, y: 12, w: 6, h: 6, minW: 4, minH: 4 },
            // Right sidebar
            { i: 'movement', x: 12, y: 0, w: 6, h: 6, minW: 4, minH: 4 },
            { i: 'imu', x: 12, y: 6, w: 6, h: 6, minW: 4, minH: 4 },
            { i: 'motors', x: 12, y: 12, w: 6, h: 6, minW: 4, minH: 4 },
            { i: 'battery', x: 18, y: 0, w: 6, h: 5, minW: 4, minH: 3 },
            { i: 'health', x: 18, y: 5, w: 6, h: 5, minW: 4, minH: 3 },
            { i: 'packet', x: 18, y: 10, w: 6, h: 4, minW: 4, minH: 3 },
            { i: 'alerts', x: 18, y: 14, w: 6, h: 4, minW: 4, minH: 3 },
        ]
    },
    DATA: {
        lg: [
            // Top row: 4 equal data panels
            { i: 'motors', x: 0, y: 0, w: 6, h: 9, minW: 4, minH: 4 },
            { i: 'imu', x: 6, y: 0, w: 6, h: 9, minW: 4, minH: 4 },
            { i: 'odometry', x: 12, y: 0, w: 6, h: 9, minW: 4, minH: 4 },
            { i: 'health', x: 18, y: 0, w: 6, h: 9, minW: 4, minH: 4 },
            // Bottom row: operational panels
            { i: 'movement', x: 0, y: 9, w: 5, h: 9, minW: 4, minH: 5 },
            { i: 'lidar', x: 5, y: 9, w: 5, h: 9, minW: 4, minH: 5 },
            { i: 'camera', x: 10, y: 9, w: 5, h: 9, minW: 4, minH: 5 },
            { i: 'battery', x: 15, y: 9, w: 3, h: 9, minW: 3, minH: 4 },
            { i: 'packet', x: 18, y: 9, w: 6, h: 5, minW: 4, minH: 3 },
            { i: 'alerts', x: 18, y: 14, w: 6, h: 4, minW: 4, minH: 3 },
        ]
    }
};

interface WindowState {
    layouts: Layouts;
    updateLayouts: (currentLayout: Layout[], allLayouts: Layouts) => void;
    applyPreset: (presetName: 'DEFAULT' | 'FOCUS' | 'DATA') => void;
    resetLayout: () => void;
}

export const useWindowStore = create<WindowState>()(
    persist(
        (set) => ({
            layouts: PRESETS.DEFAULT,

            updateLayouts: (_currentLayout, allLayouts) =>
                set(() => ({
                    layouts: allLayouts,
                })),

            applyPreset: (presetName) =>
                set(() => ({
                    layouts: PRESETS[presetName]
                })),

            resetLayout: () =>
                set(() => ({
                    layouts: PRESETS.DEFAULT,
                })),
        }),
        {
            name: 'robot-dashboard-layout-v3', // v3 for 10-panel tiling WM
            storage: createJSONStorage(() => localStorage),
        }
    )
);
