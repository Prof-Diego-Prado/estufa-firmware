// Página do dashboard real (HTML + CSS + JS), embutida na memória flash da
// ESP32 via PROGMEM. Não depende de nenhum arquivo externo nem de internet
// para renderizar — só as chamadas a /data e ao stream da câmera são de
// rede, e ambas para a própria placa (local) ou via túnel (remoto).
#pragma once
#include <Arduino.h>

const char DASHBOARD_HTML[] PROGMEM = R"HTMLPAGE(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Estufa Automatizada</title>
<style>
  :root{
    --bg:#070b12; --bg-2:#0a1019; --surface:#0d141f; --surface-2:#121b29;
    --border:#1e2b3f; --text:#e8eefb; --muted:#8195b0; --muted-2:#54687f;
    --accent:#22c7d9; --accent-2:#7c5cff; --ok:#22b884; --warn:#f2a93c; --danger:#ef4a5a;
    --font-display: ui-monospace, "SF Mono", "Cascadia Code", "JetBrains Mono", "Roboto Mono", Menlo, Consolas, monospace;
    --font-body: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
    --radius:16px;
  }
  @media (prefers-color-scheme: light){
    :root{
      --bg:#eef2f8; --bg-2:#e6ecf5; --surface:#ffffff; --surface-2:#f4f7fc;
      --border:#d7e0ee; --text:#101826; --muted:#54637a; --muted-2:#8494a8;
      --accent:#0e8fa3; --accent-2:#6d4bd6; --ok:#0f9d64; --warn:#b6790a; --danger:#d8394a;
    }
  }
  :root[data-theme="dark"]{
    --bg:#070b12; --bg-2:#0a1019; --surface:#0d141f; --surface-2:#121b29;
    --border:#1e2b3f; --text:#e8eefb; --muted:#8195b0; --muted-2:#54687f;
    --accent:#22c7d9; --accent-2:#7c5cff; --ok:#22b884; --warn:#f2a93c; --danger:#ef4a5a;
  }
  :root[data-theme="light"]{
    --bg:#eef2f8; --bg-2:#e6ecf5; --surface:#ffffff; --surface-2:#f4f7fc;
    --border:#d7e0ee; --text:#101826; --muted:#54637a; --muted-2:#8494a8;
    --accent:#0e8fa3; --accent-2:#6d4bd6; --ok:#0f9d64; --warn:#b6790a; --danger:#d8394a;
  }

  *{box-sizing:border-box;}
  html,body{margin:0; padding:0;}
  body{
    background:
      radial-gradient(60rem 30rem at 12% -10%, color-mix(in srgb, var(--accent) 16%, transparent), transparent 60%),
      radial-gradient(50rem 26rem at 100% 0%, color-mix(in srgb, var(--accent-2) 14%, transparent), transparent 60%),
      var(--bg);
    color:var(--text); font-family:var(--font-body); min-height:100vh;
  }
  .scene{max-width:1280px; margin:0 auto; padding:28px 20px 56px;}
  :focus-visible{outline:2px solid var(--accent); outline-offset:2px; border-radius:6px;}
  .eyebrow{font-family:var(--font-display); font-size:.7rem; text-transform:uppercase; letter-spacing:.16em; color:var(--muted); font-weight:600;}
  .tabular{font-variant-numeric:tabular-nums;}

  .topbar{
    display:flex; align-items:center; justify-content:space-between; gap:16px;
    flex-wrap:wrap; padding-bottom:20px; margin-bottom:24px; border-bottom:1px solid var(--border);
  }
  .brand{display:flex; align-items:baseline; gap:10px;}
  .brand .mark{color:var(--accent); font-family:var(--font-display); font-size:1.1rem;}
  .brand h1{
    font-family:var(--font-display); font-size:clamp(1.2rem, 1.6vw + .9rem, 1.55rem);
    font-weight:700; letter-spacing:-.01em; margin:0; text-wrap:balance;
    background:linear-gradient(90deg, var(--text), var(--accent) 65%, var(--accent-2));
    -webkit-background-clip:text; background-clip:text; color:transparent;
  }
  .topbar-meta{display:flex; align-items:center; gap:10px; flex-wrap:wrap;}
  .chip{
    display:inline-flex; align-items:center; gap:7px; padding:6px 12px; border-radius:100px;
    border:1px solid var(--border); background:var(--surface); font-family:var(--font-display);
    font-size:.72rem; letter-spacing:.05em; color:var(--muted); white-space:nowrap;
  }
  button.chip{cursor:pointer; font:inherit; letter-spacing:.05em;}
  .chip .dot{width:7px; height:7px; border-radius:50%; background:var(--muted-2); flex:none;}
  .chip.online .dot{background:var(--ok); box-shadow:0 0 0 3px color-mix(in srgb, var(--ok) 25%, transparent);}
  .chip.offline{color:var(--danger); border-color:color-mix(in srgb, var(--danger) 45%, var(--border));}
  .chip.offline .dot{background:var(--danger);}
  @media (prefers-reduced-motion:no-preference){ .chip.online .dot{animation:pulse 2.4s ease-in-out infinite;} }
  @keyframes pulse{0%,100%{opacity:1;} 50%{opacity:.45;}}

  .hero{display:grid; grid-template-columns:1.5fr 1fr; gap:16px; margin-bottom:28px;}
  @media (max-width:900px){ .hero{grid-template-columns:1fr;} }
  .panel{background:var(--surface); border:1px solid var(--border); border-radius:var(--radius); padding:18px;}
  .panel-head{display:flex; align-items:center; justify-content:space-between; margin-bottom:12px;}
  .panel-head h2{margin:0; font-size:.95rem; font-weight:600;}

  .cam-frame{position:relative; width:100%; aspect-ratio:16/10; border-radius:12px; overflow:hidden; background:#03080a; border:1px solid var(--border);}
  .cam-frame img{width:100%; height:100%; object-fit:cover; display:block; transition:filter .3s;}
  .cam-frame.offline img{filter:brightness(.25) saturate(.3);}
  .cam-overlay{position:absolute; inset:0; pointer-events:none; font-family:var(--font-display); font-size:.68rem; letter-spacing:.04em;}
  .cam-tl{position:absolute; top:10px; left:12px; color:#c9f7ff; text-shadow:0 1px 4px rgba(0,0,0,.6);}
  .cam-tr{position:absolute; top:10px; right:12px; display:flex; align-items:center; gap:6px; color:#ff8f97; text-shadow:0 1px 4px rgba(0,0,0,.6);}
  .cam-tr .rec-dot{width:8px; height:8px; border-radius:50%; background:#ff4d5e;}
  @media (prefers-reduced-motion:no-preference){ .cam-tr .rec-dot{animation:pulse 1.4s ease-in-out infinite;} }
  .cam-bottom{position:absolute; left:0; right:0; bottom:0; padding:8px 12px; background:linear-gradient(0deg, rgba(0,0,0,.55), transparent); color:#d7f6ff; display:flex; justify-content:space-between; gap:8px;}
  .no-signal{
    position:absolute; inset:0; display:flex; flex-direction:column; align-items:center; justify-content:center;
    gap:6px; background:rgba(3,8,10,.88); color:#ffb3ba; opacity:0; pointer-events:none; transition:opacity .3s;
    font-family:var(--font-display); text-align:center; padding:16px;
  }
  .cam-frame.offline .no-signal{opacity:1;}
  .no-signal .big{font-size:1.05rem; letter-spacing:.08em;}
  .no-signal .sub{font-size:.72rem; color:#ffcdd2; max-width:280px;}

  .vitals{display:flex; flex-direction:column; gap:12px;}
  .vitals-grid{display:grid; grid-template-columns:1fr 1fr; gap:10px; margin-top:10px;}
  .vital{background:var(--surface-2); border:1px solid var(--border); border-radius:12px; padding:12px; display:flex; flex-direction:column; gap:4px;}
  .vital .label{font-size:.72rem; color:var(--muted); font-family:var(--font-display); letter-spacing:.04em;}
  .vital .val{font-family:var(--font-display); font-size:1.5rem; font-weight:700;}
  .vital .val small{font-size:.85rem; font-weight:500; color:var(--muted); margin-left:2px;}
  .vital.state .val{font-size:1.05rem;}
  .vital.state.on .val{color:var(--ok);}

  section.group{margin-bottom:28px;}
  .group > .eyebrow{margin-bottom:10px; display:block;}
  .cards{display:grid; gap:14px; grid-template-columns:repeat(auto-fit, minmax(230px, 1fr));}
  .card{background:var(--surface); border:1px solid var(--border); border-radius:var(--radius); padding:16px; display:flex; flex-direction:column; gap:10px;}
  .card-top{display:flex; align-items:baseline; justify-content:space-between; gap:8px;}
  .card-top h3{margin:0; font-size:.85rem; font-weight:600; color:var(--muted);}
  .trend{font-family:var(--font-display); font-size:.72rem; color:var(--muted-2);}
  .card .reading{display:flex; align-items:baseline; gap:6px;}
  .card .reading .n{font-family:var(--font-display); font-size:2.1rem; font-weight:700; line-height:1;}
  .card .reading .u{color:var(--muted); font-size:1rem;}
  .card .reading.warn .n{color:var(--warn);}
  .spark{width:100%; height:44px; display:block;}

  .actuator-card{background:var(--surface); border:1px solid var(--border); border-radius:var(--radius); padding:18px; display:flex; flex-direction:column; gap:14px;}
  .actuator-head{display:flex; align-items:center; justify-content:space-between; gap:12px;}
  .actuator-head h3{margin:0; font-size:1rem; font-weight:700;}
  .status-pill{font-family:var(--font-display); font-size:.72rem; letter-spacing:.04em; padding:4px 10px; border-radius:100px; background:var(--surface-2); color:var(--muted); border:1px solid var(--border);}
  .status-pill.on{color:var(--ok); border-color:color-mix(in srgb, var(--ok) 45%, var(--border)); background:color-mix(in srgb, var(--ok) 12%, var(--surface-2));}
  .switch-row{display:flex; align-items:center; justify-content:space-between; gap:10px;}
  .switch-row .txt{font-size:.88rem; color:var(--muted);}
  .switch{position:relative; width:48px; height:26px; flex:none;}
  .switch input{position:absolute; opacity:0; width:100%; height:100%; margin:0; cursor:pointer;}
  .switch .track{position:absolute; inset:0; background:var(--surface-2); border:1px solid var(--border); border-radius:26px; transition:.25s;}
  .switch .track::before{content:""; position:absolute; height:18px; width:18px; left:3px; top:2px; background:var(--muted); border-radius:50%; transition:.25s;}
  .switch input:checked + .track{background:color-mix(in srgb, var(--accent) 30%, var(--surface-2)); border-color:var(--accent);}
  .switch input:checked + .track::before{transform:translateX(21px); background:var(--accent);}
  .switch input:focus-visible + .track{outline:2px solid var(--accent); outline-offset:2px;}
  .divider{height:1px; background:var(--border); border:0; margin:2px 0;}
  .threshold-row{display:flex; flex-direction:column; gap:6px;}
  .threshold-row .top{display:flex; justify-content:space-between; font-size:.8rem; color:var(--muted);}
  input[type="range"]{-webkit-appearance:none; width:100%; height:4px; border-radius:4px; background:var(--surface-2); accent-color:var(--accent);}
  .pump-status{font-family:var(--font-display); font-size:.78rem; color:var(--muted);}
  .pump-status.active{color:var(--ok);}

  footer{margin-top:12px; padding-top:18px; border-top:1px solid var(--border); display:flex; justify-content:space-between; flex-wrap:wrap; gap:8px; font-size:.78rem; color:var(--muted-2);}
  footer .sig{font-family:var(--font-display);}

  .modal-overlay{position:fixed; inset:0; background:rgba(2,6,10,.72); display:flex; align-items:center; justify-content:center; padding:20px; z-index:50;}
  .modal-overlay[hidden]{display:none;}
  .modal{background:var(--surface); border:1px solid var(--border); border-radius:16px; padding:22px; width:100%; max-width:380px; display:flex; flex-direction:column; gap:12px;}
  .modal h2{margin:0; font-size:1rem;}
  .modal p{margin:0; font-size:.78rem; color:var(--muted); line-height:1.4;}
  .field{display:flex; flex-direction:column; gap:5px;}
  .field label{font-size:.72rem; color:var(--muted); font-family:var(--font-display); letter-spacing:.04em;}
  .field input{background:var(--surface-2); border:1px solid var(--border); border-radius:8px; padding:9px 10px; color:var(--text); font-family:var(--font-body); font-size:.9rem;}
  .modal-actions{display:flex; justify-content:flex-end; gap:10px; margin-top:4px;}
  .btn{font-family:var(--font-display); font-size:.78rem; letter-spacing:.03em; padding:9px 16px; border-radius:9px; border:1px solid var(--border); background:var(--surface-2); color:var(--text); cursor:pointer;}
  .btn.primary{background:var(--accent); border-color:var(--accent); color:#04222a;}
</style>
</head>
<body>

<div class="scene">

  <div class="topbar">
    <div class="brand">
      <span class="mark">◆</span>
      <h1>ESTUFA // PAINEL DE CONTROLE</h1>
    </div>
    <div class="topbar-meta">
      <span class="chip" id="statusChip"><span class="dot"></span><span id="statusText">conectando…</span></span>
      <span class="chip">SESSÃO <span id="sessionClock" class="tabular">00:00:00</span></span>
      <button class="chip" id="settingsBtn" type="button">⚙ Acesso</button>
    </div>
  </div>

  <div class="hero">
    <div class="panel">
      <div class="panel-head">
        <h2>Monitoramento — ESTUFA-CAM 01</h2>
        <span class="eyebrow">AO VIVO</span>
      </div>
      <div class="cam-frame offline" id="camFrame">
        <img id="camStream" alt="Stream da câmera da estufa">
        <div class="cam-overlay">
          <div class="cam-tl">ESTUFA-CAM-01</div>
          <div class="cam-tr"><span class="rec-dot"></span>AO VIVO</div>
          <div class="cam-bottom">
            <span>ESP32-CAM</span>
            <span id="camClock" class="tabular">--:--:--</span>
          </div>
        </div>
        <div class="no-signal">
          <span class="big">📡 SEM SINAL</span>
          <span class="sub">Não foi possível conectar à câmera. Verifique a rede/túnel e as credenciais de acesso.</span>
        </div>
      </div>
    </div>

    <div class="panel vitals">
      <div class="panel-head" style="margin-bottom:0;">
        <h2>Leitura Atual</h2>
        <span class="eyebrow">A CADA 2S</span>
      </div>
      <div class="vitals-grid">
        <div class="vital">
          <span class="label">TEMPERATURA</span>
          <span class="val tabular"><span id="v-temp">--</span><small>°C</small></span>
        </div>
        <div class="vital">
          <span class="label">UMIDADE DO AR</span>
          <span class="val tabular"><span id="v-hum">--</span><small>%</small></span>
        </div>
        <div class="vital" id="v-pump-wrap">
          <span class="label">BOMBA</span>
          <span class="val" id="v-pump">--</span>
        </div>
        <div class="vital" id="v-uv-wrap">
          <span class="label">FITA ROXA</span>
          <span class="val" id="v-uv">--</span>
        </div>
      </div>
    </div>
  </div>

  <section class="group">
    <span class="eyebrow">CLIMA</span>
    <div class="cards">
      <div class="card">
        <div class="card-top"><h3>Temperatura</h3><span class="trend" id="t-temp">--</span></div>
        <div class="reading"><span class="n tabular" id="n-temp">--</span><span class="u">°C</span></div>
        <canvas class="spark" id="s-temp"></canvas>
      </div>
      <div class="card">
        <div class="card-top"><h3>Umidade do Ar</h3><span class="trend" id="t-hum">--</span></div>
        <div class="reading"><span class="n tabular" id="n-hum">--</span><span class="u">%</span></div>
        <canvas class="spark" id="s-hum"></canvas>
      </div>
    </div>
  </section>

  <section class="group">
    <span class="eyebrow">SOLO</span>
    <div class="cards">
      <div class="card">
        <div class="card-top"><h3>Umidade do Solo 1</h3><span class="trend" id="t-soil1">--</span></div>
        <div class="reading" id="r-soil1"><span class="n tabular" id="n-soil1">--</span><span class="u">%</span></div>
        <canvas class="spark" id="s-soil1"></canvas>
      </div>
      <div class="card">
        <div class="card-top"><h3>Umidade do Solo 2</h3><span class="trend" id="t-soil2">--</span></div>
        <div class="reading" id="r-soil2"><span class="n tabular" id="n-soil2">--</span><span class="u">%</span></div>
        <canvas class="spark" id="s-soil2"></canvas>
      </div>
    </div>
  </section>

  <section class="group">
    <span class="eyebrow">ATUADORES</span>
    <div class="cards">
      <div class="actuator-card">
        <div class="actuator-head">
          <h3>💦 Bomba de Irrigação</h3>
          <span class="status-pill" id="pumpPill">DESLIGADA</span>
        </div>
        <div class="switch-row">
          <span class="txt">Controle manual</span>
          <label class="switch"><input type="checkbox" id="pumpSwitch"><span class="track"></span></label>
        </div>
        <hr class="divider">
        <div class="switch-row">
          <span class="txt">Irrigação automática</span>
          <label class="switch"><input type="checkbox" id="autoSwitch"><span class="track"></span></label>
        </div>
        <div class="threshold-row">
          <div class="top"><span>Limite mínimo do solo</span><span id="thLabel" class="tabular">30%</span></div>
          <input type="range" id="thRange" min="5" max="70" value="30">
        </div>
        <span class="pump-status" id="pumpStatusText">modo automático desligado</span>
      </div>

      <div class="actuator-card">
        <div class="actuator-head">
          <h3>🟣 Fita LED Roxa (simulação UV)</h3>
          <span class="status-pill" id="uvPill">DESLIGADA</span>
        </div>
        <div class="switch-row">
          <span class="txt">Ligar fita roxa</span>
          <label class="switch"><input type="checkbox" id="uvSwitch"><span class="track"></span></label>
        </div>
        <hr class="divider">
        <span class="pump-status">tempo ligado nesta sessão: <span id="uvRuntime" class="tabular">00:00:00</span></span>
      </div>
    </div>
  </section>

  <footer>
    <span>Painel hospedado na própria ESP32-CAM · dados atualizados a cada 2s.</span>
    <span class="sig">estufa.local // build v1</span>
  </footer>

</div>

<div class="modal-overlay" id="settingsOverlay" hidden>
  <div class="modal">
    <h2>Acesso ao painel</h2>
    <p>Use o mesmo usuário/senha configurados no firmware da ESP32-CAM (AUTH_USER/AUTH_PASS). Ficam salvos só neste navegador.</p>
    <div class="field"><label>Usuário</label><input type="text" id="inUser" autocomplete="username"></div>
    <div class="field"><label>Senha</label><input type="password" id="inPass" autocomplete="current-password"></div>
    <div class="field">
      <label>Endereço do stream da câmera (opcional)</label>
      <input type="text" id="inStream" placeholder="ex: https://estufa-cam.trycloudflare.com">
    </div>
    <p>Deixe em branco se o painel e a câmera estiverem no mesmo endereço (uso na rede local). Preencha ao acessar por um túnel remoto, quando painel e câmera têm endereços diferentes.</p>
    <div class="modal-actions">
      <button class="btn" id="settingsCancel" type="button">Cancelar</button>
      <button class="btn primary" id="settingsSave" type="button">Salvar</button>
    </div>
  </div>
</div>

<script>
(function(){
  "use strict";
  const HIST_LEN = 30;
  const clamp = (v,a,b) => Math.max(a, Math.min(b,v));
  const pad2 = n => String(Math.floor(n)).padStart(2,'0');
  const fmtClock = s => `${pad2(s/3600)}:${pad2((s/60)%60)}:${pad2(s%60)}`;

  const state = {
    user: localStorage.getItem('estufaUser') || '',
    pass: localStorage.getItem('estufaPass') || '',
    streamBase: localStorage.getItem('estufaStreamBase') || '',
    threshold: 30,
    uvSeconds: 0,
    hist: { temp:[], hum:[], soil1:[], soil2:[] }
  };
  let offlineCount = 0;
  let camWatchdog = null, camRetryTimer = null;

  function authHeader(){ return 'Basic ' + btoa(state.user + ':' + state.pass); }

  function pushHist(key, v){
    const h = state.hist[key];
    h.push(v);
    if(h.length > HIST_LEN) h.shift();
  }

  function trendArrow(hist){
    const n = hist.length;
    if(n < 2) return '—';
    const d = hist[n-1] - hist[n-2];
    if(Math.abs(d) < 0.05) return '—';
    return d > 0 ? `▲ ${d.toFixed(1)}` : `▼ ${Math.abs(d).toFixed(1)}`;
  }

  function tokenColor(name){
    return getComputedStyle(document.documentElement).getPropertyValue(name).trim();
  }

  function drawSpark(canvas, data, colorHex){
    if(data.length < 2) return;
    const dpr = window.devicePixelRatio || 1;
    const w = canvas.clientWidth, h = canvas.clientHeight;
    if(w === 0 || h === 0) return;
    canvas.width = w*dpr; canvas.height = h*dpr;
    const ctx = canvas.getContext('2d');
    ctx.setTransform(dpr,0,0,dpr,0,0);
    ctx.clearRect(0,0,w,h);

    const min = Math.min(...data), max = Math.max(...data);
    const range = (max - min) || 1;
    const padX = 3, padY = 6;
    const stepX = (w - padX*2) / (data.length - 1);
    const pts = data.map((v,i) => [padX + i*stepX, h - padY - ((v - min)/range) * (h - padY*2)]);

    ctx.strokeStyle = 'rgba(128,128,128,0.18)';
    ctx.lineWidth = 1;
    ctx.beginPath(); ctx.moveTo(0, h/2); ctx.lineTo(w, h/2); ctx.stroke();

    const grad = ctx.createLinearGradient(0,0,0,h);
    grad.addColorStop(0, colorHex + '4d');
    grad.addColorStop(1, colorHex + '00');
    ctx.beginPath();
    ctx.moveTo(pts[0][0], h - padY);
    pts.forEach(p => ctx.lineTo(p[0], p[1]));
    ctx.lineTo(pts[pts.length-1][0], h - padY);
    ctx.closePath();
    ctx.fillStyle = grad;
    ctx.fill();

    ctx.beginPath();
    pts.forEach((p,i) => i === 0 ? ctx.moveTo(p[0],p[1]) : ctx.lineTo(p[0],p[1]));
    ctx.strokeStyle = colorHex;
    ctx.lineWidth = 2; ctx.lineJoin = 'round'; ctx.lineCap = 'round';
    ctx.stroke();

    const last = pts[pts.length-1];
    ctx.beginPath(); ctx.arc(last[0], last[1], 6, 0, Math.PI*2);
    ctx.fillStyle = colorHex + '33'; ctx.fill();
    ctx.beginPath(); ctx.arc(last[0], last[1], 3, 0, Math.PI*2);
    ctx.fillStyle = colorHex; ctx.fill();
  }

  function setToggle(switchId, pillId, isOn, onText, offText){
    const sw = document.getElementById(switchId);
    const pill = document.getElementById(pillId);
    if(document.activeElement !== sw) sw.checked = isOn;
    pill.textContent = isOn ? onText : offText;
    pill.classList.toggle('on', isOn);
  }

  function setOnline(isOnline){
    const chip = document.getElementById('statusChip');
    chip.classList.toggle('online', isOnline);
    chip.classList.toggle('offline', !isOnline);
    document.getElementById('statusText').textContent = isOnline ? 'SISTEMA ONLINE' : 'SISTEMA OFFLINE';
  }

  async function refresh(){
    if(!state.user) return; // sem credenciais salvas ainda: não tenta buscar (evita popup de login em loop)
    try{
      const res = await fetch('/data', { cache:'no-store', headers:{ Authorization: authHeader() } });
      if(res.status === 401) throw new Error('unauthorized');
      const d = await res.json();

      document.getElementById('v-temp').textContent = d.temperatura >= 0 ? d.temperatura.toFixed(1) : '--';
      document.getElementById('v-hum').textContent = d.umidade_ar >= 0 ? d.umidade_ar.toFixed(0) : '--';
      document.getElementById('n-temp').textContent = d.temperatura >= 0 ? d.temperatura.toFixed(1) : '--';
      document.getElementById('n-hum').textContent = d.umidade_ar >= 0 ? d.umidade_ar.toFixed(0) : '--';
      if(d.temperatura >= 0) { pushHist('temp', d.temperatura); document.getElementById('t-temp').textContent = trendArrow(state.hist.temp); }
      if(d.umidade_ar >= 0) { pushHist('hum', d.umidade_ar); document.getElementById('t-hum').textContent = trendArrow(state.hist.hum); }

      document.getElementById('n-soil1').textContent = d.solo1;
      document.getElementById('n-soil2').textContent = d.solo2;
      pushHist('soil1', d.solo1); pushHist('soil2', d.solo2);
      document.getElementById('t-soil1').textContent = trendArrow(state.hist.soil1);
      document.getElementById('t-soil2').textContent = trendArrow(state.hist.soil2);
      state.threshold = d.limite;
      document.getElementById('r-soil1').classList.toggle('warn', d.solo1 < d.limite);
      document.getElementById('r-soil2').classList.toggle('warn', d.solo2 < d.limite);

      const pumpVital = document.getElementById('v-pump-wrap');
      document.getElementById('v-pump').textContent = d.bomba ? 'LIGADA' : 'DESLIGADA';
      pumpVital.classList.add('state'); pumpVital.classList.toggle('on', d.bomba);
      const uvVital = document.getElementById('v-uv-wrap');
      document.getElementById('v-uv').textContent = d.uv ? 'LIGADA' : 'DESLIGADA';
      uvVital.classList.add('state'); uvVital.classList.toggle('on', d.uv);

      setToggle('pumpSwitch','pumpPill', d.bomba, 'LIGADA','DESLIGADA');
      setToggle('uvSwitch','uvPill', d.uv, 'LIGADA','DESLIGADA');
      document.getElementById('autoSwitch').checked = d.auto;
      document.getElementById('thRange').value = d.limite;
      document.getElementById('thLabel').textContent = d.limite + '%';

      const statusText = document.getElementById('pumpStatusText');
      if(!d.auto){ statusText.textContent = 'modo automático desligado'; statusText.classList.remove('active'); }
      else if(d.bomba){ statusText.textContent = 'irrigando agora…'; statusText.classList.add('active'); }
      else { statusText.textContent = 'monitorando solo…'; statusText.classList.remove('active'); }

      if(d.uv) state.uvSeconds += 2;
      document.getElementById('uvRuntime').textContent = fmtClock(state.uvSeconds);

      drawSpark(document.getElementById('s-temp'), state.hist.temp, tokenColor('--accent'));
      drawSpark(document.getElementById('s-hum'), state.hist.hum, tokenColor('--accent'));
      drawSpark(document.getElementById('s-soil1'), state.hist.soil1, d.solo1 < d.limite ? tokenColor('--warn') : tokenColor('--accent'));
      drawSpark(document.getElementById('s-soil2'), state.hist.soil2, d.solo2 < d.limite ? tokenColor('--warn') : tokenColor('--accent'));

      setOnline(true);
      offlineCount = 0;
    }catch(e){
      offlineCount++;
      if(offlineCount > 1) setOnline(false);
    }
  }

  // ---------------- câmera (stream real) ----------------
  const camFrame = document.getElementById('camFrame');
  const camStream = document.getElementById('camStream');

  function streamHost(){
    if(state.streamBase) return state.streamBase.replace(/\/$/, '');
    return `${location.protocol}//${location.hostname}:81`;
  }

  // Navegadores modernos bloqueiam credenciais embutidas na URL
  // (https://user:pass@host) para recursos de outra origem (a porta 81
  // conta como origem diferente da 80) — por segurança contra phishing.
  // O navegador vai pedir o login nativamente (uma vez, cacheia depois).
  function setCamOnline(){ camFrame.classList.remove('offline'); }
  function setCamOffline(){
    camFrame.classList.add('offline');
    clearTimeout(camRetryTimer);
    camRetryTimer = setTimeout(connectCam, 4000);
  }

  function connectCam(){
    clearTimeout(camWatchdog); clearTimeout(camRetryTimer);
    if(!state.user){ setCamOffline(); return; }
    camWatchdog = setTimeout(setCamOffline, 8000);
    camStream.src = streamHost() + '/stream?_=' + Date.now();
  }

  camStream.addEventListener('load', () => { clearTimeout(camWatchdog); setCamOnline(); });
  camStream.addEventListener('error', () => { clearTimeout(camWatchdog); setCamOffline(); });

  // ---------------- controles ----------------
  document.getElementById('pumpSwitch').addEventListener('change', e => {
    fetch('/pump?state=' + (e.target.checked ? 'on':'off'), { headers:{ Authorization: authHeader() } });
  });
  document.getElementById('uvSwitch').addEventListener('change', e => {
    fetch('/uv?state=' + (e.target.checked ? 'on':'off'), { headers:{ Authorization: authHeader() } });
  });
  document.getElementById('autoSwitch').addEventListener('change', e => {
    fetch('/auto?state=' + (e.target.checked ? 'on':'off'), { headers:{ Authorization: authHeader() } });
  });
  document.getElementById('thRange').addEventListener('change', e => {
    document.getElementById('thLabel').textContent = e.target.value + '%';
    fetch('/threshold?value=' + e.target.value, { headers:{ Authorization: authHeader() } });
  });

  // ---------------- configurações de acesso ----------------
  const settingsOverlay = document.getElementById('settingsOverlay');
  function openSettings(){
    document.getElementById('inUser').value = state.user;
    document.getElementById('inPass').value = state.pass;
    document.getElementById('inStream').value = state.streamBase;
    settingsOverlay.hidden = false;
  }
  function closeSettings(){ settingsOverlay.hidden = true; }

  document.getElementById('settingsBtn').addEventListener('click', openSettings);
  document.getElementById('settingsCancel').addEventListener('click', closeSettings);
  document.getElementById('settingsSave').addEventListener('click', () => {
    state.user = document.getElementById('inUser').value.trim();
    state.pass = document.getElementById('inPass').value;
    state.streamBase = document.getElementById('inStream').value.trim();
    localStorage.setItem('estufaUser', state.user);
    localStorage.setItem('estufaPass', state.pass);
    localStorage.setItem('estufaStreamBase', state.streamBase);
    closeSettings();
    connectCam();
    refresh();
  });

  // ---------------- relógios ----------------
  const startTime = performance.now();
  function updateClocks(){
    const elapsed = Math.floor((performance.now() - startTime)/1000);
    document.getElementById('sessionClock').textContent = fmtClock(elapsed);
    document.getElementById('camClock').textContent = new Date().toLocaleTimeString('pt-BR');
  }

  window.addEventListener('resize', () => {
    drawSpark(document.getElementById('s-temp'), state.hist.temp, tokenColor('--accent'));
    drawSpark(document.getElementById('s-hum'), state.hist.hum, tokenColor('--accent'));
    drawSpark(document.getElementById('s-soil1'), state.hist.soil1, tokenColor('--accent'));
    drawSpark(document.getElementById('s-soil2'), state.hist.soil2, tokenColor('--accent'));
  });

  if(!state.user) openSettings();
  connectCam();
  refresh();
  updateClocks();
  setInterval(updateClocks, 1000);
  setInterval(refresh, 2000);
})();
</script>
</body>
</html>
)HTMLPAGE";
