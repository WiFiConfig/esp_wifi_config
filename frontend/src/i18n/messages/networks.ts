import { params } from '@nanostores/i18n'
import { i18n } from '../index'

export const networkMessages = i18n('networks', {
  title: 'Available Networks',
  scan: 'Scan',
  scanning: 'Scanning...',
  empty: 'Click Scan to find networks',
  hidden: '(hidden)',
  cancel: 'Cancel',
  connect: 'Connect',
  password: 'Password',
  connectingTo: params<{ ssid: string }>('Connecting to "{ssid}"...'),
  connectingWait: 'This may take a few seconds.',
  connectSuccess: params<{ ssid: string }>('Connected to "{ssid}"'),
  connectSuccessIp: params<{ ip: string }>('IP address: {ip}'),
  connectFailed: 'Connection failed',
  connectFailedDesc: 'Could not connect to the network. Check the password and try again.',
  dismiss: 'OK',
  retry: 'Try Again',
})
