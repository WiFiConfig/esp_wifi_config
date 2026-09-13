import { useState, useEffect } from 'preact/hooks';
import { useStore } from '@nanostores/preact';
import { Card } from './ui/Card';
import { Button } from './ui/Button';
import { ConnectionModal } from './ConnectionModal';
import { networkMessages } from '../i18n/messages/networks';
import { api } from '../api/client';
import { scanResults, scanning, scanNetworks } from '../stores/networks';
import './NetworkList.css';

interface Props {
  onConnect: (ssid: string) => void;
}

export function NetworkList({ onConnect }: Props) {
  const networks = useStore(scanResults);
  const isScanning = useStore(scanning);
  const t = useStore(networkMessages);
  const [selectedSsid, setSelectedSsid] = useState<string | null>(null);
  const [password, setPassword] = useState('');
  const [connecting, setConnecting] = useState(false);
  const [connectingSsid, setConnectingSsid] = useState<string | null>(null);

  useEffect(() => {
    if (scanResults.get().length === 0) scanNetworks();
  }, []);

  const connect = async (ssid: string) => {
    setConnecting(true);
    try {
      // Add network first if password provided
      if (password) {
        await api.addNetwork(ssid, password);
      }
      await api.connect(ssid);
      setConnectingSsid(ssid);
      setSelectedSsid(null);
      setPassword('');
    } catch (e) {
      console.error('Connect failed:', e);
    } finally {
      setConnecting(false);
    }
  };

  const handleDismiss = () => {
    if (connectingSsid) onConnect(connectingSsid);
    setConnectingSsid(null);
  };

  const handleRetry = () => {
    const ssid = connectingSsid;
    setConnectingSsid(null);
    if (ssid) {
      setSelectedSsid(ssid);
    }
  };

  return (
    <>
      <Card
        title={t.title}
        action={
          <Button size="sm" variant="secondary" onClick={scanNetworks} loading={isScanning}>
            {t.scan}
          </Button>
        }
      >
        {networks.length === 0 ? (
          <div class="network-empty">
            {isScanning ? t.scanning : t.empty}
          </div>
        ) : (
          <div class="network-list">
            {networks.map((net) => (
              <div class="network-item" key={net.ssid}>
                <div class="network-info">
                  <div class="network-ssid">
                    {net.auth !== 'OPEN' && <span class="lock">🔒</span>}
                    {net.ssid || t.hidden}
                  </div>
                  <div class="network-meta text-sm text-muted">
                    {net.rssi} dBm • {net.auth}
                  </div>
                </div>
                <Button
                  size="sm"
                  onClick={() => setSelectedSsid(selectedSsid === net.ssid ? null : net.ssid)}
                >
                  {selectedSsid === net.ssid ? t.cancel : t.connect}
                </Button>

                {selectedSsid === net.ssid && (
                  <div class="network-connect-form">
                    {net.auth !== 'OPEN' && (
                      <input
                        type="password"
                        placeholder={t.password}
                        value={password}
                        onInput={(e) => setPassword((e.target as HTMLInputElement).value)}
                      />
                    )}
                    <Button
                      onClick={() => connect(net.ssid)}
                      loading={connecting}
                      class="w-full"
                    >
                      {t.connect}
                    </Button>
                  </div>
                )}
              </div>
            ))}
          </div>
        )}
      </Card>

      {connectingSsid && (
        <ConnectionModal
          ssid={connectingSsid}
          onDismiss={handleDismiss}
          onRetry={handleRetry}
        />
      )}
    </>
  );
}
