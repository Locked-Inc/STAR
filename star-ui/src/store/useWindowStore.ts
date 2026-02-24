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

export interface ViewDef {
    label: string;
    icon: string;
    desc: string;
    panels: string[];
    layouts: Layouts;
    scrollable?: boolean; // only Full view allows scrolling
}

export type ViewName = 'OVERVIEW' | 'TELEOP' | 'NAVIGATION' | 'DEBUG' | 'CONFIG' | 'FULL';

// --- View definitions - 6 purpose-specific views ---
// Each shows 6-8 panels that fill viewport (except FULL which scrolls)

export const VIEWS: Record<ViewName, ViewDef> = {
    OVERVIEW: {
        label: 'Overview',
        icon: 'home',
        desc: 'Key vitals at a glance',
        panels: ['movement', 'camera', 'motors', 'battery', 'health', 'alerts', 'imu', 'odometry'],
        layouts: {
            lg: [
                { i: 'movement', x: 0, y: 0, w: 6, h: 9, minW: 4, minH: 5 },
                { i: 'camera', x: 6, y: 0, w: 6, h: 9, minW: 4, minH: 5 },
                { i: 'odometry', x: 12, y: 0, w: 6, h: 9, minW: 4, minH: 5 },
                { i: 'battery', x: 18, y: 0, w: 6, h: 5, minW: 4, minH: 3 },
                { i: 'health', x: 18, y: 5, w: 6, h: 4, minW: 4, minH: 3 },
                { i: 'motors', x: 0, y: 9, w: 6, h: 9, minW: 4, minH: 5 },
                { i: 'imu', x: 6, y: 9, w: 6, h: 9, minW: 4, minH: 5 },
                { i: 'alerts', x: 12, y: 9, w: 12, h: 9, minW: 6, minH: 5 },
            ],
        },
    },
    TELEOP: {
        label: 'Teleop',
        icon: 'ctrl',
        desc: 'Manual driving controls',
        panels: ['movement', 'camera', 'motors', 'battery', 'imu', 'alerts'],
        layouts: {
            lg: [
                { i: 'movement', x: 0, y: 0, w: 8, h: 12, minW: 5, minH: 6 },
                { i: 'camera', x: 8, y: 0, w: 10, h: 12, minW: 6, minH: 6 },
                { i: 'battery', x: 18, y: 0, w: 6, h: 6, minW: 4, minH: 4 },
                { i: 'imu', x: 18, y: 6, w: 6, h: 6, minW: 4, minH: 4 },
                { i: 'motors', x: 0, y: 12, w: 12, h: 6, minW: 6, minH: 4 },
                { i: 'alerts', x: 12, y: 12, w: 12, h: 6, minW: 6, minH: 4 },
            ],
        },
    },
    NAVIGATION: {
        label: 'Navigation',
        icon: 'nav',
        desc: 'Autonomous navigation & mapping',
        panels: ['camera', 'lidar', 'odometry', 'gps', 'nav2', 'imu'],
        layouts: {
            lg: [
                { i: 'camera', x: 0, y: 0, w: 12, h: 12, minW: 6, minH: 6 },
                { i: 'lidar', x: 12, y: 0, w: 12, h: 12, minW: 6, minH: 6 },
                { i: 'odometry', x: 0, y: 12, w: 6, h: 6, minW: 4, minH: 4 },
                { i: 'gps', x: 6, y: 12, w: 6, h: 6, minW: 4, minH: 4 },
                { i: 'nav2', x: 12, y: 12, w: 6, h: 6, minW: 4, minH: 4 },
                { i: 'imu', x: 18, y: 12, w: 6, h: 6, minW: 4, minH: 4 },
            ],
        },
    },
    DEBUG: {
        label: 'Debug',
        icon: 'dbg',
        desc: 'Transport & system diagnostics',
        panels: ['packet', 'transport', 'diaglog', 'timeseries', 'health', 'alerts'],
        layouts: {
            lg: [
                { i: 'packet', x: 0, y: 0, w: 12, h: 9, minW: 6, minH: 5 },
                { i: 'transport', x: 12, y: 0, w: 6, h: 9, minW: 4, minH: 5 },
                { i: 'health', x: 18, y: 0, w: 6, h: 9, minW: 4, minH: 5 },
                { i: 'diaglog', x: 0, y: 9, w: 12, h: 9, minW: 6, minH: 5 },
                { i: 'timeseries', x: 12, y: 9, w: 6, h: 9, minW: 4, minH: 5 },
                { i: 'alerts', x: 18, y: 9, w: 6, h: 9, minW: 4, minH: 5 },
            ],
        },
    },
    CONFIG: {
        label: 'Config',
        icon: 'cfg',
        desc: 'PID tuning & firmware',
        panels: ['pid', 'firmware', 'health', 'motors', 'timeseries', 'battery'],
        layouts: {
            lg: [
                { i: 'pid', x: 0, y: 0, w: 8, h: 9, minW: 5, minH: 5 },
                { i: 'firmware', x: 8, y: 0, w: 8, h: 9, minW: 5, minH: 5 },
                { i: 'health', x: 16, y: 0, w: 8, h: 9, minW: 4, minH: 5 },
                { i: 'motors', x: 0, y: 9, w: 8, h: 9, minW: 5, minH: 5 },
                { i: 'timeseries', x: 8, y: 9, w: 8, h: 9, minW: 5, minH: 5 },
                { i: 'battery', x: 16, y: 9, w: 8, h: 9, minW: 4, minH: 5 },
            ],
        },
    },
    FULL: {
        label: 'Full',
        icon: 'full',
        desc: 'All panels (scrollable)',
        scrollable: true,
        panels: [
            'movement', 'camera', 'odometry', 'battery', 'health',
            'motors', 'lidar', 'imu', 'timeseries',
            'gps', 'packet', 'transport', 'alerts',
            'pid', 'firmware', 'nav2', 'diaglog',
        ],
        layouts: {
            lg: [
                { i: 'movement', x: 0, y: 0, w: 6, h: 6, minW: 4, minH: 4 },
                { i: 'camera', x: 6, y: 0, w: 6, h: 6, minW: 4, minH: 4 },
                { i: 'odometry', x: 12, y: 0, w: 6, h: 6, minW: 4, minH: 4 },
                { i: 'battery', x: 18, y: 0, w: 6, h: 3, minW: 3, minH: 2 },
                { i: 'health', x: 18, y: 3, w: 6, h: 3, minW: 3, minH: 2 },
                { i: 'motors', x: 0, y: 6, w: 6, h: 6, minW: 4, minH: 4 },
                { i: 'lidar', x: 6, y: 6, w: 6, h: 6, minW: 4, minH: 4 },
                { i: 'imu', x: 12, y: 6, w: 6, h: 6, minW: 4, minH: 4 },
                { i: 'timeseries', x: 18, y: 6, w: 6, h: 6, minW: 4, minH: 4 },
                { i: 'gps', x: 0, y: 12, w: 6, h: 6, minW: 4, minH: 4 },
                { i: 'packet', x: 6, y: 12, w: 6, h: 6, minW: 4, minH: 3 },
                { i: 'transport', x: 12, y: 12, w: 6, h: 6, minW: 4, minH: 3 },
                { i: 'alerts', x: 18, y: 12, w: 6, h: 6, minW: 4, minH: 3 },
                { i: 'pid', x: 0, y: 18, w: 6, h: 6, minW: 4, minH: 4 },
                { i: 'firmware', x: 6, y: 18, w: 6, h: 6, minW: 4, minH: 4 },
                { i: 'nav2', x: 12, y: 18, w: 6, h: 6, minW: 4, minH: 4 },
                { i: 'diaglog', x: 18, y: 18, w: 6, h: 6, minW: 4, minH: 4 },
            ],
        },
    },
};

// --- Store ---

interface WindowState {
    activeView: ViewName;
    // Per-view layout overrides (when user drags/resizes panels)
    viewLayouts: Partial<Record<ViewName, Layouts>>;
    maximizedPanel: string | null;

    setView: (view: ViewName) => void;
    updateLayouts: (currentLayout: Layout[], allLayouts: Layouts) => void;
    toggleMaximize: (panelId: string) => void;
    resetLayout: () => void;
}

export const useWindowStore = create<WindowState>()(
    persist(
        (set, get) => ({
            activeView: 'OVERVIEW' as ViewName,
            viewLayouts: {},
            maximizedPanel: null,

            setView: (view) => set({
                activeView: view,
                maximizedPanel: null, // reset maximize on view switch
            }),

            updateLayouts: (_currentLayout, allLayouts) => {
                const { activeView } = get();
                set((state) => ({
                    viewLayouts: {
                        ...state.viewLayouts,
                        [activeView]: allLayouts,
                    },
                }));
            },

            toggleMaximize: (panelId) => set((state) => ({
                maximizedPanel: state.maximizedPanel === panelId ? null : panelId,
            })),

            resetLayout: () => set((state) => ({
                viewLayouts: {
                    ...state.viewLayouts,
                    [state.activeView]: undefined, // clear overrides for current view
                },
                maximizedPanel: null,
            })),
        }),
        {
            name: 'robot-dashboard-v6', // v6 for view-based layout
            storage: createJSONStorage(() => localStorage),
        }
    )
);
