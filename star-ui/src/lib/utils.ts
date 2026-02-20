import { type ClassValue, clsx } from 'clsx'
import { twMerge } from 'tailwind-merge'

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs))
}

export function formatPercent(value: number, decimals = 1): string {
  return `${value.toFixed(decimals)}%`
}

export function formatMps(value: number): string {
  return `${value.toFixed(3)} m/s`
}

export function formatRad(value: number): string {
  return `${(value * (180 / Math.PI)).toFixed(1)}deg`
}

export function formatMs(value: number): string {
  if (value < 1000) return `${value.toFixed(0)} ms`
  return `${(value / 1000).toFixed(1)} s`
}

export function formatBytes(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`
  return `${(bytes / 1024 / 1024).toFixed(2)} MB`
}

export function clamp(value: number, min: number, max: number): number {
  return Math.max(min, Math.min(max, value))
}
