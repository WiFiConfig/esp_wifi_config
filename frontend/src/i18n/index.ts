import { persistentAtom } from '@nanostores/persistent'
import { localeFrom, browser, createI18n } from '@nanostores/i18n'
import type { ComponentsJSON } from '@nanostores/i18n'

import es from './translations/es.json'
import fr from './translations/fr.json'
import de from './translations/de.json'
import vi from './translations/vi.json'

/** Locale codes the UI ships translations for ('en' is the base language). */
export const available = ['en', 'es', 'fr', 'de', 'vi'] as const
export type LocaleCode = (typeof available)[number]

const translations: Record<string, ComponentsJSON> = { es, fr, de, vi }

/**
 * Merge extra namespaces (or extra keys inside existing namespaces) into the
 * bundled catalog for `code`. Downstream projects use this to add their own
 * message namespaces; call it before the first render so the catalog is
 * complete when the active locale is loaded.
 */
export function registerTranslations(code: string, catalog: ComponentsJSON): void {
  const existing = translations[code] ?? {}
  const merged: ComponentsJSON = { ...existing }
  for (const [namespace, messages] of Object.entries(catalog)) {
    merged[namespace] = { ...existing[namespace], ...messages }
  }
  translations[code] = merged
}

/** Persisted user choice (localStorage key "locale"); undefined = follow the browser. */
export const localeSettings = persistentAtom<string | undefined>(
  'locale',
  undefined
)

export const locale = localeFrom(
  localeSettings,
  browser({ available, fallback: 'en' })
)

export const i18n = createI18n(locale, {
  async get(code) {
    return translations[code] ?? {}
  },
})

export function setLocale(code: string) {
  localeSettings.set(code)
}
