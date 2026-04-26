// STAR UI screenshot capture
//
// Drives a headless Chromium-based browser (Microsoft Edge on macOS, falls back
// to Chromium / Google Chrome) against a running Vite dev server, switches
// through every view defined in the window store, and writes a PNG per view
// into docs/ui-screenshots/.
//
// Usage:
//   node scripts/ui/capture-screenshots.js [--url http://localhost:5173] \
//                                          [--out docs/ui-screenshots] \
//                                          [--browser /path/to/Edge]
//
// ASCII-only. No emoji. No external CDN access.

const fs = require('fs');
const path = require('path');
const puppeteer = require('puppeteer-core');

const DEFAULT_URL = 'http://localhost:5173/';
const DEFAULT_OUT = path.resolve(__dirname, '..', '..', 'docs', 'ui-screenshots');
const VIEWPORT_W = 1920;
const VIEWPORT_H = 1080;

// View names declared in star-ui/src/store/useWindowStore.ts.
// Keep this list in sync with that file.
const VIEWS = [
  { id: 'OVERVIEW',   file: 'overview.png' },
  { id: 'TELEOP',     file: 'teleop.png' },
  { id: 'NAVIGATION', file: 'navigation.png' },
  { id: 'DEBUG',      file: 'debug.png' },
  { id: 'CONFIG',     file: 'config.png' },
  { id: 'FULL',       file: 'full.png' },
];

// Extra interactive states to capture beyond the default view layouts.
// Each entry runs against the OVERVIEW view, after the page renders, and
// produces an additional screenshot.
const EXTRA_STATES = [
  {
    file: 'layouts-dropdown.png',
    description: 'Top-bar Layouts dropdown open showing view picker',
    interact: async (page) => {
      // The Layouts toggle button has visible text "LAYOUTS" rendered in
      // upper-case via CSS; the underlying string is "Layouts".
      const clicked = await page.evaluate(() => {
        const buttons = Array.from(document.querySelectorAll('button'));
        const btn = buttons.find(b => /layouts/i.test(b.textContent || ''));
        if (!btn) return false;
        btn.click();
        return true;
      });
      if (!clicked) throw new Error('Layouts button not found');
    },
  },
  {
    file: 'estop-active.png',
    description: 'Emergency stop activated state (red pulse + resume button)',
    interact: async (page) => {
      const clicked = await page.evaluate(() => {
        const buttons = Array.from(document.querySelectorAll('button'));
        const btn = buttons.find(b => /e-?stop|emergency stop/i.test(b.getAttribute('aria-label') || '')
                                      || /e-?stop/i.test(b.textContent || ''));
        if (!btn) return false;
        btn.click();
        return true;
      });
      if (!clicked) throw new Error('E-Stop button not found');
    },
  },
];

const CANDIDATE_BROWSERS = [
  '/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge',
  '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  '/Applications/Chromium.app/Contents/MacOS/Chromium',
  '/usr/bin/google-chrome',
  '/usr/bin/chromium',
  '/usr/bin/chromium-browser',
];

function parseArgs(argv) {
  const args = { url: DEFAULT_URL, out: DEFAULT_OUT, browser: null };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === '--url' && argv[i + 1]) { args.url = argv[++i]; }
    else if (a === '--out' && argv[i + 1]) { args.out = path.resolve(argv[++i]); }
    else if (a === '--browser' && argv[i + 1]) { args.browser = argv[++i]; }
  }
  return args;
}

function findBrowser(explicit) {
  if (explicit && fs.existsSync(explicit)) { return explicit; }
  for (const p of CANDIDATE_BROWSERS) {
    if (fs.existsSync(p)) { return p; }
  }
  return null;
}

function sleep(ms) { return new Promise(r => setTimeout(r, ms)); }

async function captureView(page, view, outDir) {
  // Pre-seed Zustand persist key so the dashboard boots into the desired view.
  // Layouts use defaults when viewLayouts[view] is missing -- which is what we
  // want: we always screenshot the default layout, not user-saved overrides.
  const storeState = {
    state: { activeView: view.id, viewLayouts: {}, maximizedPanel: null },
    version: 0,
  };
  await page.evaluateOnNewDocument((payload) => {
    window.localStorage.setItem('robot-dashboard-v6', payload);
  }, JSON.stringify(storeState));

  // Force-reload so the persist middleware reads the seeded value.
  await page.goto('about:blank');
  await page.goto(page.url() === 'about:blank' ? DEFAULT_URL : page.url(),
                  { waitUntil: 'networkidle2', timeout: 30000 });

  // Animations / staggered fade-ins finish in ~1 s.
  await sleep(1500);

  const outFile = path.join(outDir, view.file);
  await page.screenshot({ path: outFile, type: 'png', fullPage: false });
  return outFile;
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  const browserPath = findBrowser(args.browser);
  if (!browserPath) {
    console.error('No Chromium-based browser found. Install Edge / Chrome / Chromium, or pass --browser.');
    process.exit(2);
  }
  console.log('[capture] browser  :', browserPath);
  console.log('[capture] url      :', args.url);
  console.log('[capture] out dir  :', args.out);

  fs.mkdirSync(args.out, { recursive: true });

  const browser = await puppeteer.launch({
    executablePath: browserPath,
    headless: 'new',
    defaultViewport: { width: VIEWPORT_W, height: VIEWPORT_H },
    args: [
      '--no-sandbox',
      '--disable-gpu',
      '--hide-scrollbars',
      `--window-size=${VIEWPORT_W},${VIEWPORT_H}`,
    ],
  });

  const page = await browser.newPage();
  page.on('pageerror', (err) => console.warn('[page error]', err.message));
  page.on('requestfailed', (req) => {
    // Filter the WebSocket /ws connection -- it always fails because no gateway is running.
    if (!String(req.url()).includes('/ws')) {
      console.warn('[req failed]', req.url(), '-', req.failure() && req.failure().errorText);
    }
  });

  // Initial navigation to set page.url() so subsequent goto() works.
  await page.goto(args.url, { waitUntil: 'networkidle2', timeout: 30000 });

  const captured = [];
  const failed = [];
  for (const view of VIEWS) {
    try {
      console.log('[capture] view ->', view.id);
      const out = await captureView(page, view, args.out);
      captured.push({ view: view.id, file: out });
    } catch (err) {
      console.error('[capture] FAILED', view.id, '-', err.message);
      failed.push({ view: view.id, error: err.message });
    }
  }

  // Extra interactive-state screenshots, anchored on the OVERVIEW view.
  for (const extra of EXTRA_STATES) {
    try {
      console.log('[capture] extra ->', extra.file);
      await captureView(page, { id: 'OVERVIEW', file: '__discard__.png' }, args.out);
      // Discard the dummy file; we only used it to seed OVERVIEW.
      const dummy = path.join(args.out, '__discard__.png');
      if (fs.existsSync(dummy)) fs.unlinkSync(dummy);
      await extra.interact(page);
      await sleep(500);
      const outFile = path.join(args.out, extra.file);
      await page.screenshot({ path: outFile, type: 'png', fullPage: false });
      captured.push({ view: extra.file, file: outFile });
    } catch (err) {
      console.error('[capture] FAILED extra', extra.file, '-', err.message);
      failed.push({ view: extra.file, error: err.message });
    }
  }

  await browser.close();

  console.log('');
  console.log('[capture] SUMMARY');
  console.log('  captured:', captured.length);
  for (const c of captured) { console.log('   -', c.view, '->', c.file); }
  if (failed.length) {
    console.log('  failed  :', failed.length);
    for (const f of failed) { console.log('   -', f.view, ':', f.error); }
    process.exit(1);
  }
}

main().catch((err) => {
  console.error('[capture] fatal:', err);
  process.exit(1);
});
