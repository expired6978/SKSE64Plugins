   // Diagnostic API only: no wrappers, listener, timer or keyboard changes
   // exist until explicitly armed. It does not inject any input.
   function ReadVRInputTargetSnapshot()
   {
      // Inspect only a fixed set of clips. No handlers, focus or keyboard APIs
      // are called, and this does not arm recording or change routing.
      var receivers = [this,this.racePanel,this.searchWidget,this.vrFilterButton,this.bottomBar,this.bottomBar.playerInfo,this.vrNameButton,this.navPanel];
      var labels = ["owner","racePanel","searchWidget","filter","bottomBar","playerInfo","name","buttonPanel"];
      var i = 0;
      while(i < Math.min(this.navPanel.buttons.length,6))
      {
         receivers.push(this.navPanel.buttons[i]);
         labels.push("stockButton" + i);
         i++;
      }
      var result = [];
      i = 0;
      while(i < receivers.length)
      {
         var clip = receivers[i];
         var row = {role:labels[i],present:clip != undefined};
         if(clip != undefined)
         {
            row.target = String(clip._target);
            if(clip._parent != undefined) row.parentTarget = String(clip._parent._target);
            row.initialized = clip.initialized;
            row.enabled = clip.enabled;
            row.disabled = clip.disabled;
            row.focusEnabled = clip.focusEnabled;
            row.visible = clip._visible;
            row.alpha = clip._alpha;
            if(typeof clip.getDepth == "function") row.depth = clip.getDepth();
            row.label = clip.label;
            row.onPressKind = typeof clip.onPress;
            row.onReleaseKind = typeof clip.onRelease;
            row.handleMousePressKind = typeof clip.handleMousePress;
            row.handleMouseReleaseKind = typeof clip.handleMouseRelease;
            row.hitAreaKind = typeof clip.hitArea;
            if(clip.hitArea != undefined) row.hitAreaTarget = clip.hitArea._target;
            row.hitTestDisable = clip.hitTestDisable;
            if(typeof clip.getBounds == "function")
            {
               row.bounds = clip.getBounds(_root);
               var x = (row.bounds.xMin + row.bounds.xMax) / 2;
               var y = (row.bounds.yMin + row.bounds.yMax) / 2;
               if(typeof clip.hitTest == "function") row.centerShapeHit = clip.hitTest(x,y,true);
            }
            if(clip.background != undefined && typeof clip.background.getBounds == "function") row.backgroundBounds = clip.background.getBounds(_root);
         }
         result.push(row);
         i++;
      }
      this.vrInputTargetSnapshot = result;
      return true;
   }
   function RecordVRInputHandler(a_receiver, a_name, a_arguments)
   {
      if(!this.vrInputTraceArmed) return undefined;
      if(a_name == "dispatchEvent" && a_arguments[0].type == "stateChange")
      {
         this.vrInputTraceIgnoredStateChanges++;
         return undefined;
      }
      var point = {x:_root._xmouse,y:_root._ymouse};
      _root.localToGlobal(point);
      var index = a_arguments[0];
      var button = a_arguments[2];
      var name = a_name;
      var state = String(a_receiver.state);
      if(a_name == "handleInput")
      {
         var details = a_arguments[0].details;
         index = a_arguments[0].controllerIdx;
         button = undefined;
         state = "code=" + String(details.code) + ";value=" + String(details.value) + ";nav=" + String(details.navEquivalent);
      }
      if(a_name == "handleClick" || a_name == "onReleaseOutside") button = a_arguments[1];
      if(a_name == "dispatchEvent")
      {
         name += ":" + String(a_arguments[0].type);
         index = a_arguments[0].controllerIdx;
         button = a_arguments[0].button;
      }
      var ok = _global.skse.plugins.CharGen.RecordVRMovieInput(name,String(a_receiver._target),Number(a_receiver._xmouse),Number(a_receiver._ymouse),Number(index),Number(button),point.x,point.y,Number(a_receiver.getDepth()),state);
      if(!ok) this.vrInputTraceFault = "native_record_rejected";
   }
   function ArmVRStockButtonAssay()
   {
      // Compare a pre-created, ordinary renderer with the added hit overlays.
      // No keyboard or input injection: the test button only reaches Zoom.
      var button = this.navPanel.buttons[3];
      if(this.vrInputTraceArmed || this.vrStockButtonAssayActive || this.navPanel._buttonCount > 3 || button == undefined || button._visible || button.label != "") return false;
      if(typeof button.setButtonData != "function" || button._listeners == undefined || button._keyCodes == undefined) return false;
      if(typeof this.navPanel.doUpdateButtons != "function" || !this.ArmVRInputTrace()) return false;
      this.vrStockButtonAssaySaved = {button:button,label:button.label,visible:button._visible,x:button._x,y:button._y,click:button._listeners.click,keyCodes:button._keyCodes.slice(),update:this.navPanel.doUpdateButtons,takenOver:false};
      this.vrStockButtonAssayActive = true;
      var owner = this;
      var original = this.navPanel.doUpdateButtons;
      this.vrStockButtonAssayUpdate = function()
      {
         // A real menu action may claim this slot during refresh. Do not let
         // addEventListener retain the assay's Zoom listener on that action.
         var saved = owner.vrStockButtonAssaySaved;
         if(saved != undefined && saved.button._listeners.click === saved.assayClicks)
         {
            // updateBottomBar assigns labels/listeners BEFORE doUpdateButtons.
            // Keep any freshly appended real listener, removing only ours.
            saved.assayClicks.splice(0,1);
            if(saved.assayClicks.length == 0) saved.button._listeners.click = saved.click;
         }
         var result;
         try { result = original.apply(this,arguments); }
         catch(error)
         {
            if(saved != undefined && this.buttons[3] === saved.button && this._buttonCount > 3) saved.takenOver = true;
            owner.DisarmVRInputTrace();
            throw error;
         }
         owner.PlaceVRStockButtonAssay();
         return result;
      };
      this.navPanel.doUpdateButtons = this.vrStockButtonAssayUpdate;
      try { this.PlaceVRStockButtonAssay(); }
      catch(error)
      {
         this.vrInputTraceFault = "stock_button_assay_setup_failed";
         this.DisarmVRInputTrace();
         return false;
      }
      return true;
   }
   function PlaceVRStockButtonAssay()
   {
      if(!this.vrStockButtonAssayActive) return undefined;
      var saved = this.vrStockButtonAssaySaved;
      if(this.navPanel.buttons[3] !== saved.button || this.navPanel._buttonCount > 3)
      {
         saved.takenOver = this.navPanel.buttons[3] === saved.button;
         this.vrInputTraceFault = "stock_button_assay_slot_taken";
         this.DisarmVRInputTrace();
         return undefined;
      }
      var button = saved.button;
      button.setButtonData({text:"Filter Test",controls:null});
      // Reserve a normally unused fourth slot beside the working Light button.
      button._x = 220;
      button._y = this.vrStockButtonAssaySaved.y;
      button._visible = true;
      button._listeners.click = [{listenerObject:this,listenerFunction:"onZoomClicked"}];
      saved.assayClicks = button._listeners.click;
   }
   function RestoreVRStockButtonAssay()
   {
      if(!this.vrStockButtonAssayActive) return undefined;
      this.vrStockButtonAssayActive = false;
      var saved = this.vrStockButtonAssaySaved;
      if(this.navPanel.doUpdateButtons === this.vrStockButtonAssayUpdate) this.navPanel.doUpdateButtons = saved.update;
      else this.vrInputTraceFault = "stock_assay_update_changed";
      if(!saved.takenOver)
      {
         saved.button._keyCodes.splice(0);
         var i = 0;
         while(i < saved.keyCodes.length) { saved.button._keyCodes.push(saved.keyCodes[i]); i++; }
         saved.button.label = saved.label;
         saved.button._visible = saved.visible;
         saved.button._x = saved.x;
         saved.button._y = saved.y;
         saved.button._listeners.click = saved.click;
      }
      delete this.vrStockButtonAssaySaved;
      delete this.vrStockButtonAssayUpdate;
   }
   function WrapVRInputHandler(a_receiver, a_name)
   {
      if(a_receiver == undefined || typeof a_receiver[a_name] != "function") return undefined;
      var i = 0;
      while(i < this.vrInputTraceWrappers.length)
      {
         if(this.vrInputTraceWrappers[i].receiver === a_receiver && this.vrInputTraceWrappers[i].name == a_name) return undefined;
         i++;
      }
      var owner = this;
      var original = a_receiver[a_name];
      var name = a_name;
      var wrapper = function()
      {
         // Preserve original receiver, argument list, result and exceptions.
         // Observer failure must never suppress the original handler.
         try { owner.RecordVRInputHandler(this,name,arguments); }
         catch(error) { owner.vrInputTraceFault = "movie_observer_error"; }
         return original.apply(this,arguments);
      };
      this.vrInputTraceWrappers.push({receiver:a_receiver,name:a_name,original:original,wrapper:wrapper});
      a_receiver[a_name] = wrapper;
   }
   function ArmVRInputTrace()
   {
      if(!_global.skse.IsVR() || this.vrInputTraceArmed) return false;
      if(this.colorField._visible || this.textEntry._visible || this.makeupPanel._visible) return false;
      if(this.vrFilterButton == undefined || this.vrNameButton == undefined) return false;
      if(this.vrFilterButton._listeners.click[0].listenerFunction == undefined || this.vrNameButton._listeners.click[0].listenerFunction == undefined) return false;
      if(!_global.skse.plugins.CharGen.BeginVRInputTrace()) return false;
      this.vrInputTraceArmed = true;
      this.vrInputTraceFault = "";
      this.vrInputTraceIgnoredStateChanges = 0;
      this.vrInputTraceStarted = getTimer();
      this.vrInputTraceWrappers = [];
      this.vrInputTraceSavedDisabled = this.searchWidget.isDisabled;
      this.vrInputTraceSavedFilter = this.vrFilterButton._listeners.click[0].listenerFunction;
      this.vrInputTraceSavedName = this.vrNameButton._listeners.click[0].listenerFunction;
      try
      {
      this.searchWidget.isDisabled = true;
      this.vrFilterButton._listeners.click[0].listenerFunction = "onZoomClicked";
      this.vrNameButton._listeners.click[0].listenerFunction = "onZoomClicked";
      var receivers = [this,this.vrFilterButton,this.vrNameButton,this.searchWidget,this.searchWidget.textField,this.searchWidget.icon,this.racePanel,this.bottomBar.playerInfo];
      var i = 0;
      while(i < this.navPanel.buttons.length)
      {
         receivers.push(this.navPanel.buttons[i]);
         i++;
      }
      var names = ["onRollOver","onRollOut","onPress","onRelease","onReleaseOutside","handleMousePress","handleMouseRelease","handlePress","handleRelease","handleInput","onZoomClicked","handleClick","dispatchEvent"];
      i = 0;
      while(i < receivers.length)
      {
         var j = 0;
         while(j < names.length)
         {
            this.WrapVRInputHandler(receivers[i],names[j]);
            j++;
         }
         i++;
      }
      var owner = this;
      this.vrInputTraceMouseListener = {};
      this.vrInputTraceMouseListener.onMouseDown = function()
      {
         owner.RecordVRInputHandler(_root,"movie_mouse_down",arguments);
         owner.RecordVRInputShapeHits();
      };
      this.vrInputTraceMouseListener.onMouseUp = function()
      {
         owner.RecordVRInputHandler(_root,"movie_mouse_up",arguments);
         owner.RecordVRInputShapeHits();
      };
      Mouse.addListener(this.vrInputTraceMouseListener);
      this.vrInputTraceTimer = setInterval(this,"CheckVRInputTraceDeadline",1000);
      _global.skse.plugins.CharGen.ReadVRInputTrace();
      return true;
      }
      catch(error)
      {
         this.vrInputTraceFault = "arm_failed";
         this.DisarmVRInputTrace();
         return false;
      }
   }
   function RecordVRInputShapeHits()
   {
      var point = {x:_root._xmouse,y:_root._ymouse};
      _root.localToGlobal(point);
      var receivers = [this.vrFilterButton,this.vrNameButton];
      var i = 0;
      while(i < receivers.length)
      {
         var receiver = receivers[i];
         // Shape membership is not a claim about the topmost click target.
         this.RecordVRInputHandler(receiver,"shape_hit:" + String(receiver.hitTest(point.x,point.y,true)),[]);
         i++;
      }
   }
   function CheckVRInputTraceDeadline()
   {
      if(getTimer() - this.vrInputTraceStarted >= 90000) this.DisarmVRInputTrace();
   }
   function DisarmVRInputTrace()
   {
      if(!this.vrInputTraceArmed) return false;
      this.vrInputTraceArmed = false;
      this.RestoreVRStockButtonAssay();
      clearInterval(this.vrInputTraceTimer);
      delete this.vrInputTraceTimer;
      Mouse.removeListener(this.vrInputTraceMouseListener);
      delete this.vrInputTraceMouseListener;
      var i = this.vrInputTraceWrappers.length - 1;
      while(i >= 0)
      {
         var saved = this.vrInputTraceWrappers[i];
         if(saved.receiver[saved.name] === saved.wrapper) saved.receiver[saved.name] = saved.original;
         else this.vrInputTraceFault = "handler_changed_during_assay";
         i--;
      }
      this.vrInputTraceWrappers = [];
      this.vrFilterButton._listeners.click[0].listenerFunction = this.vrInputTraceSavedFilter;
      this.vrNameButton._listeners.click[0].listenerFunction = this.vrInputTraceSavedName;
      this.searchWidget.isDisabled = this.vrInputTraceSavedDisabled;
      _global.skse.plugins.CharGen.EndVRInputTrace();
      _global.skse.plugins.CharGen.ReadVRInputTrace();
      return true;
   }
