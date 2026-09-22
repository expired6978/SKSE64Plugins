   function SetupVRMenuAppearance()
   {
      if(this.vrAppearanceSetup) return undefined;
      this.vrAppearanceSetup = true;
      // This is the stock active-tint/capacity counter, not a slider value.
      // Its former top-centre position overlaps the consolidated VR tabs.
      this.racePanel.tintCount._visible = false;
      var api = _global.skse.plugins.CharGen;
      if(api.avatarLightingSupported)
      {
         // Disable only RaceMenu's old Papyrus light; all other lights are untouched.
         skse.SendModEvent(_global.eventPrefix + "ToggleLight","",0);
         api.SetAvatarLighting(Boolean(this.bShowLight));
      }
      var background = this.racePanel.ListBackground;
      var owner = this;
      this.vrBackgroundImageState = "absent";
      if(background == undefined)
      {
         this.vrAppearanceSetup = false;
         this.vrBackgroundImageState = "error: missing panel background";
         return undefined;
      }
      var originalUnload = this.onUnload;
      this.onUnload = function()
      {
         owner.vrAppearanceSetup = false;
         clearInterval(owner.vrAppearanceTimer);
         clearInterval(owner.vrBackgroundMountTimer);
         if(owner.vrBackgroundLoader != undefined)
         {
            owner.vrBackgroundLoader.removeListener(owner.vrBackgroundListener);
            owner.vrBackgroundLoader.unloadClip(owner.vrBackgroundImage);
         }
         if(originalUnload != undefined) originalUnload.apply(this,arguments);
      };
      this.vrBackgroundBounds = background.getBounds(background);
      var backgroundColor = Number(api.menuBackgroundColor);
      if(!isNaN(backgroundColor) && backgroundColor >= 0)
      {
         new Color(background).setRGB(backgroundColor);
      }
      this.vrTextColor = Number(api.menuTextColor);
      this.ApplyVRMenuProfile("VR Normal");
      if(!isNaN(this.vrTextColor) && this.vrTextColor >= 0)
      {
         this.ApplyVRTextColor();
      }
      this.vrAppearanceTimer = setInterval(this,"ApplyVRTextColor",500);
      var url = api.menuBackgroundImage;
      if(url != undefined && url != "")
      {
         if(api.MountMenuBackgroundImage == undefined || api.GetMenuBackgroundImageState == undefined || api.GetMenuBackgroundProjectionScale == undefined)
         {
            this.vrBackgroundImageState = "error: native texture registration";
            return undefined;
         }
         api.MountMenuBackgroundImage();
         this.vrBackgroundImageState = "waiting: native texture registration";
         this.vrBackgroundMountDeadline = getTimer()+5000;
         this.vrBackgroundMountTimer = setInterval(this,"CheckVRBackgroundMount",50);
         this.CheckVRBackgroundMount();
      }
   }
   function CheckVRBackgroundMount()
   {
      if(!this.vrAppearanceSetup || this.vrBackgroundImageState != "waiting: native texture registration") return undefined;
      var api = _global.skse.plugins.CharGen;
      var state = Number(api.GetMenuBackgroundImageState());
      var projection = Number(api.GetMenuBackgroundProjectionScale());
      if(projection < 0)
      {
         clearInterval(this.vrBackgroundMountTimer);
         this.vrBackgroundImageState = "error: unavailable VR surface projection";
      }
      else if(state == 2 && projection > 0)
      {
         clearInterval(this.vrBackgroundMountTimer);
         this.vrBackgroundProjectionScale = projection;
         this.LoadVRBackgroundImage();
      }
      else if(state == 3 || getTimer() >= this.vrBackgroundMountDeadline)
      {
         clearInterval(this.vrBackgroundMountTimer);
         this.vrBackgroundImageState = state == 3 ? "error: native texture registration" : (state == 2 ? "error: VR surface projection timeout" : "error: native texture registration timeout");
      }
   }
   function LoadVRBackgroundImage()
   {
         var api = _global.skse.plugins.CharGen;
         var background = this.racePanel.ListBackground;
         var owner = this;
         var url = api.menuBackgroundImage;
         this.vrBackgroundExpectedWidth = Number(api.menuBackgroundImageWidth);
         this.vrBackgroundExpectedHeight = Number(api.menuBackgroundImageHeight);
         this.vrBackgroundExpectedClipWidth = Number(api.menuBackgroundClipWidth);
         this.vrBackgroundExpectedClipHeight = Number(api.menuBackgroundClipHeight);
         if(!(this.vrBackgroundExpectedClipWidth > 0)) this.vrBackgroundExpectedClipWidth = this.vrBackgroundExpectedWidth;
         if(!(this.vrBackgroundExpectedClipHeight > 0)) this.vrBackgroundExpectedClipHeight = this.vrBackgroundExpectedHeight;
         this.vrBackgroundImageState = "loading";
         // The stock panel reserves depth 2 between background (1) and rows (3).
         // A sibling prevents the background colour transform tinting the PNG.
         var imageDepth = background.getDepth()+1;
         if(this.racePanel.getInstanceAtDepth(imageDepth) != undefined)
         {
            this.vrBackgroundImageState = "error: occupied background depth";
            return undefined;
         }
         this.vrBackgroundImage = this.racePanel.createEmptyMovieClip("vrPNGBackground",imageDepth);
         this.vrBackgroundImage._xscale = background._xscale;
         this.vrBackgroundImage._yscale = background._yscale;
         this.vrBackgroundImage._rotation = background._rotation;
         this.vrBackgroundOriginX = background._x;
         this.vrBackgroundOriginY = background._y;
         this.vrBackgroundLoader = new MovieClipLoader();
         this.vrBackgroundListener = new Object();
         this.vrBackgroundListener.onLoadInit = function(target)
         {
            if(!owner.vrAppearanceSetup) return undefined;
            var imageBounds = target.getBounds(target);
            owner.vrBackgroundImageWidth = imageBounds.xMax-imageBounds.xMin;
            owner.vrBackgroundImageHeight = imageBounds.yMax-imageBounds.yMin;
            // Skyrim may deliver its error artwork through onLoadInit, rather
            // than onLoadError. Validate the native loader's wrapper dimensions:
            // registered VR textures use a 64x64 clip regardless of PNG size.
            // The decoded PNG dimensions remain the aspect-fitting authority.
            if(!(owner.vrBackgroundExpectedWidth > 0 && owner.vrBackgroundExpectedHeight > 0) ||
               Math.abs(owner.vrBackgroundImageWidth-owner.vrBackgroundExpectedClipWidth) > 0.1 ||
               Math.abs(owner.vrBackgroundImageHeight-owner.vrBackgroundExpectedClipHeight) > 0.1)
            {
               owner.vrBackgroundImageState = "error: unexpected image dimensions";
               target.removeMovieClip();
               return undefined;
            }
            owner.FitVRBackgroundImage();
         };
         this.vrBackgroundListener.onLoadError = function(target,errorCode)
         {
            owner.vrBackgroundImageState = "error: " + errorCode;
            target.removeMovieClip();
         };
         this.vrBackgroundLoader.addListener(this.vrBackgroundListener);
         if(!this.vrBackgroundLoader.loadClip(url,this.vrBackgroundImage))
         {
            this.vrBackgroundImageState = "error: loadClip";
            this.vrBackgroundImage.removeMovieClip();
         }
   }
   function FitVRBackgroundImage()
   {
      var background = this.racePanel.ListBackground;
      var bounds = this.vrBackgroundBounds;
      var imageWidth = this.vrBackgroundExpectedWidth;
      var imageHeight = this.vrBackgroundExpectedHeight;
      if(imageWidth <= 0 || imageHeight <= 0) return undefined;
      // Include every Scaleform ancestor's non-uniform scale. Fitting in the
      // unscaled sprite alone would distort the PNG under the stock X/Y scales.
      var origin = {x:0,y:0};
      var axisX = {x:1,y:0};
      var axisY = {x:0,y:1};
      background.localToGlobal(origin);
      background.localToGlobal(axisX);
      background.localToGlobal(axisY);
      var scaleX = Math.sqrt(Math.pow(axisX.x-origin.x,2)+Math.pow(axisX.y-origin.y,2));
      var scaleY = Math.sqrt(Math.pow(axisY.x-origin.x,2)+Math.pow(axisY.y-origin.y,2));
      if(scaleX <= 0 || scaleY <= 0) return undefined;
      var width = bounds.xMax-bounds.xMin;
      var height = bounds.yMax-bounds.yMin;
      // Stage coordinates are not world-space lengths. Include the measured
      // texture-U/V surface metric relative to the movie's visible frame.
      var projectedX = scaleX*this.vrBackgroundProjectionScale;
      this.vrBackgroundPanelAspect = width*projectedX/(height*scaleY);
      var fit = Math.min(width*projectedX/imageWidth,height*scaleY/imageHeight);
      this.vrBackgroundImage._width = imageWidth*fit/projectedX*Math.abs(background._xscale/100);
      this.vrBackgroundImage._height = imageHeight*fit/scaleY*Math.abs(background._yscale/100);
      // _width/_height on the sibling already include its own Scaleform scale.
      this.vrBackgroundImage._x = this.vrBackgroundOriginX+(bounds.xMin+(width-imageWidth*fit/projectedX)/2)*background._xscale/100;
      this.vrBackgroundImage._y = background._y+(bounds.yMin+(height-imageHeight*fit/scaleY)/2)*background._yscale/100;
      this.vrBackgroundImageState = "loaded";
   }
   function ApplyVRTextColor()
   {
      if(this.PublishVRMenuText != undefined) this.PublishVRMenuText();
      this.PollVRMenuView();
      this.RefreshVRMenuExtensions();
      this.vrAppearanceVisited = 0;
      this.ColorVRTextChildren(this,0);
      if(this.vrBackgroundImageState == "loaded") this.FitVRBackgroundImage();
      this.ApplyVRStyleClips();
   }
   function ColorVRTextChildren(parent,depth)
   {
      if(depth > 12 || this.vrAppearanceVisited >= 2048) return undefined;
      for(var key in parent)
      {
         var child = parent[key];
         // Ignore owner/model references, and never follow _parent back up.
         if(child == undefined || child._parent != parent) continue;
         this.vrAppearanceVisited++;
         if(child instanceof TextField)
         {
            // Preserve semantic green active-tab/feedback colours; replace the
            // default neutral (white/grey) text, including dynamically drawn rows.
            var old = child.textColor;
            var red = old >> 16 & 255;
            var green = old >> 8 & 255;
            var blue = old & 255;
            if(this.vrTextColor >= 0 && (old == child.vrAppliedNeutralColor || (red == green && green == blue)))
            {
               child.textColor = this.vrTextColor;
               child.vrAppliedNeutralColor = this.vrTextColor;
            }
            var profile = this.vrMenuProfile;
            if(profile != undefined)
            {
               if(profile.textOpacity >= 0) child._alpha = profile.textOpacity;
               if((green > red && green > blue) || old == child.vrAppliedAccentColor)
               {
                  if(profile.accentColor >= 0) { child.textColor = profile.accentColor; child.vrAppliedAccentColor = profile.accentColor; }
                  if(profile.accentOpacity >= 0) child._alpha = profile.accentOpacity;
               }
            }
         }
         else if(child instanceof MovieClip) this.ColorVRTextChildren(child,depth+1);
         if(this.vrAppearanceVisited >= 2048) break;
      }
   }
   function ApplyVRMenuProfile(a_name)
   {
      var api = _global.skse.plugins.CharGen;
      if(api.menuProfiles == undefined) return false;
      var profile = api.menuProfiles[a_name];
      if(profile == undefined) return false;
      this.vrMenuProfileName = a_name;
      this.vrMenuProfile = profile;
      var background = this.racePanel.ListBackground;
      if(profile.backgroundColor >= 0) new Color(background).setRGB(profile.backgroundColor);
      if(profile.backgroundOpacity >= 0) background._alpha = profile.backgroundOpacity;
      if(profile.textColor >= 0) this.vrTextColor = profile.textColor;
      if(this.vrPanelOriginal == undefined)
         this.vrPanelOriginal = {x:this.racePanel._x,y:this.racePanel._y,xscale:this.racePanel._xscale,yscale:this.racePanel._yscale};
      var original = this.vrPanelOriginal;
      var scale = profile.panelScale > 0 ? profile.panelScale/100 : 1;
      this.racePanel._xscale = original.xscale*scale;
      this.racePanel._yscale = original.yscale*scale;
      this.racePanel._x = profile.panelX != -9999 ? profile.panelX : original.x;
      this.racePanel._y = profile.panelY != -9999 ? profile.panelY : original.y;
      if(profile.colorPickerScale > 0)
         this.colorField._xscale = this.colorField._yscale = profile.colorPickerScale;
      this.LayoutVRPanel();
      this.ApplyVRStyleClips();
      if(this.PositionVRTextButtons != undefined) this.PositionVRTextButtons();
      if(this.colorField != undefined) this.PositionVRColorPicker();
      return true;
   }
   function ApplyVRStyleClips()
   {
      var profile = this.vrMenuProfile;
      if(profile == undefined) return undefined;
      if(this.vrBackgroundImage != undefined && profile.imageOpacity >= 0) this.vrBackgroundImage._alpha = profile.imageOpacity;
      var selection = this.itemList != undefined && this.itemList.selectedClip != undefined ? this.itemList.selectedClip.selectIndicator : undefined;
      if(selection != undefined && profile.selectionColor >= 0) new Color(selection).setRGB(profile.selectionColor);
      if(selection != undefined && profile.selectionOpacity >= 0) selection._alpha = profile.selectionOpacity;
      var border = this.racePanel.vrPanelBorder;
      if(profile.borderColor >= 0 && border == undefined)
         border = this.racePanel.createEmptyMovieClip("vrPanelBorder",this.racePanel.getNextHighestDepth());
      if(border != undefined)
      {
         border.clear();
         if(profile.borderColor >= 0)
         {
            var b = this.racePanel.ListBackground.getBounds(this.racePanel);
            border.lineStyle(2,profile.borderColor,profile.borderOpacity >= 0 ? profile.borderOpacity : 100);
            border.moveTo(b.xMin,b.yMin); border.lineTo(b.xMax,b.yMin);
            border.lineTo(b.xMax,b.yMax); border.lineTo(b.xMin,b.yMax); border.lineTo(b.xMin,b.yMin);
         }
      }
   }
   function LayoutVRPanel()
   {
      if(this.vrMenuProfile == undefined) return undefined;
      var background = this.racePanel.ListBackground;
      if(this.vrLayoutBackground == undefined) this.vrLayoutBackground = {y:background._y,height:background._height};
      background._y = this.vrLayoutBackground.y;
      background._height = this.vrLayoutBackground.height;
      if(this.vrMenuProfile.consolidate != 1)
      {
         var old = this.vrLayoutOriginal;
         if(old == undefined || !this.vrLayoutConsolidated) return undefined;
         this.racePanel.slidingCategoryList._y = old.categoryY;
         this.racePanel.categoryButtons._y = old.categoryButtonsY;
         this.searchWidget._y = old.searchY;
         this.itemList._y = old.listY;
         this.itemList.listHeight = old.listHeight;
         this.ITEMLIST_HEIGHT_FULL = old.fullHeight;
         this.ITEMLIST_HEIGHT_SHARED = old.sharedHeight;
         this.bottomBar._x = old.barX; this.bottomBar._y = old.barY;
         this.BOTTOMBAR_SHOWN_Y = old.barShownY;
         this.bottomBar.background._visible = old.barVisible;
         this.modeSelect._x = old.modeX; this.modeSelect._y = old.modeY;
         this.modeSelect._xscale = old.modeScaleX; this.modeSelect._yscale = old.modeScaleY;
         this.bottomBar.playerInfo._x = old.infoX; this.bottomBar.playerInfo._y = old.infoY;
         this.navPanel._x = old.navX; this.navPanel._y = old.navY;
         for(var k = 0; k < this.navPanel.buttons.length; k++)
         {
            var restored = this.navPanel.buttons[k];
            this.RestoreVRFooterText(restored.textField);
            if(restored.vrFooterOriginalHeight != undefined) restored.background._height = restored.vrFooterOriginalHeight;
         }
         this.RestoreVRFooterText(this.bottomBar.playerInfo.PlayerRace);
         this.raceDescription._y = this.vrDescriptionOriginalY;
         if(this.bottomBar.playerInfo.RaceLabel != undefined) { this.bottomBar.playerInfo.RaceLabel._x = this.vrRaceLabelOriginalX; this.bottomBar.playerInfo.RaceLabel._visible = this.vrRaceLabelOriginalVisible; }
         this.bottomBar.playerInfo.PlayerRace._x = this.vrPlayerRaceOriginalX;
         this.bottomBar.playerInfo.PlayerRace._y = this.vrPlayerRaceOriginalY;
         this.vrLayoutConsolidated = false;
         return undefined;
      }
      // Keep native-registered clips and renderers in their existing hierarchy.
      // Moving the clips, not copying them, preserves the working mouse route.
      var panel = this.racePanel.ListBackground.getBounds(this);
      if(this.vrLayoutOriginal == undefined)
      {
         this.vrDescriptionOriginalY = this.raceDescription._y;
         if(this.bottomBar.playerInfo.RaceLabel != undefined) { this.vrRaceLabelOriginalX = this.bottomBar.playerInfo.RaceLabel._x; this.vrRaceLabelOriginalVisible = this.bottomBar.playerInfo.RaceLabel._visible; }
         this.vrPlayerRaceOriginalX = this.bottomBar.playerInfo.PlayerRace._x;
         this.vrPlayerRaceOriginalY = this.bottomBar.playerInfo.PlayerRace._y;
         this.vrLayoutOriginal = {listY:this.itemList._y,listHeight:this.itemList.listHeight,fullHeight:this.ITEMLIST_HEIGHT_FULL,sharedHeight:this.ITEMLIST_HEIGHT_SHARED,categoryY:this.racePanel.slidingCategoryList._y,categoryButtonsY:this.racePanel.categoryButtons._y,searchY:this.searchWidget._y,barX:this.bottomBar._x,barY:this.bottomBar._y,barShownY:this.BOTTOMBAR_SHOWN_Y,barVisible:this.bottomBar.background._visible,modeX:this.modeSelect._x,modeY:this.modeSelect._y,modeScaleX:this.modeSelect._xscale,modeScaleY:this.modeSelect._yscale,infoX:this.bottomBar.playerInfo._x,infoY:this.bottomBar.playerInfo._y};
      }
      if(this.vrLayoutOriginal.navX == undefined) { this.vrLayoutOriginal.navX = this.navPanel._x; this.vrLayoutOriginal.navY = this.navPanel._y; }
      this.vrLayoutConsolidated = true;
      this.racePanel.slidingCategoryList._y = this.vrLayoutOriginal.categoryY+75;
      this.racePanel.categoryButtons._y = this.vrLayoutOriginal.categoryButtonsY+75;
      this.searchWidget._y = this.vrLayoutOriginal.searchY+75;
      this.itemList._y = this.vrLayoutOriginal.listY+75;
      this.ITEMLIST_HEIGHT_FULL = this.vrLayoutOriginal.fullHeight-175;
      this.ITEMLIST_HEIGHT_SHARED = this.vrLayoutOriginal.sharedHeight-175;
      this.itemList.listHeight = this.raceDescription._visible ? this.ITEMLIST_HEIGHT_SHARED : this.ITEMLIST_HEIGHT_FULL;
      this.raceDescription._y = this.vrDescriptionOriginalY-45;
      this.bottomBar.background._visible = false;
      this.bottomBar._x = panel.xMin;
      this.bottomBar._y = panel.yMax-160;
      this.BOTTOMBAR_SHOWN_Y = this.bottomBar._y;
      this.modeSelect._width = panel.xMax-panel.xMin-16;
      var modeBounds = this.modeSelect.getBounds(this);
      this.modeSelect._x += panel.xMin+8-modeBounds.xMin;
      this.modeSelect._y += panel.yMin+8-modeBounds.yMin;
      this.navPanel._x = 12; this.navPanel._y = 8;
      this.bottomBar.playerInfo._x = 12;
      if(this.bottomBar.playerInfo.RaceLabel != undefined) this.bottomBar.playerInfo.RaceLabel._visible = false;
      this.bottomBar.playerInfo.PlayerRace._x = 0;
      this.bottomBar.playerInfo.PlayerRace._y = 0;
      this.SizeVRFooterText(this.bottomBar.playerInfo.PlayerRace);
      // Legacy name fields are hidden by PositionVRTextButtons; move all
      // remaining race fields together without relying on a localized name.
      var available = panel.xMax-panel.xMin-24;
      var x = 0;
      var y = 0;
      for(var i = 0; i < this.navPanel.buttons.length; i++)
      {
         var button = this.navPanel.buttons[i];
         if(button == this.vrFilterButton) continue;
         if(button._visible == false || button.label == "") continue;
         this.SizeVRFooterText(button.textField);
         if(button.update != undefined) button.update();
         if(button.background != undefined && button.textField != undefined)
         {
            if(button.vrFooterOriginalHeight == undefined) button.vrFooterOriginalHeight = button.background._height;
            button.background._height = button.textField._height+8;
         }
         if(x > 0 && x+button._width > available) { x = 0; y += 40; }
         button._x = x; button._y = y;
         x += button._width+8;
      }
      // Race is a padded final footer row, not pinned to the panel's bottom edge.
      this.bottomBar.playerInfo._y = y+52;
      var extraFooterHeight = Math.max(0,y+100-160);
      this.bottomBar._y -= extraFooterHeight;
      this.BOTTOMBAR_SHOWN_Y = this.bottomBar._y;
      this.ITEMLIST_HEIGHT_FULL -= extraFooterHeight;
      this.ITEMLIST_HEIGHT_SHARED -= extraFooterHeight;
      this.itemList.listHeight = this.raceDescription._visible ? this.ITEMLIST_HEIGHT_SHARED : this.ITEMLIST_HEIGHT_FULL;
      // Reserve a clear strip ABOVE the black/image panel for the four tabs.
      // Only the background is shortened; registered control geometry is not scaled.
      background._y = this.vrLayoutBackground.y+70;
      background._height = this.vrLayoutBackground.height-70;
   }
   function SizeVRFooterText(field)
   {
      if(field == undefined || field.getTextFormat == undefined) return undefined;
      if(field.vrFooterFormat == undefined) field.vrFooterFormat = {current:field.getTextFormat(),future:field.getNewTextFormat()};
      var format = field.getTextFormat();
      // Slider labels are 24px under the stock list's 1.036 vertical scale.
      // 25px here matches their visible height without distorting the glyphs.
      format.size = 25;
      field.setTextFormat(format); field.setNewTextFormat(format);
      field.autoSize = "left";
   }
   function RestoreVRFooterText(field)
   {
      if(field == undefined || field.vrFooterFormat == undefined) return undefined;
      field.setTextFormat(field.vrFooterFormat.current);
      field.setNewTextFormat(field.vrFooterFormat.future);
   }
   function PositionVRColorPicker()
   {
      var panel = this.racePanel.ListBackground.getBounds(this);
      var picker = this.colorField.getBounds(this);
      var centre = (panel.xMin+panel.xMax)/2;
      var halfWidth = (picker.xMax-picker.xMin)/2;
      var left = Stage.visibleRect.x;
      var right = left+Stage.visibleRect.width;
      if(halfWidth*2 <= right-left) centre = Math.max(left+halfWidth,Math.min(right-halfWidth,centre));
      this.colorField._x += centre-(picker.xMin+picker.xMax)/2;
   }
   function UpdateVRWorldPlacement()
   {
      var api = _global.skse.plugins.CharGen;
      if(api.SetMenuPolarPlacement == undefined || this.vrMenuProfile == undefined) return undefined;
      // World placement is shared between normal and face views. The face
      // profile still supplies styling and face-camera settings, not a second
      // independently positioned menu.
      var profile = api.menuProfiles != undefined && api.menuProfiles["VR Normal"] != undefined ? api.menuProfiles["VR Normal"] : this.vrMenuProfile;
      var modal = this.colorField._visible;
      var sculpt = this.vertexEditor.enabled && this.vertexEditor.vrWorkspaceBounds != undefined;
      var bounds = modal ? this.colorField.getBounds(this) : this.racePanel.ListBackground.getBounds(this);
      var point = {x:(bounds.xMin+bounds.xMax)/2,y:(bounds.yMin+bounds.yMax)/2};
      if(sculpt && !modal)
      {
         bounds = this.vertexEditor.vrWorkspaceBounds;
         point = {x:(bounds.xMin+bounds.xMax)/2,y:(bounds.yMin+bounds.yMax)/2};
         this.vertexEditor.localToGlobal(point);
      }
      else this.localToGlobal(point);
      var azimuth = modal ? profile.pickerAzimuth : profile.azimuth;
      var elevation = modal ? profile.pickerElevation : profile.elevation;
      var distance = modal ? profile.pickerDistance : profile.distance;
      var scale = modal ? profile.pickerSurfaceScale : profile.surfaceScale;
      // Positive angles are LEFT of the starting avatar-facing view.
      if(azimuth == undefined || azimuth == -9999) azimuth = modal ? 38 : 30;
      // Sculpt's wider workspace needs its own leftward placement. Modal
      // picker placement and the ordinary Sliders/Presets angle are unchanged.
      if(sculpt && !modal) azimuth = profile.sculptAzimuth == undefined || profile.sculptAzimuth == -9999 ? 48 : profile.sculptAzimuth;
      if(elevation == undefined || elevation == -9999) elevation = 0;
      if(distance == undefined || distance == -9999) distance = 100;
      if(scale == undefined || scale == -9999) scale = 100;
      if(api.GetMenuPolarPlacementState != undefined) this.vrWorldPlacementState = Number(api.GetMenuPolarPlacementState());
      var view = api.GetMenuView != undefined ? Number(api.GetMenuView()) : 0;
      var placementKey = view+":"+modal+":"+sculpt+":"+this.vertexEditor.vrLargeCanvas+":"+azimuth+":"+elevation+":"+distance+":"+scale;
      // Polling observes transitions; it does not continuously reposition the
      // surface. Row selection, action wrapping and headset motion are not
      // placement events. Retry only while the native surface is not ready.
      if(placementKey == this.vrWorldPlacementKey && this.vrWorldPlacementState != 0) return undefined;
      if(api.SetMenuPolarPlacement(azimuth,elevation,distance,scale,point.x,point.y)) this.vrWorldPlacementKey = placementKey;
   }
   function ApplyVRCategoryPresentation()
   {
      var api = _global.skse.plugins.CharGen;
      if(api.GetMenuCategoryPresentation == undefined) return undefined;
      var entries = this.categoryList.entryList;
      for(var i = entries.length-1; i >= 0; i--)
      {
         var entry = entries[i];
         var settings = entry.vrProvider != undefined ? api.GetMenuCategoryPresentation(entry.flag,entry.vrProvider,entry.vrSection) : api.GetMenuCategoryPresentation(entry.flag);
         if(settings == undefined) continue;
         if(settings.visible == 0 && i != 0) entries.splice(i,1);
         else
         {
            if(settings.label != "") entry.text = settings.label;
            if(settings.order != -9999) entry.priority = settings.order;
         }
      }
   }
   function RequestVRMenuView()
   {
      var api = _global.skse.plugins.CharGen;
      var current = Number(api.GetMenuView());
      if(!api.SetMenuView(current == 1 ? 0 : 1)) this.setStatusText("View change unavailable",2000);
   }
   function PollVRMenuView()
   {
      var api = _global.skse.plugins.CharGen;
      if(api.avatarLightingSupported)
      {
         api.UpdateAvatarLighting();
         var lightState = Number(api.GetAvatarLightingState());
         if(lightState == 2 && this.vrLightFault != 2)
         {
            this.bShowLight = false;
            this.updateBottomBar();
            this.setStatusText("Front lighting unavailable: avatar/head/scene not resolved",3000);
         }
         this.vrLightFault = lightState;
      }
      if(api.GetMenuView == undefined) return undefined;
      var current = Number(api.GetMenuView());
      if(current == 1) api.UpdateMenuView();
      var name = current == 1 ? "VR Face" : "VR Normal";
      if(name != this.vrMenuProfileName)
      {
         this.ApplyVRMenuProfile(name);
         this.bPlayerZoom = current == 1;
         this.updateBottomBar();
         if(this.vertexEditor.enabled) this.vertexEditor.updateBottomBar();
      }
      if(current == 2 && this.vrMenuViewFault != 2) this.setStatusText("Face view unavailable: tracking origin/head not resolved",3000);
      this.vrMenuViewFault = current;
      this.UpdateVRWorldPlacement();
   }
   function RefreshVRMenuExtensions()
   {
      var api = _global.skse.plugins.CharGen;
      if(api.GetMenuExtensions == undefined || !this.bMenuInitialized) return undefined;
      var revision = Number(api.GetMenuExtensionsRevision());
      var items = this.itemList.entryList;
      var categories = this.categoryList.entryList;
      var count = 0;
      var catCount = 0;
      for(var i = 0; i < items.length; i++) if(items[i].vrNativeExtension) count++;
      for(var i = 0; i < categories.length; i++) if(categories[i].vrNativeExtension) catCount++;
      if(revision == this.vrExtensionsRevision && count == this.vrExtensionCount && catCount == this.vrExtensionCategoryCount) return undefined;
      for(var i = items.length-1; i >= 0; i--) if(items[i].vrNativeExtension) items.splice(i,1);
      for(var i = categories.length-1; i >= 0; i--) if(categories[i].vrNativeExtension) categories.splice(i,1);
      var sections = api.GetMenuExtensions();
      this.vrExtensionCount = 0;
      this.vrExtensionCategoryCount = 0;
      for(var i = 0; i < sections.length; i++)
      {
         var section = sections[i];
         var conflict = false;
         for(var j = 1; j < categories.length; j++) if(categories[j].flag & section.flag) conflict = true;
         if(conflict) { this.vrExtensionFault = "category flag conflict: " + section.provider + ":" + section.id; continue; }
         var cat = {type:RaceMenuDefines.ENTRY_TYPE_CAT,bDontHide:false,filterFlag:1,text:section.label,flag:section.flag,priority:section.order,enabled:true,vrNativeExtension:true,vrProvider:section.provider,vrSection:section.id};
         categories.push(cat);
         categories[0].flag |= section.flag;
         this.vrExtensionCategoryCount++;
         var presentation = api.GetMenuCategoryPresentation(section.flag,section.provider,section.id);
         for(var j = 0; j < section.controls.length; j++)
         {
            var control = section.controls[j];
            if(presentation.controlIds != "" && (","+presentation.controlIds+",").indexOf(","+control.id+",") < 0) continue;
            var entry = {type:RaceMenuDefines.ENTRY_TYPE_SLIDER,text:control.label,filterFlag:section.flag,callbackName:"",sliderMin:control.minimum,sliderMax:control.maximum,sliderID:0,position:control.value,interval:control.step,enabled:true,vrNativeExtension:true,vrProvider:section.provider,vrControl:control.id};
            entry.internalCallback = function()
            {
               var entry = this.entryObject;
               _global.skse.plugins.CharGen.SetMenuExtensionValue(entry.vrProvider,entry.vrControl,this.position);
            };
            entry.GetTextureList = function() { return null; };
            entry.isColorEnabled = entry.hasColor = entry.hasGlow = function() { return false; };
            items.push(entry);
            this.vrExtensionCount++;
         }
      }
      this.ApplyVRCategoryPresentation();
      // Hidden categories still expose their controls through All; hiding is
      // presentation only. Count after those presentation rules to avoid churn.
      this.vrExtensionCategoryCount = 0;
      for(var i = 0; i < categories.length; i++) if(categories[i].vrNativeExtension) this.vrExtensionCategoryCount++;
      this.vrExtensionsRevision = revision;
      this.categoryList.requestInvalidate();
      this.itemList.requestInvalidate();
   }
