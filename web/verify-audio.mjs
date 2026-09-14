import fs from 'node:fs';import assert from 'node:assert/strict';
const {instance:{exports:e}}=await WebAssembly.instantiate(fs.readFileSync(new URL('./firmware.wasm',import.meta.url)));
new Uint8Array(e.memory.buffer,e.pw_eeprom(),65536).set(fs.readFileSync(new URL('./fixtures/furret-eeprom-64KiB.bin',import.meta.url)));e.pw_boot();e.BeepSetOutputMode(2);e.BeepSelectScore(e.g_testScore.value);
for(let i=0;i<16;i++)e.pw_tick();const raw=new Uint32Array(e.memory.buffer,e.pw_audio_events(),e.pw_audio_head()*4),events=[];for(let i=0;i<raw.length;i+=4)events.push({us:raw[i]+raw[i+1]*4294967296,period:raw[i+2],mode:raw[i+3]});
assert.equal(events.length,4);assert.deepEqual(events.map(e=>e.period),[24,0,27,0]);assert.ok(events.every((e,i)=>!i||e.us>events[i-1].us));const rest=events[2].us-events[1].us;assert.ok(rest>9000&&rest<11000,'The short rest is captured inside a 62.5 ms foreground interval.');assert.equal(e.pw_audio_period(),0);
fs.writeFileSync(new URL('./generated/audio-verification.json',import.meta.url),JSON.stringify({events,shortRestUs:rest},null,2)+'\n');console.log(JSON.stringify({events,shortRestUs:rest}));
// Exercise the EEPROM sound bank through the same native score loader and timer.
const bank=[];
for(let id=0;id<16;id++){
 e.pw_boot();assert.equal(e.pw_play_score(id),1,`EEPROM score ${id} loads.`);for(let i=0;i<160;i++)e.pw_tick();
 const head=e.pw_audio_head(),raw=new Uint32Array(e.memory.buffer,e.pw_audio_events(),4096),tones=[];for(let i=0;i<head;i++)if(raw[(i&1023)*4+2])tones.push(raw[(i&1023)*4+2]);assert.ok(tones.length>0,`Score ${id} produces audible periods.`);assert.equal(e.pw_audio_period(),0,`Score ${id} completes.`);bank.push({id,transitions:head,tones:tones.length});
}
// A deterministic audio-clock harness checks the actual scheduler with native
// transitions. It verifies short rests, slow playback, and recovery after a stall.
class Param {calls=[];value=0;cancelScheduledValues(t){this.calls=this.calls.filter(c=>c.at<t)}setValueAtTime(value,at){this.calls.push({value,at})}setTargetAtTime(value,at,constant){this.calls.push({value,at,constant})}}
let clock;
class AudioClock {currentTime=0;sampleRate=48000;state='running';destination={};constructor(){clock=this}node(){return {connect(next){return next},frequency:new Param(),gain:new Param(),start(){}}}createOscillator(){return this.osc=this.node()}createGain(){return this.gain=this.node()}createBiquadFilter(){return this.node()}async resume(){}async close(){}}
globalThis.AudioContext=AudioClock;
const {createFirmwareAudio}=await import('./client/src/audio.js');const output=await createFirmwareAudio();
const sequence=events.map(e=>[e.us,e.period,e.mode]);
for(let block=0;block<8;block++){clock.currentTime=block*.0625;output.packet({from:block*62500,speed:1,serial:1,initialPeriod:0,initialVolume:2,events:sequence.filter(e=>e[0]>=block*62500&&e[0]<(block+1)*62500)});}
assert.equal(output.metrics().late,0);const changes=clock.gain.gain.calls.filter(c=>c.value>.001||c.constant===.0008);const restAt=changes.find(c=>c.value===0&&c.at>.12),nextTone=changes.find(c=>c.value>0&&c.at>restAt.at);assert.ok(Math.abs((nextTone.at-restAt.at)*1e6-rest)<.01,'Web Audio retains the native short rest.');
output.reset();clock.currentTime=2;output.packet({from:0,speed:.125,serial:2,initialPeriod:0,initialVolume:2,events:sequence.filter(e=>e[0]<62500)});clock.currentTime=2.5;output.packet({from:62500,speed:.125,serial:2,initialPeriod:0,initialVolume:2,events:sequence.filter(e=>e[0]>=62500&&e[0]<125000)});assert.equal(output.metrics().late,0,'Slow playback has enough lookahead.');
clock.currentTime=10;output.packet({from:125000,speed:.125,serial:2,initialPeriod:24,initialVolume:2,events:[]});assert.equal(output.metrics().resyncs,1,'The queue recovers from a wall-clock stall.');assert.equal(output.metrics().late,0);
fs.writeFileSync(new URL('./generated/audio-verification.json',import.meta.url),JSON.stringify({events,shortRestUs:rest,bank,clockChecks:'short rests, slow playback, stall recovery'},null,2)+'\n');console.log('16 EEPROM scores and browser audio-clock scheduling: passed.');
