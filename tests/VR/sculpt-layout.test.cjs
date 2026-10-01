// Tests project-owned AS2 layout logic; no upstream classes/assets required.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync('tools/vr-racesex-patches/Sculpt.as.inc','utf8');
const functions = {};
// AS2 Math.min/max compare two operands, unlike JavaScript's variadic versions.
// Model the actual runtime so a third (height) constraint cannot disappear.
const flashMath = Object.create(Math);
flashMath.min = (a,b) => Math.min(a,b);
flashMath.max = (a,b) => Math.max(a,b);
const context = vm.createContext({_global:{skse:{IsVR:()=>true,plugins:{CharGen:{}}}},Stage:{visibleRect:{x:0,y:0,width:1024,height:1024}},Math:flashMath});
for(const match of source.matchAll(/function (\w+)\(([^)]*)\)\s*\{/g)) {
  let end=match.index+match[0].length, depth=1;
  for(;depth;end++) { if(source[end]==='{') depth++; if(source[end]==='}') depth--; }
  functions[match[1]]=vm.runInContext('(function('+match[2]+'){'+source.slice(match.index+match[0].length,end-1)+'})',context);
}
if(process.argv[3]) {
  const rebuilt=fs.readFileSync(process.argv[3],'utf8');
  // The live staticPanel is attached by onLoad with an explicit pool size.
  // Execute that path: a constructor-only surrogate missed the six-slot cap.
  const loadMatch=/function onLoad\(\)\s*\{/.exec(rebuilt);assert.ok(loadMatch);
  let loadEnd=loadMatch.index+loadMatch[0].length,loadDepth=1;
  for(;loadDepth;loadEnd++){if(rebuilt[loadEnd]==='{')loadDepth++;if(rebuilt[loadEnd]==='}')loadDepth--;}
  const loadBody=rebuilt.slice(loadMatch.index+loadMatch[0].length,loadEnd-1).replace(/\bsuper\.onLoad\(\);/,'');
  const load=vm.runInContext('(function(){'+loadBody+'})',context);
  for(const [vr,expected] of [[true,6],[false,6]]) {
    context._global.skse.IsVR=()=>vr;
    let attached;
    const editor={wireframeDisplay:{addEventListener:()=>{}},brushWindow:{addEventListener:()=>{}},navPanel:{_x:0,_y:0},bottomBar:{playerInfo:{RaceLabel:{},PlayerRace:{},NameLabel:{}},getNextHighestDepth:()=>1,attachMovie:(renderer,name,depth,init)=>{assert.equal(renderer,'ButtonPanel');assert.equal(name,'staticPanel');attached={...init,buttons:Array.from({length:init.maxButtons},()=>({}))};return attached;}}};
    load.call(editor);assert.equal(attached.buttons.length,expected);
  }
  context._global.skse.IsVR=()=>true;
  console.log('Private rebuilt Sculpt onLoad: original six-slot initializer; capacity qualified at late registration');
  for(const name of Object.keys(functions)) {
    const match=new RegExp('function '+name+'\\(([^)]*)\\)\\s*\\{').exec(rebuilt);assert.ok(match,'rebuilt helper '+name);
    let end=match.index+match[0].length,depth=1;
    for(;depth;end++){if(rebuilt[end]==='{')depth++;if(rebuilt[end]==='}')depth--;}
    functions[name]=vm.runInContext('(function('+match[1]+'){'+rebuilt.slice(match.index+match[0].length,end-1)+'})',context);
  }
}
function fixture(transformed=false,brushControls=false) {
  const editor={...functions,_x:0,_y:0,_xscale:100,_yscale:100};
  function transforms(c) {
    c.localToGlobal=p=>{p.x=c._x+p.x*c._xscale/100;p.y=c._y+p.y*c._yscale/100;if(c._parent)c._parent.localToGlobal(p);};
    c.globalToLocal=p=>{if(c._parent)c._parent.globalToLocal(p);p.x=(p.x-c._x)*100/c._xscale;p.y=(p.y-c._y)*100/c._yscale;};
    return c;
  }
  function rect(w,h,parent=editor) {
    const c=transforms({_x:0,_y:0,_xscale:100,_yscale:100,_parent:parent});
    c.getBounds=(target=parent)=>{
      const a={x:-23,y:-17},b={x:w-23,y:h-17};c.localToGlobal(a);c.localToGlobal(b);target.globalToLocal(a);target.globalToLocal(b);
      return {xMin:a.x,yMin:a.y,xMax:b.x,yMax:b.y};
    };
    return c;
  }
  editor._parent=transforms({_x:transformed?41:0,_y:transformed?19:0,_xscale:transformed?120:100,_yscale:transformed?85:100,SizeVRFooterText:()=>{},vrMenuProfile:{},racePanel:{_x:-478,ListBackground:{getBounds:()=>{throw Error('hidden Sliders panel must not anchor Sculpt');}}}});
  transforms(editor);
  if(transformed)Object.assign(editor,{_x:37,_y:28,_xscale:83,_yscale:94});
  editor._parent.modeSelect=rect(410,70,editor._parent);
  editor.getNextHighestDepth=()=>100;
  editor.createEmptyMovieClip=()=>({clear:()=>{},beginFill:()=>{},moveTo:()=>{},lineTo:()=>{},endFill:()=>{},swapDepths:()=>{}});
  editor.wireframeDisplay=Object.assign(rect(1074,1074),{bLoadedAssets:true,foreground:{fixedWidth:1024,fixedHeight:1024}});
  editor.brushWindow=rect(360,240); editor.historyWindow=rect(300,200); editor.meshWindow=rect(300,180);
  if(brushControls) {
    const brush=editor.brushWindow=rect(443.05,238.15);
    const list=brush.brushList=Object.assign(rect(214,200,brush),{_x:-107,_y:-68});
    const ownBounds=brush.getBounds;
    brush.getBounds=(target=editor)=>{
      const a=ownBounds(target),b=list.getBounds(target);
      return {xMin:Math.min(a.xMin,b.xMin),yMin:Math.min(a.yMin,b.yMin),xMax:Math.max(a.xMax,b.xMax),yMax:Math.max(a.yMax,b.yMax)};
    };
    editor.historyWindow=rect(214.55,238.75);editor.meshWindow=rect(275.5,209.8);
    brush.background=rect(217.5,238.15,brush);
    brush.brushSlidingList=Object.assign(rect(466,28,brush),{_x:-106.5,_y:-104.3});
  }
  const button=()=>({_visible:true,label:'Test',_width:120,_height:30,textField:{_height:25},background:{},update:()=>{}});
  editor.navPanel={buttons:Array.from({length:4},button)};
  editor.bottomBar={background:{},playerInfo:{},staticPanel:{buttons:Array.from({length:5},button)}};
  editor.bottomBar.staticPanel.buttons[3].vrSculptAction=editor.bottomBar.staticPanel.buttons[4].vrSculptAction=true;
  editor.tempText={};
  return editor;
}
function assertFrame(e) {
  for(const c of [e.wireframeDisplay,e.brushWindow,e.historyWindow,e.meshWindow]) {
    const b=e.GetVRSculptBounds(c),a={x:b.xMin,y:b.yMin},z={x:b.xMax,y:b.yMax};e.localToGlobal(a);e.localToGlobal(z);
    assert.ok(a.x>=16-0.001 && a.y>=90-0.001);assert.ok(z.x<=1008+0.001 && z.y<=1008+0.001);
  }
  const a={x:e.vrWorkspaceBounds.xMin,y:e.vrWorkspaceBounds.yMin},z={x:e.vrWorkspaceBounds.xMax,y:e.vrWorkspaceBounds.yMax};e.localToGlobal(a);e.localToGlobal(z);
  assert.ok(Math.abs(a.x-16)<0.001 && Math.abs(a.y-90)<0.001);assert.ok(z.x<=1008+0.001 && z.y<=1008+0.001);
  assert.equal(e.vrSculptBackground._visible,true);
}
const e=fixture(); e.LayoutVRSculpt();assertFrame(e);
const first={...e.vrWorkspaceBounds}, scale=e._xscale;
e.LayoutVRSculpt();for(const key of Object.keys(first))assert.ok(Math.abs(e.vrWorkspaceBounds[key]-first[key])<0.001);assert.ok(Math.abs(e._xscale-scale)<0.001);
for(const c of [e.brushWindow,e.historyWindow]) assert.ok(c.getBounds().xMax<e.wireframeDisplay.getBounds().xMin);
assert.ok(e.meshWindow.getBounds().yMin>e.historyWindow.getBounds().yMax);
assert.ok(e.meshWindow.getBounds().xMax<e.wireframeDisplay.getBounds().xMin);
for(const i of [3,4]){
  const b=e.bottomBar.staticPanel.buttons[i];
  assert.equal(b.background._visible,true);assert.equal(b.background._alpha,12);
  assert.equal(b._y+e.bottomBar.staticPanel._y,0);
  assert.ok(b._x>e.navPanel.buttons[3]._x);
}
const normalCanvas=e.wireframeDisplay._xscale*e._xscale;
// Longer translated labels may wrap: reserve a complete first-row area,
// keep added actions above stock actions, and include both in frame fitting.
const wrapped=fixture();
for(const b of [...wrapped.navPanel.buttons,...wrapped.bottomBar.staticPanel.buttons]) b._width=150;
wrapped.LayoutVRSculpt();assertFrame(wrapped);
assert.equal(wrapped.bottomBar.staticPanel.buttons[4]._y+wrapped.bottomBar.staticPanel._y,38);
assert.ok(wrapped.bottomBar.staticPanel.buttons[4]._y+30<wrapped.bottomBar.staticPanel.buttons[0]._y);
e.vrLargeCanvas=true;e.LayoutVRSculpt();assertFrame(e);
assert.ok(e.wireframeDisplay._xscale*e._xscale>normalCanvas*1.2);
assert.equal(e.wireframeDisplay.foreground.fixedWidth,1024);
assert.equal(e.wireframeDisplay.foreground.fixedHeight,1024);
const moved=fixture(true);moved.LayoutVRSculpt();assertFrame(moved);moved.LayoutVRSculpt();assertFrame(moved);moved.vrLargeCanvas=true;moved.LayoutVRSculpt();assertFrame(moved);
// Real stock brush proportions: 214-unit rails beneath 443-unit categories.
// Enlarge the actual component/hit bounds uniformly, without compounding or
// overlapping subsequent sidebar sections; retain clipping/frame safeguards.
for(const transformed of [false,true]) {
  const sized=fixture(transformed,true),list=sized.brushWindow.brushList;
  const initial=list.getBounds(sized.brushWindow),center=(initial.xMin+initial.xMax)/2;
  sized.LayoutVRSculpt();assertFrame(sized);
  const doubled=list.getBounds(sized.brushWindow);
  assert.ok(Math.abs(doubled.xMax-doubled.xMin-428)<0.001);
  assert.ok(Math.abs(doubled.yMax-doubled.yMin-400)<0.001);
  assert.ok(Math.abs((doubled.xMin+doubled.xMax)/2-center)<0.001);
  assert.equal(list._y,-68);assert.equal(list._xscale,200);assert.equal(list._yscale,200);
  assert.equal(sized.brushWindow.brushSlidingList._xscale,200);
  assert.equal(sized.brushWindow.brushSlidingList._yscale,200);
  const stable={x:list._x,y:list._y};
  for(let i=0;i<5;i++) {
    sized.vrLargeCanvas=i%2===1;sized.LayoutVRSculpt();assertFrame(sized);
    assert.ok(Math.abs(list._x-stable.x)<0.001);assert.equal(list._y,stable.y);
    assert.equal(list._xscale,200);assert.equal(list._yscale,200);
    const brush=sized.GetVRSculptBounds(sized.brushWindow),history=sized.historyWindow.getBounds(),mesh=sized.meshWindow.getBounds();
    assert.ok(brush.yMax<history.yMin);
    assert.ok(mesh.yMin>history.yMax);
    assert.ok(mesh.xMax<sized.wireframeDisplay.getBounds().xMin);
    assert.ok(sized.BOTTOMBAR_SHOWN_Y>Math.max(sized.wireframeDisplay.getBounds().yMax,mesh.yMax));
    assert.ok(brush.yMax-brush.yMin<=260.001);
    assert.ok(history.yMax-history.yMin<=160.001);
    assert.ok(mesh.yMax-mesh.yMin<=180.001);
    assert.ok(Math.abs(sized.brushWindow._xscale-sized.brushWindow._yscale)<0.001);
  }
}
// Hidden category content moving during the stock selection tween must not
// alter the sidebar fit or push History/footer around.
const masked=fixture(false,true);masked.LayoutVRSculpt();
const maskedScale=masked.brushWindow._xscale,maskedFooter=masked.BOTTOMBAR_SHOWN_Y;
masked.brushWindow.brushSlidingList.getBounds=()=>({xMin:-900,yMin:-400,xMax:900,yMax:400});
masked.LayoutVRSculpt();assertFrame(masked);
assert.ok(Math.abs(masked.brushWindow._xscale-maskedScale)<0.001);
assert.ok(Math.abs(masked.BOTTOMBAR_SHOWN_Y-maskedFooter)<0.001);
let current=1;const requests=[];
const preflight=fixture();preflight.ReadVRSculptRotationPreflight();
assert.equal(preflight.vrRotationPreflightAvailable,false);
context._global.skse.plugins.CharGen.GetFaceViewDiagnostics=()=>({rotationTopologyAvailable:true,menuSharesTrackingOrigin:true,quadSharesTrackingOrigin:false});
preflight.ReadVRSculptRotationPreflight();
assert.equal(preflight.vrRotationPreflightAvailable,true);
assert.equal(preflight.vrRotationPreflight.menuSharesTrackingOrigin,true);
assert.equal(preflight.vrRotationPreflight.quadSharesTrackingOrigin,false);
context._global.skse.plugins.CharGen={GetMenuView:()=>current,SetMenuView:v=>{requests.push(v);current=v;}};
// OCU scans just Done in navPanel, but scans every staticPanel renderer.
// Verify ownership/registration, not only the actions' visible geometry.
const owned=fixture(),pool=owned.bottomBar.staticPanel;
pool.buttons=Array.from({length:8},(_,i)=>{
  const listeners=new Set();return {_visible:i<3,label:i<3?'Stock '+i:'',listeners,
    addEventListener:(type,target,method)=>listeners.add(method),
    removeEventListener:(type,target,method)=>listeners.delete(method)};
});
pool._buttonCount=3;
pool.addButton=data=>{const b=pool.buttons[pool._buttonCount++];b.label=data.text;b._visible=true;return b;};
for(let i=0;i<5;i++) {
  owned.RefreshVRSculptActions();assert.equal(pool._buttonCount,5);
  assert.equal(pool.buttons[3].label,'Normal view');assert.equal(pool.buttons[4].label,'Large canvas');
  assert.deepEqual([...pool.buttons[3].listeners],['onVRSculptFaceClicked']);
  assert.deepEqual([...pool.buttons[4].listeners],['onVRCanvasSizeClicked']);
  assert.ok(pool.buttons.slice(0,16).filter(b=>b.vrSculptAction).length===2);
  assert.ok(owned.navPanel.buttons.slice(0,1).every(b=>!b.vrSculptAction));
}
// Stock setPlatform clears/reuses renderers before our updateBottomBar hook.
pool._buttonCount=3;owned.vrLargeCanvas=true;current=0;owned.RefreshVRSculptActions();
assert.equal(pool._buttonCount,5);assert.equal(pool.buttons[3].label,'Face view');
assert.equal(pool.buttons[4].label,'Standard canvas');
assert.ok(pool.buttons.slice(0,3).every((b,i)=>b.label==='Stock '+i && b.listeners.size===0));
// Do not steal slots occupied by another extension or exceed the pool.
pool._buttonCount=6;pool.buttons[5].label='Other';owned.RefreshVRSculptActions();
assert.equal(owned.vrSculptActionFault,'action_panel_busy');assert.equal(pool.buttons[5].label,'Other');
pool._buttonCount=7;for(const b of pool.buttons)b.vrSculptAction=false;
owned.RefreshVRSculptActions();assert.equal(owned.vrSculptActionFault,'action_panel_capacity');assert.equal(pool._buttonCount,7);
current=1;
// Rotation actions share the fully scanned staticPanel, without a second
// laser route. Rebuilds remove listeners before reusing the eight slots.
const turns=[];let yaw=0,yawQueued=false;
context._global.skse.plugins.CharGen.SetViewYaw=angle=>{turns.push(angle);return true;};
context._global.skse.plugins.CharGen.GetFaceViewDiagnostics=()=>({yawDegrees:yaw,yawQueued});
// Actual 0.1.75 live pool: six renderers, three stock actions. Even if the
// rotation capacity is unavailable, retain the two already working actions.
owned.vrLargeCanvas=false;pool.buttons.length=6;pool._buttonCount=3;for(const b of pool.buttons)b.vrSculptAction=false;
owned.RefreshVRSculptActions();assert.equal(pool._buttonCount,5);
assert.equal(owned.vrSculptActionFault,'rotation_panel_capacity');
assert.equal(pool.buttons[3].label,'Normal view');assert.equal(pool.buttons[4].label,'Large canvas');
// Reproduce the real six-slot pool, not a constructor/init surrogate. Once
// the platform is ready, late registration attaches proper MappedButtons.
owned._platform=6;owned._bPS3Switch=false;pool.buttonRenderer='MappedButton';
pool.buttonInitializer={hiddenBackground:true};let attachedSlots=0;
pool.getNextHighestDepth=()=>100+attachedSlots;
pool.attachMovie=(renderer,name,depth,init)=>{
 assert.equal(renderer,'MappedButton');assert.equal(name,'button'+(6+attachedSlots));
 assert.equal(depth,100+attachedSlots);assert.equal(init,pool.buttonInitializer);
 attachedSlots++;const listeners=new Set();return {_visible:true,label:'',listeners,
 setPlatform:(platform,ps3)=>{assert.equal(platform,6);assert.equal(ps3,false);},setButtonData:()=>{},
 addEventListener:(type,target,method)=>listeners.add(method),removeEventListener:(type,target,method)=>listeners.delete(method)};
};
owned.RefreshVRSculptActions();assert.equal(pool.buttons.length,8);assert.equal(pool.maxButtons,8);
assert.equal(pool._buttonCount,8);assert.equal(owned.vrSculptActionFault,'');assert.equal(attachedSlots,2);
owned.RefreshVRSculptActions();assert.equal(attachedSlots,2);assert.equal(pool._buttonCount,8);
// A failed attachment never enters the scanned array or loses Face/canvas.
pool.buttons.length=6;pool._buttonCount=3;for(const b of pool.buttons)b.vrSculptAction=false;
pool.attachMovie=()=>undefined;owned.RefreshVRSculptActions();
assert.equal(pool.buttons.length,6);assert.equal(pool._buttonCount,5);
assert.equal(owned.vrSculptActionFault,'rotation_panel_capacity');
// Restore preallocated-pool coverage independently of the late allocator.
delete owned._platform;
for(let i=6;i<9;i++) {
 const listeners=new Set();pool.buttons.push({_visible:false,label:'',listeners,
 addEventListener:(type,target,method)=>listeners.add(method),removeEventListener:(type,target,method)=>listeners.delete(method)});
}
pool._buttonCount=3;for(const b of pool.buttons)b.vrSculptAction=false;
for(let i=0;i<5;i++) {
  owned.RefreshVRSculptActions();assert.equal(pool._buttonCount,8);
  assert.deepEqual(pool.buttons.slice(5,8).map(b=>b.label),['Turn left','Centre view','Turn right']);
  assert.deepEqual([...pool.buttons[5].listeners],['onVRViewLeftClicked']);
  assert.deepEqual([...pool.buttons[6].listeners],['onVRViewCentreClicked']);
  assert.deepEqual([...pool.buttons[7].listeners],['onVRViewRightClicked']);
}
owned.onVRViewLeftClicked();owned.onVRViewRightClicked();owned.onVRViewCentreClicked();
assert.deepEqual(turns,[5,-5,0]);
yaw=60;owned.onVRViewLeftClicked();assert.equal(turns.at(-1),60);
yaw=-60;owned.onVRViewRightClicked();assert.equal(turns.at(-1),-60);
yawQueued=true;const turnCount=turns.length;owned.onVRViewLeftClicked();assert.equal(turns.length,turnCount);
yawQueued=false;owned.wireframeDisplay.foreground.painting=true;
owned.onVRViewRightClicked();assert.equal(turns.length,turnCount);
const rotationLayout=fixture(false,true);
rotationLayout.bottomBar.staticPanel.buttons.push(...Array.from({length:3},()=>({_visible:true,label:'Centre view',vrSculptAction:true,_width:150,_height:30,textField:{_height:25},background:{},update:()=>{}})));
rotationLayout.LayoutVRSculpt();assertFrame(rotationLayout);
rotationLayout.vrLargeCanvas=true;rotationLayout.LayoutVRSculpt();assertFrame(rotationLayout);
e.UpdateVRSculptView(true);assert.equal(current,0);e.UpdateVRSculptView(true);assert.equal(requests.length,1);
current=1;e.UpdateVRSculptView(false);assert.equal(current,1);assert.equal(e.vrSculptFace,1);assert.equal(e.vrSculptBackground._visible,false);assert.equal(e._parent.modeSelect._x,e.vrTabsOriginal.x);
e.UpdateVRSculptView(true);assert.equal(current,1);
// Detail toggle must not relocate an active paint/rotate/pan gesture.
e.updateBottomBar=()=>{throw Error('unexpected resize');};e.wireframeDisplay.foreground.painting=true;
e.onVRCanvasSizeClicked();assert.equal(e.vrLargeCanvas,true);assert.equal(e.vrCanvasActionCount,1);assert.equal(e.vrCanvasResizeDenied,true);
console.log('Sculpt layout: stable reflow, one-side bounds, drawable-height fit, larger canvas, texture-local coordinates, independent view, active-gesture guard pass');
// Private rebuilt constructor is the capacity source, not an assumed fixture.
if(process.argv[4]) {
 const panelSource=fs.readFileSync(process.argv[4],'utf8');
 const match=/function ButtonPanel\(([^)]*)\)\s*\{/.exec(panelSource);assert.ok(match);
 let end=match.index+match[0].length,depth=1;
 for(;depth;end++){if(panelSource[end]==='{')depth++;if(panelSource[end]==='}')depth--;}
 const body=panelSource.slice(match.index+match[0].length,end-1).replace(/\bsuper\(\);/,'');
 context.VertexEditor=function(){};
 const construct=vm.runInContext('(function('+match[1]+'){'+body+'})',context);
 for(const [vr,sculpt,expected] of [[true,true,6],[true,false,6],[false,true,4]]) {
  context._global.skse.IsVR=()=>vr;
  const panel={maxButtons:4,_parent:{_name:'bottomBar',_parent:sculpt?new context.VertexEditor():{}},getNextHighestDepth:()=>1,attachMovie:()=>({_visible:true})};
  construct.call(panel);assert.equal(panel.buttons.length,expected);
 }
 context._global.skse.IsVR=()=>true;
 console.log('Private rebuilt generic panel: established +two VR footer slots retained; flat panels unchanged');
}
// Optional maintainer-only test of privately rebuilt/decompiled ModeSwitcher.
if(process.argv[2]) {
  const text=fs.readFileSync(process.argv[2],'utf8');const methods={};
  for(const match of text.matchAll(/function (\w+)\(([^)]*)\)\s*\{/g)) {
    if(match[1]==='ModeSwitcher') continue; // AS2 constructor's super() is not a standalone JS function.
    let end=match.index+match[0].length,depth=1;
    for(;depth;end++) {if(text[end]==='{')depth++;if(text[end]==='}')depth--;}
    methods[match[1]]=vm.runInContext('(function('+match[2]+'){'+text.slice(match.index+match[0].length,end-1)+'})',context);
  }
  context.gfx={controls:{ButtonGroup:function(){this.buttons=[];this.addButton=b=>{this.buttons.push(b);this.length=this.buttons.length;};this.getButtonAt=i=>this.buttons[i];this.setSelectedButton=b=>{this.selectedButton=b;};this.indexOf=b=>this.buttons.indexOf(b);this.addEventListener=()=>{};}}};
  for(const vr of [true,false]) {
    context._global.skse.IsVR=()=>vr;context._global.skse.plugins.CharGen.bEnableSculpting=true;
    const tabs=[];const group={...methods,_modes:[],_padding:25,_offset:5,Lock:()=>{},getNextHighestDepth:()=>1,dispatchEvent:event=>tabs.push(event)};
    group.createEmptyMovieClip=()=>({_width:500,getNextHighestDepth:()=>1,attachMovie:()=>({tab:{textField:{_width:60},background:{}},enabled:true,addEventListener:()=>{}})});
    group.InitExtensions();assert.equal(group.buttonGroup.length,vr?3:4);
    assert.deepEqual(Array.from(group._modes,b=>b.tab.textField.text),vr?['$Sliders','$Presets','$Sculpt']:['$Sliders','$Presets','$Camera','$Sculpt']);
    group.setMode(3);assert.equal(group.getMode(),3);
    group.onItemChanged({item:group.buttonGroup.selectedButton});assert.equal(tabs.at(-1).index,3);
    group.onItemRollOver({target:group.buttonGroup.selectedButton});assert.equal(tabs.at(-1).index,3);
    group.nextCategory();assert.equal(group.getMode(),0);group.previousCategory();assert.equal(group.getMode(),3);
    // A laser can activate the third visible tab by ordinal, bypassing the
    // RadioButton event. It must open Sculpt, not reject the old Camera ID.
    group.setMode(0);group.setMode(2);assert.equal(group.getMode(),vr?3:2);
    if(vr) {assert.equal(group.vrLastRequestedMode,2);assert.ok(group.vrModeRequestSequence>=3);}
  }
  console.log('Private rebuilt tabs: VR Camera removed; visible Sculpt ordinal and legacy semantic ID accepted; next/previous, selection and rollover preserved; non-VR unchanged');
}
