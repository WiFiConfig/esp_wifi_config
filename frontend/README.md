# ESP WiFi Config - Web UI

Modern, responsive web interface for ESP WiFi Config. Built with Preact +
TypeScript + Vite, and embedded in the firmware as gzipped static assets.

## Quick Start

```bash
# Install dependencies (package-lock.json is tracked; use npm ci for exact versions)
npm install

# Development server (hot reload; API calls proxied to a device or the test server)
npm run dev

# Production build (runs tsc, then vite build)
npm run build
```

## Features

- Status display with signal strength (polled every 5 s)
- Network scanning and connection with a live connection-progress modal
- Saved networks management
- Localized UI (English, Spanish, French, German, Vietnamese) with a language selector
- Dark mode support (auto-detect)
- Mobile-first responsive design
- Lightweight: about 15 KB gzipped total (see [Build for ESP32](#build-for-esp32))

## Development against the test server

`tools/test_server/test_server.py` (in the repo root) emulates the device REST
API in memory. `vite.config.ts` proxies `/api` to `http://127.0.0.1:8080`, which
is the test server's default port, so the dev server talks to it transparently:

```bash
# Terminal 1 (repo root)
pip install -r tools/test_server/requirements.txt
python3 tools/test_server/test_server.py            # --port, --config, --no-aps, --no-vars

# Terminal 2
cd frontend && npm run dev                         # http://localhost:5173
```

Point the proxy at a real device instead by editing the `target` in
`vite.config.ts`. See `tools/test_server/README.md` for the server options.

## Project Structure

```
frontend/
├── src/
│   ├── api/client.ts          # REST API client (all /api/wifi/* calls)
│   ├── types.ts               # API response types
│   ├── components/
│   │   ├── ui/                # Button, Card primitives
│   │   ├── StatusCard.tsx     # Connection state display
│   │   ├── NetworkList.tsx    # Scan + connect flow
│   │   ├── ConnectionModal.tsx# Polls /status after connect; success/failed/retry
│   │   ├── SavedNetworks.tsx  # Saved network management
│   │   └── LanguageSelector.tsx
│   ├── i18n/
│   │   ├── index.ts           # locale store, i18n instance, registerTranslations()
│   │   ├── messages/*.ts      # English base strings, one namespace per file
│   │   └── translations/*.json# Non-English catalogs (bundled into app.js)
│   ├── stores/networks.ts     # Scan results / scanning state (nanostores)
│   ├── styles/                # variables.css, base.css, utilities.css
│   ├── lib.ts                 # Barrel for downstream consumers (see below)
│   ├── App.tsx                # Status-page shell used by this repo
│   └── main.tsx               # Mount point
├── dist/                      # Build output (only the .gz files are tracked)
├── package.json / package-lock.json
├── vite.config.ts
└── tsconfig.json
```

Every component pairs a `.tsx` with a `.css` imported by relative path, and all
imports between sources are relative, so the tree can be compiled from another
project root.

## Internationalization

Strings are managed with `@nanostores/i18n`. English base strings are defined
per namespace in `src/i18n/messages/{app,status,networks,saved}.ts`; the other
languages live in `src/i18n/translations/{es,fr,de,vi}.json`, keyed by
namespace, and are statically imported so they ship inside `app.js`.

The active locale comes from, in order: the `locale` key in `localStorage`
(set by the LanguageSelector), then `navigator.language`, then `en`.

Inside a component:

```tsx
import { useStore } from '@nanostores/preact';
import { networkMessages } from '../i18n/messages/networks';

const t = useStore(networkMessages);
<h2>{t.title}</h2>                      // plain string
<p>{t.connectingTo({ ssid })}</p>       // params<{ ssid: string }>() string
```

### Adding a language

1. Add `src/i18n/translations/<code>.json` with every namespace and key
   present in the existing JSON files.
2. In `src/i18n/index.ts`, import the file, add it to the `translations` map,
   and add `<code>` to the `available` list.
3. Add `{ code: '<code>', label: '<XX>' }` to `LANGUAGES` in
   `src/components/LanguageSelector.tsx`.

### Adding strings

Add the English text to the relevant `src/i18n/messages/*.ts` namespace (or
create a new namespace file), then add the same key to each translation JSON.

## Using the UI from another project

The sources are designed to be compiled by a downstream Preact/Vite project,
for example by including this repo as a git submodule and aliasing
`@wificonfig/ui` to `<submodule>/frontend/src`:

```ts
// vite.config.ts (downstream)
// A plain string alias resolves the bare import to a directory (EISDIR);
// use two regex entries so '@wificonfig/ui' hits lib.ts and deep imports map to src/.
const ui = path.resolve(__dirname, 'esp_wifi_config/frontend/src')
resolve: { alias: [
  { find: /^@wificonfig\/ui$/,      replacement: path.join(ui, 'lib.ts') },
  { find: /^@wificonfig\/ui\/(.*)$/, replacement: path.join(ui, '$1') },
] }
// tsconfig.json (downstream)
"paths": { "@wificonfig/ui": ["./esp_wifi_config/frontend/src/lib.ts"],
           "@wificonfig/ui/*": ["./esp_wifi_config/frontend/src/*"] }
```

The downstream project must install the same runtime dependencies (`preact`,
`nanostores`, `@nanostores/i18n`, `@nanostores/persistent`,
`@nanostores/preact`) and enable `resolveJsonModule` in its tsconfig.

`src/lib.ts` exports everything needed to build a custom shell: the `api`
client and types, the `i18n` instance, `locale`/`setLocale`/`available`,
`registerTranslations`, the message namespaces (`appMessages`,
`networkMessages`, `savedMessages`, `statusMessages`), the `scanResults` /
`scanning` / `scanNetworks` store, the `LANGUAGES` list and every component.
`App.tsx` and `main.tsx` are intentionally not part of the barrel; import the
global styles yourself:

```tsx
import { StatusCard, NetworkList, LanguageSelector } from '@wificonfig/ui';
import '@wificonfig/ui/styles/variables.css';
import '@wificonfig/ui/styles/base.css';
import '@wificonfig/ui/styles/utilities.css';
```

### Extending translations

Downstream code can define its own namespaces with the shared `i18n` instance
and merge translations for them into the bundled catalogs with
`registerTranslations(localeCode, partialCatalog)`. `@nanostores/i18n` loads a
locale's catalog once, when the first message store is mounted, so register
before the first render:

```ts
import { i18n, registerTranslations } from '@wificonfig/ui';
import de from './translations/de.json';   // { "setup": { ... } }

registerTranslations('de', de);            // merges the "setup" namespace into the German catalog
export const setupMessages = i18n('setup', { welcome: 'Welcome' });
```

Keys inside an existing namespace are merged too, so a downstream can override
individual strings (for example `app.title`) the same way.

## Customization

### Change Theme

Edit `src/styles/variables.css`:

```css
:root {
  --color-primary: #3b82f6;    /* Change accent color */
  --color-bg: #f8fafc;         /* Background */
  --color-surface: #ffffff;    /* Card background */
}
```

### Add Custom Component

1. Create the component in `src/components/` (and its strings in `src/i18n/messages/`)
2. Import it in `App.tsx`
3. Rebuild: `npm run build`

## Build for ESP32

`npm run build` writes to `dist/` and gzips each file alongside the original.
Only the gzipped assets and `index.html` are tracked in git and embedded in the
firmware. Measured sizes for the current build:

| File | Raw | Gzipped |
|------|-----|---------|
| `index.html` | 693 B | (served raw) |
| `assets/app.js` | 32.8 KB | 12.9 KB |
| `assets/index.css` | 7.6 KB | 2.0 KB |
| **Embedded total** | | **15.6 KB** |

After a build, regenerate the C-array header so the embedded copy matches. It
is what Arduino and PlatformIO builds (no component CMake, so no `EMBED_FILES`)
link instead of the linker symbols:

```bash
python3 tools/generate_arduino_assets.py          # writes src/arduino/webui_assets.h
python3 tools/generate_arduino_assets.py --check  # verifies it is up to date
```

## API Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | /api/wifi/status | WiFi status |
| GET | /api/wifi/scan | Scan networks |
| GET | /api/wifi/networks | Saved networks |
| POST | /api/wifi/networks | Add network |
| DELETE | /api/wifi/networks/:ssid | Delete network |
| POST | /api/wifi/connect | Connect |
| POST | /api/wifi/disconnect | Disconnect |
| POST | /api/wifi/factory_reset | Factory reset |
