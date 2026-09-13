/**
 * Public entry point for downstream projects that compile these sources
 * directly (e.g. via a git submodule aliased as `@wificonfig/ui`).
 *
 * `App.tsx` / `main.tsx` are deliberately not re-exported: compose your own
 * shell from the pieces below. Global styles live in `./styles/*.css` and
 * must be imported by the consuming app.
 */

// API
export { api } from './api/client';
export type {
  WifiStatus,
  ScanResult,
  SavedNetwork,
  APStatus,
  APConfig,
  Variable,
} from './types';

// i18n
export { i18n, locale, localeSettings, setLocale, available, registerTranslations } from './i18n';
export type { LocaleCode } from './i18n';
export { appMessages } from './i18n/messages/app';
export { networkMessages } from './i18n/messages/networks';
export { savedMessages } from './i18n/messages/saved';
export { statusMessages } from './i18n/messages/status';

// Stores
export { scanResults, scanning, scanNetworks } from './stores/networks';

// Components
export { StatusCard } from './components/StatusCard';
export { NetworkList } from './components/NetworkList';
export { SavedNetworks } from './components/SavedNetworks';
export { ConnectionModal } from './components/ConnectionModal';
export { LanguageSelector, LANGUAGES } from './components/LanguageSelector';
export { Button } from './components/ui/Button';
export { Card } from './components/ui/Card';
