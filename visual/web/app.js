'use strict';
let current = null;
let view = 'path';
let autoTimer = null;
let busy = false;
const $ = (id) => document.getElementById(id);
const fmt = (point) => `(${point[0]}, ${point[1]})`;
const setText = (id, text) => { $(id).textContent = String(text); };
function toast(message){const t=$('toast');t.textContent=message;t.classList.add('show');clearTimeout(toast.timer);toast.timer=setTimeout(()=>t.classList.remove('show'),4200);}

async function request(command){
  if(busy) return;
  busy=true;
  try{
    const response=await fetch('/api/action',{
      method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({command})
    });
    const data=await response.json();
    if(!response.ok || !data.ok) throw new Error(data.error || `Server returned ${response.status}`);
    current=data;
    render();
  }catch(e){toast(e.message);stopAuto();}finally{busy=false;}
}
async function initialize(){
  try{
    const response=await fetch('/api/state');
    if(!response.ok)throw new Error('C++ engine unavailable');
    const state=await response.json();
    if(!state.ok)throw new Error(state.error);
    current=state;render();setText('connection','C++ ENGINE CONNECTED');
  }catch(error){setText('connection','ENGINE OFFLINE');toast('Could not connect to the local simulator. Run python visual/server.py.');}
}
function render(){
  if(!current)return;
  setText('tickTop',`TICK ${String(current.tick).padStart(3,'0')}`);
  setText('simTime',`${current.tick*5} S`);
  setText('mapState',`DOOR / ${current.world.doorOpen?'OPEN':'CLOSED'} · SPILL / ${current.world.hazardOn?'ON':'OFF'}`);
  setText('hazardBtnText',current.world.hazardOn?'Clear chemical spill':'Activate chemical spill');
  setText('doorBtnText',current.world.doorOpen?'Close door':'Open door');
  setText('controlAdvice',current.lastEvent);
  renderMap(); renderTelemetry();renderInputs();renderEvents();
}
function renderMap(){
  const {world,player,security,hollow,trooper,belief}=current;
  const grid=$('mapGrid');
  grid.replaceChildren();
  const entities=[{p:player,id:'P',className:'player-color',title:'Player'},{p:security,id:'S',className:'security-color',title:'Security GOAP'},{p:hollow,id:'H',className:'hollow-color',title:'Hollow belief'},{p:trooper,id:'T',className:'trooper-color',title:'Trooper D* Lite'}];
  const groups=new Map();
  for(const e of entities){const k=e.p.join(',');if(!groups.has(k))groups.set(k,[]);groups.get(k).push(e);}
  const fragment=document.createDocumentFragment();
  let best=Math.max(...belief,0.00001);
  for(let y=0;y<world.height;y++)for(let x=0;x<world.width;x++){
    const cell=document.createElement('div');
    const wall=world.rows[y][x]==='#';const door=x===world.door[0]&&y===world.door[1];const hazard=x===world.hazard[0]&&y===world.hazard[1]&&world.hazardOn;
    cell.className=`tile ${wall?'wall':'floor'}${door?' door '+(world.doorOpen?'open':'closed'):''}${hazard?' hazard':''}`;
    const prob=belief[y*world.width+x];
    if(view==='belief'&&!wall){const strength=Math.sqrt(prob/best);cell.style.setProperty('--heat',`rgba(217,76,115,${Math.max(0.015,Math.min(.77,strength*.72))})`);}
    const key=`${x},${y}`;
    const at=groups.get(key);
    if(at){const sym=document.createElement('div');sym.className=`agent-symbol ${at[0].className}`;sym.textContent=at[0].id;
      if(at.length>1){sym.dataset.stack=`+${at.length-1}`;sym.title=at.map(a=>a.title).join(', ');}cell.append(sym);}
    cell.title=`(${x}, ${y}) · ${wall?'Wall':door?'Containment door':hazard?'Chemical spill':'Floor'}${view==='belief'?` · ${(prob*100).toFixed(1)}% belief`:''}`;
    cell.addEventListener('mouseenter',()=>setText('hoverInfo',`X ${String(x).padStart(2,'0')} / Y ${String(y).padStart(2,'0')}${view==='belief'?` · ${(prob*100).toFixed(2)}%`:''}`));
    if(!wall)cell.addEventListener('click',()=>request(`PLAYER ${x} ${y}`));
    fragment.append(cell);
  }
  grid.append(fragment);
  const line=$('routeLine');
  if(view==='path'&&current.trooperPath.length>0){
    line.setAttribute('points',current.trooperPath.map(([x,y])=>`${x+.5},${y+.5}`).join(' '));
    $('routeLayer').style.display='block';
  }else{$('routeLayer').style.display='none';}
}
function renderTelemetry(){
  const {securityAI, director, hollowTarget, hollow, trooperPath,trooperExpansions}=current;
  setText('goapAction',securityAI.lastAction||'Observe');
  setText('securityStatus',`Guard HP ${Math.round(securityAI.health*100)}% · Ammo ${securityAI.ammo} · Target ${securityAI.knowsTarget?'CONFIRMED':'UNKNOWN'}`);
  setText('goapCost',securityAI.planCost.toFixed(1));
  setText('goapExpanded',securityAI.expanded);
  const plan=$('goapPlan');plan.replaceChildren();
  (securityAI.steps.length?securityAI.steps:['No pending actions']).forEach((p,i)=>{
    if(i){const arrow=document.createElement('span');arrow.className='plan-arrow';arrow.textContent='→';plan.append(arrow);}
    const step=document.createElement('span');step.className='plan-step';step.textContent=p;plan.append(step);
  });
  setText('hollowTarget',fmt(hollowTarget));setText('hollowLocation',fmt(hollow));
  setText('pathLen',trooperPath.length||'NO PATH');setText('pathExpanded',trooperExpansions);
  setText('trooperState',trooperPath.length>0?'Orange dashed line = planned D* Lite route. Toggle the door to compare.':'Goal unreachable. Open the door or move the player.');
  setText('directorChoice',director.chosen);
  setText('directorWaveCount',`${current.state.activeHollows} active Hollow wave(s) · ${Math.round(current.state.cooldown)} s since last wave`);
  const bars=$('utilityBars');bars.replaceChildren();
  for(const sc of director.scores){
    const el=document.createElement('div');el.className=`utility-row${sc.allowed?'':' blocked'}`;el.title=sc.reason;
    const head=document.createElement('div');head.className='utility-row-head';
    const label=document.createElement('span');label.textContent=sc.name;
    const value=document.createElement('b');value.textContent=sc.allowed?sc.utility.toFixed(3):'BLOCKED';
    head.append(label,value);const track=document.createElement('div');track.className='bar-track';const bar=document.createElement('div');bar.className='bar-fill';bar.style.width=`${Math.min(sc.utility,1)*100}%`;track.append(bar);el.append(head,track);bars.append(el);
  }
}
function renderInputs(){
  const state=current.state;
  const vals={health:Math.round(state.health*100),ammo:state.ammo,exposure:Math.round(state.exposure*100),stress:Math.round(state.stress*100),securityHealth:Math.round(current.securityAI.health*100),securityAmmo:current.securityAI.ammo};
  for(const [key,value] of Object.entries(vals)){
    const input=$(`${key}Slider`);
    if(document.activeElement!==input)input.value=value;
    input.style.setProperty('--range',`${((Number(input.value)-Number(input.min))/(Number(input.max)-Number(input.min)))*100}%`);
    setText(`${key}Value`,key.toLowerCase().includes('ammo')?value:`${value}%`);
  }
  $('safeToggle').checked=state.safeZone;
}
function renderEvents(){
  const log=$('eventLog');log.replaceChildren();
  for(const str of current.history){
    const entry=document.createElement('div');entry.className='event';
    const match=str.match(/^(T\d+) · (.*)$/);
    const t=document.createElement('span');t.className='event-index';t.textContent=match?match[1]:'LOG';
    const msg=document.createElement('span');msg.textContent=match?match[2]:str;
    entry.append(t,msg);log.append(entry);
  }
}
function stopAuto(){if(autoTimer){clearInterval(autoTimer);autoTimer=null;}$('autoplayBtn').classList.remove('running');setText('autoStatus','OFF');}
function toggleAuto(){
  if(autoTimer){stopAuto();return;}
  $('autoplayBtn').classList.add('running');setText('autoStatus','ON');
  autoTimer=setInterval(()=>{if(!busy)request('STEP');},1200);
}
$('stepBtn').addEventListener('click',()=>request('STEP'));
$('gunshotBtn').addEventListener('click',()=>request('GUNSHOT'));
$('spotBtn').addEventListener('click',()=>request('SPOT'));
$('doorBtn').addEventListener('click',()=>request('DOOR'));
$('hazardBtn').addEventListener('click',()=>request('HAZARD'));
$('lostBtn').addEventListener('click',()=>request('LOST'));
$('resetBtn').addEventListener('click',()=>{stopAuto();request('RESET');});
$('autoplayBtn').addEventListener('click',toggleAuto);
$('safeToggle').addEventListener('change',event=>request(`SAFE ${event.target.checked?1:0}`));
for(const input of document.querySelectorAll('input[type="range"]')){
  input.addEventListener('input',()=>{const value=Number(input.value);setText(`${input.dataset.setting}Value`,input.dataset.setting.toLowerCase().includes('ammo')?value:`${value}%`);input.style.setProperty('--range',`${(value/Number(input.max))*100}%`);});
  input.addEventListener('change',()=>{const key=input.dataset.setting;const val=key.toLowerCase().includes('ammo')?Number(input.value):Number(input.value)/100;request(`SET ${key} ${val}`);});
}
for(const button of document.querySelectorAll('.mode')){
  button.addEventListener('click',()=>{view=button.dataset.view;document.querySelectorAll('.mode').forEach(b=>b.classList.toggle('active',b===button));renderMap();});
}
window.addEventListener('beforeunload',stopAuto);
initialize();
