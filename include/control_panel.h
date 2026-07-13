#ifndef MOSS_CONTROL_PANEL_H
#define MOSS_CONTROL_PANEL_H

const char CONTROL_PANEL_HTML[] PROGMEM = R"MOSSHTML(<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MOSS Motor Control</title>
<style>
:root{color-scheme:dark;--bg:#101412;--panel:#1a211d;--line:#344139;--text:#f2f5f3;--muted:#a5b0aa;--green:#53d98c;--red:#ff6b6b;--amber:#f4c95d}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font-family:Arial,"Microsoft YaHei",sans-serif;font-size:15px}
header{border-bottom:1px solid var(--line);padding:18px 20px;background:#141a17;position:sticky;top:0;z-index:2}
.head{max-width:920px;margin:auto;display:flex;align-items:center;justify-content:space-between;gap:16px}.brand{font-size:20px;font-weight:700}.brand span{color:var(--green)}
.online{font-size:13px;color:var(--muted)}main{max-width:920px;margin:0 auto;padding:20px}.toolbar{display:flex;gap:10px;margin-bottom:18px}
button{border:1px solid var(--line);background:#222c27;color:var(--text);min-height:42px;padding:8px 14px;border-radius:6px;cursor:pointer;font-weight:600}
button:hover{border-color:var(--green)}button:disabled{opacity:.45;cursor:not-allowed}.danger{background:#542626;border-color:#8e3d3d;color:#fff}.danger:hover{border-color:var(--red)}
.axes{display:grid;grid-template-columns:1fr 1fr;gap:16px}.axis{border:1px solid var(--line);background:var(--panel);border-radius:6px;padding:18px;min-width:0}
.axis-head{display:flex;justify-content:space-between;align-items:start;margin-bottom:18px}.axis h2{margin:0;font-size:18px}.position{font:700 28px Consolas,monospace;color:var(--green)}.state{font-size:12px;color:var(--muted);text-align:right}
.jog{display:grid;grid-template-columns:repeat(5,minmax(0,1fr));gap:8px;margin-bottom:18px}.jog button{font-size:17px;padding:6px;min-width:0}.jog .axis-stop{color:var(--red)}
.capture{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:8px;margin-bottom:18px}.capture button{font-size:13px;padding:7px;min-width:0}.capture .center{border-color:#617167;color:var(--green)}
.fields{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:8px}.field{min-width:0}.field label{display:block;color:var(--muted);font-size:12px;margin-bottom:5px}.field input{width:100%;min-width:0;height:40px;border:1px solid var(--line);background:#0e1210;color:var(--text);border-radius:4px;padding:8px;font:15px Consolas,monospace}
.settings{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-top:14px}.toggle{display:flex;align-items:center;gap:8px;color:var(--muted)}.toggle input{width:18px;height:18px;accent-color:var(--green)}
.range{height:8px;background:#0d110f;border:1px solid var(--line);margin:18px 0 8px;position:relative}.range i{position:absolute;width:3px;height:14px;background:var(--green);top:-4px;left:50%}.range-labels{display:flex;justify-content:space-between;color:var(--muted);font:12px Consolas,monospace}
#notice{min-height:22px;margin-top:16px;color:var(--amber)}#notice.error{color:var(--red)}
@media(max-width:700px){.axes{grid-template-columns:minmax(0,1fr)}.head{align-items:flex-start}.brand{font-size:17px}main{padding:14px}.axis{padding:14px}.jog,.capture,.fields{gap:6px}.jog button{padding:4px;font-size:15px}.capture button{padding:5px 2px;font-size:12px}}
</style>
</head>
<body>
<header><div class="head"><div class="brand"><span>MOSS</span> // MOTOR CALIBRATION</div><div id="connection" class="online">CONNECTING</div></div></header>
<main>
<div class="toolbar"><button type="button" onclick="refreshAll()" title="Refresh status">Refresh</button><button type="button" class="danger" onclick="stopMotor('all')" title="Emergency stop">Stop all</button></div>
<div class="axes" id="axes"></div><div id="notice"></div>
</main>
<script>
const axisNames=['pan','tilt'];
const labels=['PAN','TILT'];
const axes=document.getElementById('axes');

function axisTemplate(axis){return `<section class="axis" id="axis-${axis}">
  <div class="axis-head"><div><h2>${labels[axis]}</h2><div class="state" id="state-${axis}">IDLE</div></div><div class="position" id="pos-${axis}">0</div></div>
  <div class="jog">
    <button onclick="jog(${axis},-50)" title="Move negative 50 steps">&laquo;</button>
    <button onclick="jog(${axis},-10)" title="Move negative 10 steps">&#8592;</button>
    <button class="axis-stop" onclick="stopMotor('${axisNames[axis]}')" title="Stop and release">&#9632;</button>
    <button onclick="jog(${axis},10)" title="Move positive 10 steps">&#8594;</button>
    <button onclick="jog(${axis},50)" title="Move positive 50 steps">&raquo;</button>
  </div>
  <div class="capture"><button onclick="captureLimit(${axis},'min')">Set min</button><button class="center" onclick="captureLimit(${axis},'center')">Set center</button><button onclick="captureLimit(${axis},'max')">Set max</button></div>
  <div class="range"><i></i></div><div class="range-labels"><span id="range-min-${axis}">0</span><span>ESTIMATED</span><span id="range-max-${axis}">0</span></div>
  <div class="fields"><div class="field"><label>MIN</label><input type="number" id="min-${axis}"></div><div class="field"><label>CENTER</label><input type="number" id="center-${axis}"></div><div class="field"><label>MAX</label><input type="number" id="max-${axis}"></div></div>
  <div class="settings"><label class="toggle"><input type="checkbox" id="reversed-${axis}"> Reverse direction</label><button onclick="saveAxis(${axis})">Save</button></div>
</section>`}
axes.innerHTML=axisTemplate(0)+axisTemplate(1);

async function request(path,options){const response=await fetch(path,options);let data={};try{data=await response.json()}catch(e){}if(!response.ok)throw new Error(data.error||`HTTP ${response.status}`);return data}
function body(data){return{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)}}
function notice(message,error=false){const el=document.getElementById('notice');el.textContent=message;el.className=error?'error':''}

async function refreshAll(){try{const status=await request('/api/v1/motor/status');axisNames.forEach((name,axis)=>render(axis,status[name]));document.getElementById('connection').textContent='ONLINE'}catch(error){document.getElementById('connection').textContent='OFFLINE';notice(error.message,true)}}
function setIfIdle(id,value){const el=document.getElementById(id);if(document.activeElement!==el)el.value=value}
function render(axis,data){document.getElementById(`pos-${axis}`).textContent=data.pos;document.getElementById(`state-${axis}`).textContent=data.busy?'MOVING':'IDLE';setIfIdle(`min-${axis}`,data.min);setIfIdle(`center-${axis}`,data.center);setIfIdle(`max-${axis}`,data.max);const reversed=document.getElementById(`reversed-${axis}`);if(document.activeElement!==reversed)reversed.checked=data.reversed;document.getElementById(`range-min-${axis}`).textContent=data.min;document.getElementById(`range-max-${axis}`).textContent=data.max}
async function jog(axis,steps){try{await request('/api/v1/motor/relative',body({motor:axis,steps,speed:8,accel:true}));notice(`${labels[axis]} command accepted`);setTimeout(refreshAll,120)}catch(error){notice(error.message,true)}}
async function stopMotor(motor){try{await request('/api/v1/motor/stop',body({motor}));notice(`${motor.toUpperCase()} stopped and released`);setTimeout(refreshAll,120)}catch(error){notice(error.message,true)}}
async function captureLimit(axis,limit){try{await request('/api/v1/motor/limit/capture',body({motor:axis,limit}));notice(`${labels[axis]} ${limit} captured`);await refreshAll()}catch(error){notice(error.message,true)}}
async function saveAxis(axis){const config={motor:axis,min:Number(document.getElementById(`min-${axis}`).value),center:Number(document.getElementById(`center-${axis}`).value),max:Number(document.getElementById(`max-${axis}`).value),reversed:document.getElementById(`reversed-${axis}`).checked};try{await request('/api/v1/motor/config',body(config));notice(`${labels[axis]} configuration saved`);await refreshAll()}catch(error){notice(error.message,true)}}
refreshAll();setInterval(refreshAll,1000);
</script>
</body>
</html>)MOSSHTML";

#endif
