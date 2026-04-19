/**
 * @file useAppRoute.ts
 * @brief React hook for application route navigation and synchronization
 * @copyright Copyright (c) 2026 Locked Inc.
 * @license MIT License
 */

import { useCallback, useEffect, useState } from 'react';
import type { AppRoute } from '../types/dashboard';

function normalizeRoute(pathname: string): AppRoute {
  return pathname === '/ros' ? '/ros' : '/';
}

export function useAppRoute(): { route: AppRoute; navigate: (route: AppRoute) => void } {
  const [route, setRoute] = useState<AppRoute>(() => normalizeRoute(window.location.pathname));

  useEffect(() => {
    const canonical = normalizeRoute(window.location.pathname);
    if (window.location.pathname !== canonical) {
      window.history.replaceState({}, '', canonical);
    }

    const handlePopState = () => setRoute(normalizeRoute(window.location.pathname));
    window.addEventListener('popstate', handlePopState);
    return () => window.removeEventListener('popstate', handlePopState);
  }, []);

  const navigate = useCallback((nextRoute: AppRoute): void => {
    if (window.location.pathname !== nextRoute) {
      window.history.pushState({}, '', nextRoute);
    }
    setRoute(nextRoute);
  }, []);

  return { route, navigate };
}
