import { StrictMode, Component } from 'react'
import type { ErrorInfo, ReactNode } from 'react'
import { createRoot } from 'react-dom/client'
import './index.css'
import App from './App.tsx'

interface EBState { error: Error | null }

class ErrorBoundary extends Component<{ children: ReactNode }, EBState> {
  state: EBState = { error: null };
  static getDerivedStateFromError(error: Error): EBState { return { error }; }
  componentDidCatch(error: Error, info: ErrorInfo): void { console.error(error, info); }
  render() {
    if (this.state.error) {
      return (
        <pre style={{
          color: '#ef4444', background: '#0f1117', padding: '2rem',
          margin: 0, whiteSpace: 'pre-wrap', fontFamily: 'monospace', fontSize: '13px',
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
