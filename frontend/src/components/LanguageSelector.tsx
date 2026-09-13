import { useStore } from '@nanostores/preact'
import { locale, setLocale } from '../i18n'
import type { LocaleCode } from '../i18n'

export const LANGUAGES: ReadonlyArray<{ code: LocaleCode; label: string }> = [
  { code: 'en', label: 'EN' },
  { code: 'es', label: 'ES' },
  { code: 'fr', label: 'FR' },
  { code: 'de', label: 'DE' },
  { code: 'vi', label: 'VI' },
]

export function LanguageSelector() {
  const current = useStore(locale)

  return (
    <select
      class="language-selector"
      value={current}
      onChange={(e) => setLocale((e.target as HTMLSelectElement).value)}
    >
      {LANGUAGES.map((lang) => (
        <option key={lang.code} value={lang.code}>
          {lang.label}
        </option>
      ))}
    </select>
  )
}
