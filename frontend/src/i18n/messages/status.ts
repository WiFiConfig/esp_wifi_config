import { params } from '@nanostores/i18n'
import { i18n } from '../index'

export const statusMessages = i18n('status', {
  loading: 'Loading...',
  error: 'Unable to load status',
  connected: params<{ ssid: string }>('Connected to "{ssid}"'),
  disconnected: 'Disconnected',
})
