import assert from 'node:assert/strict';
import {createPhysics,stepPhysics} from './physics.js';
function run(p,seconds){let max=0;for(let i=0;i<seconds*256;i++){stepPhysics(p);max=Math.max(max,Math.abs(p.x));assert.ok(p.accel.every(Number.isFinite));}return max;}
const free=createPhysics();free.tether='free';free.drag=[.12,.075];run(free,1);assert.ok(free.x>.10&&free.y>.05,'Free motion follows a distant pointer.');free.drag=null;run(free,4);assert.ok(Math.abs(free.y-(-.1+.024))<.001,'Released device lands on the ground.');
const low=createPhysics(),high=createPhysics();low.mass=.010;high.mass=.050;low.wind=high.wind=6;const a=run(low,8),b=run(high,8);assert.ok(a>b*1.3,'Mass changes wind response.');
const fixed=createPhysics();fixed.tether='fixed';fixed.wind=24;run(fixed,3);assert.equal(fixed.x,0);
console.log(JSON.stringify({freeDragAndRelease:'passed',lightDisplacementM:a,heavyDisplacementM:b,fixedConstraint:'passed'}));
