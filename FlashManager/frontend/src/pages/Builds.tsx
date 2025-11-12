import React, { useState } from 'react';
import { useQuery, useMutation } from '@apollo/client';
import { BUILD_JOBS_QUERY, TRIGGER_BUILD_MUTATION } from '../graphql/queries';
import { Component } from '../types';
import { Link } from 'react-router-dom';

export const Builds: React.FC = () => {
  const [showTrigger, setShowTrigger] = useState(false);
  const [component, setComponent] = useState<Component>('ESP32_FIRMWARE');
  const [branch, setBranch] = useState('main');

  const { data, loading, refetch } = useQuery(BUILD_JOBS_QUERY, {
    variables: { limit: 50 },
  });

  const [triggerBuild, { loading: triggering }] = useMutation(TRIGGER_BUILD_MUTATION, {
    onCompleted: () => {
      setShowTrigger(false);
      refetch();
    },
  });

  const handleTrigger = async () => {
    await triggerBuild({
      variables: {
        input: { component, branch },
      },
    });
  };

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
                  className="border-transparent text-gray-500 hover:border-gray-300 hover:text-gray-700 inline-flex items-center px-1 pt-1 border-b-2 text-sm font-medium"
                >
                  Dashboard
                </Link>
                <Link
                  to="/builds"
                  className="border-primary-500 text-gray-900 inline-flex items-center px-1 pt-1 border-b-2 text-sm font-medium"
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
          </div>
        </div>
      </nav>

      <div className="max-w-7xl mx-auto py-6 sm:px-6 lg:px-8">
        <div className="px-4 py-6 sm:px-0">
          <div className="flex justify-between items-center mb-6">
            <h2 className="text-2xl font-bold text-gray-900">Build Jobs</h2>
            <button
              onClick={() => setShowTrigger(!showTrigger)}
              className="btn btn-primary"
              data-testid="trigger-build-button"
            >
              Trigger Build
            </button>
          </div>

          {showTrigger && (
            <div className="bg-white shadow rounded-lg p-6 mb-6" data-testid="trigger-build-form">
              <h3 className="text-lg font-medium mb-4">Trigger New Build</h3>
              <div className="space-y-4">
                <div>
                  <label className="block text-sm font-medium text-gray-700">Component</label>
                  <select
                    value={component}
                    onChange={(e) => setComponent(e.target.value as Component)}
                    className="mt-1 block w-full rounded-md border-gray-300 shadow-sm focus:border-primary-500 focus:ring-primary-500"
                    data-testid="component-select"
                  >
                    <option value="ESP32_FIRMWARE">ESP32 Firmware</option>
                    <option value="PI5_OS">Raspberry Pi 5 OS</option>
                    <option value="ANDROID_APP">Android App</option>
                    <option value="ROBOT_GATEWAY">Robot Gateway</option>
                    <option value="SERVER_BACKEND">Server Backend</option>
                  </select>
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-700">Branch</label>
                  <input
                    type="text"
                    value={branch}
                    onChange={(e) => setBranch(e.target.value)}
                    className="mt-1 block w-full rounded-md border-gray-300 shadow-sm focus:border-primary-500 focus:ring-primary-500"
                    placeholder="main"
                    data-testid="branch-input"
                  />
                </div>
                <div className="flex space-x-2">
                  <button
                    onClick={handleTrigger}
                    disabled={triggering}
                    className="btn btn-primary"
                    data-testid="submit-trigger"
                  >
                    {triggering ? 'Triggering...' : 'Start Build'}
                  </button>
                  <button
                    onClick={() => setShowTrigger(false)}
                    className="btn btn-secondary"
                  >
                    Cancel
                  </button>
                </div>
              </div>
            </div>
          )}

          <div className="bg-white shadow rounded-lg overflow-hidden">
            {loading ? (
              <div className="p-6 text-center text-gray-500">Loading...</div>
            ) : data?.buildJobs?.length === 0 ? (
              <div className="p-6 text-center text-gray-500">No builds yet</div>
            ) : (
              <table className="min-w-full divide-y divide-gray-200" data-testid="builds-table">
                <thead className="bg-gray-50">
                  <tr>
                    <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                      Component
                    </th>
                    <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                      Branch
                    </th>
                    <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                      Status
                    </th>
                    <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                      Created
                    </th>
                    <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                      Duration
                    </th>
                  </tr>
                </thead>
                <tbody className="bg-white divide-y divide-gray-200">
                  {data?.buildJobs?.map((build: any) => (
                    <tr key={build.id}>
                      <td className="px-6 py-4 whitespace-nowrap text-sm font-medium text-gray-900">
                        {build.component.replace(/_/g, ' ')}
                      </td>
                      <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">
                        {build.branch}
                      </td>
                      <td className="px-6 py-4 whitespace-nowrap">
                        <span className={`px-2 inline-flex text-xs leading-5 font-semibold rounded-full ${
                          build.status === 'COMPLETED' ? 'bg-green-100 text-green-800' :
                          build.status === 'RUNNING' ? 'bg-blue-100 text-blue-800' :
                          build.status === 'FAILED' ? 'bg-red-100 text-red-800' :
                          'bg-gray-100 text-gray-800'
                        }`}>
                          {build.status}
                        </span>
                      </td>
                      <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">
                        {new Date(build.createdAt).toLocaleString()}
                      </td>
                      <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">
                        {build.buildDuration ? `${(build.buildDuration / 1000).toFixed(0)}s` : '-'}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            )}
          </div>
        </div>
      </div>
    </div>
  );
};
