/**
 * @file vite-env.d.ts
 * @brief Vite ambient environment type declarations for Project Star UI.
 * @author Project Star UI Contributors
 * @copyright Copyright (c) Project Star UI Contributors.
 * @license Licensed under the repository license.
 */
/// <reference types="vite/client" />

interface ImportMetaEnv {
  readonly VITE_DEMO_MODE?: string;
  readonly VITE_WS_PORT?: string;
}

interface ImportMeta {
  readonly env: ImportMetaEnv;
}
