   // Experimental menu-owned diagnostics; no engine writes or VR dependency.
   // Attach to the live RaceMenu owner; invoke on the UI thread only.
   // A generation is valid only for this movie's current live slider set.
   function InvalidateDiagnosticControls()
   {
      if(this.diagnosticGeneration == undefined)
         this.diagnosticGeneration = Math.floor(Math.random() * 8000000) + 1;
      else
         this.diagnosticGeneration = this.diagnosticGeneration % 16000000 + 1;
      this.diagnosticSnapshot = undefined;
      this.diagnosticIdentity = undefined;
      this.diagnosticSnapshotJson = "";
   }
   function DiagnosticIsSexSlider(entry)
   {
      return entry.callbackName == "ChangeSex" || entry.sliderID == RaceMenuDefines.STATIC_SLIDER_SEX;
   }
   function DiagnosticJsonQuote(value)
   {
      var text = String(value);
      var result = '"';
      var slash = String.fromCharCode(92);
      for(var i = 0; i < text.length; i++)
      {
         var ch = text.charAt(i);
         if(ch == '"') result += slash + '"';
         else if(ch == slash) result += slash + slash;
         else if(ch == String.fromCharCode(10)) result += slash + 'n';
         else if(ch == String.fromCharCode(13)) result += slash + 'r';
         else if(ch == String.fromCharCode(9)) result += slash + 't';
         else if(text.charCodeAt(i) < 32)
         {
            var code = text.charCodeAt(i).toString(16);
            while(code.length < 4) code = '0' + code;
            result += slash + 'u' + code;
         }
         else result += ch;
      }
      return result + '"';
   }
   function DiagnosticJsonNumber(value)
   {
      return typeof value == "number" && !isNaN(value) && Math.abs(value) < 1e308 ? String(value) : "null";
   }
   function PublishDiagnosticSnapshotJson()
   {
      var data = this.diagnosticSnapshot;
      var result = '{"schema":1,"status":' + this.DiagnosticJsonQuote(data.status) +
         ',"generation":' + this.DiagnosticJsonNumber(data.generation) +
         ',"mode":' + this.DiagnosticJsonNumber(data.mode) +
         ',"allCoversEverySlider":' + String(data.allCoversEverySlider) + ',"sliders":[';
      for(var i = 0; i < data.sliders.length; i++)
      {
         var s = data.sliders[i];
         if(i > 0) result += ',';
         result += '{"slot":' + this.DiagnosticJsonNumber(s.slot) +
            ',"id":' + this.DiagnosticJsonNumber(s.id) +
            ',"name":' + this.DiagnosticJsonQuote(s.name) +
            ',"callback":' + this.DiagnosticJsonQuote(s.callback) +
            ',"minimum":' + this.DiagnosticJsonNumber(s.minimum) +
            ',"maximum":' + this.DiagnosticJsonNumber(s.maximum) +
            ',"step":' + this.DiagnosticJsonNumber(s.step) +
            ',"value":' + this.DiagnosticJsonNumber(s.value) +
            ',"enabled":' + String(s.enabled) +
            ',"categoryMask":' + this.DiagnosticJsonNumber(s.categoryMask) +
            ',"inAll":' + String(s.inAll) +
            ',"action":' + this.DiagnosticJsonQuote(s.action) + '}';
      }
      result += '],"races":[';
      for(var i = 0; i < data.races.length; i++)
      {
         var race = data.races[i];
         if(i > 0) result += ',';
         result += '{"slot":' + this.DiagnosticJsonNumber(race.slot) +
            ',"id":' + this.DiagnosticJsonNumber(race.id) +
            ',"name":' + this.DiagnosticJsonQuote(race.name) +
            ',"active":' + String(race.active) +
            ',"enabled":' + String(race.enabled) + '}';
      }
      this.diagnosticSnapshotJson = result + ']}';
   }
   function PublishDiagnosticResultJson()
   {
      var result = this.diagnosticResult;
      this.diagnosticResultJson = '{"ok":' + String(result.ok) +
         ',"code":' + this.DiagnosticJsonQuote(result.code) +
         ',"message":' + this.DiagnosticJsonQuote(result.message) +
         ',"generation":' + this.DiagnosticJsonNumber(result.generation) +
         ',"requestGeneration":' + this.DiagnosticJsonNumber(this.diagnosticRequest.generation) +
         ',"slot":' + this.DiagnosticJsonNumber(this.diagnosticRequest.slot) +
         ',"value":' + this.DiagnosticJsonNumber(this.diagnosticRequest.value) +
         ',"raceId":' + this.DiagnosticJsonNumber(this.diagnosticRequest.raceId) +
         ',"sliderId":' + this.DiagnosticJsonNumber(this.diagnosticRequest.sliderId) +
         ',"callback":' + this.DiagnosticJsonQuote(this.diagnosticRequest.callback) + '}';
   }
   function DiagnosticAvailability()
   {
      if(this.modeSelect == undefined || this.modeSelect.getMode == undefined ||
         this.itemList == undefined || this.categoryList == undefined ||
         this.categoryList.entryList == undefined || this.categoryList.entryList.length == 0 ||
         this.racePanel == undefined)
         return "menu-not-ready";
      if(this.modeSelect.getMode() != 0) return "sliders-tab-inactive";
      if(this.bRaceChanging) return "race-change-pending";
      if(!this.bMenuInitialized || this.diagnosticRebuilding) return "sliders-rebuilding";
      if(!this.racePanel._visible || this.itemList.entryList == undefined) return "sliders-unavailable";
      if(this.colorField != undefined && this.colorField._visible ||
         this.makeupPanel != undefined && this.makeupPanel._visible ||
         this.textEntry != undefined && this.textEntry._visible)
         return "modal-open";
      if(this.itemList.disableInput || this.itemList.disableSelection) return "slider-input-disabled";
      return "ready";
   }
   function RefreshDiagnosticControls()
   {
      // Each query publishes a new token, even if a previous query was ready.
      this.InvalidateDiagnosticControls();
      var state = this.DiagnosticAvailability();
      var snapshot = {schema:1,status:state,generation:this.diagnosticGeneration,
                      mode:this.modeSelect != undefined && this.modeSelect.getMode != undefined ? this.modeSelect.getMode() : -1,
                      sliders:[],races:[],allCoversEverySlider:true};
      this.diagnosticSnapshot = snapshot;
      if(state != "ready")
      {
         this.PublishDiagnosticSnapshotJson();
         return 0;
      }
      var entries = this.itemList.entryList;
      this.diagnosticIdentity = {entries:this.DiagnosticCaptureIdentity(entries),
         categories:this.DiagnosticCaptureIdentity(this.categoryList.entryList)};
      var allMask = this.categoryList.entryList[0].flag;
      var raceEnabled = this.DiagnosticRaceCategoryEnabled();
      for(var i = 0; i < entries.length; i++)
      {
         var entry = entries[i];
         if(entry.type == RaceMenuDefines.ENTRY_TYPE_SLIDER && this.DiagnosticSliderOffered(entry))
         {
            var enabled = Boolean(entry.enabled);
            if(entry.hasColor != undefined && entry.hasColor() &&
               entry.isColorEnabled != undefined && !entry.isColorEnabled()) enabled = false;
            var inAll = (entry.filterFlag & allMask) != 0;
            if(!inAll) snapshot.allCoversEverySlider = false;
            snapshot.sliders.push({slot:i,id:entry.sliderID,name:entry.text,
               callback:entry.callbackName,minimum:entry.sliderMin,maximum:entry.sliderMax,
               step:entry.interval,value:entry.position,enabled:enabled,
               categoryMask:entry.filterFlag,inAll:inAll,
               action:this.DiagnosticIsSexSlider(entry) ? "select-sex" : "set-slider"});
         }
         else if(entry.type == RaceMenuDefines.ENTRY_TYPE_RACE)
         {
            snapshot.races.push({slot:i,id:entry.raceID,name:entry.text,
               active:entry == this.itemList.listState.activeEntry,enabled:raceEnabled && Boolean(entry.enabled)});
         }
      }
      this.PublishDiagnosticSnapshotJson();
      return snapshot.sliders.length;
   }
   function DiagnosticRaceCategoryEnabled()
   {
      if(this.bLimitedMenu) return false;
      for(var i = 0; i < this.categoryList.entryList.length; i++)
      {
         var category = this.categoryList.entryList[i];
         if(category.flag == RaceMenuDefines.CATEGORY_RACE)
            return Boolean(category.enabled) && category.filterFlag != 0;
      }
      return false;
   }
   function DiagnosticCaptureIdentity(entries)
   {
      // Freeze routing, range and availability values separately from the live
      // objects. Object identity alone does not detect in-place list updates.
      var fields = ["type","sliderID","raceID","text","callbackName","diagnosticExtension",
         "diagnosticProvider","diagnosticControl","sliderMin","sliderMax","interval","position",
         "enabled","filterFlag","flag","hasColor","isColorEnabled"];
      var captured = [];
      for(var i = 0; i < entries.length; i++)
      {
         var values = {};
         for(var j = 0; j < fields.length; j++) values[fields[j]] = entries[i][fields[j]];
         captured.push({entry:entries[i],values:values});
      }
      return captured;
   }
   function DiagnosticIdentityMatches(captured, entries)
   {
      if(captured == undefined || captured.length != entries.length) return false;
      for(var i = 0; i < captured.length; i++)
      {
         if(captured[i].entry !== entries[i]) return false;
         for(var field in captured[i].values)
            if(captured[i].values[field] !== entries[i][field]) return false;
      }
      return true;
   }
   function DiagnosticSliderOffered(entry)
   {
      // Include All: hidden extension sections deliberately keep their
      // controls reachable through the All category.
      for(var i = 0; i < this.categoryList.entryList.length; i++)
      {
         var category = this.categoryList.entryList[i];
         if(category.enabled && category.filterFlag != 0 &&
            (entry.filterFlag & category.flag) != 0) return true;
      }
      return false;
   }
   function DiagnosticReject(code, message)
   {
      this.diagnosticResult = {ok:false,code:code,message:message,
                                 generation:this.diagnosticGeneration};
      this.PublishDiagnosticResultJson();
      return false;
   }
   function DiagnosticValidateRequest(generation)
   {
      var state = this.DiagnosticAvailability();
      if(state != "ready") return this.DiagnosticReject(state,"RaceMenu sliders are not currently available: " + state);
      if(typeof generation != "number" || generation != this.diagnosticGeneration)
         return this.DiagnosticReject("stale-generation","The live menu or slider set changed; query controls again");
      if(this.diagnosticSnapshot == undefined || this.diagnosticSnapshot.status != "ready" ||
         this.diagnosticSnapshot.generation != generation || this.diagnosticIdentity == undefined)
         return this.DiagnosticReject("snapshot-required","Query the ready live controls before submitting an action");
      if(!this.DiagnosticIdentityMatches(this.diagnosticIdentity.entries,this.itemList.entryList) ||
         !this.DiagnosticIdentityMatches(this.diagnosticIdentity.categories,this.categoryList.entryList))
      {
         this.InvalidateDiagnosticControls();
         return this.DiagnosticReject("slider-identity-changed","The published controls changed or reordered; query controls again");
      }
      return true;
   }
   function DiagnosticFindEntry(slot, type)
   {
      if(typeof slot != "number" || isNaN(slot) || slot < 0 || Math.floor(slot) != slot ||
         slot >= this.itemList.entryList.length) return null;
      var entry = this.itemList.entryList[slot];
      return entry != undefined && entry.type == type ? entry : null;
   }
   function DiagnosticValidValue(entry, value)
   {
      if(typeof value != "number" || isNaN(value) || Math.abs(value) >= 1e308 ||
         typeof entry.sliderMin != "number" || typeof entry.sliderMax != "number" ||
         isNaN(entry.sliderMin) || isNaN(entry.sliderMax) ||
         Math.abs(entry.sliderMin) >= 1e308 || Math.abs(entry.sliderMax) >= 1e308 ||
         value < entry.sliderMin || value > entry.sliderMax) return false;
      var step = Number(entry.interval);
      if(isNaN(step) || Math.abs(step) >= 1e308 || step < 0) return false;
      if(step > 0)
      {
         var units = (value - entry.sliderMin) / step;
         if(Math.abs(units - Math.round(units)) > 0.0001) return false;
      }
      return true;
   }
   function DiagnosticApplySlider(generation, slot, value, allowSex)
   {
      this.diagnosticRequest = {generation:generation,slot:slot,value:value};
      if(!this.DiagnosticValidateRequest(generation)) return false;
      var entry = this.DiagnosticFindEntry(slot,RaceMenuDefines.ENTRY_TYPE_SLIDER);
      if(entry == null) return this.DiagnosticReject("slider-not-live","That slot is not a live slider; query controls again");
      this.diagnosticRequest.sliderId = entry.sliderID;
      this.diagnosticRequest.callback = entry.callbackName;
      var request = this.diagnosticRequest;
      if(!this.DiagnosticSliderOffered(entry))
         return this.DiagnosticReject("slider-not-offered","The live menu offers no category for this slider");
      if(this.DiagnosticIsSexSlider(entry) && !allowSex)
         return this.DiagnosticReject("use-select-sex","Sex changes require SelectDiagnosticSex");
      if(!this.DiagnosticIsSexSlider(entry) && allowSex)
         return this.DiagnosticReject("not-sex-slider","That slot is not the live sex selector");
      if(!entry.enabled || entry.hasColor != undefined && entry.hasColor() &&
         entry.isColorEnabled != undefined && !entry.isColorEnabled())
         return this.DiagnosticReject("slider-disabled","The live menu has disabled this slider");
      if(!this.DiagnosticValidValue(entry,value))
         return this.DiagnosticReject("invalid-value","Value must lie within the live slider range and step");
      if(entry.diagnosticExtension)
      {
         if(typeof this.diagnosticExtensionDispatch != "function")
            return this.DiagnosticReject("callback-unavailable","The extension menu callback is unavailable");
         this.InvalidateDiagnosticControls();
         // This GFx callback is asynchronous and returns no acknowledgement.
         this.diagnosticExtensionDispatch(entry.diagnosticProvider,entry.diagnosticControl,value);
      }
      else
      {
         if(entry.callbackName == undefined || entry.callbackName == "")
            return this.DiagnosticReject("callback-unavailable","This slider has no active menu callback");
         this.InvalidateDiagnosticControls();
         // This is the same GameDelegate callback and argument pair used by
         // SliderListEntry.changedCallback, not a direct engine-data write.
         gfx.io.GameDelegate.call(entry.callbackName,[value,entry.sliderID]);
         gfx.io.GameDelegate.call("PlaySound",["UIMenuFocus"]);
         skse.SendModEvent(_global.eventPrefix + "SliderChange",entry.callbackName,value);
      }
      entry.position = value;
      if(!entry.diagnosticExtension && entry.sliderID != undefined &&
         _global.skse.plugins.CharGen.GetSliderData != undefined)
         entry.extraData = _global.skse.plugins.CharGen.GetSliderData(entry.sliderID,value);
      this.itemList.requestUpdate();
      if(this.itemList.listState.selectedEntry == entry) this.updateItemDescriptor();
      this.diagnosticRequest = request;
      this.diagnosticResult = {ok:true,code:"dispatched",message:"Menu callback dispatched; observe the resulting menu state",
                                 generation:this.diagnosticGeneration,slot:slot,value:value};
      this.PublishDiagnosticResultJson();
      return true;
   }
   function SetDiagnosticSlider(generation, slot, value)
   {
      return this.DiagnosticApplySlider(generation,slot,value,false);
   }
   function SelectDiagnosticSex(generation, slot, value)
   {
      return this.DiagnosticApplySlider(generation,slot,value,true);
   }
   function SelectDiagnosticRace(generation, raceId)
   {
      this.diagnosticRequest = {generation:generation,raceId:raceId};
      var request = this.diagnosticRequest;
      if(!this.DiagnosticValidateRequest(generation)) return false;
      if(typeof raceId != "number" || isNaN(raceId) || Math.floor(raceId) != raceId)
         return this.DiagnosticReject("invalid-race","Race ID must be one of the enumerated live choices");
      if(!this.DiagnosticRaceCategoryEnabled())
         return this.DiagnosticReject("race-disabled","The live menu does not permit race selection");
      for(var i = 0; i < this.itemList.entryList.length; i++)
      {
         var entry = this.itemList.entryList[i];
         if(entry.type != RaceMenuDefines.ENTRY_TYPE_RACE || entry.raceID != raceId) continue;
         if(!entry.enabled) return this.DiagnosticReject("race-disabled","The selected race is disabled");
         if(entry == this.itemList.listState.activeEntry)
            return this.DiagnosticReject("already-selected","That race is already active");
         // onItemPress is the physical race-row path: it sets the active row,
         // opens the loading lifecycle and invokes Skyrim's ChangeRace callback.
         this.InvalidateDiagnosticControls();
         this.onItemPress({index:i});
         this.diagnosticRequest = request;
         this.diagnosticResult = {ok:true,code:"dispatched",message:"Race selection dispatched; wait for the menu to rebuild",
                                    generation:this.diagnosticGeneration,raceId:raceId};
         this.PublishDiagnosticResultJson();
         return true;
      }
      return this.DiagnosticReject("race-not-live","That race is not offered by the live menu");
   }
