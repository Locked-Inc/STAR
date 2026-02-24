import { COLORS } from '../theme';

/**
 * Camera Feed Panel - placeholder for WebRTC/MJPEG video stream.
 * Shows a camera icon and connection status while waiting for video data.
 */
export function CameraFeed() {
    return (
        <>
            <div
                className="panel-header"
                style={{
                    padding: '16px 20px 8px 20px',
                    fontSize: '11px',
                    fontWeight: 600,
                    color: 'var(--text-50)',
                    textTransform: 'uppercase' as const,
                    letterSpacing: '0.12em',
                    userSelect: 'none' as const,
                }}
            >
                Camera Feed
            </div>

            <div className="panel-body" style={{
                display: 'flex', flexDirection: 'column',
                alignItems: 'center', justifyContent: 'center',
                position: 'relative',
                background: 'var(--panel-bg)',
            }}>
                <div style={{ textAlign: 'center', color: COLORS.textMuted }}>
                    <svg
                        xmlns="http://www.w3.org/2000/svg"
                        width="48" height="48" viewBox="0 0 24 24"
                        fill="none" stroke="currentColor" strokeWidth="1.5"
                        strokeLinecap="round" strokeLinejoin="round"
                        aria-hidden="true"
                    style={{ margin: '0 auto 12px auto', opacity: 0.5 }}
                    >
                        <path d="M23 7l-7 5 7 5V7z"></path>
                        <rect x="1" y="5" width="15" height="14" rx="2" ry="2"></rect>
                    </svg>
                    <h3 style={{
                        color: COLORS.textPrimary, fontSize: '16px', fontWeight: 600,
                        margin: '0 0 6px 0', letterSpacing: '-0.02em',
                    }}>
                        No Camera Stream
                    </h3>
                    <p style={{ margin: 0, fontSize: '12px', opacity: 0.5 }}>
                        WebRTC / MJPEG | Waiting for connection
                    </p>
                </div>

                {/* Decorative reticle */}
                <div style={{
                    position: 'absolute', top: '50%', left: '50%',
                    transform: 'translate(-50%, -50%)',
                    width: '200px', height: '200px',
                    border: '1px solid var(--panel-inset-ring)',
                    borderRadius: '50%', pointerEvents: 'none',
                }} />
            </div>
        </>
    );
}
