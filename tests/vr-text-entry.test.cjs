// Execute the actual AS2 adapter bodies; this does not qualify GFx hit testing,
// OpenVR ABI or physical SteamVR/OCU keyboard behavior.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../tools/vr-racesex-patches/TextEntry.as.inc'), 'utf8');
const objectSource = source.replace(/^   function (\w+)\(/gm, '$1(').replace(/^   }[ \t]*$/gm, '},');
const prototype = Function('return ({' + objectSource + '\n})')();
if(process.env.SKEE_REBUILT_RACEMENU_AS) {
  const rebuilt=fs.readFileSync(process.env.SKEE_REBUILT_RACEMENU_AS,'utf8');
  for(const name of ['ShowVRTextDraft','PollVRTextInput','CancelVRTextInput']) {
    const signature=rebuilt.indexOf('function '+name+'(');
    assert.ok(signature>=0,'rebuilt '+name);
    const start=rebuilt.indexOf('{',signature), parameters=rebuilt.slice(signature,start).match(/\(([^)]*)\)/)[1];
    let end=start+1, braces=1;
    for(;braces;end++) { if(rebuilt[end]==='{') braces++; if(rebuilt[end]==='}') braces--; }
    prototype[name]=Function(parameters,rebuilt.slice(start+1,end-1));
  }
  console.log('Testing rebuilt live-draft/poll/cancel functions (surrogate).');
}
let beginCalls = [], cancels = [], pollResult, events = [], names = [], cleared = [], layouts = 0;
global.setInterval = () => 7;
global.clearInterval = id => cleared.push(id);
global.skyui = {components:{SearchWidget:{S_FILTER:'$FILTER'}}};
global.Shared = {GlobalFunc:{StringTrim: value => value.trim()}};
const completionCalls=[];
global.gfx={io:{GameDelegate:{call(method,args){completionCalls.push([method,args]);}}}};
const api = {
  BeginVRTextEntry(kind, initial) { beginCalls.push([kind, initial]); return {id:12, status:'pending', error:''}; },
  PollVRTextEntry(id) { assert.equal(id, 12); return pollResult; },
  CancelVRTextEntry(id) { cancels.push(id); return {status:'cancelled'}; },
  SetCharacterName(name) { names.push(name); return true; }
};
global._global = {skse:{IsVR:()=>true,plugins:{CharGen:api}}};
function field(text, bounds) {
  // AS2 TextField is not a MovieClip and has no getBounds method.
  return {text, _visible:true, SetText(text) { this.text = text; }};
}
function button() {
  return {textField:{}, width:30, clicks:[], otherEvents:['rollOver'],
    removeEventListener(event, owner, method) { assert.equal(event,'click'); this.clicks=this.clicks.filter(([o,m])=>o!==owner||m!==method); },
    addEventListener(event, owner, method) { assert.equal(event,'click'); this.clicks.push([owner,method]); }};
}
function fixture(capacity=8, used=3) {
  const buttons = Array.from({length:capacity}, button);
  const owner = Object.assign(Object.create(prototype), {
    navPanel:{buttons, _buttonCount:used, doUpdateButtons() { layouts++; },
      updateButtons() { this.doUpdateButtons(); },
      addButton(data) {
        if(this._buttonCount >= this.buttons.length) return undefined;
        const b=this.buttons[this._buttonCount++]; b.label=data.text; return b;
      }},
    colorField:{_visible:false}, textEntry:{_visible:false}, makeupPanel:{_visible:false},
    _nameFilter:{filterText:''},
    searchWidget:{textField:field('$FILTER',{}), icon:{_visible:true},
      getBounds:()=>({xMin:260,xMax:420,yMin:-930,yMax:-904}),
      dispatchEvent(event) { events.push(event); owner._nameFilter.filterText=event.data; }},
    bottomBar:{playerInfo:{NameLabel:field('Name',{xMin:610,xMax:660,yMin:0,yMax:25}),
      PlayerName:field('Prisoner',{xMin:665,xMax:780,yMin:0,yMax:25})}},
    setStatusText(text) { this.status=text; },
    SetNameText(text) { this.bottomBar.playerInfo.PlayerName.SetText(text); this.UpdateVRNameButton(text); },
    updateBottomBar() { this.refreshes=(this.refreshes||0)+1; }
  });
  return owner;
}
// Required/dynamic existing actions retain priority, capacity failure is explicit.
for(const used of [3,4,5,6]) {
  const o=fixture(8,used), original=o.navPanel.buttons.slice(0,used);
  original.forEach(b=>b.label='existing'); o.AddVRTextButtons();
  assert.equal(o.navPanel._buttonCount,used+2);
  original.forEach(b=>assert.equal(b.label,'existing'));
  assert.equal(o.vrFilterButton.clicks[0][1],'onVRFilterButtonClicked');
  assert.equal(o.vrNameButton.clicks[0][1],'onVRNameButtonClicked');
}
const full=fixture(3,3); full.AddVRTextButtons();
assert.equal(full.vrFilterButton,undefined); assert.equal(full.vrNameButton,undefined);
assert.match(full.vrTextButtonFault,/name_capacity.*filter_capacity/);
const o=fixture(); o.SetupVRTextButtons(); const update=o.navPanel.doUpdateButtons;
o.SetupVRTextButtons(); assert.equal(o.navPanel.doUpdateButtons,update);
o.AddVRTextButtons(); o.navPanel.doUpdateButtons(); assert.equal(layouts,1);
assert.equal(o.vrFilterButton._x,260); assert.equal(o.vrFilterButton._y,-930);
assert.equal(o.vrFilterButton.width,160); assert.equal(o.vrNameButton._y,0);
assert.equal(o.vrNameButton.width,30);
assert.equal(o.vrNameButton,o.navPanel.buttons[3]);
assert.equal(o.vrFilterButton,o.navPanel.buttons[4]);
assert.equal(o.vrFilterButton._parent,undefined); // no reparenting operation exists
// Double activation only opens one session; pending polls do not commit anything.
assert.equal(o.BeginVRTextInput('filter'),true); assert.deepEqual(beginCalls,[['filter','']]);
assert.equal(o.vrFilterButton.textField.border,true);
assert.equal(o.BeginVRTextInput('name'),false); assert.equal(beginCalls.length,1);
pollResult={status:'pending'}; o.PollVRTextInput(); assert.equal(events.length,0);
pollResult={status:'pending',text:'hai'}; o.PollVRTextInput();
assert.equal(o.vrFilterButton.label,'hai'); assert.equal(o.GetVRFilterText(),'');
assert.equal(events.length,0);
pollResult={status:'accepted',text:'hair',error:''}; o.PollVRTextInput();
assert.equal(o.searchWidget.textField.text,'hair'); assert.deepEqual(events,[{type:'inputEnd',data:'hair'}]);
assert.equal(o.vrFilterButton.textField.border,false); assert.equal(o.vrTextInputActive,false);
// Cancelling leaves the accepted filter/name unchanged, removes feedback/timer.
o.BeginVRTextInput('filter'); pollResult={status:'cancelled',text:'discard me',error:''}; o.PollVRTextInput();
assert.equal(o.searchWidget.textField.text,'hair'); assert.equal(events.length,1);
assert.equal(o.vrFilterButton.label,'hair');
o.BeginVRTextInput('filter'); pollResult={status:'unavailable',text:'é'.repeat(128),error:'text_exceeds_byte_budget'};
o.PollVRTextInput(); assert.equal(o.searchWidget.textField.text,'hair'); assert.equal(events.length,1);
assert.equal(o.vrTextInputError,'text_exceeds_byte_budget');
o.BeginVRTextInput('name'); pollResult={status:'pending',text:'  Hero  ',error:''}; o.PollVRTextInput();
assert.equal(o.vrNameButton.label,'Name:   Hero  '); assert.deepEqual(names,[]);
assert.equal(o.bottomBar.playerInfo.PlayerName.text,'Prisoner');
pollResult={status:'accepted',text:'  Hero  ',error:''}; o.PollVRTextInput();
assert.deepEqual(names,['Hero']); assert.equal(o.bottomBar.playerInfo.PlayerName.text,'Hero');
assert.equal(o.vrNameButton.label,'Name: Hero');
o.BeginVRTextInput('name'); pollResult={status:'accepted',text:'   ',error:''}; o.PollVRTextInput();
assert.deepEqual(names,['Hero']); assert.equal(o.vrTextInputError,'invalid_or_rejected_name');
o.BeginVRTextInput('name'); o.onUnload(); assert.deepEqual(cancels,[12]);
assert.equal(o.vrTextInputActive,false); assert.equal(o.vrTextInputTimer,undefined);
assert.ok(cleared.includes(7));
// Translated placeholder is display-only, not keyboard input. Genuine entered
// text equal to that word still survives because the model is authoritative.
const localized=fixture(); localized.searchWidget.textField.text='FILTER';
localized.BeginVRTextInput('filter'); assert.deepEqual(beginCalls.at(-1),['filter','']);
localized.CancelVRTextInput(); localized._nameFilter.filterText='FILTER';
localized.BeginVRTextInput('filter'); assert.deepEqual(beginCalls.at(-1),['filter','FILTER']);
localized.CancelVRTextInput();
// Rebuild removes stale text-action clicks before assigning current actions.
const otherConsumer={}; o.navPanel.buttons[0].clicks.push([otherConsumer,'onDoneClicked']);
o.ResetVRTextButtons(); o.navPanel.buttons.forEach((b,i)=>{
  assert.deepEqual(b.clicks,i===0?[[otherConsumer,'onDoneClicked']]:[]); assert.deepEqual(b.otherEvents,['rollOver']);
  assert.equal(b._y,0);
});
assert.equal(o.vrFilterButton,undefined); assert.equal(o.vrNameButton,undefined);
// Reused Name can take a slot previously displaced to Filter's top row.
const recycled=fixture(8,4); recycled.AddVRTextButtons(); recycled.PositionVRTextButtons();
const oldFilter=recycled.vrFilterButton;
recycled.ResetVRTextButtons(); recycled.navPanel._buttonCount=5;
recycled.AddVRTextButtons(); recycled.PositionVRTextButtons();
assert.equal(recycled.vrNameButton,oldFilter); assert.equal(recycled.vrNameButton._y,0);
const missing=fixture(); const saved=api.BeginVRTextEntry; delete api.BeginVRTextEntry;
assert.equal(missing.BeginVRTextInput('filter'),false); assert.equal(missing.vrTextInputError,'keyboard_api_unavailable');
api.BeginVRTextEntry=()=>({id:0,status:'unavailable',error:'keyboard_busy'});
assert.equal(missing.BeginVRTextInput('filter'),false); assert.equal(missing.vrTextInputActive,undefined);
api.BeginVRTextEntry=saved;
const diagnostic=fixture(); diagnostic.vrInputTraceArmed=true;
const beforeDiagnostic=beginCalls.length;
assert.equal(diagnostic.BeginVRTextInput('name'),false);
assert.equal(diagnostic.BeginVRTextInput('filter'),false);
assert.equal(beginCalls.length,beforeDiagnostic);
// Explicit creation exit uses the accepted name and final engine callback,
// never the keyboard again or the non-finishing rename API.
const exiting=fixture(); exiting.bottomBar.playerInfo.PlayerName.text='  Butch  ';
assert.equal(exiting.FinishVRCharacterCreation(),true);
assert.deepEqual(completionCalls,[['ChangeName',['Butch']]]);
exiting.vrTextInputActive=true;
assert.equal(exiting.FinishVRCharacterCreation(),false); assert.equal(completionCalls.length,1);
exiting.vrTextInputActive=false; exiting.bottomBar.playerInfo.PlayerName.text='   ';
assert.equal(exiting.FinishVRCharacterCreation(),false); assert.equal(completionCalls.length,1);
assert.match(exiting.status,/enter a character name/);
const patcher=fs.readFileSync(path.join(__dirname,'../tools/patch-vr-racesex-swf.ps1'),'utf8');
assert.match(patcher,/if\(_global\.skse\.IsVR\(\)\).*FinishVRCharacterCreation/s);
// Source guardrails supplement, but do not substitute for native/runtime tests.
const native=fs.readFileSync(path.join(__dirname,'../skee64/RaceSexMenuVRKeyboard.cpp'),'utf8');
assert.doesNotMatch(native,/std::async|std::thread|\.wait\(|\.get\(\).*future/);
assert.match(native,/i < 32/); assert.match(native,/kKeyboardTimeout/);
assert.match(native,/ReadBufferedDraft\(g_session\.streamed, g_session\.text/);
assert.match(native,/The existing UI timer[\s\S]*?RefreshBufferedDraft\(\);\s*}\s*class KeyboardFunction/);
assert.match(native,/menu->uiMovie.get\(\) == a_movie/);
assert.match(native,/g_session.owner != a_params.movie/);
assert.match(native,/if \(g_session.ownsKeyboard\) g_session.overlay->HideKeyboard/);
// Rename and creation completion must not share the final naming routine.
// These source checks would catch the regression missed by the API mock.
const rename=fs.readFileSync(path.join(__dirname,'../skee64/ScaleformCharGenFunctions.cpp'),'utf8');
const renameBody=rename.slice(rename.indexOf('void SKSEScaleform_SetCharacterName::Call'),rename.indexOf('extern float\tg_sculptOffsetX'));
assert.match(renameBody,/UpdateCharacterNameWithoutFinishing/);
assert.doesNotMatch(renameBody,/ChangeName|kHide|AddMessage/);
assert.match(renameBody,/menu->uiMovie.get\(\) == a_params.movie/);
const creation=fs.readFileSync(path.join(__dirname,'../skee64/CharacterCreationInterface.cpp'),'utf8');
assert.match(creation,/#if defined\(ENABLE_SKYRIM_VR\)[\s\S]*GetSetting\("sRSMConfirm"\)[\s\S]*GetType\(\) == RE::Setting::Type::kString[\s\S]*DispatchStaticCall\("Game", "SetGameSettingString"[\s\S]*BSFixedString\("Exit Character Creation\?"\)[\s\S]*#endif/);
assert.doesNotMatch(creation,/confirmation->SetString/);
const queuedRename=creation.slice(creation.indexOf('CharacterCreationInterface::QueueName'),creation.indexOf('void CharacterCreationInterface::FinishOnGameThread'));
assert.match(queuedRename,/UpdateCharacterNameWithoutFinishing/);
assert.doesNotMatch(queuedRename,/ChangeName|kHide|AddMessage/);
assert.match(queuedRename,/sessionGeneration_\.load\(\) != generation \|\| state_\.load\(\) != kReady/);
assert.match(queuedRename,/a_configured && !SKEE::VR::MenuOptionsPolicy::ApplyPlayerName/);
assert.match(queuedRename,/if \(a_configured\)\s*\{\s*nameStartIntent_\.Consume\(\)/);
const queuedFinish=creation.slice(creation.indexOf('CharacterCreationInterface::QueueFinish'),creation.indexOf('void CharacterCreationInterface::CancelConfiguredName'));
assert.match(queuedFinish,/const auto generation = sessionGeneration_\.load\(\)/);
assert.match(queuedFinish,/SessionLease::IsCurrent\([\s\S]*generation[\s\S]*kFinishing/);
assert.match(queuedFinish,/\[this, generation, name = std::move\(a_name\), a_useCurrentName\]/);
const main=fs.readFileSync(path.join(__dirname,'../skee64/main.cpp'),'utf8');
assert.match(main,/case SKSE::MessagingInterface::kNewGame:[\s\S]*?g_characterCreationInterface.BeginNewGame\(\)/);
assert.match(main,/case SKSE::MessagingInterface::kPreLoadGame:[\s\S]*?g_characterCreationInterface.OnSaveLoading\(\)/);
assert.match(main,/case SKSE::MessagingInterface::kPostLoadGame:[\s\S]*?if \(!message->data\) g_characterCreationInterface.CancelConfiguredName\(\)/);
const startObserver=fs.readFileSync(path.join(__dirname,'../skee64/VRNewGameIntent.cpp'),'utf8');
assert.match(startObserver,/"StartNewGame"/);
assert.doesNotMatch(startObserver,/"NEW"|ChangeName|SetFullName/);
assert.match(startObserver,/menu->uiMovie.get\(\) == args.GetMovie\(\)/);
assert.match(startObserver,/original\(args\)/);
assert.match(startObserver,/originalAccept\(menu, &observer\)/);
assert.match(startObserver,/target != module.base\(\) \+ 0x8CFC00/);
assert.match(creation,/state_\.store\(kReady\);\s*ApplyConfiguredName\(\)/);
const pointer=fs.readFileSync(path.join(__dirname,'../skee64/RaceSexMenuVRInput.cpp'),'utf8');
const constructor=pointer.slice(pointer.indexOf('bool RaceSexMenuLoadMovieHook'),pointer.indexOf('bool RaceSexMenuCanProcessHook'));
assert.ok(constructor.indexOf('ConfigureQuill') < constructor.indexOf('auto loaded ='));
assert.match(constructor,/ConfigureQuill\(a_menu->menuFlags,[\s\S]*?UseQuill\(\)/);
const repair=main.slice(main.indexOf('void ConfigureRaceSexMouseCursor'),main.indexOf('class RaceSexMouseCursorSink'));
assert.doesNotMatch(repair,/menuFlags\.(set|reset)/,'late movie repair must not change cursor ownership');
assert.match(creation.slice(creation.indexOf('void CharacterCreationInterface::Revert'),creation.indexOf('bool CharacterCreationInterface::IsActive')),/sessionGeneration_\.fetch_add\(1\)/);
assert.match(creation.slice(creation.indexOf('CharacterCreationInterface::ProcessEvent'),creation.indexOf('void CharacterCreationInterface::ObserveCurrentState')),/sessionGeneration_\.fetch_add\(1\)/);
const finish=creation.slice(creation.indexOf('void CharacterCreationInterface::FinishOnGameThread'));
assert.match(finish,/menu->ChangeName/);
assert.match(finish,/UI_MESSAGE_TYPE::kHide/);
const nameUpdate=fs.readFileSync(path.join(__dirname,'../skee64/CharacterNameUpdate.h'),'utf8');
assert.match(nameUpdate,/SetFullName\(a_name\)/);
assert.match(nameUpdate,/AddChange\(RE::TESNPC::ChangeFlags::kFullName\)/);
assert.doesNotMatch(nameUpdate,/menu->ChangeName|kHide|AddMessage/);
const privateSearchWidget=path.join(__dirname,'../tools/vr-racesex-patches/SearchWidget.as');
if(fs.existsSync(privateSearchWidget)) {
  const search=fs.readFileSync(privateSearchWidget,'utf8');
  assert.doesNotMatch(search,/ShowVirtualKeyboard/);
  // Optional private-asset qualification: execute the extracted widget's
  // startInput body without distributing the original menu source.
  const bodyStart=search.indexOf('{',search.indexOf('function startInput()'));
  let depth=1, bodyEnd=bodyStart+1;
  for(;depth;bodyEnd++) { if(search[bodyEnd]==='{') depth++; if(search[bodyEnd]==='}') depth--; }
  const startInput=Function(search.slice(bodyStart+1,bodyEnd-1));
  let searchBegins=0;
  const widget={isDisabled:true,vrTextOwner:{BeginVRTextInput(kind){assert.equal(kind,'filter');searchBegins++;}}};
  startInput.call(widget); assert.equal(searchBegins,0);
  widget.isDisabled=false; widget._bActive=true; startInput.call(widget); assert.equal(searchBegins,0);
  widget._bActive=false; startInput.call(widget); assert.equal(searchBegins,1);
}
assert.doesNotMatch(source,/attachMovie|Selection\.setFocus|AllowTextInput/);
if(process.env.SKEE_REBUILT_RACEMENU_AS) {
  const rebuilt=fs.readFileSync(process.env.SKEE_REBUILT_RACEMENU_AS,'utf8');
  const start=rebuilt.indexOf('{',rebuilt.indexOf('function ShowTextEntryField()'));
  assert.ok(start>=0);
  let end=start+1, braces=1;
  for(;braces;end++) { if(rebuilt[end]==='{') braces++; if(rebuilt[end]==='}') braces--; }
  const exitCallback=Function(rebuilt.slice(start+1,end-1));
  const callbackOwner=fixture(); let finishes=0;
  callbackOwner.FinishVRCharacterCreation=()=>{finishes++;return true;};
  completionCalls.length=0; exitCallback.call(callbackOwner);
  assert.equal(finishes,1); assert.deepEqual(completionCalls,[]);
  const isVR=_global.skse.IsVR;
  _global.skse.IsVR=()=>false;
  try {
    callbackOwner.textEntry.enabled=false; exitCallback.call(callbackOwner);
    assert.equal(finishes,1);
    assert.deepEqual(completionCalls,[['ShowVirtualKeyboard',[]],['PlaySound',['UIMenuBladeOpenSD']]]);
  } finally { _global.skse.IsVR=isVR; }
  console.log('Rebuilt exit callback: VR skips naming keyboard; flat callback retained (surrogate).');
}
console.log('VR text adapter tests passed (surrogate only; live keyboard/input unqualified).');
