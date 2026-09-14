import fs from 'node:fs';
import assert from 'node:assert/strict';
import {createPhysics,advancePhysics} from './physics.js';
const module=await WebAssembly.compile(fs.readFileSync(new URL('./firmware.wasm',import.meta.url)));
const fixture=fs.readFileSync(new URL('./fixtures/furret-eeprom-64KiB.bin',import.meta.url));
const results=[];const stats=e=>Array.from(new Uint32Array(e.memory.buffer,e.pw_stats(),34));
async function machine(data=fixture){const{exports:e}=await WebAssembly.instantiate(module,{});new Uint8Array(e.memory.buffer,e.pw_eeprom(),65536).set(data);e.pw_boot();return e;}
function press(e,b){e.pw_buttons(b);e.pw_tick();e.pw_buttons(0);for(let i=0;i<15;i++)e.pw_tick();}
function steps(e,n){for(let i=0;i<n;i++){e.pw_sensor(Math.round(20*Math.sin(i*Math.PI/4)),Math.round(40*Math.sin(i*Math.PI/4)),12);e.pw_tick();}}
assert.deepEqual(WebAssembly.Module.imports(module),[]);results.push('Freestanding WebAssembly: zero function imports.');
for(const name of fs.readdirSync(new URL('./fixtures',import.meta.url)).filter(n=>n.endsWith('.bin'))){
 const data=fs.readFileSync(new URL('./fixtures/'+name,import.meta.url)),e=await machine(data),start=stats(e);
 assert.equal(start[5],0);assert.ok(new Uint8Array(e.memory.buffer,e.pw_pixels(),6144).some(v=>v));
 for(let i=0;i<16;i++)e.pw_tick();const after=stats(e);assert.equal(after[1],16);assert.equal(after[2],1);assert.equal(after[19],start[19]+1);assert.equal(after[3]-start[3],4);
 press(e,8);if(start[10]&2)assert.equal(stats(e)[5],1);
 results.push(name+': boots, draws, 16 Hz foreground / 4 Hz display / 1 Hz RTC, input.');
}
{
 const e=await machine();e.pw_blank();assert.equal(stats(e)[10]&6,0);assert.ok(new Uint8Array(e.memory.buffer,e.pw_pixels(),6144).some(v=>v));press(e,2);for(let i=0;i<36;i++)e.pw_tick();assert.equal(stats(e)[5],14);results.push('Blank EEPROM: unpaired boot and offline IR timeout.');
}
{
 const e=await machine();steps(e,320);const s=stats(e);assert.ok(s[7]>=30&&s[7]<=35);assert.equal(s[14],4096);assert.ok(s[6]>=1);e.pw_save();const saved=Buffer.from(new Uint8Array(e.memory.buffer,e.pw_eeprom(),65536));assert.equal(saved.readUInt32BE(0x156),s[8]);assert.equal(saved.readUInt16BE(0x164),s[6]);for(const base of [0x156,0x256])assert.equal(saved[base+24],saved.subarray(base,base+24).reduce((v,n)=>(v+n)&255,1));assert.deepEqual(saved.subarray(0x280),fixture.subarray(0x280));
 const restored=await machine(saved);assert.equal(stats(restored)[8],s[8]);assert.equal(stats(restored)[6],s[6]);results.push('FFT at 2 Hz → cadence 4096 Q9 → '+s[7]+' steps, Watts; big-endian EEPROM export/reimport, both checksums, graphics preserved.');
}
{
 const e=await machine();steps(e,73);const snapshot=e.memory.buffer.slice(0);
 steps(e,80);const expected=Buffer.from(e.memory.buffer).slice();
 new Uint8Array(e.memory.buffer).set(new Uint8Array(snapshot));steps(e,80);assert.deepEqual(Buffer.from(e.memory.buffer),expected);
 const clone=await machine();new Uint8Array(clone.memory.buffer).set(new Uint8Array(snapshot));steps(clone,80);assert.deepEqual(Buffer.from(clone.memory.buffer),expected);results.push('Snapshot restore and independent Wasm fork: byte-for-byte deterministic execution.');
}
{
 for(const mode of ['still','walk','fan']){const e=await machine(),p=createPhysics();p.mode=mode;for(let i=0;i<320;i++){e.pw_sensor(...advancePhysics(p));e.pw_tick();}const s=stats(e);if(mode==='still')assert.equal(s[7],0);if(mode==='walk')assert.ok(s[7]>20);results.push('Physics '+mode+': '+s[7]+' steps / 20 device seconds, no fault.');}
}
{
 const e=await machine();e.pw_set_watts(200);press(e,8);assert.equal(stats(e)[5],1);press(e,8);assert.equal(stats(e)[18],1);press(e,2);assert.equal(stats(e)[5],2);assert.equal(stats(e)[6],197);for(let i=0;i<32;i++)e.pw_tick();press(e,2);results.push('Real button path: home → menu → Dowsing; spends exactly 3 Watts.');
}
const report={passed:results.length,results};fs.writeFileSync(new URL('./generated/verification.json',import.meta.url),JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report,null,2));
