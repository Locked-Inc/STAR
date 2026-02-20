import React, { useEffect, useRef, useState } from 'react'
import { GatewayContext } from '@/hooks/useGateway'
import type { RobotSnapshot } from '@/services/GatewayClient'
import { MockGatewayClient } from '@/services/MockGatewayClient'

const EMPTY_SNAPSHOT: RobotSnapshot = {
  telemetry: null,
  systemStatus: null,
  motors: [],
  encoders: [],
  lastUpdated: null,
}

export function GatewayProvider({ children }: { children: React.ReactNode }) {
  const clientRef = useRef(new MockGatewayClient())
  const [snapshot, setSnapshot] = useState<RobotSnapshot>(EMPTY_SNAPSHOT)
  const [connected, setConnected] = useState(false)

  useEffect(() => {
    const client = clientRef.current

    const unsubscribe = client.subscribe((s) => {
      setSnapshot(s)
      setConnected(true)
    })

    client.connect()

    return () => {
      unsubscribe()
      client.disconnect()
      setConnected(false)
    }
  }, [])

  return (
    <GatewayContext.Provider value={{ client: clientRef.current, snapshot, connected }}>
      {children}
    </GatewayContext.Provider>
  )
}
