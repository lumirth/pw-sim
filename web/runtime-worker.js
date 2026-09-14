import {createPhysics,advancePhysics,stepPhysics,pose,DT,resetPhysics,defaults} from './physics.js';
let module,e,physics=createPhysics(),paused=false,ready=false,clock=0,debt=0,nextFrame=0,nextPose=0,keys=0,compute=0,run=0,label='',serial=0,pressedAt=-1,pendingRelease=null,speed=1,substep=0;
let counters=new Uint32Array(512),history=[],loadedBytes,imports=[],historyID=0,lastHistory=0,selectedHistory=null,audioRead=0;
const u8=(p,n)=>new Uint8Array(e.memory.buffer,p,n);
const u32=(p,n)=>new Uint32Array(e.memory.buffer,p,n);
const stats=()=>Array.from(u32(e.pw_stats(),34));
function emit(){
 if(!ready)return;
 const counts=u32(e.pw_counts(),512).slice(),delta=counts.map((n,i)=>n-counters[i]);counters=counts;
 postMessage({type:'frame',serial,stats:stats(),pixels:u8(e.pw_pixels(),6144).slice(),counts,delta,work:u8(e.pw_workspace(),1802).slice(),viewBytes:u8(e.pw_view_bytes(),18).slice(),spectrum:new Uint16Array(e.memory.buffer,e.pw_spectrum(),32).slice(),samples:new Int8Array(e.memory.buffer,e.pw_ring_samples(),192).slice(),heat:u32(e.pw_read_heat(),512).slice(),writeHeat:u32(e.pw_write_heat(),512).slice(),pose:pose(physics),accel:physics.accel,physical:{contacts:physics.contacts,kinetic:physics.kinetic,tension:physics.tension,bendPower:physics.bendPower,av:physics.av},paused,label,speed,compute:run?compute/run:0,imports});
}
function capture(){return {memory:e.memory.buffer.slice(0),physics:structuredClone(physics),label,serial,substep,pendingRelease,pressedAt,keys,run};}
function historyNotice(){postMessage({type:'history',selected:selectedHistory,items:history.map(h=>({id:h.id,time:h.time,view:h.view,steps:h.steps}))});}
function remember(force=false){const s=stats();if(!force&&s[0]-lastHistory<1000)return;lastHistory=s[0];history.push({id:++historyID,time:s[0],view:s[5],steps:s[7],state:capture()});if(history.length>32)history.shift();historyNotice();}
function truncate(){if(selectedHistory!==null){const i=history.findIndex(h=>h.id===selectedHistory);if(i>=0)history=history.slice(0,i+1);selectedHistory=null;historyNotice();}}
function restore(state){u8(0,e.memory.buffer.byteLength).set(new Uint8Array(state.memory));physics=structuredClone(state.physics);label=state.label;substep=state.substep||0;pendingRelease=state.pendingRelease??null;pressedAt=state.pressedAt??-1;keys=state.keys||0;run=state.run||0;serial++;counters.fill(0);paused=true;debt=0;clock=performance.now();lastHistory=stats()[0];audioRead=e.pw_audio_head();postMessage({type:'audio-reset'});emit();postMessage({type:'pose',pose:pose(physics),paused});}
function emitAudio(from,initialPeriod,initialVolume){const head=e.pw_audio_head(),events=[];audioRead=Math.max(audioRead,head-1024);const table=u32(e.pw_audio_events(),4096);for(;audioRead<head;audioRead++){const i=(audioRead&1023)*4;events.push([table[i]+table[i+1]*4294967296,table[i+2],table[i+3]]);}postMessage({type:'audio',events,from,initialPeriod,initialVolume,period:e.pw_audio_period(),volume:stats()[33],speed,serial});}
function firmwareTick(){const from=e.pw_time_us(),initialPeriod=e.pw_audio_period(),initialVolume=stats()[33];e.pw_sensor(...physics.accel);const start=performance.now();e.pw_tick();if(pendingRelease!==null){e.pw_buttons(pendingRelease);pendingRelease=null;}compute+=performance.now()-start;run++;emitAudio(from,initialPeriod,initialVolume);remember();}
function loop(){
 const now=performance.now(),elapsed=now-clock;clock=now;
 if(ready&&!paused){debt+=Math.min(elapsed,250)*speed;let n=0;try{while(debt>=DT*1000&&n++<1024){stepPhysics(physics);debt-=DT*1000;if(++substep===16){substep=0;firmwareTick();}}}catch(error){paused=true;postMessage({type:'error',message:'Firmware stopped: '+error.message});}}
 if(ready&&now>=nextPose){postMessage({type:'pose',pose:pose(physics),paused});nextPose=now+(1000/60)-(now-nextPose)%(1000/60);}
 if(ready&&now>=nextFrame){emit();nextFrame=now+62.5-(now-nextFrame)%62.5;}
}
async function load(bytes,name){
 if(bytes.byteLength!==65536)throw Error('Select a 65,536-byte EEPROM file.');
 loadedBytes=new Uint8Array(bytes).slice();u8(e.pw_eeprom(),65536).set(loadedBytes);e.pw_boot();audioRead=0;postMessage({type:'audio-reset'});label=name;serial++;counters.fill(0);debt=compute=run=substep=0;pendingRelease=null;pressedAt=-1;keys=0;physics=createPhysics();clock=performance.now();nextFrame=clock;nextPose=clock;ready=true;paused=false;history=[];selectedHistory=null;lastHistory=0;remember(true);emit();
 postMessage({type:'loaded',label,physics});
}
onmessage=async({data:d})=>{try{
 switch(d.type){
 case 'init':{
 const bytes=await(await fetch('./firmware.wasm')).arrayBuffer();module=await WebAssembly.compile(bytes);imports=WebAssembly.Module.imports(module);({exports:e}=await WebAssembly.instantiate(module,{}));
 await load(await(await fetch('./fixtures/furret-eeprom-64KiB.bin')).arrayBuffer(),'Furret EEPROM');setInterval(loop,8);break;}
 case 'load':await load(d.bytes,d.label);break;
 case 'blank':await load(new Uint8Array(65536).fill(255).buffer,'Blank EEPROM');break;
 case 'buttons':truncate();if(d.value){keys=d.value;pressedAt=run;pendingRelease=null;e.pw_buttons(keys);}else if(pressedAt===run){keys=0;pendingRelease=0;}else{keys=0;e.pw_buttons(0);}break;
 case 'physics':truncate();Object.assign(physics,d.value);postMessage({type:'pose',pose:pose(physics),paused});break;
 case 'recenter':truncate();resetPhysics(physics,d.action||'position');postMessage({type:'pose',pose:pose(physics),paused});break;
 case 'defaults':truncate();Object.assign(physics,defaults);resetPhysics(physics);postMessage({type:'settings',physics});postMessage({type:'pose',pose:pose(physics),paused});break;
 case 'pause':postMessage({type:'audio-reset'});paused=d.value;if(!paused)truncate();clock=performance.now();debt=0;emit();break;
 case 'speed':postMessage({type:'audio-reset'});speed=Math.max(.125,Math.min(16,Number(d.value)||1));clock=performance.now();emit();break;
 case 'step':paused=true;truncate();do{stepPhysics(physics);substep++;}while(substep<16);substep=0;firmwareTick();emit();postMessage({type:'pose',pose:pose(physics),paused});break;
 case 'reset':await load(loadedBytes.buffer,label);break;
 case 'save':truncate();e.pw_save();postMessage({type:'save',bytes:u8(e.pw_eeprom(),65536).slice().buffer,label});emit();break;
 case 'raw':postMessage({type:'raw',bytes:u8(e.pw_eeprom(),65536).slice().buffer,label});break;
 case 'checkpoint':postMessage({type:'checkpoint',state:capture(),stats:stats(),pixels:u8(e.pw_pixels(),6144).slice()});break;
 case 'restore':restore(d.state);history=[];selectedHistory=null;remember(true);postMessage({type:'restored',physics});break;
 case 'rewind':{const h=history.find(h=>h.id===d.id);if(h){restore(h.state);selectedHistory=h.id;historyNotice();postMessage({type:'restored',physics});}break;}
 case 'snapshot':postMessage({type:'snapshot',module,memory:e.memory.buffer.slice(0),physics:structuredClone(physics),substep,stats:stats(),serial});break;
 case 'watts':truncate();e.pw_set_watts(d.value);emit();break;
 }
}catch(error){paused=true;postMessage({type:'error',message:error.message});}};
