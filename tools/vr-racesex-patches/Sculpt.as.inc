   function LayoutVRSculpt()
   {
      if(!_global.skse.IsVR()) return undefined;
      if(this.vrOriginalTransform == undefined) this.vrOriginalTransform = {x:this._x,y:this._y,xscale:this._xscale,yscale:this._yscale};
      var original = this.vrOriginalTransform;
      this._x = original.x; this._y = original.y;
      this._xscale = original.xscale; this._yscale = original.yscale;
      // Never anchor to racePanel: its stock hide tween moves it off-screen
      // while Sculpt is open. Use the movie's drawable frame independently.
      var frame = Stage.visibleRect;
      var topLeft = {x:frame.x+16,y:frame.y+90};
      var bottomRight = {x:frame.x+frame.width-16,y:frame.y+frame.height-16};
      this.globalToLocal(topLeft); this.globalToLocal(bottomRight);
      var panel = {xMin:topLeft.x,yMin:topLeft.y,xMax:bottomRight.x,yMax:bottomRight.y};
      var gap = 16;
      var controlsWidth = 280;
      var canvas = this.wireframeDisplay;
      if(!canvas.bLoadedAssets || canvas.foreground.fixedWidth == undefined) return undefined;
      // Resize the complete registered component. Its foreground's _xmouse /
      // _ymouse remain texture-local: no second coordinate or paint route.
      var target = this.vrLargeCanvas ? 760 : 560;
      var width = target+gap+controlsWidth;
      canvas._xscale = canvas._yscale = 100;
      var bounds = canvas.getBounds(this);
      var factor = target/(bounds.xMax-bounds.xMin);
      canvas._xscale = canvas._yscale = factor*100;
      this.PlaceVRSculptClip(canvas,panel.xMin+controlsWidth+gap,panel.yMin);
      bounds = canvas.getBounds(this);
      // One compact controls column leaves the right canvas unobstructed.
      // Cap each sidebar section uniformly (never deform brush proportions).
      var controlsX = panel.xMin;
      this.SizeVRSculptBrushControls();
      this.FitVRSculptClip(this.brushWindow,controlsWidth,260);
      this.FitVRSculptClip(this.historyWindow,controlsWidth,160);
      this.FitVRSculptClip(this.meshWindow,controlsWidth,180);
      this.PlaceVRSculptClip(this.brushWindow,controlsX,panel.yMin);
      var brush = this.GetVRSculptBounds(this.brushWindow);
      this.PlaceVRSculptClip(this.historyWindow,controlsX,brush.yMax+gap);
      var history = this.historyWindow.getBounds(this);
      this.PlaceVRSculptClip(this.meshWindow,controlsX,history.yMax+gap);
      var mesh = this.meshWindow.getBounds(this);
      this.BOTTOMBAR_SHOWN_Y = Math.max(bounds.yMax,mesh.yMax)+gap;
      this.bottomBar._x = panel.xMin;
      this.bottomBar._y = this.BOTTOMBAR_SHOWN_Y;
      this.bottomBar.background._visible = false;
      this.bottomBar.playerInfo._visible = false;
      this.tempText._visible = false;
      this.navPanel._x = this.bottomBar.staticPanel._x = 0;
      this.navPanel._y = 0;
      this.bottomBar.staticPanel._y = 0;
      var firstHeight = this.LayoutVRSculptButtons(this.navPanel,width,this.bottomBar.staticPanel);
      this.bottomBar.staticPanel._y = firstHeight+8;
      var secondHeight = this.LayoutVRSculptButtons(this.bottomBar.staticPanel,width);
      // Actions remain registered in staticPanel, which OCU scans completely.
      // Position them beside the first-row hints after moving their own panel.
      this.LayoutVRSculptButtons(this.navPanel,width,this.bottomBar.staticPanel);
      this.vrWorkspaceBounds = {xMin:panel.xMin,yMin:panel.yMin,xMax:panel.xMin+width,yMax:this.BOTTOMBAR_SHOWN_Y+firstHeight+8+secondHeight};
      // Include both dimensions. A wide detail canvas must not cross the
      // movie edge, and neither ancestor transforms nor negative local shape
      // origins may cause the old left-edge regression.
      // Flash's AS2 Math.min only compares two operands. Nest the calls:
      // a JavaScript-style third argument silently discarded the height cap.
      var fit = Math.min(1,Math.min((panel.xMax-panel.xMin)/width,(panel.yMax-panel.yMin)/(this.vrWorkspaceBounds.yMax-panel.yMin)));
      this._xscale *= fit; this._yscale *= fit;
      var top = {x:panel.xMin,y:panel.yMin};
      this.localToGlobal(top); this._parent.globalToLocal(top);
      var desired = {x:frame.x+16,y:frame.y+90};
      this._parent.globalToLocal(desired);
      this._x += desired.x-top.x; this._y += desired.y-top.y;
      this.DrawVRSculptBackground();
      this.PositionVRSculptTabs();
   }
   function DrawVRSculptBackground()
   {
      if(this.vrSculptBackground == undefined)
      {
         this.vrSculptBackground = this.createEmptyMovieClip("vrSculptBackground",this.getNextHighestDepth());
         // Put the new backing beneath the canvas without reparenting any
         // registered component. The canvas moves to the formerly free depth.
         this.vrSculptBackground.swapDepths(this.wireframeDisplay);
      }
      var background = this.vrSculptBackground;
      var profile = this._parent.vrMenuProfile;
      var color = profile.backgroundColor >= 0 ? profile.backgroundColor : 0;
      var alpha = profile.backgroundOpacity >= 0 ? profile.backgroundOpacity : 50;
      var b = this.vrWorkspaceBounds;
      background.clear(); background.beginFill(color,alpha);
      background.moveTo(b.xMin,b.yMin); background.lineTo(b.xMax,b.yMin);
      background.lineTo(b.xMax,b.yMax); background.lineTo(b.xMin,b.yMax);
      background.lineTo(b.xMin,b.yMin); background.endFill();
      background._visible = true;
   }
   function PositionVRSculptTabs()
   {
      var tabs = this._parent.modeSelect;
      if(this.vrTabsOriginal == undefined) this.vrTabsOriginal = {x:tabs._x,xscale:tabs._xscale,yscale:tabs._yscale};
      var left = {x:this.vrWorkspaceBounds.xMin,y:this.vrWorkspaceBounds.yMin};
      var right = {x:this.vrWorkspaceBounds.xMax,y:this.vrWorkspaceBounds.yMin};
      this.localToGlobal(left); this.localToGlobal(right);
      this._parent.globalToLocal(left); this._parent.globalToLocal(right);
      tabs._xscale = this.vrTabsOriginal.xscale; tabs._yscale = this.vrTabsOriginal.yscale;
      var b = tabs.getBounds(this._parent);
      tabs._x += (left.x+right.x-b.xMin-b.xMax)/2;
   }
   function PlaceVRSculptClip(clip,x,y)
   {
      var bounds = this.GetVRSculptBounds(clip);
      clip._x += x-bounds.xMin;
      clip._y += y-bounds.yMin;
   }
   function FitVRSculptClip(clip,width,height)
   {
      if(clip.vrOriginalScale == undefined) clip.vrOriginalScale = {x:clip._xscale,y:clip._yscale};
      clip._xscale = clip.vrOriginalScale.x;
      clip._yscale = clip.vrOriginalScale.y;
      var bounds = this.GetVRSculptBounds(clip);
      var factor = width/(bounds.xMax-bounds.xMin);
      if(height != undefined) factor = Math.min(factor,height/(bounds.yMax-bounds.yMin));
      clip._xscale *= factor; clip._yscale *= factor;
   }
   function SizeVRSculptBrushControls()
   {
      var list = this.brushWindow.brushList;
      if(list == undefined) return undefined;
      if(list.vrOriginalTransform == undefined)
         list.vrOriginalTransform = {x:list._x,y:list._y,xscale:list._xscale,yscale:list._yscale};
      var original = list.vrOriginalTransform;
      list._x = original.x; list._y = original.y;
      list._xscale = original.xscale; list._yscale = original.yscale;
      var bounds = list.getBounds(this.brushWindow);
      var center = (bounds.xMin+bounds.xMax)/2;
      // Stock brush rails occupy half the category selector's width. Enlarge
      // the registered list uniformly, including labels and hit geometry;
      // do not stretch only artwork or add a second pointer route.
      list._xscale *= 2; list._yscale *= 2;
      bounds = list.getBounds(this.brushWindow);
      list._x += center-(bounds.xMin+bounds.xMax)/2;
      var selector = this.brushWindow.brushSlidingList;
      if(selector != undefined)
      {
         if(selector.vrOriginalTransform == undefined)
            selector.vrOriginalTransform = {x:selector._x,y:selector._y,xscale:selector._xscale,yscale:selector._yscale};
         var saved = selector.vrOriginalTransform;
         selector._xscale = saved.xscale*2; selector._yscale = saved.yscale*2;
         // Match the rail width while retaining the stock mask, selection
         // tween and renderer input. Enlarge labels AND their hit geometry.
         selector._x = saved.x-106.5; selector._y = saved.y-28;
      }
      // Preserve the top origin below the category selector. The enclosing
      // workspace measures expanded bounds and reflows History/Head Parts.
   }
   function GetVRSculptBounds(clip)
   {
      if(clip != this.brushWindow || clip.brushSlidingList == undefined)
         return clip.getBounds(this);
      // getBounds includes all six category labels, even those outside the
      // sliding selector's mask. Its width changes with selected brush/tween.
      // Measure the fixed visible viewport instead, plus the real rail and
      // window backing geometry, so selection cannot shrink the controls.
      var b = clip.background.getBounds(this);
      var rails = clip.brushList.getBounds(this);
      var selector = clip.brushSlidingList;
      var a = {x:0,y:0}; var z = {x:213,y:28};
      selector.localToGlobal(a); selector.localToGlobal(z);
      this.globalToLocal(a); this.globalToLocal(z);
      return {xMin:Math.min(b.xMin,Math.min(rails.xMin,a.x)),yMin:Math.min(b.yMin,Math.min(rails.yMin,a.y)),xMax:Math.max(b.xMax,Math.max(rails.xMax,z.x)),yMax:Math.max(b.yMax,Math.max(rails.yMax,z.y))};
   }
   function RefreshVRSculptActions()
   {
      var panel = this.bottomBar.staticPanel;
      if(panel == undefined) return undefined;
      for(var i = 0; i < panel.buttons.length; i++)
      {
         var old = panel.buttons[i];
         if(!old.vrSculptAction) continue;
         old.removeEventListener("click",this,"onVRSculptFaceClicked");
         old.removeEventListener("click",this,"onVRCanvasSizeClicked");
         old.removeEventListener("click",this,"onVRViewLeftClicked");
         old.removeEventListener("click",this,"onVRViewCentreClicked");
         old.removeEventListener("click",this,"onVRViewRightClicked");
         // setPlatform may already have cleared the panel back to stock.
         if(i >= panel._buttonCount) old.vrSculptAction = false;
      }
      // Only remove our trailing renderers. Never clear stock actions or take
      // over an extension's slot if another consumer appended after ours.
      while(panel._buttonCount > 0 && panel.buttons[panel._buttonCount-1].vrSculptAction)
      {
         var old = panel.buttons[panel._buttonCount-1];
         old.vrSculptAction = false;
         old.label = ""; old._visible = false;
         panel._buttonCount--;
      }
      this.vrSculptActionFault = "";
      for(var i = 0; i < panel._buttonCount; i++)
      {
         if(panel.buttons[i].vrSculptAction)
         {
            this.vrSculptActionFault = "action_panel_busy";
            return undefined;
         }
      }
      var api = _global.skse.plugins.CharGen;
      var rotation = api.SetViewYaw != undefined && api.GetFaceViewDiagnostics != undefined;
      // onLoad/constructor capacity is not authoritative: the live GFx panel
      // can still have six slots once SKSE and the input platform are ready.
      // Grow the existing scanned panel here, preserving stock renderers and
      // using its own linkage, initializer and current input platform.
      var required = panel._buttonCount+(rotation ? 5 : 2);
      this.vrSculptInitialSlots = panel.buttons.length;
      if(required <= 16 && panel.buttonRenderer == "MappedButton" && this._platform != undefined)
      {
         while(panel.buttons.length < required)
         {
            var slot = panel.buttons.length;
            var added = panel.attachMovie(panel.buttonRenderer,"button"+slot,panel.getNextHighestDepth(),panel.buttonInitializer);
            if(added == undefined || added.setPlatform == undefined || added.setButtonData == undefined)
            {
               if(added != undefined) added.removeMovieClip();
               break;
            }
            added.setPlatform(this._platform,this._bPS3Switch);
            added._visible = false;
            panel.buttons.push(added);
            panel.maxButtons = panel.buttons.length;
         }
      }
      var spare = panel.buttons.length-panel._buttonCount;
      if(spare < 2)
      {
         this.vrSculptActionFault = "action_panel_capacity";
         return undefined;
      }
      // Preserve Face/canvas even if another extension or an older panel
      // leaves insufficient slots for rotation. Never remove working actions.
      if(rotation && spare < 5)
      {
         rotation = false;
         this.vrSculptActionFault = "rotation_panel_capacity";
      }
      var face = panel.addButton({text:_global.skse.plugins.CharGen.GetMenuView() == 1 ? "Normal view" : "Face view",controls:null});
      face.vrSculptAction = true;
      face.addEventListener("click",this,"onVRSculptFaceClicked");
      var canvas = panel.addButton({text:this.vrLargeCanvas ? "Standard canvas" : "Large canvas",controls:null});
      canvas.vrSculptAction = true;
      canvas.addEventListener("click",this,"onVRCanvasSizeClicked");
      if(rotation)
      {
         var left = panel.addButton({text:"Turn left",controls:null});
         left.vrSculptAction = true;
         left.addEventListener("click",this,"onVRViewLeftClicked");
         var centre = panel.addButton({text:"Centre view",controls:null});
         centre.vrSculptAction = true;
         centre.addEventListener("click",this,"onVRViewCentreClicked");
         var right = panel.addButton({text:"Turn right",controls:null});
         right.vrSculptAction = true;
         right.addEventListener("click",this,"onVRViewRightClicked");
      }
   }
   function LayoutVRSculptButtons(panel,width,actions)
   {
      if(panel == undefined) return 0;
      var x = 0; var y = 0; var height = 0;
      var count = panel.buttons.length+(actions == undefined ? 0 : actions.buttons.length);
      for(var i = 0; i < count; i++)
      {
         var extra = i >= panel.buttons.length;
         var button = extra ? actions.buttons[i-panel.buttons.length] : panel.buttons[i];
         if(extra && !button.vrSculptAction || !extra && actions == undefined && button.vrSculptAction) continue;
         if(!button._visible || button.label == "") continue;
         this._parent.SizeVRFooterText(button.textField);
         button.update();
         button.background._height = button.textField._height+8;
         if(button.vrSculptAction)
         {
            // Text-only controls have no mapped-key icon to hit. Retain an
            // ordinary MovieClip shape for pointer hit testing, not a second
            // click path or an invisible overlay over the sculpt canvas.
            button.background._visible = true;
            button.background._alpha = 12;
         }
         if(x > 0 && x+button._width > width) { x = 0; y += height+8; height = 0; }
         // Both panels are siblings under bottomBar. Offset only our actions;
         // their parent, listeners, native registration and picking stay intact.
         button._x = x+(extra ? panel._x-actions._x : 0);
         button._y = y+(extra ? panel._y-actions._y : 0);
         x += button._width+12;
         height = Math.max(height,button._height);
      }
      return y+height;
   }
   function onVRCanvasSizeClicked()
   {
      this.vrCanvasActionCount = this.vrCanvasActionCount == undefined ? 1 : this.vrCanvasActionCount+1;
      var foreground = this.wireframeDisplay.foreground;
      this.vrCanvasResizeDenied = foreground.painting || foreground.rotating || foreground.panning;
      if(this.vrCanvasResizeDenied) return undefined;
      this.vrLargeCanvas = !this.vrLargeCanvas;
      this.updateBottomBar();
      this._parent.vrWorldPlacementKey = undefined;
   }
   function onVRSculptFaceClicked()
   {
      this.vrFaceActionCount = this.vrFaceActionCount == undefined ? 1 : this.vrFaceActionCount+1;
      this._parent.RequestVRMenuView();
   }
   function ReadVRSculptRotationPreflight()
   {
      var api = _global.skse.plugins.CharGen;
      this.vrRotationPreflightAvailable = api.GetFaceViewDiagnostics != undefined;
      this.vrRotationPreflight = this.vrRotationPreflightAvailable ? api.GetFaceViewDiagnostics() : undefined;
      // Read-only API bridge: UI.GetBool cannot inspect an Invoke return
      // object. Keep availability separate from the returned topology fields.
   }
   function RequestVRViewTurn(step)
   {
      var foreground = this.wireframeDisplay.foreground;
      if(foreground.painting || foreground.rotating || foreground.panning) return undefined;
      var api = _global.skse.plugins.CharGen;
      if(api.SetViewYaw == undefined || api.GetFaceViewDiagnostics == undefined) return undefined;
      var state = api.GetFaceViewDiagnostics();
      if(state.yawQueued) return undefined;
      // Use applied native state, never an optimistic angle cached by AS2.
      // Centre remains usable after rejection or an ownership change.
      var angle = step == 0 ? 0 : Math.max(-60,Math.min(60,Number(state.yawDegrees)+step));
      this.vrViewTurnAccepted = api.SetViewYaw(angle);
   }
   function onVRViewLeftClicked()
   {
      this.RequestVRViewTurn(5);
   }
   function onVRViewCentreClicked()
   {
      this.RequestVRViewTurn(0);
   }
   function onVRViewRightClicked()
   {
      this.RequestVRViewTurn(-5);
   }
   function UpdateVRSculptView(bShow)
   {
      if(!_global.skse.IsVR()) return undefined;
      if(!bShow)
      {
         if(this.vrSculptBackground != undefined) this.vrSculptBackground._visible = false;
         if(this.vrTabsOriginal != undefined)
         {
            var tabs = this._parent.modeSelect;
            tabs._x = this.vrTabsOriginal.x;
            tabs._xscale = this.vrTabsOriginal.xscale; tabs._yscale = this.vrTabsOriginal.yscale;
         }
      }
      var api = _global.skse.plugins.CharGen;
      if(api.GetMenuView == undefined || api.SetMenuView == undefined) return undefined;
      if(bShow && !this.vrViewActive)
      {
         this.vrPreviousView = Number(api.GetMenuView()) == 1 ? 1 : 0;
         api.SetMenuView(this.vrSculptFace == 1 ? 1 : 0);
         this.vrViewActive = true;
      }
      else if(!bShow && this.vrViewActive)
      {
         this.vrSculptFace = Number(api.GetMenuView()) == 1 ? 1 : 0;
         api.SetMenuView(this.vrPreviousView);
         this.vrViewActive = false;
      }
      this._parent.vrWorldPlacementKey = undefined;
   }
