import React from 'react';
import { useQuery } from '@apollo/client';
import { ME_QUERY, BUILD_JOBS_QUERY, DEVICES_QUERY, FLASH_JOBS_QUERY } from '../graphql/queries';
import { Link } from 'react-router-dom';

export const Dashboard: React.FC = () => {
  const { data: userData } = useQuery(ME_QUERY);
  const { data: buildsData } = useQuery(BUILD_JOBS_QUERY, {
    variables: { limit: 5 },
  });
  const { data: devicesData } = useQuery(DEVICES_QUERY);
  const { data: flashJobsData } = useQuery(FLASH_JOBS_QUERY, {
    variables: { limit: 5 },
  });

  const pendingBuilds = buildsData?.buildJobs?.filter((b: any) => b.status === 'PENDING' || b.status === 'RUNNING') || [];
  const onlineDevices = devicesData?.devices?.filter((d: any) => d.status === 'ONLINE') || [];
  const pendingFlashJobs = flashJobsData?.flashJobs?.filter((f: any) => f.status === 'PENDING') || [];

  return (
    <div className="min-h-screen bg-gray-100">
      <nav className="bg-white shadow-sm">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="flex justify-between h-16">
            <div className="flex">
              <div className="flex-shrink-0 flex items-center">
                <h1 className="text-xl font-bold text-gray-900">FlashManager</h1>
              </div>
              <div className="hidden sm:ml-6 sm:flex sm:space-x-8">
                <Link
                  to="/dashboard"
                  className="border-primary-500 text-gray-900 inline-flex items-center px-1 pt-1 border-b-2 text-sm font-medium"
                >
                  Dashboard
                </Link>
                <Link
                  to="/builds"
                  className="border-transparent text-gray-500 hover:border-gray-300 hover:text-gray-700 inline-flex items-center px-1 pt-1 border-b-2 text-sm font-medium"
                >
                  Builds
                </Link>
                <Link
                  to="/devices"
                  className="border-transparent text-gray-500 hover:border-gray-300 hover:text-gray-700 inline-flex items-center px-1 pt-1 border-b-2 text-sm font-medium"
                >
                  Devices
                </Link>
                <Link
                  to="/flash-jobs"
                  className="border-transparent text-gray-500 hover:border-gray-300 hover:text-gray-700 inline-flex items-center px-1 pt-1 border-b-2 text-sm font-medium"
                >
                  Flash Jobs
                </Link>
              </div>
            </div>
            <div className="flex items-center">
              <span className="text-sm text-gray-700" data-testid="user-email">
                {userData?.me?.email}
              </span>
            </div>
          </div>
        </div>
      </nav>

      <div className="max-w-7xl mx-auto py-6 sm:px-6 lg:px-8">
        <div className="px-4 py-6 sm:px-0">
          <h2 className="text-2xl font-bold text-gray-900 mb-6">Dashboard</h2>

          <div className="grid grid-cols-1 gap-6 sm:grid-cols-2 lg:grid-cols-3 mb-8" data-testid="stats-grid">
            <div className="bg-white overflow-hidden shadow rounded-lg">
              <div className="p-5">
                <div className="flex items-center">
                  <div className="flex-shrink-0">
                    <div className="rounded-md bg-primary-500 p-3">
                      <svg className="h-6 w-6 text-white" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9 12l2 2 4-4m6 2a9 9 0 11-18 0 9 9 0 0118 0z" />
                      </svg>
                    </div>
                  </div>
                  <div className="ml-5 w-0 flex-1">
                    <dl>
                      <dt className="text-sm font-medium text-gray-500 truncate">Active Builds</dt>
                      <dd className="text-lg font-semibold text-gray-900" data-testid="active-builds-count">
                        {pendingBuilds.length}
                      </dd>
                    </dl>
                  </div>
                </div>
              </div>
            </div>

            <div className="bg-white overflow-hidden shadow rounded-lg">
              <div className="p-5">
                <div className="flex items-center">
                  <div className="flex-shrink-0">
                    <div className="rounded-md bg-green-500 p-3">
                      <svg className="h-6 w-6 text-white" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9 3v2m6-2v2M9 19v2m6-2v2M5 9H3m2 6H3m18-6h-2m2 6h-2M7 19h10a2 2 0 002-2V7a2 2 0 00-2-2H7a2 2 0 00-2 2v10a2 2 0 002 2zM9 9h6v6H9V9z" />
                      </svg>
                    </div>
                  </div>
                  <div className="ml-5 w-0 flex-1">
                    <dl>
                      <dt className="text-sm font-medium text-gray-500 truncate">Online Devices</dt>
                      <dd className="text-lg font-semibold text-gray-900" data-testid="online-devices-count">
                        {onlineDevices.length}
                      </dd>
                    </dl>
                  </div>
                </div>
              </div>
            </div>

            <div className="bg-white overflow-hidden shadow rounded-lg">
              <div className="p-5">
                <div className="flex items-center">
                  <div className="flex-shrink-0">
                    <div className="rounded-md bg-yellow-500 p-3">
                      <svg className="h-6 w-6 text-white" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M13 10V3L4 14h7v7l9-11h-7z" />
                      </svg>
                    </div>
                  </div>
                  <div className="ml-5 w-0 flex-1">
                    <dl>
                      <dt className="text-sm font-medium text-gray-500 truncate">Pending Flash Jobs</dt>
                      <dd className="text-lg font-semibold text-gray-900" data-testid="pending-flash-count">
                        {pendingFlashJobs.length}
                      </dd>
                    </dl>
                  </div>
                </div>
              </div>
            </div>
          </div>

          <div className="grid grid-cols-1 gap-6 lg:grid-cols-2">
            <div className="bg-white shadow rounded-lg p-6">
              <h3 className="text-lg font-medium text-gray-900 mb-4">Recent Builds</h3>
              {buildsData?.buildJobs?.length === 0 ? (
                <p className="text-gray-500 text-sm">No recent builds</p>
              ) : (
                <div className="space-y-3" data-testid="recent-builds">
                  {buildsData?.buildJobs?.slice(0, 5).map((build: any) => (
                    <div key={build.id} className="flex items-center justify-between py-2 border-b border-gray-200 last:border-0">
                      <div className="flex-1 min-w-0">
                        <p className="text-sm font-medium text-gray-900 truncate">{build.component}</p>
                        <p className="text-sm text-gray-500">{build.branch}</p>
                      </div>
                      <div className="ml-4 flex-shrink-0">
                        <span className={`px-2 inline-flex text-xs leading-5 font-semibold rounded-full ${
                          build.status === 'COMPLETED' ? 'bg-green-100 text-green-800' :
                          build.status === 'RUNNING' ? 'bg-blue-100 text-blue-800' :
                          build.status === 'FAILED' ? 'bg-red-100 text-red-800' :
                          'bg-gray-100 text-gray-800'
                        }`}>
                          {build.status}
                        </span>
                      </div>
                    </div>
                  ))}
                </div>
              )}
            </div>

            <div className="bg-white shadow rounded-lg p-6">
              <h3 className="text-lg font-medium text-gray-900 mb-4">Devices</h3>
              {devicesData?.devices?.length === 0 ? (
                <p className="text-gray-500 text-sm">No devices registered</p>
              ) : (
                <div className="space-y-3" data-testid="devices-list">
                  {devicesData?.devices?.slice(0, 5).map((device: any) => (
                    <div key={device.id} className="flex items-center justify-between py-2 border-b border-gray-200 last:border-0">
                      <div className="flex-1 min-w-0">
                        <p className="text-sm font-medium text-gray-900 truncate">{device.name}</p>
                        <p className="text-sm text-gray-500">{device.platform}</p>
                      </div>
                      <div className="ml-4 flex-shrink-0">
                        <span className={`px-2 inline-flex text-xs leading-5 font-semibold rounded-full ${
                          device.status === 'ONLINE' ? 'bg-green-100 text-green-800' :
                          device.status === 'BUSY' ? 'bg-yellow-100 text-yellow-800' :
                          'bg-gray-100 text-gray-800'
                        }`}>
                          {device.status}
                        </span>
                      </div>
                    </div>
                  ))}
                </div>
              )}
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};
