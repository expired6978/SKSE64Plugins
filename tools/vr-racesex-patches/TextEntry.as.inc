   function FinishVRCharacterCreation()
   {
      if(this.vrTextInputActive) return false;
      var name = Shared.GlobalFunc.StringTrim(this.bottomBar.playerInfo.PlayerName.text);
      if(name == "" || name == undefined)
      {
         this.setStatusText("Please enter a character name",3000);
         return false;
      }
      // This is deliberately the final engine naming/completion callback,
      // unlike the non-finishing SetCharacterName used by the Name keyboard.
      gfx.io.GameDelegate.call("ChangeName",[name]);
      return true;
   }
   function SetupVRTextButtons()
   {
      if(this.vrTextButtonsSetup) return undefined;
      this.vrTextButtonsSetup = true;
      this.searchWidget.vrTextOwner = this;
      var owner = this;
      var originalUpdate = this.navPanel.doUpdateButtons;
      this.navPanel.doUpdateButtons = function()
      {
         var result = originalUpdate.apply(this,arguments);
         owner.PositionVRTextButtons();
         return result;
      };
      var originalUnload = this.onUnload;
      this.onUnload = function()
      {
         owner.CancelVRTextInput();
         if(originalUnload != undefined) originalUnload.apply(this,arguments);
      };
   }
   function ResetVRTextButtons()
   {
      // clearButtons hides renderers but retains listeners. Remove only this
      // owner's old actions; preserve any other consumer's event subscriptions.
      var actions = ["onDoneClicked","onSearchClicked","onZoomClicked","onLightClicked","onChangeRaceClicked","onChooseColorClicked","onChooseTextureClicked","onVRFilterButtonClicked","onVRNameButtonClicked"];
      var i = 0;
      while(i < this.navPanel.buttons.length)
      {
         // ButtonPanel.clearButtons resets x, but not y. A slot formerly
         // used by Filter must return to the row before a new action owns it.
         this.navPanel.buttons[i]._y = 0;
         var j = 0;
         while(j < actions.length)
         {
            this.navPanel.buttons[i].removeEventListener("click",this,actions[j]);
            j++;
         }
         i++;
      }
      this.vrFilterButton = undefined;
      this.vrNameButton = undefined;
   }
   function AddVRTextButtons()
   {
      this.vrTextButtonFault = "";
      // Existing and conditional actions have already claimed their slots.
      // The VR bottom panel constructs two additional ordinary renderers.
      // Name comes before the repositioned Filter, so Filter's width cannot
      // push Name beyond the ordinary bottom-row controls.
      this.vrNameButton = this.navPanel.addButton({text:"Name: " + this.bottomBar.playerInfo.PlayerName.text,controls:null});
      if(this.vrNameButton != undefined)
         this.vrNameButton.addEventListener("click",this,"onVRNameButtonClicked");
      else this.vrTextButtonFault = "name_capacity";
      var filter = this.GetVRFilterText();
      if(filter == "") filter = skyui.components.SearchWidget.S_FILTER;
      this.vrFilterButton = this.navPanel.addButton({text:filter,controls:null});
      if(this.vrFilterButton != undefined)
         this.vrFilterButton.addEventListener("click",this,"onVRFilterButtonClicked");
      else this.vrTextButtonFault += " filter_capacity";
   }
   function GetVRFilterText()
   {
      // SetText translates $FILTER into FILTER (or a localized string).
      // The NameFilter model contains only entered text, never that hint.
      var filter = this._nameFilter.filterText;
      return filter == undefined ? "" : String(filter);
   }
   function PublishVRMenuText()
   {
      var api = _global.skse.plugins.CharGen;
      if(api.PublishMenuText != undefined)
         api.PublishMenuText(String(this.bottomBar.playerInfo.PlayerName.text),this.GetVRFilterText());
   }
   function SetVRFilterText(a_text)
   {
      if(this.vrTextInputActive) return false;
      this._nameFilter.filterText = String(a_text);
      this.searchWidget.textField.SetText(a_text == "" ? skyui.components.SearchWidget.S_FILTER : String(a_text));
      this.updateBottomBar();
      this.PublishVRMenuText();
      return true;
   }
   function UpdateVRNameButton(a_name)
   {
      if(this.vrNameButton == undefined) return undefined;
      this.vrNameButton.label = "Name: " + a_name;
      this.navPanel.updateButtons(false);
      this.PublishVRMenuText();
   }
   function onVRFilterButtonClicked()
   {
      this.BeginVRTextInput("filter");
   }
   function onVRNameButtonClicked()
   {
      this.BeginVRTextInput("name");
   }
   function PositionVRTextButtons()
   {
      if(this.LayoutVRPanel != undefined) this.LayoutVRPanel();
      // Keep the pre-created renderers in their original registered panel.
      // Convert live field bounds into that panel AFTER platform/layout moves.
      if(this.vrFilterButton != undefined)
      {
         var bounds = this.searchWidget.getBounds(this.navPanel);
         this.vrFilterButton._x = bounds.xMin;
         this.vrFilterButton._y = bounds.yMin;
         this.vrFilterButton.width = bounds.xMax - bounds.xMin;
         this.searchWidget.textField._visible = false;
         this.searchWidget.icon._visible = false;
      }
      if(this.vrNameButton != undefined)
      {
         // TextFields do not provide MovieClip.getBounds. Keep this renderer
         // in its normal row instead of measuring the hidden legacy fields.
         if(this.vrMenuProfile == undefined || this.vrMenuProfile.consolidate != 1) this.vrNameButton._y = 0;
         this.bottomBar.playerInfo.NameLabel._visible = false;
         this.bottomBar.playerInfo.PlayerName._visible = false;
      }
   }
   function BeginVRTextInput(a_kind)
   {
      if(this.vrTextInputActive || this.vrInputTraceArmed || this.vrStockButtonAssayActive || this.colorField._visible || this.textEntry._visible || this.makeupPanel._visible || (a_kind == "filter" && this.searchWidget.isDisabled))
         return false;
      var api = _global.skse.plugins.CharGen;
      if(api.BeginVRTextEntry == undefined || api.PollVRTextEntry == undefined || api.CancelVRTextEntry == undefined)
      {
         this.vrTextInputError = "keyboard_api_unavailable";
         this.setStatusText("Keyboard unavailable",3000);
         return false;
      }
      var initial = a_kind == "name" ? this.bottomBar.playerInfo.PlayerName.text : this.GetVRFilterText();
      var request = api.BeginVRTextEntry(a_kind,initial);
      this.vrTextInputError = request.error;
      this.vrTextInputStatus = request.status;
      if(request.status != "pending")
      {
         this.setStatusText("Keyboard unavailable: " + request.error,3000);
         return false;
      }
      this.vrTextInputActive = true;
      this.vrTextInputId = request.id;
      this.vrTextInputKind = a_kind;
      this.vrTextInputInitial = initial;
      this.SetVRTextInputFeedback(true);
      this.vrTextInputTimer = setInterval(this,"PollVRTextInput",50);
      return true;
   }
   function SetVRTextInputFeedback(a_active)
   {
      var button = this.vrTextInputKind == "name" ? this.vrNameButton : this.vrFilterButton;
      var field = button != undefined ? button.textField : (this.vrTextInputKind == "name" ? this.bottomBar.playerInfo.PlayerName : this.searchWidget.textField);
      field.background = a_active;
      field.backgroundColor = 2105376;
      field.border = a_active;
      field.borderColor = 16777215;
   }
   function PollVRTextInput()
   {
      if(!this.vrTextInputActive) return undefined;
      var result = _global.skse.plugins.CharGen.PollVRTextEntry(this.vrTextInputId);
      if(result.status == "pending")
      {
         this.ShowVRTextDraft(result.text);
         return undefined;
      }
      clearInterval(this.vrTextInputTimer);
      this.vrTextInputTimer = undefined;
      this.SetVRTextInputFeedback(false);
      this.vrTextInputActive = false;
      this.vrTextInputStatus = result.status;
      this.vrTextInputError = result.error;
      this.ShowVRTextDraft(this.vrTextInputInitial);
      if(result.status == "accepted")
      {
         if(this.vrTextInputKind == "filter")
         {
            this.searchWidget.textField.SetText(result.text == "" ? skyui.components.SearchWidget.S_FILTER : result.text);
            this.searchWidget.dispatchEvent({type:"inputEnd",data:result.text});
         }
         else
         {
            var name = Shared.GlobalFunc.StringTrim(result.text);
            if(name != "" && name != undefined && _global.skse.plugins.CharGen.SetCharacterName(name))
               this.SetNameText(name);
            else this.vrTextInputError = "invalid_or_rejected_name";
         }
      }
      // Cancellation/unavailability never changes the accepted name/filter.
      this.updateBottomBar();
   }
   function ShowVRTextDraft(a_text)
   {
      // Display only; do not rename the actor or update the filter model until
      // Done. This supplies SteamVR's missing editable text preview.
      if(a_text == undefined) return undefined;
      var button = this.vrTextInputKind == "name" ? this.vrNameButton : this.vrFilterButton;
      if(button == undefined) return undefined;
      button.label = this.vrTextInputKind == "name" ? "Name: " + a_text : String(a_text);
      this.navPanel.updateButtons(false);
   }
   function CancelVRTextInput()
   {
      if(this.vrTextInputTimer != undefined) clearInterval(this.vrTextInputTimer);
      this.vrTextInputTimer = undefined;
      if(this.vrTextInputActive)
      {
         _global.skse.plugins.CharGen.CancelVRTextEntry(this.vrTextInputId);
         this.SetVRTextInputFeedback(false);
         this.ShowVRTextDraft(this.vrTextInputInitial);
      }
      this.vrTextInputActive = false;
   }
