import { useEffect, useState } from 'react';
import type { AppRoute } from '../types/dashboard';

function normalizeRoute(pathname: string): AppRoute {
  return pathname === '/ros' ? '/ros' : '/';
}

export function useAppRoute(): { route: AppRoute; navigate: (route: AppRoute) => void } {
  const [route, setRoute] = useState<AppRoute>(() => normalizeRoute(window.location.pathname));

  useEffect(() => {
    const handlePopState = () => setRoute(normalizeRoute(window.location.pathname));
    window.addEventListener('popstate', handlePopState);
    return () => window.removeEventListener('popstate', handlePopState);
  }, []);

  function navigate(nextRoute: AppRoute): void {
    if (window.location.pathname !== nextRoute) {
      window.history.pushState({}, '', nextRoute);
    }
    setRoute(nextRoute);
  }

  return { route, navigate };
}
