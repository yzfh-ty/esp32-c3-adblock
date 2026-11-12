#pragma once
// Dashboard HTML for the C3 AdBlocker web UI, kept in its own header so the
// Arduino IDE preprocessor doesn't choke on the inlined markup (issue #6).

const char PAGE[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1">
<title>C3 AdBlock</title><style>
body{font:14px system-ui,sans-serif;margin:0;background:#0d1117;color:#c9d1d9}
header{background:#161b22;padding:14px 18px;border-bottom:1px solid #30363d}
h1{margin:0;font-size:18px}h1 span{color:#3fb950}.wrap{padding:16px;max-width:1000px;margin:auto}
.cards{display:flex;flex-wrap:wrap;gap:10px;margin-bottom:16px}
.card{background:#161b22;border:1px solid #30363d;border-radius:8px;padding:12px 16px;flex:1;min-width:120px}
.card .v{font-size:22px;font-weight:600}.card .l{color:#8b949e;font-size:12px}
table{width:100%;border-collapse:collapse;background:#161b22;border-radius:8px;overflow:hidden;margin-bottom:18px}
th,td{padding:8px 10px;text-align:left;border-bottom:1px solid #21262d;font-size:13px}
th{background:#21262d;color:#8b949e}tr:hover td{background:#1c2128}
.b{color:#f85149}.a{color:#3fb950}.tag{background:#30363d;border-radius:4px;padding:1px 6px;font-size:11px}
button{background:#21262d;color:#c9d1d9;border:1px solid #30363d;border-radius:5px;padding:4px 9px;cursor:pointer}
button:hover{background:#30363d}.ban{color:#f85149}input{background:#0d1117;border:1px solid #30363d;color:#c9d1d9;border-radius:5px;padding:6px}
h2{font-size:14px;color:#8b949e;margin:18px 0 8px}
</style></head><body>
<header><h1>🛡️ C3 AdBlock <span id=host></span></h1></header><div class=wrap>
<div id=credwarn style="display:none;background:#3b1d1d;border:1px solid #f85149;color:#ffb3ae;border-radius:8px;padding:10px 14px;margin-bottom:14px;font-size:13px">
⚠️ <b>正在使用默认密码。</b><code>secrets.h</code> 中的 WEB_PASS/OTA_PASS 仍是占位值——它们公开在仓库里，任何人都能读取。请设置真实密码后重新烧录。
</div>
<div id=blockbar style="display:flex;align-items:center;gap:12px;margin-bottom:14px;padding:12px 14px;background:#161b22;border:1px solid #30363d;border-radius:8px">
<span id=blockdot style=font-size:20px>🛡️</span><b id=blockstate style=flex:1 data-on=1>拦截运行中</b>
<select id=pausedur style="background:#0d1117;border:1px solid #30363d;color:#c9d1d9;border-radius:5px;padding:5px"><option value=30>30 秒</option><option value=300 selected>5 分钟</option><option value=1800>30 分钟</option><option value=0>直到我重新开启</option></select>
<button id=pausebtn onclick=togglePause()>暂停</button></div>
<div class=cards id=sys></div>
<h2>客户端</h2><table id=ct><thead><tr><th>客户端</th><th>MAC</th><th>已拦截</th><th>已放行</th><th></th></tr></thead><tbody></tbody></table>
<h2>自定义拦截域名</h2>
<div style=margin-bottom:8px><input id=dom placeholder="ads.example.com" size=30><button onclick=addDom()>拦截域名</button></div>
<table id=cl><tbody></tbody></table>
<h2>拦截列表 &mdash; 上传</h2>
<form id=upf style=margin-bottom:6px><input type=file id=blf accept=.bin><button>上传拦截列表</button> <span id=upmsg style=color:#8b949e></span></form>
<div style="color:#8b949e;font-size:12px;margin-bottom:18px">用 <code>tools/build_blocklist.py</code> 生成 <code>blocklist.bin</code>，然后在这里上传——无需 USB</div>
<h2>拦截列表 &mdash; 远程自动更新</h2>
<div style=margin-bottom:6px><input id=uurl placeholder="https://host/blocklist.bin" size=40> 每隔 <input id=uiv size=2 value=24> 小时
<button onclick=saveUpd()>保存</button> <button onclick=fetchNow()>立即拉取</button></div>
<div style="color:#8b949e;font-size:12px;margin-bottom:18px">设备会按计划拉取预生成的 <code>blocklist.bin</code>（例如 GitHub Release 附件）。上次更新：<span id=ustat>&mdash;</span></div>
<h2>固件 &mdash; OTA 更新</h2>
<form id=fwf style=margin-bottom:6px><input type=file id=fwb accept=.bin><button>刷写固件</button> <span id=fwmsg style=color:#8b949e></span></form>
<div style="color:#8b949e;font-size:12px;margin-bottom:18px">上传 <code>.pio/build/&lt;env&gt;/firmware.bin</code>（<code>c3</code>、<code>esp32dev</code> …）——设备会校验并重启进入新固件</div>
<h2>上游 DNS &mdash; 转发</h2>
<div style=margin-bottom:6px><input id=dnsip placeholder="9.9.9.9" size=18>
<button onclick=saveDns()>保存</button> <button onclick="presetDns('1.1.1.1')">Cloudflare</button> <button onclick="presetDns('94.140.14.14')">AdGuard</button> <button onclick="dnsMode('router')">使用路由器的 DNS</button> <button onclick="dnsMode('reset')">默认 (Quad9)</button> <span id=dnsmsg style=color:#8b949e></span></div>
<div style="color:#8b949e;font-size:12px;margin-bottom:18px">放行查询会转发到哪里。如果这台设备是路由器分配的 Pi-hole/AdGuard 的<i>备用</i>，请保持默认——否则恰好在需要备用时会转发失败。</div>
<h2>WIFI</h2>
<div style=margin-bottom:18px><button onclick="if(confirm('忘记已保存的 WiFi 并重启进入配置门户？'))forgetWifi()">忘记 WiFi</button></div>
</div><script>
function fmt(n){return n.toLocaleString()}
function esc(s){return String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]))}
// A plain <img>/<form> CSRF can't set a custom header, only same-origin fetch()
// can — so requiring this on every mutating request blocks drive-by CSRF even
// though the server can't otherwise tell a forged request from a real one over
// plain HTTP Basic Auth (browsers auto-replay cached Basic Auth cross-origin).
const CSRF_HDRS={'X-Requested-With':'c3-adblock'}
function togglePause(){fetch(blockstate.dataset.on=='1'?'/pause?s='+pausedur.value:'/resume',{headers:CSRF_HDRS}).then(load);}
async function load(){let s=await(await fetch('/stats.json')).json();
host.textContent='@ '+s.ip;
credwarn.style.display=s.defcreds?'block':'none';
let on=s.blocking!==false;blockstate.dataset.on=on?'1':'0';
blockdot.textContent=on?'🛡️':'⏸️';blockbar.style.borderColor=on?'#30363d':'#f0883e';
blockstate.textContent=on?'拦截运行中':(s.resumeIn>0?'已暂停 — '+s.resumeIn+' 秒后恢复':'已暂停');
pausebtn.textContent=on?'暂停':'恢复';pausedur.style.display=on?'':'none';
sys.innerHTML=[['累计拦截',fmt(s.blocked),'b'],['累计放行',fmt(s.allowed),'a'],['列表域名',fmt(s.domains)+' 条',''],
['客户端',s.clients.length,''],['WiFi 信号',s.rssi+' dBm',''],['温度',s.temp+' °C',''],['空闲内存',Math.round(s.heap/1024)+' KB',''],['运行时间',s.uptime,'']]
.map(c=>`<div class=card><div class="v ${c[2]}">${c[1]}</div><div class=l>${c[0]}</div></div>`).join('');
ct.tBodies[0].innerHTML=s.clients.sort((a,b)=>(b.blocked+b.allowed)-(a.blocked+a.allowed)).map(c=>
`<tr><td>${c.ip}${c.banned?' <span class=tag style=color:#f85149>已封禁</span>':''}</td><td>${c.mac}</td>
<td class=b>${fmt(c.blocked)}</td><td class=a>${fmt(c.allowed)}</td>
<td><button class=ban data-ip="${c.ip}">${c.banned?'解除封禁':'封禁'}</button></td></tr>`).join('');
cl.tBodies[0].innerHTML=s.custom.map(d=>`<tr><td>${esc(d)}</td><td style=text-align:right><button class=rmbtn data-d="${esc(d)}">移除</button></td></tr>`).join('')||'<tr><td style=color:#8b949e>暂无</td></tr>';
if(document.activeElement!=uurl)uurl.value=s.upurl||'';
if(document.activeElement!=uiv)uiv.value=s.upiv||24;
if(document.activeElement!=dnsip)dnsip.value=s.dns||'';
dnsmsg.textContent=s.dnsSource||'';
ustat.textContent=s.upstat||'—';}
function addDom(){let d=dom.value.trim();if(d){fetch('/addblock?d='+encodeURIComponent(d),{headers:CSRF_HDRS}).then(()=>{dom.value='';load()})}}
ct.addEventListener('click',e=>{if(e.target.classList.contains('ban'))fetch('/ban?ip='+e.target.dataset.ip,{headers:CSRF_HDRS}).then(load)});
cl.addEventListener('click',e=>{if(e.target.classList.contains('rmbtn'))fetch('/unblock?d='+encodeURIComponent(e.target.dataset.d),{headers:CSRF_HDRS}).then(load)});
function saveUpd(){fetch('/setupdate?u='+encodeURIComponent(uurl.value.trim())+'&h='+(parseInt(uiv.value)||24),{headers:CSRF_HDRS}).then(load)}
function fetchNow(){ustat.textContent='拉取中...';fetch('/fetchnow',{headers:CSRF_HDRS}).then(r=>r.text()).then(t=>{ustat.textContent=t;load()})}
function forgetWifi(){fetch('/forgetwifi',{headers:CSRF_HDRS}).then(r=>r.text()).then(t=>alert(t))}
function saveDns(){let v=dnsip.value.trim();if(!v)return;fetch('/setdns?ip='+encodeURIComponent(v),{headers:CSRF_HDRS}).then(async r=>{if(!r.ok)alert('✗ '+await r.text());load()})}
function presetDns(ip){dnsip.value=ip;saveDns()}
function dnsMode(m){fetch('/setdns?'+m+'=1',{headers:CSRF_HDRS}).then(load)}
fwf.onsubmit=async e=>{e.preventDefault();let f=fwb.files[0];if(!f)return;fwmsg.textContent='正在刷写 '+(f.size/1048576).toFixed(2)+' MB...';
let fd=new FormData();fd.append('f',f);
try{let r=await fetch('/update',{method:'POST',headers:CSRF_HDRS,body:fd});fwmsg.textContent=r.ok?'✓ 正在重启，约 15 秒后重新连接':'✗ '+await r.text();}
catch(_){fwmsg.textContent='✓ 正在重启，约 15 秒后重新连接';}};
upf.onsubmit=async e=>{e.preventDefault();let f=blf.files[0];if(!f)return;
upmsg.textContent='正在上传 '+(f.size/1048576).toFixed(2)+' MB...';
let fd=new FormData();fd.append('f',f);
try{let r=await fetch('/upload',{method:'POST',headers:CSRF_HDRS,body:fd});upmsg.textContent=r.ok?'✓ 已更新':'✗ '+await r.text();}
catch(_){upmsg.textContent='✗ 上传失败';}
blf.value='';setTimeout(load,600);};
load();setInterval(load,3000);
</script></body></html>)HTML";
