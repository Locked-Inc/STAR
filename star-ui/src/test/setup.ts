import '@testing-library/jest-dom';
import { vi } from 'vitest';

// Minimal stub so components that call ctx.<method> don't crash under jsdom.
HTMLCanvasElement.prototype.getContext = vi.fn(() => ({
  canvas: document.createElement('canvas'),
  clearRect: vi.fn(),
  fillRect: vi.fn(),
  beginPath: vi.fn(),
  moveTo: vi.fn(),
  lineTo: vi.fn(),
  arc: vi.fn(),
  stroke: vi.fn(),
  fill: vi.fn(),
  save: vi.fn(),
  restore: vi.fn(),
})) as unknown as typeof HTMLCanvasElement.prototype.getContext;