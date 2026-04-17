/// <reference types="vite/client" />

interface ImportMetaEnv {
  readonly VITE_DEMO_MODE?: string;
  readonly VITE_WS_PORT?: string;
}

interface ImportMeta {
  readonly env: ImportMetaEnv;
}