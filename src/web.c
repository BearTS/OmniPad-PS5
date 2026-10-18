#include "web.h"
#include "ps5_vpad.h"
#include "shellui_inject.h"
#include "bt_host.h"
#include "web_mappings.h"
#include "version.h"
#include "log.h"
#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

static int g_server_fd = -1;

static const char HTML_INDEX[] =
"<!DOCTYPE html>\n"
"<html lang=\"en\">\n"
"<head>\n"
"  <meta charset=\"UTF-8\">\n"
"  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no, viewport-fit=cover\">\n"
"  <meta http-equiv=\"Cache-Control\" content=\"no-cache, no-store, must-revalidate\">\n"
"  <meta http-equiv=\"Pragma\" content=\"no-cache\">\n"
"  <meta http-equiv=\"Expires\" content=\"0\">\n"
"  <title>OmniPad PS5 Universal</title>\n"
"  <style>\n"
"    :root {\n"
"      --bg: #070a12;\n"
"      --card: rgba(18, 25, 41, 0.75);\n"
"      --accent: #3b82f6;\n"
"      --accent-glow: rgba(59, 130, 246, 0.35);\n"
"      --success: #10b981;\n"
"      --warning: #f59e0b;\n"
"      --danger: #ef4444;\n"
"      --text: #f8fafc;\n"
"      --text-muted: #94a3b8;\n"
"      --border: rgba(255, 255, 255, 0.08);\n"
"    }\n"
"    * { box-sizing: border-box; margin: 0; padding: 0; -webkit-tap-highlight-color: transparent; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; }\n"
"    body { background: radial-gradient(circle at 50% 0%, #152238 0%, var(--bg) 100%); color: var(--text); min-height: 100vh; padding: clamp(14px, 3.5vw, 28px); }\n"
"    .container { max-width: 1050px; margin: 0 auto; }\n"
"    header { display: flex; align-items: center; justify-content: space-between; flex-wrap: wrap; gap: 12px; padding-bottom: 20px; border-bottom: 1px solid var(--border); margin-bottom: 24px; }\n"
"    .logo-group h1 { font-size: clamp(22px, 5vw, 28px); font-weight: 800; background: linear-gradient(135deg, #60a5fa, #c084fc); -webkit-background-clip: text; -webkit-text-fill-color: transparent; letter-spacing: -0.5px; }\n"
"    .logo-group p { font-size: 13px; color: var(--text-muted); margin-top: 3px; }\n"
"    .badge-fw { background: rgba(59, 130, 246, 0.12); color: #93c5fd; border: 1px solid rgba(59, 130, 246, 0.25); padding: 4px 12px; border-radius: 999px; font-size: 11px; font-weight: 700; text-transform: uppercase; letter-spacing: 0.5px; }\n"
"    .actions-bar { display: flex; gap: 10px; margin-bottom: 24px; flex-wrap: wrap; }\n"
"    button { background: var(--card); color: var(--text); border: 1px solid var(--border); padding: 10px 18px; border-radius: 10px; font-size: 13px; font-weight: 600; cursor: pointer; transition: all 0.15s ease; display: inline-flex; align-items: center; justify-content: center; gap: 8px; backdrop-filter: blur(12px); min-height: 42px; }\n"
"    button:active { transform: scale(0.97); }\n"
"    button:hover { border-color: rgba(255, 255, 255, 0.25); background: rgba(255, 255, 255, 0.06); }\n"
"    button.btn-danger { color: #f87171; border-color: rgba(239, 68, 68, 0.3); background: rgba(239, 68, 68, 0.1); }\n"
"    button.btn-danger:hover { background: rgba(239, 68, 68, 0.2); }\n"
"    .slots-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(240px, 1fr)); gap: 16px; margin-bottom: 28px; }\n"
"    .card { background: var(--card); border: 1px solid var(--border); border-radius: 16px; padding: 20px; backdrop-filter: blur(16px); box-shadow: 0 8px 24px rgba(0,0,0,0.25); display: flex; flex-direction: column; transition: border-color 0.2s, transform 0.2s; }\n"
"    .card:hover { transform: translateY(-2px); }\n"
"    .card.connected { border-color: rgba(16, 185, 129, 0.35); background: rgba(16, 26, 46, 0.85); }\n"
"    .card-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; }\n"
"    .slot-label { font-size: 13px; font-weight: 700; text-transform: uppercase; letter-spacing: 0.8px; color: var(--text-muted); }\n"
"    .dot { width: 10px; height: 10px; border-radius: 50%; background: #334155; display: inline-block; }\n"
"    .dot.active { background: var(--success); box-shadow: 0 0 12px var(--success); }\n"
"    .dot.pending { background: var(--warning); box-shadow: 0 0 12px var(--warning); }\n"
"    .pad-title { font-size: 16px; font-weight: 700; color: #fff; margin-bottom: 10px; min-height: 22px; word-break: break-word; }\n"
"    .pills { display: flex; flex-wrap: wrap; gap: 6px; margin-bottom: 14px; }\n"
"    .pill { display: inline-flex; align-items: center; gap: 4px; background: rgba(255, 255, 255, 0.05); border: 1px solid var(--border); border-radius: 6px; padding: 3px 8px; font-size: 11px; font-weight: 600; color: #cbd5e1; }\n"
"    .pill.green { background: rgba(16, 185, 129, 0.12); color: #6ee7b7; border-color: rgba(16, 185, 129, 0.3); }\n"
"    .pill.blue { background: rgba(59, 130, 246, 0.12); color: #93c5fd; border-color: rgba(59, 130, 246, 0.3); }\n"
"    .meta-list { font-size: 13px; color: var(--text-muted); line-height: 1.8; margin-top: auto; }\n"
"    .meta-list span { color: var(--text); font-weight: 600; }\n"
"    .empty-state { text-align: center; color: #64748b; font-size: 13px; font-weight: 500; padding: 24px 0; }\n"
"    footer { text-align: center; color: #64748b; font-size: 12px; margin-top: 36px; line-height: 1.6; }\n"
"  </style>\n"
"</head>\n"
"<body>\n"
"  <div class=\"container\">\n"
"    <header>\n"
"      <div class=\"logo-group\">\n"
"        <h1>OmniPad PS5 Universal</h1>\n"
"        <p>Universal Controller Hub &bull; DualSense Emulation &bull; Zero Lag 250Hz</p>\n"
"      </div>\n"
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
"      <div class=\"badge-fw\">v" ANYPAD_VERSION "-b" STR(ANYPAD_BUILD) " &bull; FW 7.00 - 13.60</div>\n"
"    </header>\n"
"\n"
"    <div class=\"actions-bar\">\n"
"      <button onclick=\"window.open('/api/log', '_blank')\">\n"
"        <span>&#128220;</span> View Logs\n"
"      </button>\n"
"      <button onclick=\"refreshData()\">\n"
"        <span>&#8635;</span> Refresh\n"
"      </button>\n"
"      <button onclick=\"btPair()\" id=\"btPairBtn\">\n"
"        <span>&#128246;</span> Pair Bluetooth\n"
"      </button>\n"
"      <button onclick=\"btDisable()\">\n"
"        <span>&#9711;</span> Disable BT\n"
"      </button>\n"
"      <button onclick=\"window.open('/mappings', '_blank')\">\n"
"        <span>&#9881;</span> Controller Mappings\n"
"      </button>\n"
"      <label style=\"display:inline-flex;align-items:center;gap:8px;background:var(--card);border:1px solid var(--border);padding:6px 10px;border-radius:10px;font-size:13px;\">\n"
"        Emulate as:\n"
"        <select id=\"vpadType\" onchange=\"setVpadType(this.value)\" style=\"background:transparent;color:var(--text);border:none;font-size:13px;font-weight:600;\">\n"
"          <option value=\"3\">DualSense</option>\n"
"          <option value=\"2\">DualShock 4</option>\n"
"        </select>\n"
"      </label>\n"
"      <label style=\"display:inline-flex;align-items:center;gap:8px;background:var(--card);border:1px solid var(--border);padding:6px 10px;border-radius:10px;font-size:13px;cursor:pointer;\" title=\"Taps the console's Avcap2 haptic stream so game rumble reaches BT pads even when games don't call scePadSetVibration. Experimental.\">\n"
"        <input type=\"checkbox\" id=\"hapticToggle\" onchange=\"setHaptic(this.checked)\" style=\"accent-color:#3b82f6;\">\n"
"        Game Rumble (Avcap2)\n"
"      </label>\n"
"      <label style=\"display:inline-flex;align-items:center;gap:6px;background:var(--card);border:1px solid var(--border);padding:6px 10px;border-radius:10px;font-size:13px;\" title=\"If no BT pad is connected for this long, OmniPad releases the chip so your idle DualSense can page-scan back in. 0 = never release automatically.\">\n"
"        Auto-release idle:\n"
"        <input id=\"idleReleaseMin\" type=\"number\" min=\"0\" max=\"240\" value=\"5\" onchange=\"setIdleRelease(this.value)\" style=\"width:48px;background:transparent;color:var(--text);border:1px solid var(--border);border-radius:6px;padding:2px 4px;font-size:13px;\">\n"
"        min\n"
"        <button onclick=\"releaseNow()\" style=\"margin-left:6px;padding:4px 10px;background:transparent;color:var(--text);border:1px solid var(--border);border-radius:6px;font-size:12px;cursor:pointer;\" title=\"Immediately close the Bluetooth chip. Virtual BT pads disconnect; your native DualSense can reconnect right away. Pairing re-opens the chip.\">Release now</button>\n"
"      </label>\n"
"      <button class=\"btn-danger\" onclick=\"disconnectAll()\">\n"
"        <span>&#128268;</span> Disconnect Controllers\n"
"      </button>\n"
"      <button class=\"btn-danger\" onclick=\"stopPayload()\">\n"
"        <span>&#9209;</span> Stop Payload\n"
"      </button>\n"
"    </div>\n"
"    <div id=\"btStatus\" style=\"padding:8px 0;font-size:13px;opacity:.85;\"></div>\n"
"    <div id=\"hapticTune\" style=\"display:none;padding:4px 0 8px 0;font-size:13px;display:flex;gap:16px;align-items:center;flex-wrap:wrap;\">\n"
"      <span style=\"opacity:.65\">Rumble gain</span>\n"
"      <input id=\"hapticGain\" type=\"range\" min=\"0.1\" max=\"4.0\" step=\"0.1\" value=\"1.0\" oninput=\"document.getElementById('hapticGainV').textContent=parseFloat(this.value).toFixed(1)+'x'\" onchange=\"saveHapticSettings()\" style=\"width:200px;accent-color:#3b82f6;\">\n"
"      <span id=\"hapticGainV\">1.0x</span>\n"
"      <label style=\"display:inline-flex;align-items:center;gap:6px;cursor:pointer;\" title=\"Keep the Avcap2 stream warm so the first rumble has no cold-start delay. Costs a small amount of CPU while idle.\">\n"
"        <input id=\"hapticEager\" type=\"checkbox\" onchange=\"saveHapticSettings()\" style=\"accent-color:#3b82f6;\">\n"
"        Low-latency (keep stream warm)\n"
"      </label>\n"
"    </div>\n"
"    <div id=\"btScanList\" style=\"display:none;padding:4px 0 12px 0;\"></div>\n"
"\n"
"    <div class=\"slots-grid\" id=\"slotsContainer\">\n"
"      <!-- Filled dynamically -->\n"
"    </div>\n"
"\n"
"    <footer>\n"
"      OmniPad PS5 Universal v" ANYPAD_VERSION " (Build " STR(ANYPAD_BUILD) ") &bull; Zero Lag 250Hz &bull; Universal Gamepad Engine\n"
"    </footer>\n"
"  </div>\n"
"\n"
"  <script>\n"
"    async function refreshData() {\n"
"      try {\n"
"        const res = await fetch('/api/status');\n"
"        if (!res.ok) return;\n"
"        const data = await res.json();\n"
"        render(data);\n"
"      } catch (e) {}\n"
"    }\n"
"\n"
"    function render(data) {\n"
"      const container = document.getElementById('slotsContainer');\n"
"      container.innerHTML = '';\n"
"\n"
"      data.slots.forEach((s, idx) => {\n"
"        const card = document.createElement('div');\n"
"        const isLive = s.status === 3;\n"
"        const isPending = s.status === 1 || s.status === 2;\n"
"        card.className = 'card' + (isLive ? ' connected' : '');\n"
"        const dotClass = isLive ? 'active' : (isPending ? 'pending' : '');\n"
"        const vdaHandle = s.handle >= 0 ? s.handle : (s.alt_handle > 0 ? s.alt_handle : '--');\n"
"        \n"
"        let connBadge = '';\n"
"        if (s.conn === 4) {\n"
"          connBadge = '<span class=\"pill blue\">&#128225; 2.4G Wireless Dongle</span>';\n"
"        } else if (s.conn === 3) {\n"
"          connBadge = '<span class=\"pill green\">&#128268; USB-C Wired</span>';\n"
"        } else if (s.conn === 1 || s.conn === 2) {\n"
"          connBadge = '<span class=\"pill blue\">&#128246; Bluetooth Direct</span>';\n"
"        } else if (isLive) {\n"
"          connBadge = '<span class=\"pill blue\">&#127918; Connected</span>';\n"
"        }\n"
"\n"
"        if (isLive) {\n"
"          card.innerHTML = `\n"
"            <div class=\"card-header\">\n"
"              <span class=\"slot-label\">Player ${idx + 1}</span>\n"
"              <span class=\"dot ${dotClass}\"></span>\n"
"            </div>\n"
"            <div class=\"pad-title\">${s.name || 'Virtual DualSense'}</div>\n"
"            <div class=\"pills\">\n"
"              ${connBadge}\n"
"              <span class=\"pill blue\">&#9889; 250Hz Zero-Lag</span>\n"
"            </div>\n"
"            <div class=\"meta-list\">\n"
"              Status: <span>Connected & Ready</span><br>\n"
"              Handle VDA: <span>${vdaHandle}</span><br>\n"
"              User: <span>${s.user_id ? '0x' + s.user_id.toString(16) : 'Primary'}</span><br>\n"
"              Injected: <span>${s.injected || 0} frames</span>\n"
"            </div>\n"
"          `;\n"
"        } else {\n"
"          card.innerHTML = `\n"
"            <div class=\"card-header\">\n"
"              <span class=\"slot-label\">Player ${idx + 1}</span>\n"
"              <span class=\"dot ${dotClass}\"></span>\n"
"            </div>\n"
"            <div class=\"pad-title\" style=\"color:#64748b;\">${isPending ? 'Identifying...' : 'Slot Available'}</div>\n"
"            <div class=\"empty-state\">${isPending ? 'Syncing with console...' : 'Waiting for Controller or 2.4G Dongle'}</div>\n"
"          `;\n"
"        }\n"
"        container.appendChild(card);\n"
"      });\n"
"    }\n"
"\n"
"    async function disconnectAll() {\n"
"      await fetch('/api/disconnect?slot=-1', { method: 'POST' });\n"
"      refreshData();\n"
"    }\n"
"\n"
"    async function stopPayload() {\n"
"      if (!confirm('Stop OmniPad PS5?\\n\\nThis will:\\n  1. Disconnect every virtual pad we own\\n  2. Shut down Bluetooth cleanly (restores the chip to the state the system expects)\\n  3. Exit the payload\\n\\nYour native DualSense keeps working.')) return;\n"
"      /* Shut Bluetooth down FIRST so the HCI chip state is restored before\n"
"       * the payload exits. Then post /api/exit which drops the stop flag. */\n"
"      try {\n"
"        document.getElementById('btStatus').textContent = 'Stopping: disabling BT...';\n"
"        await fetch('/api/bt/disable', {method:'POST'});\n"
"        /* small pause so host_close has time to send HCI_Disconnect / restore */\n"
"        await new Promise(r => setTimeout(r, 1500));\n"
"        document.getElementById('btStatus').textContent = 'Stopping: exiting payload...';\n"
"        await fetch('/api/exit', {method:'POST'});\n"
"      } catch(e) {}\n"
"      /* The server will be gone within ~1 s; show a final state. */\n"
"      setTimeout(() => {\n"
"        document.body.innerHTML = '<div style=\\'padding:40px;text-align:center;color:#94a3b8;font-family:system-ui\\'><h2>Payload stopped</h2><p>Your native DualSense should keep working. If it stops pairing, hard-reboot the console.</p></div>';\n"
"      }, 2500);\n"
"    }\n"
"\n"
"    async function btPair() {\n"
"      /* Start a wide scan; the list below populates as devices appear. */\n"
"      const btn = document.getElementById('btPairBtn');\n"
"      btn.disabled = true;\n"
"      try {\n"
"        await fetch('/api/bt/scan?seconds=90', {method:'POST'});\n"
"        document.getElementById('btStatus').textContent = 'BT: scanning 90 s \u2014 put your pad in pairing mode now';\n"
"        document.getElementById('btScanList').style.display = 'block';\n"
"      } catch(e) {\n"
"        document.getElementById('btStatus').textContent = 'BT scan failed: ' + e;\n"
"      }\n"
"      setTimeout(() => { btn.disabled = false; }, 2000);\n"
"      refreshBtStatus();\n"
"      refreshBtDiscovered();\n"
"    }\n"
"    async function btPairMac(mac) {\n"
"      try {\n"
"        await fetch('/api/bt/pair?mac=' + encodeURIComponent(mac) + '&seconds=120', {method:'POST'});\n"
"        document.getElementById('btStatus').textContent = 'BT: pairing with ' + mac + '\u2026';\n"
"      } catch(e) {\n"
"        document.getElementById('btStatus').textContent = 'BT pair failed: ' + e;\n"
"      }\n"
"      refreshBtStatus();\n"
"    }\n"
"    function codLabel(cod) {\n"
"      /* Minor-device class for peripherals (major 5): 1=joystick, 2=gamepad. */\n"
"      const minor = (cod >> 2) & 0x0F;\n"
"      if (minor === 2) return 'Gamepad';\n"
"      if (minor === 1) return 'Joystick';\n"
"      return 'HID';\n"
"    }\n"
"    async function refreshBtDiscovered() {\n"
"      try {\n"
"        const r = await fetch('/api/bt/discovered');\n"
"        const d = await r.json();\n"
"        const list = document.getElementById('btScanList');\n"
"        if (!d.devices || d.devices.length === 0) {\n"
"          list.innerHTML = '<em style=\\'opacity:.6\\'>No gamepads discovered yet. Make sure your pad is in pairing mode (LED fast-blinking).</em>';\n"
"          return;\n"
"        }\n"
"        list.innerHTML = d.devices.map(dev =>\n"
"          `<div style=\\'display:flex;align-items:center;gap:10px;padding:8px 10px;margin:4px 0;background:rgba(255,255,255,0.03);border:1px solid var(--border);border-radius:8px;\\'>` +\n"
"          `<code style=\\'flex:1;font-size:13px\\'>${dev.mac}</code>` +\n"
"          `<span class=\\'pill blue\\'>${codLabel(dev.cod)}</span>` +\n"
"          `<button onclick=\\'btPairMac(\"${dev.mac}\")\\'>Pair this one</button>` +\n"
"          `</div>`\n"
"        ).join('');\n"
"      } catch(e) {}\n"
"    }\n"
"    async function btDisable() {\n"
"      try {\n"
"        await fetch('/api/bt/disable', {method: 'POST'});\n"
"        document.getElementById('btStatus').textContent = 'BT: disabled';\n"
"      } catch(e) {}\n"
"      refreshBtStatus();\n"
"    }\n"
"    async function setVpadType(t) {\n"
"      try {\n"
"        const r = await fetch('/api/vpad/type?type=' + t, {method:'POST'});\n"
"        const d = await r.json();\n"
"        document.getElementById('btStatus').textContent = 'Device type: ' + d.name + ' (reconnect the pad for it to apply)';\n"
"      } catch(e) {}\n"
"    }\n"
"    async function refreshVpadType() {\n"
"      try {\n"
"        const r = await fetch('/api/vpad/type');\n"
"        const d = await r.json();\n"
"        const sel = document.getElementById('vpadType');\n"
"        if (sel) sel.value = String(d.type);\n"
"      } catch(e) {}\n"
"    }\n"
"    async function setHaptic(on) {\n"
"      try {\n"
"        const r = await fetch('/api/bt/haptic?on=' + (on ? '1' : '0'), {method:'POST'});\n"
"        const d = await r.json();\n"
"        document.getElementById('btStatus').textContent = 'Game Rumble (Avcap2): ' + (d.haptic ? 'on' : 'off');\n"
"        document.getElementById('hapticTune').style.display = d.haptic ? 'flex' : 'none';\n"
"      } catch(e) {}\n"
"    }\n"
"    async function refreshHaptic() {\n"
"      try {\n"
"        const r = await fetch('/api/bt/haptic');\n"
"        const d = await r.json();\n"
"        const box = document.getElementById('hapticToggle');\n"
"        if (box) box.checked = !!d.haptic;\n"
"        document.getElementById('hapticTune').style.display = d.haptic ? 'flex' : 'none';\n"
"        const r2 = await fetch('/api/bt/haptic/settings');\n"
"        const s = await r2.json();\n"
"        const g = document.getElementById('hapticGain');\n"
"        if (g) { g.value = s.gain; document.getElementById('hapticGainV').textContent = parseFloat(s.gain).toFixed(1)+'x'; }\n"
"        const e = document.getElementById('hapticEager');\n"
"        if (e) e.checked = !!s.eager;\n"
"      } catch(e) {}\n"
"    }\n"
"    async function setIdleRelease(m) {\n"
"      try {\n"
"        await fetch('/api/bt/idle_release?minutes=' + m, {method:'POST'});\n"
"      } catch(e) {}\n"
"    }\n"
"    async function releaseNow() {\n"
"      try {\n"
"        await fetch('/api/bt/disable', {method:'POST'});\n"
"        document.getElementById('btStatus').textContent = 'BT: released — your DualSense can reconnect now';\n"
"      } catch(e) {}\n"
"      refreshBtStatus();\n"
"    }\n"
"    async function refreshIdleRelease() {\n"
"      try {\n"
"        const r = await fetch('/api/bt/idle_release');\n"
"        const d = await r.json();\n"
"        const el = document.getElementById('idleReleaseMin');\n"
"        if (el) el.value = d.minutes;\n"
"      } catch(e) {}\n"
"    }\n"
"    async function saveHapticSettings() {\n"
"      const gain = document.getElementById('hapticGain').value;\n"
"      const eager = document.getElementById('hapticEager').checked ? '1' : '0';\n"
"      try {\n"
"        await fetch('/api/bt/haptic/settings?gain=' + gain + '&eager=' + eager, {method:'POST'});\n"
"      } catch(e) {}\n"
"    }\n"
"    async function refreshBtStatus() {\n"
"      try {\n"
"        const r = await fetch('/api/bt/status');\n"
"        const s = await r.json();\n"
"        const el = document.getElementById('btStatus');\n"
"        if (s.pairing) el.textContent = 'BT: pairing — ' + s.pairing_left + 's left';\n"
"        else if (s.up) el.textContent = 'BT: host up';\n"
"        else el.textContent = 'BT: off (click Pair Bluetooth to start)';\n"
"      } catch(e) {}\n"
"    }\n"
"\n"
"    setInterval(refreshData, 1000);\n"
"    setInterval(refreshBtStatus, 2000);\n"
"    setInterval(() => { if (document.getElementById('btScanList').style.display !== 'none') refreshBtDiscovered(); }, 2000);\n"
"    refreshData();\n"
"    refreshBtStatus();\n"
"    refreshVpadType();\n"
"    refreshHaptic();\n"
"    refreshIdleRelease();\n"
"  </script>\n"
"</body>\n"
"</html>\n";

#ifndef SO_REUSEPORT
#define SO_REUSEPORT 0x0200
#endif

int web_init(int port)
{
    g_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_fd < 0) return 0;

    int opt = 1;
    setsockopt(g_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_REUSEPORT
    setsockopt(g_server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons((uint16_t)port);
    sin.sin_addr.s_addr = INADDR_ANY;

    int bound = 0;
    for (int retry = 0; retry < 10; retry++) {
        if (bind(g_server_fd, (struct sockaddr *)&sin, sizeof(sin)) == 0) {
            bound = 1;
            break;
        }
        if (retry == 0) {
            log_line("web: port %d busy, waiting for release...", port);
        }
        usleep(250000); /* 250ms */
    }

    if (!bound) {
        log_line("web: bind error on port %d (%s)", port, strerror(errno));
        close(g_server_fd);
        g_server_fd = -1;
        return 0;
    }

    if (listen(g_server_fd, 8) != 0) {
        close(g_server_fd);
        g_server_fd = -1;
        return 0;
    }

    fcntl(g_server_fd, F_SETFL, fcntl(g_server_fd, F_GETFL) | O_NONBLOCK);
    log_line("web: Dashboard server listening at http://0.0.0.0:%d/", port);
    return 1;
}

static void handle_client(int cfd)
{
    /* Clear O_NONBLOCK inherited from listening socket */
    int flags = fcntl(cfd, F_GETFL, 0);
    if (flags >= 0) fcntl(cfd, F_SETFL, flags & ~O_NONBLOCK);

    /* Set 100ms socket read timeout to prevent stalling the main 250Hz loop */
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000; /* 100 ms */
    setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(cfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct pollfd pfd;
    pfd.fd = cfd;
    pfd.events = POLLIN;
    int pr = poll(&pfd, 1, 100);
    if (pr <= 0) { close(cfd); return; }

    char req[2048];
    ssize_t n = read(cfd, req, sizeof(req) - 1);
    if (n <= 0) { close(cfd); return; }
    req[n] = '\0';

    char res[4096];
    int res_len = 0;

    if (strncmp(req, "OPTIONS", 7) == 0) {
        res_len = snprintf(res, sizeof(res),
            "HTTP/1.1 204 No Content\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n\r\n");
        write(cfd, res, (size_t)res_len);
        close(cfd);
        return;
    }

    if (strncmp(req, "GET /api/status", 15) == 0) {
        char json[2048];
        int jlen = snprintf(json, sizeof(json),
            "{\"version\":\"%s\",\"build\":%d,\"slots\":[",
            ANYPAD_VERSION, ANYPAD_BUILD);

        for (int i = 0; i < MAX_SLOTS; i++) {
            vpad_slot_info_t info;
            vpad_get_slot_info(i, &info);
            jlen += snprintf(json + jlen, sizeof(json) - (size_t)jlen,
                "%s{\"status\":%d,\"name\":\"%s\",\"conn\":%d,\"handle\":%d,\"alt_handle\":%d,\"user_id\":%u,\"battery\":%d,\"charging\":%d,\"injected\":%u}",
                i > 0 ? "," : "",
                info.status, info.name, info.conn_type, info.handle, info.alt_handle,
                (unsigned)info.user_id, info.battery_level, info.battery_charging,
                (unsigned)info.packets_injected);
        }
        snprintf(json + jlen, sizeof(json) - (size_t)jlen, "]}");

        res_len = snprintf(res, sizeof(res),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n\r\n%s",
            strlen(json), json);
        write(cfd, res, (size_t)res_len);
    }
    else if (strncmp(req, "POST /api/pair", 14) == 0) {
        const char *ok = "{\"status\":\"usb_plug_and_play_active\"}";
        res_len = snprintf(res, sizeof(res),
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n%s",
            strlen(ok), ok);
        write(cfd, res, (size_t)res_len);
    }
    else if (strncmp(req, "POST /api/press_ps", 18) == 0) {
        char *p = strstr(req, "slot=");
        int slot = p ? atoi(p + 5) : 0;
        if (slot < 0) {
            for (int i = 0; i < MAX_SLOTS; i++) {
                if (vpad_is_live(i)) {
                    vpad_slot_info_t info;
                    vpad_get_slot_info(i, &info);
                    shellui_press_ps_button(info.handle);
                    vpad_press_ps_button(i);
                }
            }
        } else if (slot >= 0 && slot < MAX_SLOTS) {
            vpad_slot_info_t info;
            vpad_get_slot_info(slot, &info);
            shellui_press_ps_button(info.handle);
            vpad_press_ps_button(slot);
        }
        const char *ok = "{\"status\":\"ps_pressed\"}";
        res_len = snprintf(res, sizeof(res),
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n%s",
            strlen(ok), ok);
        write(cfd, res, (size_t)res_len);
    }
    else if (strncmp(req, "POST /api/disconnect", 20) == 0) {
        char *p = strstr(req, "slot=");
        int slot = p ? atoi(p + 5) : 0;
        if (slot < 0) {
            for (int i = 0; i < MAX_SLOTS; i++) vpad_remove(i);
        } else {
            vpad_remove(slot);
        }
        const char *ok = "{\"status\":\"disconnected\"}";
        res_len = snprintf(res, sizeof(res),
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n%s",
            strlen(ok), ok);
        write(cfd, res, (size_t)res_len);
    }
    else if (strncmp(req, "POST /api/rebind", 16) == 0) {
        char *p = strstr(req, "slot=");
        int slot = p ? atoi(p + 5) : 0;
        char *pu = strstr(req, "user=");
        int32_t user = pu ? (int32_t)strtol(pu + 5, NULL, 0) : -1;
        vpad_rebind_user(slot, user);
        const char *ok = "{\"status\":\"rebound\"}";
        res_len = snprintf(res, sizeof(res),
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n%s",
            strlen(ok), ok);
        write(cfd, res, (size_t)res_len);
    }
    else if (strncmp(req, "POST /api/exit", 14) == 0) {
        const char *ok = "{\"status\":\"shutting_down\"}";
        res_len = snprintf(res, sizeof(res),
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n%s",
            strlen(ok), ok);
        write(cfd, res, (size_t)res_len);
        close(cfd);
        int sfd = open("/data/anypad/stop", O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (sfd >= 0) close(sfd);
        return;
    }
    else if (strncmp(req, "GET /api/log", 12) == 0) {
        int lfd = open("/data/anypad/anypad.log", O_RDONLY);
        if (lfd < 0) lfd = open("anypad.log", O_RDONLY);
        if (lfd >= 0) {
            off_t sz = lseek(lfd, 0, SEEK_END);
            off_t start = 0;
            if (sz > 32768) start = sz - 32768;
            lseek(lfd, start, SEEK_SET);
            char logbuf[32768];
            ssize_t rb = read(lfd, logbuf, sizeof(logbuf));
            close(lfd);
            if (rb < 0) rb = 0;
            res_len = snprintf(res, sizeof(res),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain; charset=utf-8\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Content-Length: %zd\r\n"
                "Connection: close\r\n\r\n", rb);
            write(cfd, res, (size_t)res_len);
            if (rb > 0) write(cfd, logbuf, (size_t)rb);
        } else {
            const char *err = "Log file not found";
            res_len = snprintf(res, sizeof(res),
                "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n%s",
                strlen(err), err);
            write(cfd, res, (size_t)res_len);
        }
    }
    else if (strncmp(req, "POST /api/send_btn", 18) == 0) {
        char *p_slot = strstr(req, "slot=");
        int slot = p_slot ? atoi(p_slot + 5) : 0;
        char *p_code = strstr(req, "code=");
        char *p_btn = strstr(req, "btn=");
        uint32_t btn_mask = PAD_BTN_CROSS;
        if (p_code) {
            int c = atoi(p_code + 5);
            switch (c) {
            case 1: btn_mask = PAD_BTN_CROSS; break;
            case 2: btn_mask = PAD_BTN_CIRCLE; break;
            case 3: btn_mask = PAD_DPAD_UP; break;
            case 4: btn_mask = PAD_DPAD_DOWN; break;
            case 5: btn_mask = PAD_DPAD_LEFT; break;
            case 6: btn_mask = PAD_DPAD_RIGHT; break;
            case 7: btn_mask = PAD_BTN_PS; break;
            case 8: btn_mask = PAD_BTN_SQUARE; break;
            case 9: btn_mask = PAD_BTN_TRIANGLE; break;
            case 10: btn_mask = PAD_BTN_OPTIONS; break;
            case 11: btn_mask = PAD_BTN_SHARE; break;
            case 12: btn_mask = PAD_BTN_L1; break;
            case 13: btn_mask = PAD_BTN_R1; break;
            default: btn_mask = PAD_BTN_CROSS; break;
            }
        } else if (p_btn) {
            if (strncmp(p_btn + 4, "dpad_up", 7) == 0) btn_mask = PAD_DPAD_UP;
            else if (strncmp(p_btn + 4, "dpad_down", 9) == 0) btn_mask = PAD_DPAD_DOWN;
            else if (strncmp(p_btn + 4, "dpad_left", 9) == 0) btn_mask = PAD_DPAD_LEFT;
            else if (strncmp(p_btn + 4, "dpad_right", 10) == 0) btn_mask = PAD_DPAD_RIGHT;
            else if (strncmp(p_btn + 4, "circle", 6) == 0) btn_mask = PAD_BTN_CIRCLE;
            else if (strncmp(p_btn + 4, "cross", 5) == 0) btn_mask = PAD_BTN_CROSS;
            else if (strncmp(p_btn + 4, "square", 6) == 0) btn_mask = PAD_BTN_SQUARE;
            else if (strncmp(p_btn + 4, "triangle", 8) == 0) btn_mask = PAD_BTN_TRIANGLE;
            else if (strncmp(p_btn + 4, "options", 7) == 0) btn_mask = PAD_BTN_OPTIONS;
            else if (strncmp(p_btn + 4, "share", 5) == 0) btn_mask = PAD_BTN_SHARE;
            else if (strncmp(p_btn + 4, "ps", 2) == 0) btn_mask = PAD_BTN_PS;
        }

        /* If PS button is requested, also synthesize inside SceShellUI */
        if (btn_mask & PAD_BTN_PS) {
            if (slot < 0) {
                for (int i = 0; i < MAX_SLOTS; i++) {
                    if (vpad_is_live(i)) {
                        vpad_slot_info_t info;
                        vpad_get_slot_info(i, &info);
                        shellui_press_ps_button(info.handle);
                    }
                }
            } else if (slot >= 0 && slot < MAX_SLOTS && vpad_is_live(slot)) {
                vpad_slot_info_t info;
                vpad_get_slot_info(slot, &info);
                shellui_press_ps_button(info.handle);
            }
        }

        if (slot < 0) {
            for (int i = 0; i < MAX_SLOTS; i++) {
                if (vpad_is_live(i)) {
                    pad_state_t st;
                    pad_state_neutral(&st);
                    st.buttons = btn_mask;
                    vpad_update(i, &st);
                }
            }
            usleep(150000);
            for (int i = 0; i < MAX_SLOTS; i++) {
                if (vpad_is_live(i)) {
                    pad_state_t st;
                    pad_state_neutral(&st);
                    vpad_update(i, &st);
                }
            }
        } else if (slot >= 0 && slot < MAX_SLOTS && vpad_is_live(slot)) {
            pad_state_t st;
            pad_state_neutral(&st);
            st.buttons = btn_mask;
            vpad_update(slot, &st);
            usleep(150000);
            pad_state_neutral(&st);
            vpad_update(slot, &st);
        }
        const char *ok = "{\"status\":\"button_sent\"}";
        res_len = snprintf(res, sizeof(res),
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n%s",
            strlen(ok), ok);
        write(cfd, res, (size_t)res_len);
    }
    /* ---- Bluetooth control endpoints ---- */
    else if (strncmp(req, "GET /api/bt/status", 18) == 0) {
        char body[512];
        int up = bt_host_is_up();
        int left = bt_host_get_pairing_seconds_left();
        int blen = snprintf(body, sizeof body,
            "{\"up\":%s,\"pairing\":%s,\"pairing_left\":%d}",
            up ? "true":"false", left > 0 ? "true":"false", left);
        res_len = snprintf(res, sizeof(res),
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\nContent-Length: %d\r\n"
            "Connection: close\r\n\r\n%s", blen, body);
        write(cfd, res, (size_t)res_len);
    }
    else if (strncmp(req, "POST /api/bt/scan", 17) == 0) {
        /* Open the BT chip and run inquiry WITHOUT a MAC filter: on_discover
         * records every gamepad-class device it hears; dashboard reads
         * /api/bt/discovered to render the list. Still auto-pairs any pad
         * in pairing mode; the dashboard "Pair this one" flow swaps in a
         * MAC filter via /api/bt/pair?mac=... before clicking. */
        int seconds = 90;
        const char *p = strstr(req, "seconds=");
        if (p) seconds = atoi(p + 8);
        if (seconds <= 0 || seconds > 300) seconds = 90;
        int ok = bt_host_is_up() || bt_host_init();
        if (ok) {
            /* Clear any lingering MAC filter so the scan is wide-open. */
            unsigned char zero[6] = {0};
            extern void host_set_allowed_mac(void *, const unsigned char[6]);
            /* bt_host doesn't expose the host_t pointer; use the public
             * bt_host_pair_mac(NULL,...) path implicitly by starting a
             * normal pair window instead. */
            (void)zero; (void)host_set_allowed_mac;
            bt_host_start_pairing(seconds);
        }
        const char *body = ok ? "{\"status\":\"scanning\"}" : "{\"status\":\"bt_init_failed\"}";
        res_len = snprintf(res, sizeof(res),
            "HTTP/1.1 %s\r\nContent-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\nContent-Length: %zu\r\n"
            "Connection: close\r\n\r\n%s",
            ok ? "200 OK" : "503 Service Unavailable", strlen(body), body);
        write(cfd, res, (size_t)res_len);
    }
    else if (strncmp(req, "POST /api/bt/haptic", 19) == 0) {
        const char *p = strstr(req, "on=");
        int on = p && (p[3] == '1' || p[3] == 't');
        bt_haptic_enable(on);
        char body[64];
        int blen = snprintf(body, sizeof body, "{\"haptic\":%s}", on ? "true" : "false");
        res_len = snprintf(res, sizeof(res),
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\nContent-Length: %d\r\n"
            "Connection: close\r\n\r\n%s", blen, body);
        write(cfd, res, (size_t)res_len);
    }
    else if (strncmp(req, "POST /api/bt/idle_release", 25) == 0) {
        const char *p = strstr(req, "minutes=");
        int m = p ? atoi(p + 8) : 5;
        bt_host_idle_release_set(m);
        char body[64];
        int blen = snprintf(body, sizeof body, "{\"minutes\":%d}", bt_host_idle_release_get());
        res_len = snprintf(res, sizeof(res),
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\nContent-Length: %d\r\n"
            "Connection: close\r\n\r\n%s", blen, body);
        write(cfd, res, (size_t)res_len);
    }
    else if (strncmp(req, "GET /api/bt/idle_release", 24) == 0) {
        char body[64];
        int blen = snprintf(body, sizeof body, "{\"minutes\":%d}", bt_host_idle_release_get());
        res_len = snprintf(res, sizeof(res),
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\nContent-Length: %d\r\n"
            "Connection: close\r\n\r\n%s", blen, body);
        write(cfd, res, (size_t)res_len);
    }
    else if (strncmp(req, "GET /api/bt/haptic/settings", 27) == 0) {
        float gain; int eager;
        bt_haptic_settings_get(&gain, &eager);
        char body[96];
        int blen = snprintf(body, sizeof body,
            "{\"gain\":%.2f,\"eager\":%s}", gain, eager ? "true" : "false");
        res_len = snprintf(res, sizeof(res),
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\nContent-Length: %d\r\n"
            "Connection: close\r\n\r\n%s", blen, body);
        write(cfd, res, (size_t)res_len);
    }
    else if (strncmp(req, "POST /api/bt/haptic/settings", 28) == 0) {
        float gain; int eager;
        bt_haptic_settings_get(&gain, &eager);
        const char *p = strstr(req, "gain=");
        if (p) gain = (float)atof(p + 5);
        p = strstr(req, "eager=");
        if (p) eager = (p[6] == '1' || p[6] == 't');
        bt_haptic_settings_set(gain, eager);
        bt_haptic_settings_get(&gain, &eager);
        char body[96];
        int blen = snprintf(body, sizeof body,
            "{\"gain\":%.2f,\"eager\":%s}", gain, eager ? "true" : "false");
        res_len = snprintf(res, sizeof(res),
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\nContent-Length: %d\r\n"
            "Connection: close\r\n\r\n%s", blen, body);
        write(cfd, res, (size_t)res_len);
    }
    else if (strncmp(req, "GET /api/bt/haptic", 18) == 0) {
        char body[64];
        int blen = snprintf(body, sizeof body, "{\"haptic\":%s}",
                            bt_haptic_is_enabled() ? "true" : "false");
        res_len = snprintf(res, sizeof(res),
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\nContent-Length: %d\r\n"
            "Connection: close\r\n\r\n%s", blen, body);
        write(cfd, res, (size_t)res_len);
    }
    else if (strncmp(req, "GET /api/vpad/type", 18) == 0) {
        int t = vpad_get_device_type();
        char body[64];
        int blen = snprintf(body, sizeof body,
            "{\"type\":%d,\"name\":\"%s\"}",
            t, t == 2 ? "DualShock 4" : "DualSense");
        res_len = snprintf(res, sizeof(res),
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\nContent-Length: %d\r\n"
            "Connection: close\r\n\r\n%s", blen, body);
        write(cfd, res, (size_t)res_len);
    }
    else if (strncmp(req, "POST /api/vpad/type", 19) == 0) {
        const char *p = strstr(req, "type=");
        int t = p ? atoi(p + 5) : 0;
        int before = vpad_get_device_type();
        vpad_set_device_type(t);
        int after = vpad_get_device_type();
        int changed = (before != after);
        char body[128];
        int blen = snprintf(body, sizeof body,
            "{\"type\":%d,\"name\":\"%s\",\"changed\":%s,\"note\":\"applies to next virtual pad; reconnect\"}",
            after, after == 2 ? "DualShock 4" : "DualSense", changed ? "true" : "false");
        res_len = snprintf(res, sizeof(res),
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\nContent-Length: %d\r\n"
            "Connection: close\r\n\r\n%s", blen, body);
        write(cfd, res, (size_t)res_len);
    }
    else if (strncmp(req, "GET /api/bt/discovered", 22) == 0) {
        bt_discovered_t rows[16];
        int n = bt_host_discovered(rows, 16);
        char body[1024]; int blen = 0;
        blen += snprintf(body + blen, sizeof body - blen, "{\"devices\":[");
        long now = now_ms();
        for (int i = 0; i < n && blen < (int)sizeof body - 100; i++) {
            blen += snprintf(body + blen, sizeof body - blen,
                "%s{\"mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\",\"cod\":%u,\"age_ms\":%ld}",
                i ? "," : "",
                rows[i].mac[5], rows[i].mac[4], rows[i].mac[3],
                rows[i].mac[2], rows[i].mac[1], rows[i].mac[0],
                rows[i].cod, now - rows[i].last_seen_ms);
        }
        blen += snprintf(body + blen, sizeof body - blen, "]}");
        res_len = snprintf(res, sizeof(res),
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\nContent-Length: %d\r\n"
            "Connection: close\r\n\r\n%s", blen, body);
        write(cfd, res, (size_t)res_len);
    }
    else if (strncmp(req, "POST /api/bt/pair", 17) == 0) {
        /* Pair a specific MAC: ?mac=AA:BB:CC:DD:EE:FF&seconds=120.
         * Without ?mac, falls back to a wide scan window (backwards-
         * compatible with the earlier UI). */
        int seconds = 120;
        unsigned char mac[6] = {0};
        int have_mac = 0;
        const char *p = strstr(req, "seconds=");
        if (p) seconds = atoi(p + 8);
        if (seconds <= 0 || seconds > 300) seconds = 120;
        p = strstr(req, "mac=");
        if (p) {
            p += 4;
            unsigned x[6];
            if (sscanf(p, "%2x%*[:%-]%2x%*[:%-]%2x%*[:%-]%2x%*[:%-]%2x%*[:%-]%2x",
                       &x[5],&x[4],&x[3],&x[2],&x[1],&x[0]) == 6) {
                for (int i = 0; i < 6; i++) mac[i] = (unsigned char)x[i];
                have_mac = 1;
            }
        }
        int ok;
        if (have_mac) {
            ok = bt_host_pair_mac(mac, seconds);
        } else {
            ok = bt_host_is_up() || bt_host_init();
            if (ok) bt_host_start_pairing(seconds);
        }
        const char *body = ok ? "{\"status\":\"pairing_started\"}" : "{\"status\":\"bt_init_failed\"}";
        res_len = snprintf(res, sizeof(res),
            "HTTP/1.1 %s\r\nContent-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\nContent-Length: %zu\r\n"
            "Connection: close\r\n\r\n%s",
            ok ? "200 OK" : "503 Service Unavailable", strlen(body), body);
        write(cfd, res, (size_t)res_len);
    }
    else if (strncmp(req, "POST /api/bt/disable", 20) == 0) {
        bt_host_cleanup();
        const char *body = "{\"status\":\"bt_disabled\"}";
        res_len = snprintf(res, sizeof(res),
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\nContent-Length: %zu\r\n"
            "Connection: close\r\n\r\n%s", strlen(body), body);
        write(cfd, res, (size_t)res_len);
    }
    else if (strncmp(req, "POST /api/bt/forget", 19) == 0) {
        /* Accepts ?mac=AA:BB:... Not implemented in the facade yet. */
        const char *body = "{\"status\":\"not_implemented\"}";
        res_len = snprintf(res, sizeof(res),
            "HTTP/1.1 501 Not Implemented\r\nContent-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\nContent-Length: %zu\r\n"
            "Connection: close\r\n\r\n%s", strlen(body), body);
        write(cfd, res, (size_t)res_len);
    }
    /* ---- Mapping (per-controller button remap) endpoints ---- */
    else if (strncmp(req, "GET /api/mapping/", 17) == 0) {
        char path[256]; char id[16] = {0};
        /* Pull VVVV_PPPP token after /api/mapping/ */
        const char *p = req + 17;
        size_t n_id = 0;
        while (n_id + 1 < sizeof id && isalnum((unsigned char)*p)) id[n_id++] = *p++;
        snprintf(path, sizeof path, "/data/omnipad/maps/%s.map", id);
        FILE *f = fopen(path, "r");
        if (f) {
            /* Convert the .map file lines (key = value) into a JSON object. */
            char body[1024]; int blen = 0;
            blen += snprintf(body + blen, sizeof body - blen, "{");
            char line[128]; int first = 1;
            while (fgets(line, sizeof line, f) && blen < (int)sizeof body - 80) {
                char *hash = strchr(line, '#'); if (hash) *hash = 0;
                char *eq = strchr(line, '='); if (!eq) continue;
                *eq = 0; char *k = line, *v = eq + 1;
                while (*k == ' ' || *k == '\t') k++;
                char *ke = k + strlen(k); while (ke > k && (ke[-1]==' '||ke[-1]=='\t'||ke[-1]=='\n'||ke[-1]=='\r')) *--ke = 0;
                while (*v == ' ' || *v == '\t') v++;
                char *ve = v + strlen(v); while (ve > v && (ve[-1]==' '||ke[-1]=='\t'||ve[-1]=='\n'||ve[-1]=='\r')) *--ve = 0;
                if (!*k) continue;
                blen += snprintf(body + blen, sizeof body - blen,
                                 "%s\"%s\":\"%s\"", first ? "" : ",", k, v);
                first = 0;
            }
            fclose(f);
            blen += snprintf(body + blen, sizeof body - blen, "}");
            res_len = snprintf(res, sizeof(res),
                "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                "Access-Control-Allow-Origin: *\r\nContent-Length: %d\r\n"
                "Connection: close\r\n\r\n%s", blen, body);
            write(cfd, res, (size_t)res_len);
        } else {
            const char *body = "{\"error\":\"not_found\"}";
            res_len = snprintf(res, sizeof(res),
                "HTTP/1.1 404 Not Found\r\nContent-Type: application/json\r\n"
                "Access-Control-Allow-Origin: *\r\nContent-Length: %zu\r\n"
                "Connection: close\r\n\r\n%s", strlen(body), body);
            write(cfd, res, (size_t)res_len);
        }
    }
    else if (strncmp(req, "POST /api/mapping/", 18) == 0) {
        /* Dashboard POSTs a flat JSON object; we persist it as key=value lines. */
        char id[16] = {0};
        const char *p = req + 18;
        size_t n_id = 0;
        while (n_id + 1 < sizeof id && isalnum((unsigned char)*p)) id[n_id++] = *p++;
        char *body_start = strstr(req, "\r\n\r\n");
        if (body_start) body_start += 4; else body_start = (char*)req + n;
        mkdir("/data/omnipad", 0755);
        mkdir("/data/omnipad/maps", 0755);
        char path[256];
        snprintf(path, sizeof path, "/data/omnipad/maps/%s.map", id);
        FILE *f = fopen(path, "w");
        if (!f) {
            const char *body = "{\"error\":\"write_failed\"}";
            res_len = snprintf(res, sizeof(res),
                "HTTP/1.1 500 Internal Server Error\r\nContent-Type: application/json\r\n"
                "Access-Control-Allow-Origin: *\r\nContent-Length: %zu\r\n"
                "Connection: close\r\n\r\n%s", strlen(body), body);
            write(cfd, res, (size_t)res_len);
        } else {
            fprintf(f, "# OmniPad: written by the mappings page.\n");
            /* Shallow key:value extraction — assumes a flat object with string
             * values, which is what the mappings page sends. */
            char *cur = body_start;
            while (cur && *cur) {
                char *k = strchr(cur, '"'); if (!k) break;
                char *k_end = strchr(k + 1, '"'); if (!k_end) break;
                char *colon = strchr(k_end, ':'); if (!colon) break;
                char *v = strchr(colon, '"'); if (!v) break;
                char *v_end = strchr(v + 1, '"'); if (!v_end) break;
                *k_end = 0; *v_end = 0;
                fprintf(f, "%s = %s\n", k + 1, v + 1);
                cur = v_end + 1;
            }
            fclose(f);
            const char *body = "{\"status\":\"saved\"}";
            res_len = snprintf(res, sizeof(res),
                "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                "Access-Control-Allow-Origin: *\r\nContent-Length: %zu\r\n"
                "Connection: close\r\n\r\n%s", strlen(body), body);
            write(cfd, res, (size_t)res_len);
        }
    }
    else if (strncmp(req, "GET /mappings", 13) == 0) {
        /* Serve the embedded mappings page (writes header + body in two
         * sends so we don't need a 25 KB stack buffer). */
        res_len = snprintf(res, sizeof(res),
            "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n"
            "Cache-Control: no-cache\r\nContent-Length: %zu\r\n"
            "Connection: close\r\n\r\n", HTML_MAPPINGS_LEN);
        write(cfd, res, (size_t)res_len);
        /* Chunked write to tolerate small write() returns on slow clients. */
        size_t off = 0;
        while (off < HTML_MAPPINGS_LEN) {
            ssize_t w = write(cfd, HTML_MAPPINGS + off, HTML_MAPPINGS_LEN - off);
            if (w <= 0) break;
            off += (size_t)w;
        }
    }
    else {
        /* Serve Web Dashboard HTML */
        size_t hlen = sizeof(HTML_INDEX) - 1;
        res_len = snprintf(res, sizeof(res),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "Cache-Control: no-cache, no-store, must-revalidate\r\n"
            "Pragma: no-cache\r\n"
            "Expires: 0\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n\r\n", hlen);
        write(cfd, res, (size_t)res_len);
        write(cfd, HTML_INDEX, hlen);
    }
    close(cfd);
}

void web_poll(long now)
{
    (void)now;
    if (g_server_fd < 0) return;

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int cfd = accept(g_server_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (cfd >= 0) {
        handle_client(cfd);
    }
}

void web_cleanup(void)
{
    if (g_server_fd >= 0) {
        close(g_server_fd);
        g_server_fd = -1;
    }
}
