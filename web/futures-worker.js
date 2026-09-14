import {stepPhysics} from './physics.js';
onmessage=async({data:d})=>{try{
 const started=performance.now(),branches=[];
 const options=[['No input',0,'unchanged'],['Left',4,'unchanged'],['Center',2,'unchanged'],['Right',8,'unchanged'],['Start walking',0,'walk'],['Stillness',0,'still']];
 for(const [name,key,mode] of options){
  const {exports:e}=await WebAssembly.instantiate(d.module,{});new Uint8Array(e.memory.buffer).set(new Uint8Array(d.memory));
  const p=structuredClone(d.physics);if(mode!=='unchanged'){p.mode=mode;p.drag=null;p.wind=0;if(mode==='still')p.tether='fixed';}
  const before=Array.from(new Uint32Array(e.memory.buffer,e.pw_stats(),34));
  let states=[],frames=[],steps=[];const start=performance.now();
  for(let i=0;i<192;i++){
   e.pw_buttons(i<3?key:0);for(let j=i===0?(d.substep||0):0;j<16;j++)stepPhysics(p);e.pw_sensor(...p.accel);e.pw_tick();
   const s=Array.from(new Uint32Array(e.memory.buffer,e.pw_stats(),34));
   if(states.at(-1)!==s[5])states.push(s[5]);if(i%16===15){steps.push(s[7]-before[7]);frames.push(new Uint8Array(e.memory.buffer,e.pw_pixels(),6144).slice());}
  }
  const stats=Array.from(new Uint32Array(e.memory.buffer,e.pw_stats(),34));
  branches.push({name,key,mode,stats,states,steps,frames,pixels:frames[0],deltaSteps:stats[7]-before[7],deltaWatts:stats[6]-before[6],elapsed:performance.now()-start});
 }
 postMessage({type:'futures',branches,elapsed:performance.now()-started,simulated:72,startedAt:d.stats[0],serial:d.serial});
}catch(error){postMessage({type:'error',message:error.message});}};
