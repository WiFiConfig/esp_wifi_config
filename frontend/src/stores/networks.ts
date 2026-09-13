import { atom } from 'nanostores'
import type { ScanResult } from '../types'
import { api } from '../api/client'

export const scanResults = atom<ScanResult[]>([])
export const scanning = atom(false)

export async function scanNetworks() {
  scanning.set(true)
  try {
    const results = await api.scan()
    scanResults.set(results)
  } catch (e) {
    console.error('Scan failed:', e)
  } finally {
    scanning.set(false)
  }
}
