import { useState, useEffect } from 'preact/hooks';
import { useStore } from '@nanostores/preact';
import { StatusCard } from './components/StatusCard';
import { NetworkList } from './components/NetworkList';
import { SavedNetworks } from './components/SavedNetworks';
import { LanguageSelector } from './components/LanguageSelector';
import { api } from './api/client';
import { appMessages } from './i18n/messages/app';
import type { WifiStatus } from './types';
import './styles/variables.css';
import './styles/base.css';
import './styles/utilities.css';

export function App() {
  const [status, setStatus] = useState<WifiStatus | null>(null);
  const [loading, setLoading] = useState(true);
  const t = useStore(appMessages);

  const loadStatus = async () => {
    try {
      const s = await api.getStatus();
      setStatus(s);
    } catch (e) {
      console.error('Failed to load status:', e);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    loadStatus();
    const interval = setInterval(loadStatus, 5000);
    return () => clearInterval(interval);
  }, []);

  return (
    <div class="app">
      <header class="header">
        <h1>{t.title}</h1>
        <LanguageSelector />
      </header>

      <StatusCard status={status} loading={loading} />
      <NetworkList onConnect={loadStatus} />
      <SavedNetworks />
    </div>
  );
}
