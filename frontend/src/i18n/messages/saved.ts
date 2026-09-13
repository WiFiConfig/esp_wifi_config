import { params } from '@nanostores/i18n'
import { i18n } from '../index'

export const savedMessages = i18n('saved', {
  title: 'Saved Networks',
  cancel: 'Cancel',
  add: '+ Add',
  ssid: 'SSID',
  passwordOptional: 'Password (optional)',
  addNetwork: 'Add Network',
  loading: 'Loading...',
  empty: 'No saved networks',
  priority: params<{ priority: string }>('Priority: {priority}'),
})
