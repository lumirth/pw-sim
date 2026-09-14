import * as T from 'three';
import {GLTFLoader} from 'three/addons/loaders/GLTFLoader.js';
export function mountModel(host,onDrag,onButton,onMetrics,onViewChange){
 const scene=new T.Scene();scene.background=new T.Color(0xb9dcf1);scene.fog=new T.Fog(0xc5dfeb,.7,2.5);
 const camera=new T.PerspectiveCamera(35,1,.001,8);camera.position.set(0,.035,.32);camera.lookAt(0,.018,0);
 const renderer=new T.WebGLRenderer({antialias:true});renderer.setPixelRatio(Math.min(devicePixelRatio,2));renderer.shadowMap.enabled=true;renderer.shadowMap.type=T.PCFShadowMap;renderer.outputColorSpace=T.SRGBColorSpace;renderer.toneMapping=T.NeutralToneMapping;renderer.toneMappingExposure=1;host.appendChild(renderer.domElement);
 const sky=new T.HemisphereLight(0xf4f8ff,0xb7bda5,2.8);scene.add(sky);
 const sun=new T.DirectionalLight(0xfff5df,3);sun.position.set(-.25,.6,.5);sun.castShadow=true;sun.shadow.mapSize.set(2048,2048);Object.assign(sun.shadow.camera,{left:-.3,right:.3,top:.3,bottom:-.3,near:.01,far:2});sun.shadow.bias=-.00001;sun.shadow.normalBias=.00012;scene.add(sun,sun.target);
 // A bounded sky reflection keeps the red ABS visible under daylight.
 const envScene=new T.Scene();envScene.background=new T.Color(0xc7dcee);
 const envGround=new T.Mesh(new T.PlaneGeometry(2000,2000),new T.MeshBasicMaterial({color:0x768862}));envGround.rotation.x=-Math.PI/2;envGround.position.y=-1;envScene.add(envGround);
 const pmrem=new T.PMREMGenerator(renderer),envTarget=pmrem.fromScene(envScene,.04,.1,2000);scene.environment=envTarget.texture;scene.environmentIntensity=.65;pmrem.dispose();
 const grain=new Uint8Array(64*64);for(let i=0;i<grain.length;i++)grain[i]=125+((i*16807^(i>>>3)*179)%12);const grainMap=new T.DataTexture(grain,64,64,T.RedFormat);grainMap.wrapS=grainMap.wrapT=T.RepeatWrapping;grainMap.repeat.set(24,24);grainMap.needsUpdate=true;
 const mat=(color,roughness=.65)=>new T.MeshStandardMaterial({color,roughness});
 const ground=new T.Mesh(new T.PlaneGeometry(8,8),mat(0x769a4d));ground.rotation.x=-Math.PI/2;ground.position.y=-.10;ground.receiveShadow=true;scene.add(ground);
 // Low grass geometry provides scale without image or network dependencies.
 const grassGeo=new T.BufferGeometry(),verts=[],grassColors=[];
 let seed=8131;const rand=()=>{seed=(1664525*seed+1013904223)>>>0;return seed/4294967296;};
 for(let i=0;i<1700;i++){const x=(rand()-.5)*1.4,z=(rand()-.5)*1.4,h=.002+rand()*.008,w=.0004;verts.push(x-w,-.099,z,x+w,-.099,z,x+.002*(rand()-.5),-.099+h,z);const c=new T.Color().setHSL(.23+rand()*.05,.30+rand()*.15,.30+rand()*.12);for(let j=0;j<3;j++)grassColors.push(c.r,c.g,c.b);}
 grassGeo.setAttribute('position',new T.Float32BufferAttribute(verts,3));grassGeo.setAttribute('color',new T.Float32BufferAttribute(grassColors,3));grassGeo.computeVertexNormals();const grass=new T.Mesh(grassGeo,new T.MeshStandardMaterial({vertexColors:true,side:T.DoubleSide}));scene.add(grass);
 const clouds=new T.Group();for(let i=0;i<8;i++){const c=new T.Mesh(new T.SphereGeometry(.08,12,8),mat(0xfafcfd));c.scale.set(2.1,.30,.6);c.position.set((i-3.5)*.26,.30+(i%3)*.08,-.65-(i%2)*.2);clouds.add(c);}scene.add(clouds);
 const desk=new T.Group();const top=new T.Mesh(new T.BoxGeometry(.35,.008,.16),mat(0xa78b68));top.position.set(0,-.063,-.003);top.receiveShadow=true;desk.add(top);for(const x of [-.13,.13]){const leg=new T.Mesh(new T.BoxGeometry(.008,.04,.12),mat(0x626661));leg.position.set(x,-.086,0);desk.add(leg);}desk.visible=false;scene.add(desk);
 const walker=new T.Group();scene.add(walker);
 const textureCanvas=document.createElement('canvas');textureCanvas.width=120;textureCanvas.height=84;
 const ctx=textureCanvas.getContext('2d');const lcdImage=ctx.createImageData(120,84),levels=new Float32Array(6144);let targetPixels=new Uint8Array(6144),response=65,screenDirty=true;
 const texture=new T.CanvasTexture(textureCanvas);texture.minFilter=T.NearestFilter;texture.magFilter=T.NearestFilter;texture.colorSpace=T.SRGBColorSpace;
 const fallback=new T.Mesh(new T.SphereGeometry(.024,48,24),mat(0xd64d43));fallback.scale.z=.29;walker.add(fallback);
 const buttons=[];let clipMesh;
 new GLTFLoader().load('/assets/pokewalker.glb',gltf=>{
  const asset=gltf.scene;asset.rotation.y=Math.PI/2;asset.updateMatrixWorld(true);
  const meshes=[];asset.traverse(o=>{if(o.isMesh)meshes.push(o);});
  const baseMesh=meshes.find(o=>o.material.name==='Base'),box=new T.Box3().setFromObject(baseMesh),size=box.getSize(new T.Vector3()),center=box.getCenter(new T.Vector3());
  const scale=.048/size.y;asset.scale.setScalar(scale);asset.position.copy(center).multiplyScalar(-scale);asset.updateMatrixWorld(true);
  for(const mesh of meshes){
   // These two supplied meshes have normals opposite to their triangle winding.
   if(['Base','Buttons'].includes(mesh.material.name)){mesh.geometry=mesh.geometry.clone();const normals=mesh.geometry.attributes.normal;for(let i=0;i<normals.count;i++)normals.setXYZ(i,-normals.getX(i),-normals.getY(i),-normals.getZ(i));normals.needsUpdate=true;}
   mesh.castShadow=true;mesh.receiveShadow=true;const old=mesh.material;mesh.material=new T.MeshPhysicalMaterial({name:old.name,map:old.map,normalMap:old.normalMap,color:old.color,metalness:0,roughness:old.name==='Buttons'?.36:.30,clearcoat:.22,clearcoatRoughness:.20,ior:1.46,specularIntensity:.5,bumpMap:old.normalMap?null:grainMap,bumpScale:.000002,side:old.side});}
  const cover=meshes.find(o=>o.material.name==='screen_overcover__0');if(cover)cover.visible=false;
  clipMesh=meshes.find(o=>o.material.name==='Strap');
  const display=meshes.find(o=>o.material.name==='Screen');
  const screenBox=new T.Box3().setFromObject(display),screenSize=screenBox.getSize(new T.Vector3());
  // Keep the supplied screen geometry and its recess. Project the live texture
  // onto that surface, including the six-pixel inactive LCD margin.
  display.geometry=display.geometry.clone();const pos=display.geometry.attributes.position,uv=new Float32Array(pos.count*2),v=new T.Vector3();
  for(let i=0;i<pos.count;i++){v.fromBufferAttribute(pos,i).applyMatrix4(display.matrixWorld);uv[i*2]=(v.x-screenBox.min.x)/screenSize.x;uv[i*2+1]=(v.y-screenBox.min.y)/screenSize.y;}
  display.geometry.setAttribute('uv',new T.BufferAttribute(uv,2));
  display.material=new T.MeshStandardMaterial({map:texture,roughness:.85,metalness:0,envMapIntensity:0});display.castShadow=false;
  const glass=display.clone();glass.material=new T.MeshPhysicalMaterial({color:0xffffff,transparent:true,opacity:.02,roughness:.18,clearcoat:.25,depthWrite:false});glass.position.x-=.0005;display.parent.add(glass);
  const keyMesh=meshes.find(o=>o.material.name==='Buttons');if(keyMesh)buttons.push(keyMesh);
  walker.clear();walker.add(asset);host.dataset.model='provided-glb';
 },undefined,error=>{host.dataset.model='fallback';host.dataset.modelError=error.message;});
 const cord=new T.Group(),cordGeos=[],cordSegments=256,cordSides=6;scene.add(cord);
 for(const color of [0xb6a076,0xc7b18a,0xab956d]){
  const geometry=new T.BufferGeometry(),indices=[];geometry.setAttribute('position',new T.BufferAttribute(new Float32Array((cordSegments+1)*cordSides*3),3));geometry.setAttribute('normal',new T.BufferAttribute(new Float32Array((cordSegments+1)*cordSides*3),3));
  for(let i=0;i<cordSegments;i++)for(let j=0;j<cordSides;j++){const a=i*cordSides+j,b=i*cordSides+(j+1)%cordSides;indices.push(a,b,a+cordSides,b,b+cordSides,a+cordSides);}geometry.setIndex(indices);cordGeos.push(geometry);const strand=new T.Mesh(geometry,mat(color,1));strand.castShadow=true;cord.add(strand);
 }
 const support=new T.Group();const bar=new T.Mesh(new T.CylinderGeometry(.0015,.0015,.32,12),mat(0x61686b));bar.rotation.z=Math.PI/2;support.add(bar);scene.add(support);
 const anchor=new T.Mesh(new T.SphereGeometry(.0024,16,12),mat(0x505452));scene.add(anchor);
 // The fan axis and the air direction use the same angle as the force model.
 const fanRig=new T.Group(),fan=new T.Group(),head=new T.Group();fanRig.add(fan);fan.add(head);scene.add(fanRig);fan.position.set(-.10,0,0);head.rotation.y=Math.PI/2;
 const ring=new T.Mesh(new T.TorusGeometry(.018,.0012,8,48),mat(0x616d72,.3));head.add(ring);
 const blades=new T.Group();head.add(blades);for(let i=0;i<3;i++){const blade=new T.Mesh(new T.SphereGeometry(.011,16,8),mat(0x94a7ac,.4));blade.scale.set(.6,1,.10);blade.position.y=.008;const arm=new T.Group();arm.rotation.z=i*Math.PI*2/3;arm.add(blade);blades.add(arm);}
 for(let i=0;i<8;i++){const wire=new T.Mesh(new T.CylinderGeometry(.00025,.00025,.034,6),mat(0x48535a));wire.rotation.z=i*Math.PI/8;wire.position.z=.002;head.add(wire);}
 const stand=new T.Mesh(new T.CylinderGeometry(.001,.0018,.05,10),mat(0x737c7b));stand.position.y=-.043;fan.add(stand);const foot=new T.Mesh(new T.BoxGeometry(.023,.003,.015),mat(0x596363));foot.position.y=-.074;fan.add(foot);
 const airGeometry=new T.BufferGeometry(),airVertices=new Float32Array(120*3);airGeometry.setAttribute('position',new T.BufferAttribute(airVertices,3));const air=new T.Points(airGeometry,new T.PointsMaterial({color:0xf9fcff,size:.0011,transparent:true,opacity:.65}));fanRig.add(air);
 let state={x:0,y:0,z:0,theta:0,phi:0,t:0,fan:0,tether:'pendulum',length:.10,cordDiameter:.001,diameter:.048,direction:0,q:[0,0,0,1],clip:true,fanHeight:0,twist:0,fanDirection:0},settings={zoom:1.35,environment:'outdoor',vectors:false,follow:true,elevation:12,azimuth:0,sunElevation:50,sunAzimuth:-25,sunIntensity:3},paused=false,dead=false,last=0,raf=0,frames=0,lastReport=0,worst=0;
 const arrowColors=[0xb45848,0x3d6d4f,0x3b6794],arrows=arrowColors.map((c,i)=>{const dir=new T.Vector3(i===0?1:0,i===1?1:0,i===2?1:0);const a=new T.ArrowHelper(dir,new T.Vector3(),.04,c,.006,.003);scene.add(a);return a;});
 const resize=()=>{const w=host.clientWidth,h=host.clientHeight;renderer.setSize(w,h);camera.aspect=w/h;camera.updateProjectionMatrix();};const observer=new ResizeObserver(resize);observer.observe(host);resize();
 const element=renderer.domElement;element.setAttribute('aria-label','Pokéwalker in a physical scene. Drag the device to move it.');element.style.touchAction='none';
 const ray=new T.Raycaster(),pointer=new T.Vector2(),plane=new T.Plane(new T.Vector3(0,0,1),0);let dragging=false,pressed=false,orbit=null,pointerId=null,grabLocal=new T.Vector3(),grabOrientation=[0,0,0,1];
 const pointerWorld=ev=>{const r=element.getBoundingClientRect();pointer.set((ev.clientX-r.left)/r.width*2-1,-(ev.clientY-r.top)/r.height*2+1);ray.setFromCamera(pointer,camera);const point=new T.Vector3();return ray.ray.intersectPlane(plane,point)?point:null;};
 const release=()=>{if(dragging)onDrag(null);if(pressed)onButton(0);dragging=pressed=false;orbit=null;const id=pointerId;pointerId=null;if(id!==null&&element.hasPointerCapture(id))element.releasePointerCapture(id);element.style.cursor='grab';};
 const capture=ev=>{pointerId=ev.pointerId;element.setPointerCapture(pointerId);};
 const down=ev=>{
  if(ev.button===2){ev.preventDefault();orbit={x:ev.clientX,y:ev.clientY,azimuth:settings.azimuth,elevation:settings.elevation};capture(ev);element.style.cursor='move';return;}
  if(ev.button!==0||pointerId!==null)return;
  pointerWorld(ev);const hits=ray.intersectObjects(buttons,true);
  if(hits.length){capture(ev);const p=walker.worldToLocal(hits[0].point.clone());onButton(p.x<-.004?4:p.x>.004?8:2);pressed=true;return;}
  const bodyHits=ray.intersectObject(walker,true);if(!bodyHits.length)return;
  capture(ev);dragging=true;const hit=bodyHits[0].point;grabLocal=walker.worldToLocal(hit.clone()).multiplyScalar(state.diameter/.048);grabOrientation=walker.quaternion.toArray();plane.setFromNormalAndCoplanarPoint(camera.getWorldDirection(new T.Vector3()),hit);onDrag({target:hit.toArray(),local:grabLocal.toArray(),orientation:grabOrientation});element.style.cursor='grabbing';
 };
 const move=ev=>{
  if(pointerId!==ev.pointerId)return;const r=element.getBoundingClientRect();
  if(ev.clientX<r.left||ev.clientX>r.right||ev.clientY<r.top||ev.clientY>r.bottom){release();return;}
  if(orbit){const value={azimuth:((orbit.azimuth-(ev.clientX-orbit.x)*.3+540)%360)-180,elevation:Math.max(-10,Math.min(85,orbit.elevation+(ev.clientY-orbit.y)*.25))};Object.assign(settings,value);onViewChange(value);return;}
  if(dragging){const p=pointerWorld(ev);if(p)onDrag({target:p.toArray(),local:grabLocal.toArray(),orientation:grabOrientation});}
 };
 const wheel=ev=>{ev.preventDefault();const value={zoom:Math.max(.6,Math.min(2.5,settings.zoom*Math.exp(-ev.deltaY*.001)))};Object.assign(settings,value);onViewChange(value);};
 const context=ev=>ev.preventDefault(),leave=()=>{if(dragging||pressed||orbit)release();};
 element.addEventListener('pointerdown',down);element.addEventListener('pointermove',move);element.addEventListener('pointerup',release);element.addEventListener('pointercancel',release);element.addEventListener('lostpointercapture',release);element.addEventListener('pointerleave',leave);element.addEventListener('contextmenu',context);element.addEventListener('wheel',wheel,{passive:false});window.addEventListener('blur',release);element.style.cursor='grab';
 function paint(dt){const alpha=response===0?1:1-Math.exp(-dt*1000/response);let changing=false;const light=[189,196,159],dark=[46,57,42];
  for(let y=0;y<84;y++)for(let x=0;x<120;x++){let level=0;if(x>=12&&x<108&&y>=10&&y<74){const i=(y-10)*96+x-12;levels[i]+=(targetPixels[i]-levels[i])*alpha;level=levels[i]/3;if(Math.abs(targetPixels[i]-levels[i])>.005)changing=true;}const k=(y*120+x)*4;for(let c=0;c<3;c++)lcdImage.data[k+c]=light[c]+(dark[c]-light[c])*level;lcdImage.data[k+3]=255;}
  ctx.putImageData(lcdImage,0,0);texture.needsUpdate=true;screenDirty=changing;
 }
 function draw(time){if(dead)return;const dt=Math.min(.05,(time-last)/1000||1/60);last=time;frames++;worst=Math.max(worst,dt*1000);
  const a=1-Math.exp(-dt*45);walker.position.lerp(new T.Vector3(state.x,state.y,state.z),a);walker.quaternion.slerp(new T.Quaternion(...(state.q||[0,0,0,1])),a);if(clipMesh)clipMesh.visible=state.clip;walker.scale.setScalar(state.diameter/.048);
  const anchored=state.tether==='pendulum';cord.visible=support.visible=anchor.visible=anchored;
  const anchorY=state.length+state.diameter/2;anchor.position.set(0,anchorY,0);support.position.set(0,anchorY+.002,-.002);
  if(anchored){const end=new T.Vector3(0,state.diameter/2,0).applyQuaternion(walker.quaternion).add(walker.position),start=anchor.position;
   let lo=0,hi=state.length;for(let k=0;k<12;k++){const sag=(lo+hi)/2;let length=0,prev=start;for(let i=1;i<=32;i++){const t=i/32,p=new T.Vector3().lerpVectors(start,end,t);p.y-=4*t*(1-t)*sag;length+=p.distanceTo(prev);prev=p;}if(length<state.length)lo=sag;else hi=sag;}
   const sag=(lo+hi)/2,axis=new T.Vector3().subVectors(end,start).normalize(),side=new T.Vector3(1,0,0).cross(axis);if(side.lengthSq()<.01)side.set(0,0,1).cross(axis);side.normalize();const other=axis.clone().cross(side).normalize();
   const cordRadius=(state.cordDiameter||.001)/2;
   cordGeos.forEach((geometry,j)=>{const positions=geometry.attributes.position,normals=geometry.attributes.normal;for(let i=0;i<=cordSegments;i++){const t=i/cordSegments,p=new T.Vector3().lerpVectors(start,end,t),phase=t*(Math.PI*2*state.length/(6*cordRadius*2)+(state.twist||0))+j*Math.PI*2/3;p.y-=4*t*(1-t)*sag;p.addScaledVector(side,Math.cos(phase)*cordRadius*.52).addScaledVector(other,Math.sin(phase)*cordRadius*.52);
    for(let k=0;k<cordSides;k++){const angle=k*Math.PI*2/cordSides,normal=side.clone().multiplyScalar(Math.cos(angle)).addScaledVector(other,Math.sin(angle)),index=i*cordSides+k;positions.setXYZ(index,p.x+normal.x*cordRadius*.48,p.y+normal.y*cordRadius*.48,p.z+normal.z*cordRadius*.48);normals.setXYZ(index,normal.x,normal.y,normal.z);}
   }positions.needsUpdate=true;normals.needsUpdate=true;geometry.computeBoundingSphere();});}
  const environment=state.environment||settings.environment,outdoors=environment==='outdoor';ground.material.color.set(outdoors?0x769a4d:0xc5c6c1);grass.visible=clouds.visible=outdoors;desk.visible=environment==='desk';scene.background.set(outdoors?0xb9dcf1:0xe5e6e2);scene.fog.color.copy(scene.background);
  const viewY=state.tether==='free'?(environment==='desk'?-.020:-.055):state.tether==='pendulum'?state.length*.35-.005:0;
  const frameFan=state.fan>0&&state.tether==='pendulum',fanAngle=state.direction*Math.PI/180;
  const targetX=frameFan?-.020*Math.cos(fanAngle):settings.follow&&state.tether==='free'?state.x:0,targetZ=frameFan?-.020*Math.sin(fanAngle):settings.follow&&state.tether==='free'?state.z:0;
  const previousTarget=camera.userData.target||new T.Vector3(0,viewY,0);if(!dragging)previousTarget.lerp(new T.Vector3(targetX,viewY,targetZ),1-Math.exp(-dt*6));camera.userData.target=previousTarget;
  const elevation=settings.elevation*Math.PI/180,azimuth=settings.azimuth*Math.PI/180,distance=Math.max(frameFan?.50:.32,state.tether==='pendulum'?.32+Math.max(0,state.length-.06)*2.8:0);
  camera.position.set(previousTarget.x+distance*Math.cos(elevation)*Math.sin(azimuth),previousTarget.y+distance*Math.sin(elevation),previousTarget.z+distance*Math.cos(elevation)*Math.cos(azimuth));camera.lookAt(previousTarget);camera.zoom=settings.zoom;camera.updateProjectionMatrix();
  const se=settings.sunElevation*Math.PI/180,sa=settings.sunAzimuth*Math.PI/180;sun.target.position.copy(walker.position);sun.position.copy(walker.position).add(new T.Vector3(Math.cos(se)*Math.sin(sa),Math.sin(se),Math.cos(se)*Math.cos(sa)).multiplyScalar(.8));sun.intensity=settings.sunIntensity;
  fanRig.visible=state.fan>0;fanRig.rotation.y=-state.direction*Math.PI/180;fan.position.y=state.fanHeight||0;head.rotation.y=Math.PI/2-((state.fanDirection??state.direction)-state.direction)*Math.PI/180;blades.rotation.z=state.t*state.fan*4;
  const floorY=environment==='desk'?-.059:-.10,standLength=Math.max(.006,(state.fanHeight||0)-.018-floorY);stand.scale.y=standLength/.05;stand.position.y=-.018-standLength/2;foot.position.y=floorY-(state.fanHeight||0)+.0015;
  for(let i=0;i<120;i++){const u=(state.t*.3*(state.fan/8)+i/120)%1;const angle=((state.fanDirection??state.direction)-state.direction)*Math.PI/180,along=.01+u*.24,cross=Math.cos(i*8.1)*.017;airVertices[i*3]=-.10+along*Math.cos(angle)-cross*Math.sin(angle);airVertices[i*3+1]=(state.fanHeight||0)+Math.sin(i*11.3)*.017;airVertices[i*3+2]=along*Math.sin(angle)+cross*Math.cos(angle);}air.visible=state.fan>0;airGeometry.attributes.position.needsUpdate=true;
  arrows.forEach((arrow,i)=>{arrow.visible=settings.vectors;arrow.position.copy(walker.position);const axis=new T.Vector3(i===0?1:0,i===1?1:0,i===2?1:0).applyQuaternion(walker.quaternion);arrow.setDirection(axis);});
  if(screenDirty)paint(dt);renderer.render(scene,camera);
  if(time-lastReport>=1000){onMetrics({fps:Math.round(frames*1000/(time-lastReport)),maxFrame:worst});frames=0;worst=0;lastReport=time;}
  raf=requestAnimationFrame(draw);
 }
 raf=requestAnimationFrame(draw);
 return {updatePose:(p,isPaused=false)=>{state=p;paused=isPaused;},updateScreen:p=>{targetPixels=p;screenDirty=true;},configure:s=>{Object.assign(settings,s);if(s.response!==undefined)response=s.response;},dispose:()=>{dead=true;cancelAnimationFrame(raf);observer.disconnect();window.removeEventListener('blur',release);scene.traverse(o=>{o.geometry?.dispose();if(o.material){for(const m of Array.isArray(o.material)?o.material:[o.material])m.dispose();}});texture.dispose();grainMap.dispose();envTarget.dispose();envGround.geometry.dispose();envGround.material.dispose();renderer.dispose();host.replaceChildren();}};
}
