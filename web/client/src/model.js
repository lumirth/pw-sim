import * as T from 'three';
import {GLTFLoader} from 'three/addons/loaders/GLTFLoader.js';
export function mountModel(host,onDrag,onButton,onMetrics){
 const scene=new T.Scene();scene.background=new T.Color(0xb9dcf1);scene.fog=new T.Fog(0xc5dfeb,.7,2.5);
 const camera=new T.PerspectiveCamera(35,1,.001,8);camera.position.set(0,.035,.32);camera.lookAt(0,.018,0);
 const renderer=new T.WebGLRenderer({antialias:true});renderer.setPixelRatio(Math.min(devicePixelRatio,2));renderer.shadowMap.enabled=true;renderer.shadowMap.type=T.PCFShadowMap;renderer.outputColorSpace=T.SRGBColorSpace;renderer.toneMapping=T.ACESFilmicToneMapping;renderer.toneMappingExposure=1.25;host.appendChild(renderer.domElement);
 const sky=new T.HemisphereLight(0xeef7ff,0x819269,2.3);scene.add(sky);
 const sun=new T.DirectionalLight(0xfff5df,3);sun.position.set(-.25,.6,.5);sun.castShadow=true;sun.shadow.mapSize.set(1024,1024);Object.assign(sun.shadow.camera,{left:-.3,right:.3,top:.3,bottom:-.3,near:.01,far:2});sun.shadow.bias=-.00005;scene.add(sun);
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
 const textureCanvas=document.createElement('canvas');textureCanvas.width=108;textureCanvas.height=76;
 const ctx=textureCanvas.getContext('2d');const lcdImage=ctx.createImageData(108,76),levels=new Float32Array(6144);let targetPixels=new Uint8Array(6144),response=65,screenDirty=true;
 const texture=new T.CanvasTexture(textureCanvas);texture.minFilter=T.NearestFilter;texture.magFilter=T.NearestFilter;texture.colorSpace=T.SRGBColorSpace;
 const fallback=new T.Mesh(new T.SphereGeometry(.024,48,24),mat(0xd64d43));fallback.scale.z=.29;walker.add(fallback);
 const buttons=[];
 new GLTFLoader().load('/assets/pokewalker.glb',gltf=>{
  const asset=gltf.scene;asset.rotation.y=Math.PI/2;asset.updateMatrixWorld(true);
  const meshes=[];asset.traverse(o=>{if(o.isMesh)meshes.push(o);});
  const baseMesh=meshes.find(o=>o.material.name==='Base'),box=new T.Box3().setFromObject(baseMesh),size=box.getSize(new T.Vector3()),center=box.getCenter(new T.Vector3());
  const scale=.048/size.y;asset.scale.setScalar(scale);asset.position.copy(center).multiplyScalar(-scale);asset.updateMatrixWorld(true);
  for(const mesh of meshes){mesh.castShadow=true;mesh.receiveShadow=false;mesh.material.metalness=0;mesh.material.roughness=.42;if(mesh.material.map){mesh.material.emissiveMap=mesh.material.map;mesh.material.emissive.set(0xffffff);mesh.material.emissiveIntensity=.20;}}
  const cover=meshes.find(o=>o.material.name==='screen_overcover__0');if(cover)cover.visible=false;
  const display=meshes.find(o=>o.material.name==='Screen');
  const screenBox=new T.Box3().setFromObject(display),screenSize=screenBox.getSize(new T.Vector3());
  // Keep the supplied screen geometry and its recess. Project the live texture
  // onto that surface, including the six-pixel inactive LCD margin.
  display.geometry=display.geometry.clone();const pos=display.geometry.attributes.position,uv=new Float32Array(pos.count*2),v=new T.Vector3();
  for(let i=0;i<pos.count;i++){v.fromBufferAttribute(pos,i).applyMatrix4(display.matrixWorld);uv[i*2]=(v.x-screenBox.min.x)/screenSize.x;uv[i*2+1]=(v.y-screenBox.min.y)/screenSize.y;}
  display.geometry.setAttribute('uv',new T.BufferAttribute(uv,2));
  display.material=new T.MeshStandardMaterial({map:texture,roughness:.78,metalness:0,emissive:0x6b7356,emissiveIntensity:.14});display.castShadow=false;
  const glass=display.clone();glass.material=new T.MeshPhysicalMaterial({color:0xffffff,transparent:true,opacity:.055,roughness:.08,clearcoat:1,depthWrite:false});glass.position.x-=.0005;display.parent.add(glass);
  const keyMesh=meshes.find(o=>o.material.name==='Buttons');if(keyMesh)buttons.push(keyMesh);
  walker.clear();walker.add(asset);host.dataset.model='provided-glb';
 },undefined,error=>{host.dataset.model='fallback';host.dataset.modelError=error.message;});
 const cordMaterial=new T.LineBasicMaterial({color:0x585953});const cordGeo=new T.BufferGeometry().setFromPoints([new T.Vector3(),new T.Vector3()]);const cord=new T.Line(cordGeo,cordMaterial);scene.add(cord);
 const support=new T.Group();const bar=new T.Mesh(new T.CylinderGeometry(.0015,.0015,.32,12),mat(0x61686b));bar.rotation.z=Math.PI/2;support.add(bar);scene.add(support);
 const anchor=new T.Mesh(new T.SphereGeometry(.0024,16,12),mat(0x505452));scene.add(anchor);
 // The fan axis and the air direction use the same angle as the force model.
 const fanRig=new T.Group(),fan=new T.Group();fanRig.add(fan);scene.add(fanRig);fan.position.set(-.10,-.022,0);fan.rotation.y=Math.PI/2;
 const ring=new T.Mesh(new T.TorusGeometry(.018,.0012,8,48),mat(0x616d72,.3));fan.add(ring);
 const blades=new T.Group();fan.add(blades);for(let i=0;i<3;i++){const blade=new T.Mesh(new T.SphereGeometry(.011,16,8),mat(0x94a7ac,.4));blade.scale.set(.6,1,.10);blade.position.y=.008;const arm=new T.Group();arm.rotation.z=i*Math.PI*2/3;arm.add(blade);blades.add(arm);}
 for(let i=0;i<8;i++){const wire=new T.Mesh(new T.CylinderGeometry(.00025,.00025,.034,6),mat(0x48535a));wire.rotation.z=i*Math.PI/8;wire.position.z=.002;fan.add(wire);}
 const stand=new T.Mesh(new T.CylinderGeometry(.001,.0018,.05,10),mat(0x737c7b));stand.position.y=-.043;fan.add(stand);const foot=new T.Mesh(new T.BoxGeometry(.023,.003,.015),mat(0x596363));foot.position.y=-.074;fan.add(foot);
 const airGeometry=new T.BufferGeometry(),airVertices=new Float32Array(120*3);airGeometry.setAttribute('position',new T.BufferAttribute(airVertices,3));const air=new T.Points(airGeometry,new T.PointsMaterial({color:0xf9fcff,size:.0011,transparent:true,opacity:.65}));fanRig.add(air);
 let state={x:0,y:0,z:0,theta:0,phi:0,t:0,fan:0,tether:'pendulum',length:.06,diameter:.048,direction:0},settings={zoom:1.35,environment:'outdoor',vectors:false,follow:true},paused=false,dead=false,last=0,raf=0,frames=0,lastReport=0,worst=0;
 const arrowColors=[0xb45848,0x3d6d4f,0x3b6794],arrows=arrowColors.map((c,i)=>{const dir=new T.Vector3(i===0?1:0,i===1?1:0,i===2?1:0);const a=new T.ArrowHelper(dir,new T.Vector3(),.04,c,.006,.003);scene.add(a);return a;});
 const resize=()=>{const w=host.clientWidth,h=host.clientHeight;renderer.setSize(w,h);camera.aspect=w/h;camera.updateProjectionMatrix();};const observer=new ResizeObserver(resize);observer.observe(host);resize();
 const element=renderer.domElement;element.setAttribute('aria-label','Pokéwalker in a physical scene. Drag the device to move it.');element.style.touchAction='none';
 const ray=new T.Raycaster(),pointer=new T.Vector2(),plane=new T.Plane(new T.Vector3(0,0,1),0);let dragging=false,pressed=false,offset=new T.Vector3();
 const pointerWorld=ev=>{const r=element.getBoundingClientRect();pointer.set((ev.clientX-r.left)/r.width*2-1,-(ev.clientY-r.top)/r.height*2+1);ray.setFromCamera(pointer,camera);const point=new T.Vector3();ray.ray.intersectPlane(plane,point);return point;};
 const down=ev=>{pointerWorld(ev);const hits=ray.intersectObjects(buttons,true);element.setPointerCapture(ev.pointerId);if(hits.length){const p=walker.worldToLocal(hits[0].point.clone());onButton(p.x<-.004?4:p.x>.004?8:2);pressed=true;return;}if(!ray.intersectObject(walker,true).length)return;dragging=true;offset=pointerWorld(ev).sub(walker.position);element.style.cursor='grabbing';};
 const move=ev=>{if(dragging){const p=pointerWorld(ev).sub(offset);onDrag([p.x,p.y]);}};
 const release=()=>{if(dragging)onDrag(null);if(pressed)onButton(0);dragging=pressed=false;element.style.cursor='grab';};
 element.addEventListener('pointerdown',down);element.addEventListener('pointermove',move);element.addEventListener('pointerup',release);element.addEventListener('pointercancel',release);element.style.cursor='grab';
 function paint(dt){const alpha=response===0?1:1-Math.exp(-dt*1000/response);let changing=false;const light=[189,196,159],dark=[46,57,42];
  for(let y=0;y<76;y++)for(let x=0;x<108;x++){let level=0;if(x>=6&&x<102&&y>=6&&y<70){const i=(y-6)*96+x-6;levels[i]+=(targetPixels[i]-levels[i])*alpha;level=levels[i]/3;if(Math.abs(targetPixels[i]-levels[i])>.005)changing=true;}const k=(y*108+x)*4;for(let c=0;c<3;c++)lcdImage.data[k+c]=light[c]+(dark[c]-light[c])*level;lcdImage.data[k+3]=255;}
  ctx.putImageData(lcdImage,0,0);texture.needsUpdate=true;screenDirty=changing;
 }
 function draw(time){if(dead)return;const dt=Math.min(.05,(time-last)/1000||1/60);last=time;frames++;worst=Math.max(worst,dt*1000);
  const a=1-Math.exp(-dt*45);walker.position.lerp(new T.Vector3(state.x,state.y,state.z),a);walker.rotation.z=T.MathUtils.lerp(walker.rotation.z,-state.theta,a);walker.rotation.y=T.MathUtils.lerp(walker.rotation.y,state.phi,a);walker.scale.setScalar(state.diameter/.048);
  const anchored=state.tether==='pendulum';cord.visible=support.visible=anchor.visible=anchored;
  const anchorY=state.length+state.diameter/2;anchor.position.set(0,anchorY,0);support.position.set(0,anchorY+.002,-.002);
  if(anchored){const end=new T.Vector3(0,state.diameter/2,0).applyEuler(walker.rotation).add(walker.position),positions=cordGeo.attributes.position;positions.setXYZ(0,0,anchorY,0);positions.setXYZ(1,end.x,end.y,end.z);positions.needsUpdate=true;cordGeo.computeBoundingSphere();}
  const outdoors=settings.environment==='outdoor';ground.material.color.set(outdoors?0x769a4d:0xc5c6c1);grass.visible=clouds.visible=outdoors;desk.visible=settings.environment==='desk';scene.background.set(outdoors?0xb9dcf1:0xe5e6e2);scene.fog.color.copy(scene.background);
  const viewY=state.tether==='free'?(settings.environment==='desk'?-.016:-.034):state.tether==='pendulum'?state.length*.4-.005:0;
  const targetX=settings.follow&&state.tether==='free'?state.x:0,targetZ=settings.follow&&state.tether==='free'?state.z:0;
  const cameraX=dragging?camera.position.x:T.MathUtils.lerp(camera.position.x,targetX,1-Math.exp(-dt*6));
  const cameraZ=dragging?camera.position.z:T.MathUtils.lerp(camera.position.z,targetZ+.32,1-Math.exp(-dt*6));
  camera.position.set(cameraX,viewY+.025,cameraZ);camera.lookAt(cameraX,viewY,cameraZ-.32);camera.zoom=settings.zoom/(state.tether==='pendulum'?Math.max(1,state.length/.06):1);camera.updateProjectionMatrix();
  fanRig.rotation.y=-state.direction*Math.PI/180;blades.rotation.z=state.t*state.fan*4;
  for(let i=0;i<120;i++){const u=(state.t*.3*(state.fan/8)+i/120)%1;airVertices[i*3]=-.09+u*.24;airVertices[i*3+1]=-.022+Math.sin(i*11.3)*.017;airVertices[i*3+2]=Math.cos(i*8.1)*.017;}air.visible=state.fan>0;airGeometry.attributes.position.needsUpdate=true;
  arrows.forEach((arrow,i)=>{arrow.visible=settings.vectors;arrow.position.copy(walker.position);const axis=new T.Vector3(i===0?1:0,i===1?1:0,i===2?1:0).applyEuler(walker.rotation);arrow.setDirection(axis);});
  if(screenDirty)paint(dt);renderer.render(scene,camera);
  if(time-lastReport>=1000){onMetrics({fps:Math.round(frames*1000/(time-lastReport)),maxFrame:worst});frames=0;worst=0;lastReport=time;}
  raf=requestAnimationFrame(draw);
 }
 raf=requestAnimationFrame(draw);
 return {updatePose:(p,isPaused=false)=>{state=p;paused=isPaused;},updateScreen:p=>{targetPixels=p;screenDirty=true;},configure:s=>{Object.assign(settings,s);if(s.response!==undefined)response=s.response;},dispose:()=>{dead=true;cancelAnimationFrame(raf);observer.disconnect();scene.traverse(o=>{o.geometry?.dispose();if(o.material){for(const m of Array.isArray(o.material)?o.material:[o.material])m.dispose();}});texture.dispose();renderer.dispose();host.replaceChildren();}};
}
