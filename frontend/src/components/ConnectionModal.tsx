import { useState, useEffect, useRef } from 'preact/hooks';
import { useStore } from '@nanostores/preact';
import { Button } from './ui/Button';
import { networkMessages } from '../i18n/messages/networks';
import { api } from '../api/client';
import './ConnectionModal.css';

type Status = 'connecting' | 'success' | 'failed';

interface Props {
  ssid: string;
  onDismiss: () => void;
  onRetry: () => void;
}

const POLL_INTERVAL = 500;
const TIMEOUT = 20000;

export function ConnectionModal({ ssid, onDismiss, onRetry }: Props) {
  const [status, setStatus] = useState<Status>('connecting');
  const [ip, setIp] = useState('');
  const timerRef = useRef<number | null>(null);
  const timeoutRef = useRef<number | null>(null);
  const t = useStore(networkMessages);

  useEffect(() => {
    const poll = async () => {
      try {
        const result = await api.getStatus();
        if (result.state === 'connected' && result.ssid === ssid) {
          setIp(result.ip);
          setStatus('success');
          return;
        }
      } catch {
        // keep polling
      }
      timerRef.current = window.setTimeout(poll, POLL_INTERVAL);
    };

    timerRef.current = window.setTimeout(poll, POLL_INTERVAL);

    timeoutRef.current = window.setTimeout(() => {
      if (timerRef.current) clearTimeout(timerRef.current);
      setStatus((s) => (s === 'connecting' ? 'failed' : s));
    }, TIMEOUT);

    return () => {
      if (timerRef.current) clearTimeout(timerRef.current);
      if (timeoutRef.current) clearTimeout(timeoutRef.current);
    };
  }, [ssid]);

  // Stop polling once resolved
  useEffect(() => {
    if (status !== 'connecting') {
      if (timerRef.current) clearTimeout(timerRef.current);
      if (timeoutRef.current) clearTimeout(timeoutRef.current);
    }
  }, [status]);

  return (
    <div class="modal-backdrop">
      <div class="modal" role="dialog" aria-modal="true">
        {status === 'connecting' && (
          <>
            <div class="modal-icon connecting">
              <span class="spinner-lg" />
            </div>
            <h2 class="modal-title">{t.connectingTo({ ssid })}</h2>
            <p class="modal-desc text-muted">{t.connectingWait}</p>
          </>
        )}

        {status === 'success' && (
          <>
            <div class="modal-icon success">
              <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
                <polyline points="20 6 9 17 4 12" />
              </svg>
            </div>
            <h2 class="modal-title">{t.connectSuccess({ ssid })}</h2>
            <p class="modal-desc modal-ip">{t.connectSuccessIp({ ip })}</p>
            <Button onClick={onDismiss} class="w-full">{t.dismiss}</Button>
          </>
        )}

        {status === 'failed' && (
          <>
            <div class="modal-icon failed">
              <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
                <line x1="18" y1="6" x2="6" y2="18" />
                <line x1="6" y1="6" x2="18" y2="18" />
              </svg>
            </div>
            <h2 class="modal-title">{t.connectFailed}</h2>
            <p class="modal-desc text-muted">{t.connectFailedDesc}</p>
            <div class="modal-actions">
              <Button variant="secondary" onClick={onDismiss}>{t.dismiss}</Button>
              <Button onClick={onRetry}>{t.retry}</Button>
            </div>
          </>
        )}
      </div>
    </div>
  );
}
