// Minimal vanilla-JS frontend for the esp_wifi_config REST API.
// Everything talks to /api/wifi/*; see website/docs/api/rest-api.md.
(function () {
  'use strict';
  var API = '/api/wifi';
  var $ = function (sel) { return document.querySelector(sel); };

  function api(path, opts) {
    return fetch(API + path, opts).then(function (r) {
      if (!r.ok) { throw new Error(r.status + ' ' + r.statusText); }
      return r.status === 204 ? null : r.json();
    });
  }
  function show(text, isErr) {
    var m = $('#msg'); m.textContent = text; m.hidden = !text;
    m.className = 'msg' + (isErr ? ' err' : '');
  }

  function refreshStatus() {
    return api('/status').then(function (s) {
      var el = $('#status');
      el.querySelector('[data-k=state]').textContent = s.state || '–';
      el.querySelector('[data-k=ssid]').textContent = s.ssid || '–';
      el.querySelector('[data-k=ip]').textContent = s.ip || '–';
    }).catch(function () {});
  }

  function scan() {
    var sel = $('#ssid');
    sel.innerHTML = '<option value="">Scanning…</option>';
    return api('/scan').then(function (r) {
      var seen = {};
      sel.innerHTML = '<option value="">Choose a network</option>';
      (r.networks || []).forEach(function (n) {
        if (!n.ssid || seen[n.ssid]) { return; }
        seen[n.ssid] = true;
        var o = document.createElement('option');
        o.value = n.ssid;
        o.textContent = n.ssid + '  (' + n.rssi + ' dBm' + (n.auth === 'OPEN' ? ', open' : '') + ')';
        sel.appendChild(o);
      });
    }).catch(function (e) { show('Scan failed: ' + e.message, true); });
  }

  function refreshSaved() {
    return api('/networks').then(function (r) {
      var ul = $('#saved'); ul.innerHTML = '';
      var list = r.networks || [];
      if (!list.length) { ul.innerHTML = '<li class="hint">None</li>'; return; }
      list.forEach(function (n) {
        var li = document.createElement('li');
        var name = document.createElement('span'); name.textContent = n.ssid;
        var del = document.createElement('button'); del.className = 'small'; del.textContent = 'Forget';
        del.onclick = function () {
          api('/networks/' + encodeURIComponent(n.ssid), { method: 'DELETE' })
            .then(refreshSaved).catch(function (e) { show(e.message, true); });
        };
        li.appendChild(name); li.appendChild(del); ul.appendChild(li);
      });
    }).catch(function () {});
  }

  $('#join').addEventListener('submit', function (ev) {
    ev.preventDefault();
    var ssid = $('#ssid').value, password = $('#password').value;
    if (!ssid) { return; }
    show('Saving ' + ssid + '…');
    api('/networks', {
      method: 'POST', headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ ssid: ssid, password: password, priority: 10 })
    }).then(function () {
      show('Connecting to ' + ssid + '…');
      return api('/connect', {
        method: 'POST', headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ssid: ssid })
      });
    }).then(function () {
      refreshSaved();
      var tries = 0;
      var t = setInterval(function () {
        refreshStatus();
        if (++tries > 15) { clearInterval(t); }
      }, 2000);
    }).catch(function (e) { show(e.message, true); });
  });
  $('#rescan').addEventListener('click', scan);

  refreshStatus(); refreshSaved(); scan();
  setInterval(refreshStatus, 5000);
})();
