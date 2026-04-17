/**
 * @file usePacketFeed.ts
 * @brief React hook for polling and managing recent packet records from the gateway
 * @copyright Copyright (c) 2026 Locked Inc.
 * @license MIT License
 */

import { useEffect, useState } from 'react';
import { packetPollIntervalMs } from '../lib/dashboard';
import { getRecentPackets, type PacketRecord } from '../services/GatewayService';

interface PacketFeedState {
  packets: PacketRecord[];
  sampledAtMs: number;
}

export function usePacketFeed(): PacketFeedState {
  const [feed, setFeed] = useState<PacketFeedState>(() => ({
    packets: getRecentPackets(),
    sampledAtMs: Date.now(),
  }));

  useEffect(() => {
    const interval = window.setInterval(() => {
      setFeed({
        packets: getRecentPackets(),
        sampledAtMs: Date.now(),
      });
    }, packetPollIntervalMs);

    return () => window.clearInterval(interval);
  }, []);

  return feed;
}
