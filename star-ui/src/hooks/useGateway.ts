import { createContext, useContext } from 'react'
import type { GatewayClient, RobotSnapshot } from '@/services/GatewayClient'

export interface GatewayContextValue {
  client: GatewayClient
  snapshot: RobotSnapshot
  connected: boolean
}

export const GatewayContext = createContext<GatewayContextValue | null>(null)

export function useGateway(): GatewayContextValue {
  const ctx = useContext(GatewayContext)
  if (!ctx) throw new Error('useGateway must be used within GatewayProvider')
  return ctx
}
