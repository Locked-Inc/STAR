import { StrictMode, Component } from 'react'
import type { ErrorInfo, ReactNode } from 'react'
import { createRoot } from 'react-dom/client'
import './index.css'
import App from './App.tsx'

const ERROR_DISPLAY_PADDING = '2rem' as const;
const ERROR_DISPLAY_FONT_SIZE = '13px' as const;
const ERROR_DISPLAY_TEXT_COLOR = '#ef4444' as const;
const ERROR_DISPLAY_BG_COLOR = '#0f1117' as const;
const ERROR_DISPLAY_MARGIN = 0 as const;
const ERROR_DISPLAY_WHITE_SPACE = 'pre-wrap' as const;
const ERROR_DISPLAY_FONT_FAMILY = 'monospace' as const;

interface ErrorBoundaryState { error: Error | null }
interface ErrorBoundaryProps { children: ReactNode; }

class ErrorBoundary extends Component<ErrorBoundaryProps, ErrorBoundaryState> {
  state: ErrorBoundaryState = { error: null };
  static getDerivedStateFromError(error: Error): ErrorBoundaryState { return { error }; }
  componentDidCatch(error: Error, info: ErrorInfo): void { console.error(error, info); }
  render() {
    if (this.state.error) {
      return (
        <pre style={{
          color: ERROR_DISPLAY_TEXT_COLOR, background: ERROR_DISPLAY_BG_COLOR, padding: ERROR_DISPLAY_PADDING,
          margin: ERROR_DISPLAY_MARGIN, whiteSpace: ERROR_DISPLAY_WHITE_SPACE, fontFamily: ERROR_DISPLAY_FONT_FAMILY, fontSize: ERROR_DISPLAY_FONT_SIZE,
        }}>
          {String(this.state.error)}{'\n'}{this.state.error.stack}
        </pre>
      );
    }
    return this.props.children;
  }
}

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <ErrorBoundary>
      <App />
    </ErrorBoundary>
  </StrictMode>,
)
