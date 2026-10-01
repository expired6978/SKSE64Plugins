const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../ui/diagnostics/Diagnostics.as.inc'), 'utf8');
function fixture() {
  const calls = [];
  const context = {
    RaceMenuDefines: {ENTRY_TYPE_SLIDER:2, ENTRY_TYPE_RACE:1, STATIC_SLIDER_SEX:-1, CATEGORY_RACE:2},
    _global: {eventPrefix:'test', skse:{IsVR:()=>true, plugins:{CharGen:{SetMenuExtensionValue:(...args)=>calls.push(['extension',...args])}}}},
    gfx:{io:{GameDelegate:{call:(name,args)=>calls.push([name,...args])}}},
    skse:{SendModEvent:()=>{}},
  };
  vm.createContext(context);
  vm.runInContext(source, context);
  const slider = (id=10) => ({type:2, sliderID:id,text:'Slider '+id,callbackName:'SetSlider',sliderMin:-1,sliderMax:10,interval:1,position:0,enabled:true,filterFlag:4});
  const menu = {
    modeSelect:{getMode:()=>0}, bMenuInitialized:true, racePanel:{_visible:true},
    colorField:{}, makeupPanel:{}, textEntry:{},
    categoryList:{entryList:[{flag:4,enabled:true,filterFlag:4},{flag:2,enabled:true,filterFlag:2}]},
    itemList:{entryList:[slider(),slider(11),{type:1,raceID:20,text:'Race',enabled:true}],listState:{},requestUpdate:()=>{}},
    diagnosticExtensionDispatch:(...args)=>calls.push(['extension',...args]),
    updateItemDescriptor:()=>{},onItemPress:e=>calls.push(['race',e.index]),
  };
  for (const name of source.matchAll(/^   function (\w+)\(/gm)) menu[name[1]]=context[name[1]];
  const query=()=>{menu.RefreshDiagnosticControls();return menu.diagnosticGeneration;};
  return {menu,calls,query,slider};
}
test('query and dispatch consume tokens and serialize the exact requested identity',()=>{
  const {menu,calls,query}=fixture();
  const generation=query();
  assert.equal(menu.SetDiagnosticSlider(generation,0,2),true);
  assert.deepEqual(calls[0],['SetSlider',2,10]);
  const result=JSON.parse(menu.diagnosticResultJson);
  assert.equal(result.requestGeneration,generation);
  assert.equal(result.sliderId,10); assert.equal(result.slot,0); assert.equal(result.value,2);
  assert.equal(result.callback,'SetSlider'); assert.notEqual(result.generation,generation);
  assert.equal(menu.SetDiagnosticSlider(generation,0,3),false);
  assert.equal(menu.diagnosticResult.code,'stale-generation');
  assert.equal(menu.SetDiagnosticSlider(query(),0,3),true);
});
test('ready snapshot is required and every query supersedes the previous query',()=>{
  const {menu,calls,query}=fixture();
  menu.InvalidateDiagnosticControls();
  assert.equal(menu.SetDiagnosticSlider(menu.diagnosticGeneration,0,1),false);
  assert.equal(menu.diagnosticResult.code,'snapshot-required');
  const old=query(); query();
  assert.equal(menu.SetDiagnosticSlider(old,0,1),false);assert.equal(calls.length,0);
});
for (const [name,mutate] of Object.entries({
  reorder:m=>m.itemList.entryList.reverse(),
  replace:m=>m.itemList.entryList[0]={...m.itemList.entryList[0]},
  id:m=>m.itemList.entryList[0].sliderID++,
  callback:m=>m.itemList.entryList[0].callbackName='Other',
  provider:m=>m.itemList.entryList[0].diagnosticProvider='other',
  control:m=>m.itemList.entryList[0].diagnosticControl='other',
  range:m=>m.itemList.entryList[0].sliderMax=9,
  step:m=>m.itemList.entryList[0].interval=0.5,
  manualValue:m=>m.itemList.entryList[0].position=1,
  disabled:m=>m.itemList.entryList[0].enabled=false,
  category:m=>m.categoryList.entryList[0].enabled=false,
  raceIdentity:m=>m.itemList.entryList[2].raceID=21,
})) test('rejects changed snapshot: '+name,()=>{
  const {menu,calls,query}=fixture();const generation=query();mutate(menu);
  assert.equal(menu.SetDiagnosticSlider(generation,0,2),false);
  assert.equal(menu.diagnosticResult.code,'slider-identity-changed');assert.equal(calls.length,0);
});
test('refresh/reorder cannot re-enter an action with the consumed token',()=>{
  const {menu,calls,query}=fixture();const generation=query();let reentered;
  menu.itemList.requestUpdate=()=>{menu.itemList.entryList.reverse();reentered=menu.SetDiagnosticSlider(generation,0,3);};
  assert.equal(menu.SetDiagnosticSlider(generation,0,2),true);
  assert.equal(reentered,false);assert.equal(calls.filter(c=>c[0]==='SetSlider').length,1);
  assert.equal(JSON.parse(menu.diagnosticResultJson).value,2);
});
test('extension, sex and race use menu callbacks and consume snapshots',()=>{
  const {menu,calls,query}=fixture();const entry=menu.itemList.entryList[0];
  Object.assign(entry,{diagnosticExtension:true,diagnosticProvider:'p',diagnosticControl:'c'});
  assert.equal(menu.SetDiagnosticSlider(query(),0,1),true);assert.deepEqual(calls[0],['extension','p','c',1]);
  Object.assign(entry,{diagnosticExtension:false,callbackName:'ChangeSex',sliderID:-1});
  assert.equal(menu.SetDiagnosticSlider(query(),0,1),false);assert.equal(menu.diagnosticResult.code,'use-select-sex');
  assert.equal(menu.SelectDiagnosticSex(query(),0,1),true);
  const generation=query();assert.equal(menu.SelectDiagnosticRace(generation,20),true);
  assert.deepEqual(calls.at(-1),['race',2]);
  const result=JSON.parse(menu.diagnosticResultJson);assert.equal(result.raceId,20);assert.equal(result.requestGeneration,generation);
});
test('invalid values, inactive tabs, modals and rebuilding never dispatch',()=>{
  const {menu,calls,query}=fixture();
  for(const value of [-2,11,0.5,NaN,Infinity,'2']) assert.equal(menu.SetDiagnosticSlider(query(),0,value),false);
  menu.itemList.entryList[0].interval=Infinity;assert.equal(menu.SetDiagnosticSlider(query(),0,1),false);
  for(const [set,code] of [
    [()=>menu.modeSelect.getMode=()=>2,'sliders-tab-inactive'],
    [()=>{menu.modeSelect.getMode=()=>0;menu.textEntry._visible=true;},'modal-open'],
    [()=>{menu.textEntry._visible=false;menu.diagnosticRebuilding=true;},'sliders-rebuilding'],
  ]) {set();assert.equal(menu.SetDiagnosticSlider(query(),0,1),false);assert.equal(menu.diagnosticResult.code,code);}
  assert.equal(calls.length,0);
});

test('race callbacks enter the real loading path and reject a second request',()=>{
  const {menu,calls,query}=fixture();
  menu.onItemPress=e=>{menu.bRaceChanging=true;calls.push(['race',e.index]);};
  assert.equal(menu.SelectDiagnosticRace(query(),20),true);
  assert.equal(menu.SelectDiagnosticRace(query(),20),false);
  assert.equal(menu.diagnosticResult.code,'race-change-pending');
  assert.equal(calls.length,1);
});
test('enumeration covers offered categories without switching the selected category',()=>{
  const {menu,query}=fixture();
  menu.categoryList.entryList.push({flag:8192,enabled:true,filterFlag:8192});
  menu.itemList.entryList[1].filterFlag=8192;
  query();
  const snapshot=JSON.parse(menu.diagnosticSnapshotJson);
  assert.equal(snapshot.sliders.length,2);
  assert.equal(snapshot.sliders[1].inAll,false);
  assert.equal(snapshot.allCoversEverySlider,false);
});
test('limited menu rejects races and missing extension callback rejects before token consumption',()=>{
  const {menu,calls,query}=fixture();
  menu.bLimitedMenu=true;
  assert.equal(menu.SelectDiagnosticRace(query(),20),false);
  assert.equal(menu.diagnosticResult.code,'race-disabled');
  menu.itemList.entryList[0].diagnosticExtension=true;
  menu.diagnosticExtensionDispatch=undefined;
  const token=query();
  assert.equal(menu.SetDiagnosticSlider(token,0,1),false);
  assert.equal(menu.diagnosticResult.code,'callback-unavailable');
  assert.equal(menu.diagnosticGeneration,token);
  assert.equal(calls.length,0);
});
