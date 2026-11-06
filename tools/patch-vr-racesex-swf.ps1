[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$InputSwf,

    [Parameter(Mandatory)]
    [string]$OutputSwf,

    [Parameter(Mandatory)]
    [string]$FfdecCli,

    [Parameter(Mandatory)]
    [string]$WorkingDirectory,

    [string]$SearchWidgetSource
)

$ErrorActionPreference = 'Stop'

$expectedInputSha256 = '3A012DA4FED80637CE3257B9B2B89243BEFAB29A4BEC5316A935CEA87C889963'
$resolvedInput = [IO.Path]::GetFullPath($InputSwf)
$resolvedOutput = [IO.Path]::GetFullPath($OutputSwf)
$resolvedFfdec = [IO.Path]::GetFullPath($FfdecCli)
$resolvedWork = Join-Path ([IO.Path]::GetFullPath($WorkingDirectory)) ('patch-' + [Guid]::NewGuid().ToString('N'))
$resolvedSearchWidget = if ($SearchWidgetSource) { [IO.Path]::GetFullPath($SearchWidgetSource) } else { Join-Path $resolvedWork 'SearchWidget.as' }
if ($resolvedInput -eq $resolvedOutput -or (Test-Path -LiteralPath $resolvedOutput)) {
    throw 'OutputSwf must be a new file, distinct from the original movie. Existing files are never overwritten.'
}

foreach ($path in @($resolvedInput, $resolvedFfdec) + $(if ($SearchWidgetSource) { @($resolvedSearchWidget) } else { @() })) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required file does not exist: $path"
    }
}

$inputHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedInput).Hash
if ($inputHash -ne $expectedInputSha256) {
    throw "Unexpected VR RaceSexMenu input hash. Expected $expectedInputSha256, found $inputHash."
}

New-Item -ItemType Directory -Force -Path $resolvedWork | Out-Null
$outputParent = Split-Path -Parent $resolvedOutput
New-Item -ItemType Directory -Force -Path $outputParent | Out-Null

# Generate the private derivative from the user's original asset. The public
# recipe contains only our changes, never a complete decompiled upstream class.
if (-not $SearchWidgetSource) {
    $originalScripts = Join-Path $resolvedWork 'original-scripts'
    & $resolvedFfdec -export script $originalScripts $resolvedInput | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'JPEXS failed to export the original SearchWidget.' }
    $searchText = Get-Content -Raw -LiteralPath (Join-Path $originalScripts 'scripts\__Packages\skyui\components\SearchWidget.as')
    function Add-SearchChange([string]$Needle, [string]$Replacement) {
        if ([regex]::Matches($script:searchText, [regex]::Escape($Needle)).Count -ne 1) { throw 'SearchWidget recipe anchor is missing or ambiguous.' }
        $script:searchText = $script:searchText.Replace($Needle, $Replacement)
    }
    Add-SearchChange '   var _previousFocus;' "   var _previousFocus;`r`n`tvar _initialText;"
    Add-SearchChange '   var dispatchEvent;' "   var dispatchEvent;`r`n   var icon;"
    $pressHandlers = @'
      this.textField.onPress = function()
      {
         this._parent.startInput();
      };
      this.icon.onPress = function()
      {
         this._parent.startInput();
      };
'@ -replace "`n", "`r`n"
    $initialLabel = '      this.textField.SetText(skyui.components.SearchWidget.S_FILTER);'
    # The label also occurs in endInput; insert only at the constructor anchor.
    $configAnchor = "      skyui.util.ConfigManager.registerLoadCallback(this,`"onConfigLoad`");"
    Add-SearchChange ($initialLabel + "`r`n" + $configAnchor) ($pressHandlers + "`r`n" + $initialLabel + "`r`n" + $configAnchor)
    $virtualHandler = @'
	function onVirtualKeyboard(a_text, a_error)
	{
		if(!this._bActive)
		{
			return undefined;
		}
		if(a_error)
		{
			this.textField.SetText(this._initialText);
		}
		else
		{
			this.textField.SetText(a_text);
		}
		this.endInput();
	}
'@ -replace "`n", "`r`n"
    Add-SearchChange '   function startInput()' ($virtualHandler + "`r`n   function startInput()")
    $focusAnchor = '      this._previousFocus = gfx.managers.FocusHandler.instance.getFocus(0);'
    $vrStart = @'
      if(_global.skse.IsVR())
      {
         this.vrTextOwner.BeginVRTextInput("filter");
         return undefined;
      }
'@ -replace "`n", "`r`n"
    Add-SearchChange $focusAnchor ($vrStart + "`r`n" + $focusAnchor)
    $initialValue = "`t  this._initialText = this.textField.text;`r`n`t  if(this._initialText == skyui.components.SearchWidget.S_FILTER)`r`n`t  {`r`n`t`t this._initialText = `"`";`r`n`t  }`r`n`t  this._currentInput = this._lastInput = undefined;"
    Add-SearchChange '      this._currentInput = this._lastInput = undefined;' $initialValue
    Add-SearchChange '      Selection.setSelection(0,0);' "      Selection.setSelection(0,0);`r`n`t  this.textField.background = true;`r`n`t  this.textField.backgroundColor = 2105376;`r`n`t  this.textField.border = true;`r`n`t  this.textField.borderColor = 16777215;"
    Add-SearchChange '      this.textField.maxChars = null;' "      this.textField.maxChars = null;`r`n`t  this.textField.background = false;`r`n`t  this.textField.border = false;"
    [IO.File]::WriteAllText($resolvedSearchWidget, $searchText, [Text.UTF8Encoding]::new($false))
}

$sourceXml = Join-Path $resolvedWork 'RaceSex_menu.source.xml'
$patchedXml = Join-Path $resolvedWork 'RaceSex_menu.patched.xml'
$verificationXml = Join-Path $resolvedWork 'RaceSex_menu.verification.xml'
$xmlRebuiltSwf = Join-Path $resolvedWork 'RaceSex_menu.xml-rebuilt.swf'
$searchReplacedSwf = Join-Path $resolvedWork 'RaceSex_menu.search-replaced.swf'
$sliderReplacedSwf = Join-Path $resolvedWork 'RaceSex_menu.slider-replaced.swf'
$buttonPanelReplacedSwf = Join-Path $resolvedWork 'RaceSex_menu.button-panel-replaced.swf'
$sourceScripts = Join-Path $resolvedWork 'source-scripts'
$patchedRaceMenu = Join-Path $resolvedWork 'RaceMenu.as'
$patchedRaceMenuSlider = Join-Path $resolvedWork 'RaceMenuSlider.as'
$patchedButtonPanel = Join-Path $resolvedWork 'ButtonPanel.as'
$verificationScripts = Join-Path $resolvedWork 'verification-scripts'

& $resolvedFfdec -swf2xml $resolvedInput $sourceXml
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $sourceXml -PathType Leaf)) {
    throw "JPEXS failed to export the source SWF (exit $LASTEXITCODE)."
}

$document = [System.Xml.XmlDocument]::new()
$document.PreserveWhitespace = $true
$document.Load($sourceXml)

$trackNode = $document.SelectSingleNode("//item[@type='DefineSpriteTag' and @spriteId='33']/subTags/item[@type='PlaceObject2Tag' and @characterId='32']/matrix")
$trackPlacementNode = $document.SelectSingleNode("//item[@type='DefineSpriteTag' and @spriteId='39']/subTags/item[@type='PlaceObject2Tag' and @characterId='33']/matrix")
$endNode = $document.SelectSingleNode("//item[@type='DefineSpriteTag' and @spriteId='39']/subTags/item[@type='PlaceObject2Tag' and @characterId='34']/matrix")
$sliderShape = $document.SelectSingleNode("//item[@type='DefineShape3Tag' and @shapeId='31']")
$searchShape = $document.SelectSingleNode("//item[@type='DefineShapeTag' and @shapeId='174']")
$scrollTrackShape = $document.SelectSingleNode("//item[@type='DefineShape3Tag' and @shapeId='73']")
$scrollThumbShape = $document.SelectSingleNode("//item[@type='DefineShape3Tag' and @shapeId='79']")
$colorFieldNode = $document.SelectSingleNode("//item[@type='DefineSpriteTag' and @spriteId='197']/subTags/item[@type='PlaceObject2Tag' and @characterId='170' and @name='colorField']/matrix")
if (-not $trackNode -or -not $trackPlacementNode -or -not $endNode -or -not $sliderShape -or -not $searchShape -or -not $scrollTrackShape -or -not $scrollThumbShape -or -not $colorFieldNode) {
    throw 'The expected RaceMenu 0.4.20 VR slider artwork nodes were not found.'
}
if ($trackNode.GetAttribute('scaleX') -ne '2.1213074') {
    throw "Unexpected regular-slider track scale: $($trackNode.GetAttribute('scaleX'))"
}
if ($trackPlacementNode.GetAttribute('translateX') -ne '258') {
    throw "Unexpected regular-slider track start: $($trackPlacementNode.GetAttribute('translateX'))"
}
if ($endNode.GetAttribute('translateX') -ne '7250') {
    throw "Unexpected regular-slider right-end position: $($endNode.GetAttribute('translateX'))"
}
if ($colorFieldNode.GetAttribute('hasScale') -ne 'false' -or
    $colorFieldNode.GetAttribute('translateX') -ne '10240' -or
    $colorFieldNode.GetAttribute('translateY') -ne '10240') {
    throw 'Unexpected VR ColorField placement transform.'
}
$sliderFills = @($sliderShape.SelectNodes('shapes/fillStyles/fillStyles/item'))
$searchFills = @($searchShape.SelectNodes('shapes/fillStyles/fillStyles/item'))
$scrollTrackFills = @($scrollTrackShape.SelectNodes('shapes/fillStyles/fillStyles/item'))
$scrollThumbFills = @($scrollThumbShape.SelectNodes('shapes/fillStyles/fillStyles/item'))
if ($sliderFills.Count -ne 3 -or
    $sliderFills[0].color.GetAttribute('alpha') -ne '51' -or
    $sliderFills[1].color.GetAttribute('alpha') -ne '255' -or
    $sliderFills[2].color.GetAttribute('alpha') -ne '255') {
    throw 'Unexpected regular-slider fill structure.'
}
if ($searchFills.Count -ne 2 -or
    $searchFills[0].color.GetAttribute('red') -ne '153' -or
    $searchFills[0].color.GetAttribute('green') -ne '153' -or
    $searchFills[0].color.GetAttribute('blue') -ne '153') {
    throw 'Unexpected search-field border fill structure.'
}
if ($scrollTrackFills.Count -ne 3 -or
    $scrollTrackFills[0].color.GetAttribute('alpha') -ne '255' -or
    $scrollTrackFills[1].color.GetAttribute('alpha') -ne '255' -or
    $scrollTrackFills[2].color.GetAttribute('alpha') -ne '51') {
    throw 'Unexpected vertical-scroll-track fill structure.'
}
if ($scrollThumbFills.Count -ne 2 -or
    $scrollThumbFills[0].color.GetAttribute('alpha') -ne '255' -or
    $scrollThumbFills[1].color.GetAttribute('alpha') -ne '153') {
    throw 'Unexpected vertical-scroll-thumb fill structure.'
}

# The stock track and right cap already share the same endpoint. RaceMenu's
# Slider instance also supplies its intended 42-pixel functional inset, so do
# not override or rescale either contract. The regular track is three stacked,
# high-contrast strips only a few source pixels apart; at the projected size
# those strips alias into a strong moire pattern. Keep one translucent centre fill
# and make the two decorative edge fills transparent. Their vector geometry
# remains in the movie, so component and button bounds do not change.
$sliderFills[1].color.SetAttribute('alpha', '0')
$sliderFills[2].color.SetAttribute('alpha', '0')

# SearchWidget's dense bevel uses the same near-pixel repeated-line treatment.
# DefineShapeTag has no alpha channel, so make the bevel black like the field.
# This eliminates the remaining projected outline while retaining the exact
# hit geometry; focus feedback is supplied dynamically by SearchWidget.
foreach ($channel in @('red', 'green', 'blue')) {
    $searchFills[0].color.SetAttribute($channel, '0')
}

# The vertical scrollbar uses the same dense dark/light edge pair as the old
# slider artwork.  Retain its broad translucent track but remove those two
# sub-pixel decorative strips to prevent projection moire.
$scrollTrackFills[0].color.SetAttribute('alpha', '0')
$scrollTrackFills[1].color.SetAttribute('alpha', '0')

# The moving thumb contains an inset rectangle whose edges alias after
# projection.  Removing that fill made the thumb hollow because the inner path
# is a cut-out.  Give both paths the same opaque colour instead, producing one
# visually solid, low-frequency thumb.
foreach ($channel in @('red', 'green', 'blue')) {
    $scrollThumbFills[1].color.SetAttribute($channel, '242')
}
$scrollThumbFills[1].color.SetAttribute('alpha', '255')

# ColorField is a modal child of RaceMenu.  Scale the complete child rather than
# its artwork so the HSV/alpha rails, navigation arrows, button panels, and their
# mouse hit regions remain coincident.  Its translation is still assigned by
# RaceMenu.as at runtime; only the local display and interaction scale changes.
$colorFieldNode.SetAttribute('hasScale', 'true')
$colorFieldNode.SetAttribute('nScaleBits', '18')
$colorFieldNode.SetAttribute('scaleX', '2.25')
$colorFieldNode.SetAttribute('scaleY', '2.25')

# A directly placed shape is not an addressable MovieClip: naming it did not
# make AS2 _y updates move the stock double rule. Remove this decoration from
# the VR movie rather than leaving it across Filter. No control/hit tag changes.
$categoryDivider = $document.SelectSingleNode("//item[@type='DefineSpriteTag' and @spriteId='190']/subTags/item[@type='PlaceObject2Tag' and @characterId='153' and @depth='23']")
if (-not $categoryDivider -or $categoryDivider.matrix.GetAttribute('translateY') -ne '1783') {
    throw 'Unexpected stock category divider placement.'
}
[void]$categoryDivider.ParentNode.RemoveChild($categoryDivider)

# FFDec 26.2.1's XML importer rejects a UTF-8 BOM even though the XML standard
# permits one, so write the document explicitly as BOM-less UTF-8.
$xmlSettings = [System.Xml.XmlWriterSettings]::new()
$xmlSettings.Encoding = [System.Text.UTF8Encoding]::new($false)
$xmlSettings.Indent = $false
$xmlWriter = [System.Xml.XmlWriter]::Create($patchedXml, $xmlSettings)
try {
    $document.Save($xmlWriter)
} finally {
    $xmlWriter.Dispose()
}

Remove-Item -LiteralPath $xmlRebuiltSwf, $searchReplacedSwf, $sliderReplacedSwf, $resolvedOutput -Force -ErrorAction SilentlyContinue
& $resolvedFfdec -xml2swf $patchedXml $xmlRebuiltSwf
if ($LASTEXITCODE -ne 0 -or
    -not (Test-Path -LiteralPath $xmlRebuiltSwf -PathType Leaf) -or
    (Get-Item -LiteralPath $xmlRebuiltSwf).Length -lt 1024) {
    throw "JPEXS failed to rebuild the patched SWF (exit $LASTEXITCODE)."
}

# Keep the stock search widget's visible focus state and runtime keyboard
# abstraction. RaceMenu.as adds ordinary MappedButton hit surfaces over the
# filter and name regions so VR uses the same proven CLIK click route as the
# working bottom-bar controls.
& $resolvedFfdec -replace $xmlRebuiltSwf $searchReplacedSwf '\__Packages\skyui\components\SearchWidget' $resolvedSearchWidget
if ($LASTEXITCODE -ne 0 -or
    -not (Test-Path -LiteralPath $searchReplacedSwf -PathType Leaf) -or
    (Get-Item -LiteralPath $searchReplacedSwf).Length -lt 1024) {
    throw "JPEXS failed to replace SearchWidget ActionScript (exit $LASTEXITCODE)."
}

# RaceMenu's existing Papyrus category service remains the source of category
# entries.  This generated VR-only class adds a stable INI visibility mask and
# routes real CLIK button hits into the stock keyboard-backed paths.
Remove-Item -LiteralPath $sourceScripts -Recurse -Force -ErrorAction SilentlyContinue
& $resolvedFfdec -export script $sourceScripts $searchReplacedSwf | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "JPEXS failed to export RaceMenu ActionScript for the VR extensions (exit $LASTEXITCODE)."
}
$sourceRaceMenu = Join-Path $sourceScripts 'scripts\__Packages\RaceMenu.as'
$sourceRaceMenuSlider = Join-Path $sourceScripts 'scripts\__Packages\RaceMenuSlider.as'
$sourceButtonPanel = Join-Path $sourceScripts 'scripts\__Packages\skyui\components\ButtonPanel.as'
if (-not (Test-Path -LiteralPath $sourceRaceMenu -PathType Leaf) -or
    -not (Test-Path -LiteralPath $sourceRaceMenuSlider -PathType Leaf)) {
    throw 'The source SWF did not export the expected RaceMenu ActionScript classes.'
}
$raceMenuText = Get-Content -Raw -LiteralPath $sourceRaceMenu
$raceMenuSliderText = Get-Content -Raw -LiteralPath $sourceRaceMenuSlider
$buttonPanelText = Get-Content -Raw -LiteralPath $sourceButtonPanel
$panelCapacityNeedle = '      var _loc3_ = 0;'
if (-not $buttonPanelText.Contains($panelCapacityNeedle)) { throw 'ButtonPanel constructor capacity anchor missing.' }
$capacityIndex = $buttonPanelText.IndexOf($panelCapacityNeedle)
# Preserve the established generic footer expansion. Sculpt's dynamically
# attached staticPanel has its own explicit capacity, patched below.
$capacityAddition = "      if(_global.skse.IsVR() && this._parent._name == `"bottomBar`") this.maxButtons += 2;`r`n"
$buttonPanelText = $buttonPanelText.Insert($capacityIndex, $capacityAddition)

$sliderDrawNeedle = "   function handleInput(details, pathToFocus)"
$sliderDrawReplacement = @'
   function draw()
   {
      super.draw();
      if(_global.skse.IsVR())
      {
         this.track._x = 12.9;
         // BrushSlider uses the same class but different, shorter artwork.
         this.track._width = this._parent instanceof BrushListEntry ? 164.85 : 349.6;
      }
   }
   function handleInput(details, pathToFocus)
'@ -replace "`n", "`r`n"
if (-not $raceMenuSliderText.Contains($sliderDrawNeedle)) {
    throw 'The expected RaceMenuSlider handleInput anchor was not found.'
}
$raceMenuSliderText = $raceMenuSliderText.Replace($sliderDrawNeedle, $sliderDrawReplacement)

$initNeedle = "      this.colorField._x = this.racePanel._x + this.racePanel._width / 2;`r`n      this.makeupPanel._x = this.racePanel._x + this.racePanel._width / 2;"
$initReplacement = @'
      this.colorField._x = this.racePanel._x + this.racePanel._width / 2;
      this.makeupPanel._x = this.racePanel._x + this.racePanel._width / 2;
      if(_global.skse.IsVR())
      {
         this.colorField._x = this.racePanel._x + this.racePanel._width * 0.875;
         this.SetupVRTextButtons();
         this.SetupVRMenuAppearance();
         this.PositionVRColorPicker();
      }
'@ -replace "`n", "`r`n"
if (-not $raceMenuText.Contains($initNeedle)) {
    throw 'The expected RaceMenu InitExtensions anchor was not found.'
}
$raceMenuText = $raceMenuText.Replace($initNeedle, $initReplacement)

# Stock text buttons remain in the registered bottom-bar panel, but one is
# positioned at Filter. Its bounds must not redefine the bar's animation height
# if extensions are initialized again after that positioning.
$heightNeedle = "      this.BOTTOMBAR_SHOWN_Y = Stage.originalRect.height - this.bottomBar._height;`r`n      this.BOTTOMBAR_HIDDEN_Y = Stage.originalRect.height + this.bottomBar._height;"
$heightReplacement = @'
      var vrBarHeight = this.bottomBar._height;
      if(_global.skse.IsVR())
      {
         if(this.vrBottomBarOriginalHeight == undefined) this.vrBottomBarOriginalHeight = vrBarHeight;
         vrBarHeight = this.vrBottomBarOriginalHeight;
      }
      this.BOTTOMBAR_SHOWN_Y = Stage.originalRect.height - vrBarHeight;
      this.BOTTOMBAR_HIDDEN_Y = Stage.originalRect.height + vrBarHeight;
'@ -replace "`n", "`r`n"
if ([regex]::Matches($raceMenuText, [regex]::Escape($heightNeedle)).Count -ne 1) { throw 'Bottom-bar height anchor not unique.' }
$raceMenuText = $raceMenuText.Replace($heightNeedle, $heightReplacement)

$platformNeedle = '   function SetPlatform(a_platform, a_bPS3Switch)'
$vrFunctions = @'
   function IsVRCategoryVisible(a_flag)
   {
      var _loc2_ = Number(_global.skse.plugins.CharGen.vrCategoryMask);
      return a_flag == 0 || isNaN(_loc2_) || (_loc2_ & a_flag) != 0;
   }
'@
if (-not $raceMenuText.Contains($platformNeedle)) {
    throw 'The expected RaceMenu SetPlatform anchor was not found.'
}
$raceMenuText = $raceMenuText.Replace($platformNeedle, $vrFunctions + $platformNeedle)
$textEntryText = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'vr-racesex-patches\TextEntry.as.inc')
$raceMenuText = $raceMenuText.Replace($platformNeedle, $textEntryText + $platformNeedle)
$appearanceText = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'vr-racesex-patches\Appearance.as.inc')
$raceMenuText = $raceMenuText.Replace($platformNeedle, $appearanceText + $platformNeedle)
$zoomToggle = '      this.bPlayerZoom = !this.bPlayerZoom;'
$lightToggle = '      this.bShowLight = !this.bShowLight;'
if ([regex]::Matches($raceMenuText, [regex]::Escape($lightToggle)).Count -ne 1) { throw 'Light toggle anchor not unique.' }
$lightReplacement = @'
      if(_global.skse.IsVR() && _global.skse.plugins.CharGen.avatarLightingSupported)
      {
         if(_global.skse.plugins.CharGen.SetAvatarLighting(!this.bShowLight))
         {
            this.bShowLight = !this.bShowLight;
            this.updateBottomBar();
         }
         else this.setStatusText("Lighting change unavailable; please try again",2000);
         return undefined;
      }
'@ -replace "`n", "`r`n"
$raceMenuText = $raceMenuText.Replace($lightToggle, $lightReplacement + "`r`n" + $lightToggle)
if ([regex]::Matches($raceMenuText, [regex]::Escape($zoomToggle)).Count -ne 1) { throw 'Zoom toggle anchor not unique.' }
$raceMenuText = $raceMenuText.Replace($zoomToggle, "      if(_global.skse.IsVR() && _global.skse.plugins.CharGen.SetMenuView != undefined)`r`n      {`r`n         this.RequestVRMenuView();`r`n         return undefined;`r`n      }`r`n" + $zoomToggle)
$zoomLabel = '(!this.bPlayerZoom ? "$Zoom In" : "$Zoom Out")'
$raceMenuText = $raceMenuText.Replace($zoomLabel, '(_global.skse.IsVR() && _global.skse.plugins.CharGen.GetMenuView != undefined ? (_global.skse.plugins.CharGen.GetMenuView() == 1 ? "Normal view" : "Face view") : (!this.bPlayerZoom ? "$Zoom In" : "$Zoom Out"))')
$categoryFinish = '      skse.SendModEvent(_global.eventPrefix + "CategoriesInitialized");'
if ([regex]::Matches($raceMenuText, [regex]::Escape($categoryFinish)).Count -ne 1) { throw 'Category finish callback anchor not unique.' }
$raceMenuText = $raceMenuText.Replace($categoryFinish, "      if(_global.skse.IsVR()) this.ApplyVRCategoryPresentation();`r`n" + $categoryFinish)
$nameUpdateNeedle = '      this.bottomBar.playerInfo.PlayerName.SetText(astrPlayerName);'
if ([regex]::Matches($raceMenuText, [regex]::Escape($nameUpdateNeedle)).Count -ne 1) { throw 'Name text callback anchor not unique.' }
$raceMenuText = $raceMenuText.Replace($nameUpdateNeedle, $nameUpdateNeedle + "`r`n      if(_global.skse.IsVR()) this.UpdateVRNameButton(astrPlayerName);")
$exitNameNeedle = "   function ShowTextEntryField()`r`n   {"
if ([regex]::Matches($raceMenuText, [regex]::Escape($exitNameNeedle)).Count -ne 1) { throw 'Exit naming callback anchor not unique.' }
$raceMenuText = $raceMenuText.Replace($exitNameNeedle, $exitNameNeedle + "`r`n      if(_global.skse.IsVR())`r`n      {`r`n         this.FinishVRCharacterCreation();`r`n         return undefined;`r`n      }")
$bottomStart = "   function updateBottomBar()`r`n   {`r`n      this.navPanel.clearButtons();"
if (-not $raceMenuText.Contains($bottomStart)) { throw 'Bottom-bar rebuild anchor missing.' }
$raceMenuText = $raceMenuText.Replace($bottomStart, "   function updateBottomBar()`r`n   {`r`n      if(_global.skse.IsVR()) this.ResetVRTextButtons();`r`n      this.navPanel.clearButtons();")
$bottomUpdate = '      this.navPanel.updateButtons(true);'
if ([regex]::Matches($raceMenuText, [regex]::Escape($bottomUpdate)).Count -ne 1) { throw 'Bottom-bar layout anchor not unique.' }
$raceMenuText = $raceMenuText.Replace($bottomUpdate, "      if(_global.skse.IsVR()) this.AddVRTextButtons();`r`n" + $bottomUpdate)
# All existing modal action guards also respect a keyboard session we own.
$raceMenuText = $raceMenuText.Replace('if(this.colorField._visible || this.textEntry._visible || this.makeupPanel._visible)', 'if(this.vrTextInputActive || this.colorField._visible || this.textEntry._visible || this.makeupPanel._visible)')
$inputTraceSource = Join-Path $PSScriptRoot 'vr-racesex-patches\InputTrace.as.inc'
$inputTraceText = Get-Content -Raw -LiteralPath $inputTraceSource
$raceMenuText = $raceMenuText.Replace($platformNeedle, $inputTraceText + $platformNeedle)

# Native extension sliders have no engine callback. They use the existing
# renderer/change path but dispatch only their provider's internal callback.
$sliderEntrySource = Join-Path $sourceScripts 'scripts\__Packages\SliderListEntry.as'
$sliderEntryText = Get-Content -Raw -LiteralPath $sliderEntrySource
$engineSliderCall = '         gfx.io.GameDelegate.call(this.callbackName,[this.position,this.sliderID]);'
if ([regex]::Matches($sliderEntryText, [regex]::Escape($engineSliderCall)).Count -ne 1) { throw 'Slider entry callback anchor not unique.' }
$sliderEntryText = $sliderEntryText.Replace($engineSliderCall, '         if(this.callbackName != "") gfx.io.GameDelegate.call(this.callbackName,[this.position,this.sliderID]);')
$patchedSliderEntry = Join-Path $resolvedWork 'SliderListEntry.as'
[IO.File]::WriteAllText($patchedSliderEntry, $sliderEntryText)

# Private derivatives; ship only the byte-reuse patch, never these classes.
function Replace-MenuAnchor([string]$Text, [string]$Needle, [string]$Replacement) {
    if ([regex]::Matches($Text, [regex]::Escape($Needle)).Count -ne 1) { throw "Missing/ambiguous Sculpt recipe anchor: $Needle" }
    return $Text.Replace($Needle, $Replacement)
}
$vertexText = Get-Content -Raw (Join-Path $sourceScripts 'scripts/__Packages/VertexEditor.as')
$wireframeText = Get-Content -Raw (Join-Path $sourceScripts 'scripts/__Packages/WireframeDisplay.as')
$modeText = Get-Content -Raw (Join-Path $sourceScripts 'scripts/__Packages/ModeSwitcher.as')
$vertexText = Replace-MenuAnchor $vertexText '   function InitExtensions()' ((Get-Content -Raw (Join-Path $PSScriptRoot 'vr-racesex-patches/Sculpt.as.inc')) + "`r`n   function InitExtensions()")
$vertexText = Replace-MenuAnchor $vertexText '      this.navPanel.clearButtons();' @'
      if(_global.skse.IsVR())
      {
         for(var i = 0; i < this.navPanel.buttons.length; i++)
         {
            this.navPanel.buttons[i].removeEventListener("click",this,"onVRSculptFaceClicked");
            this.navPanel.buttons[i].removeEventListener("click",this,"onVRCanvasSizeClicked");
            this.navPanel.buttons[i].vrSculptAction = false;
         }
      }
      this.navPanel.clearButtons();
'@
$vertexText = Replace-MenuAnchor $vertexText '      this.navPanel.updateButtons(true);' @'
      if(_global.skse.IsVR())
      {
         this.RefreshVRSculptActions();
      }
      this.navPanel.updateButtons(true);
      if(_global.skse.IsVR()) this.LayoutVRSculpt();
'@
$vertexText = Replace-MenuAnchor $vertexText '      this.ShowBottomBar(bShowAll);' "      this.UpdateVRSculptView(bShowAll);`r`n      if(_global.skse.IsVR() && bShowAll) this.LayoutVRSculpt();`r`n      this.ShowBottomBar(bShowAll);"
$wireframeText = Replace-MenuAnchor $wireframeText "      this.calculateBackground();`r`n   }`r`n   function onMouseWheel" "      this.calculateBackground();`r`n      if(_global.skse.IsVR()) this._parent.LayoutVRSculpt();`r`n   }`r`n   function onMouseWheel"
# Dense visible tabs, stable legacy mode IDs (Sculpt remains 3, Camera 2).
# VR laser integrations can call setMode using the visible tab ordinal rather
# than dispatching a RadioButton click. Accept visible Sculpt 2 as well as its
# legacy semantic ID 3; Camera no longer exists in VR. Outbound IDs stay legacy.
$modeText = Replace-MenuAnchor $modeText '      var _loc5_ = this.addMode("$Camera");' '      var _loc5_ = !_global.skse.IsVR() ? this.addMode("$Camera") : undefined;'
$modeText = Replace-MenuAnchor $modeText '      this.buttonGroup.addButton(_loc5_);' '      if(_loc5_ != undefined) this.buttonGroup.addButton(_loc5_);'
$modeText = Replace-MenuAnchor $modeText '      _loc5_.addEventListener("rollOver",this,"onItemRollOver");' '      if(_loc5_ != undefined) _loc5_.addEventListener("rollOver",this,"onItemRollOver");'
$modeText = Replace-MenuAnchor $modeText '      var _loc2_ = this.buttonGroup.getButtonAt(index);' @'
      if(_global.skse.IsVR())
      {
         this.vrLastRequestedMode = index;
         this.vrModeRequestSequence = this.vrModeRequestSequence == undefined ? 1 : this.vrModeRequestSequence+1;
         if(index > 2) index--;
      }
      var _loc2_ = this.buttonGroup.getButtonAt(index);
'@
$modeText = Replace-MenuAnchor $modeText '      return this._modes.indexOf(this.buttonGroup.selectedButton);' "      var index = this._modes.indexOf(this.buttonGroup.selectedButton);`r`n      return _global.skse.IsVR() && index >= 2 ? index+1 : index;"
$modeText = Replace-MenuAnchor $modeText 'index:this._modes.indexOf(event.target)' 'index:(_global.skse.IsVR() && this._modes.indexOf(event.target) >= 2 ? this._modes.indexOf(event.target)+1 : this._modes.indexOf(event.target))'
$modeText = Replace-MenuAnchor $modeText 'index:this._modes.indexOf(event.item)' 'index:(_global.skse.IsVR() && this._modes.indexOf(event.item) >= 2 ? this._modes.indexOf(event.item)+1 : this._modes.indexOf(event.item))'
foreach ($class in @(@('VertexEditor',$vertexText),@('WireframeDisplay',$wireframeText),@('ModeSwitcher',$modeText))) {
    [IO.File]::WriteAllText((Join-Path $resolvedWork ($class[0]+'.as')), $class[1], [Text.UTF8Encoding]::new($false))
}

$categoryPushNeedle = '         this.categoryList.entryList.push(_loc5_);'
$categoryPushReplacement = @'
         if(this.IsVRCategoryVisible(_loc5_.flag))
         {
            this.categoryList.entryList.push(_loc5_);
         }
'@
$categoryPushIndex = $raceMenuText.IndexOf($categoryPushNeedle)
if ($categoryPushIndex -lt 0) {
    throw 'The expected base-category insertion was not found.'
}
$raceMenuText = $raceMenuText.Remove($categoryPushIndex, $categoryPushNeedle.Length).Insert($categoryPushIndex, $categoryPushReplacement)

$fixedCategoryReplacements = [ordered]@{
    '      this.categoryList.entryList.push({type:RaceMenuDefines.ENTRY_TYPE_CAT,bDontHide:false,filterFlag:1,text:"$COLORS",flag:RaceMenuDefines.CATEGORY_COLOR,priority:_loc11_,enabled:true});' = '      if(this.IsVRCategoryVisible(RaceMenuDefines.CATEGORY_COLOR)) this.categoryList.entryList.push({type:RaceMenuDefines.ENTRY_TYPE_CAT,bDontHide:false,filterFlag:1,text:"$COLORS",flag:RaceMenuDefines.CATEGORY_COLOR,priority:_loc11_,enabled:true});'
    '      this.categoryList.entryList.push({type:RaceMenuDefines.ENTRY_TYPE_CAT,bDontHide:false,filterFlag:1,text:"$MAKEUP",flag:RaceMenuDefines.CATEGORY_WARPAINT,priority:_loc11_,enabled:true});' = '      if(this.IsVRCategoryVisible(RaceMenuDefines.CATEGORY_WARPAINT)) this.categoryList.entryList.push({type:RaceMenuDefines.ENTRY_TYPE_CAT,bDontHide:false,filterFlag:1,text:"$MAKEUP",flag:RaceMenuDefines.CATEGORY_WARPAINT,priority:_loc11_,enabled:true});'
}
foreach ($entry in $fixedCategoryReplacements.GetEnumerator()) {
    if (-not $raceMenuText.Contains($entry.Key)) {
        throw "The expected fixed-category insertion was not found: $($entry.Key)"
    }
    $raceMenuText = $raceMenuText.Replace($entry.Key, $entry.Value)
}

$overlayCategoryLabels = [ordered]@{
    BODYPAINT = 'BODY PAINT'
    HANDPAINT = 'HAND PAINT'
    FEETPAINT = 'FOOT PAINT'
    FACEPAINT = 'FACE PAINT'
}
foreach ($categoryEntry in $overlayCategoryLabels.GetEnumerator()) {
    $category = $categoryEntry.Key
    $label = $categoryEntry.Value
    $needle = "            this.categoryList.entryList.push({type:RaceMenuDefines.ENTRY_TYPE_CAT,bDontHide:false,filterFlag:1,text:`"`$$label`",flag:RaceMenuDefines.CATEGORY_$category,priority:_loc11_,enabled:true});"
    if (-not $raceMenuText.Contains($needle)) {
        throw "The expected overlay-category insertion was not found: $category"
    }
    $replacement = "            if(this.IsVRCategoryVisible(RaceMenuDefines.CATEGORY_$category)) this.categoryList.entryList.push({type:RaceMenuDefines.ENTRY_TYPE_CAT,bDontHide:false,filterFlag:1,text:`"`$$label`",flag:RaceMenuDefines.CATEGORY_$category,priority:_loc11_,enabled:true});"
    $raceMenuText = $raceMenuText.Replace($needle, $replacement)
}

[IO.File]::WriteAllText($patchedRaceMenuSlider, $raceMenuSliderText, [Text.UTF8Encoding]::new($false))
& $resolvedFfdec -replace $searchReplacedSwf $sliderReplacedSwf '\__Packages\RaceMenuSlider' $patchedRaceMenuSlider
if ($LASTEXITCODE -ne 0 -or
    -not (Test-Path -LiteralPath $sliderReplacedSwf -PathType Leaf) -or
    (Get-Item -LiteralPath $sliderReplacedSwf).Length -lt 1024) {
    throw "JPEXS failed to replace RaceMenuSlider ActionScript (exit $LASTEXITCODE)."
}

[IO.File]::WriteAllText($patchedRaceMenu, $raceMenuText, [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText($patchedButtonPanel, $buttonPanelText, [Text.UTF8Encoding]::new($false))
& $resolvedFfdec -replace $sliderReplacedSwf $buttonPanelReplacedSwf '\__Packages\skyui\components\ButtonPanel' $patchedButtonPanel
if ($LASTEXITCODE -ne 0) { throw 'JPEXS failed to replace ButtonPanel.' }
& $resolvedFfdec -replace $buttonPanelReplacedSwf $resolvedOutput '\__Packages\RaceMenu' $patchedRaceMenu
if ($LASTEXITCODE -ne 0 -or
    -not (Test-Path -LiteralPath $resolvedOutput -PathType Leaf) -or
    (Get-Item -LiteralPath $resolvedOutput).Length -lt 1024) {
    throw "JPEXS failed to replace RaceMenu ActionScript (exit $LASTEXITCODE)."
}

$extensionSwf = Join-Path $resolvedWork 'RaceSex_menu.extensions.swf'
& $resolvedFfdec -replace $resolvedOutput $extensionSwf '\__Packages\SliderListEntry' $patchedSliderEntry
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $extensionSwf -PathType Leaf)) { throw 'Native slider callback replacement failed.' }
Copy-Item -LiteralPath $extensionSwf -Destination $resolvedOutput -Force
foreach ($class in @('VertexEditor','WireframeDisplay','ModeSwitcher')) {
    $nextSwf = Join-Path $resolvedWork ($class+'.swf')
    & $resolvedFfdec -replace $resolvedOutput $nextSwf ("\__Packages\"+$class) (Join-Path $resolvedWork ($class+'.as'))
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $nextSwf)) { throw "Failed to replace $class." }
    Copy-Item -LiteralPath $nextSwf -Destination $resolvedOutput -Force
}
& $resolvedFfdec -swf2xml $resolvedOutput $verificationXml
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $verificationXml -PathType Leaf)) {
    throw "JPEXS failed to export the rebuilt SWF for verification (exit $LASTEXITCODE)."
}

$verification = [System.Xml.XmlDocument]::new()
$verification.Load($verificationXml)
$verifiedTrack = $verification.SelectSingleNode("//item[@type='DefineSpriteTag' and @spriteId='33']/subTags/item[@type='PlaceObject2Tag' and @characterId='32']/matrix")
$verifiedTrackPlacement = $verification.SelectSingleNode("//item[@type='DefineSpriteTag' and @spriteId='39']/subTags/item[@type='PlaceObject2Tag' and @characterId='33']/matrix")
$verifiedEnd = $verification.SelectSingleNode("//item[@type='DefineSpriteTag' and @spriteId='39']/subTags/item[@type='PlaceObject2Tag' and @characterId='34']/matrix")
$verifiedSliderShape = $verification.SelectSingleNode("//item[@type='DefineShape3Tag' and @shapeId='31']")
$verifiedSearchShape = $verification.SelectSingleNode("//item[@type='DefineShapeTag' and @shapeId='174']")
$verifiedScrollTrackShape = $verification.SelectSingleNode("//item[@type='DefineShape3Tag' and @shapeId='73']")
$verifiedScrollThumbShape = $verification.SelectSingleNode("//item[@type='DefineShape3Tag' and @shapeId='79']")
$verifiedColorField = $verification.SelectSingleNode("//item[@type='DefineSpriteTag' and @spriteId='197']/subTags/item[@type='PlaceObject2Tag' and @characterId='170' and @name='colorField']/matrix")
if (-not $verifiedTrack -or $verifiedTrack.GetAttribute('scaleX') -ne '2.1213074') {
	throw 'The rebuilt SWF did not retain the stock regular-slider track span.'
}
if (-not $verifiedTrackPlacement -or $verifiedTrackPlacement.GetAttribute('translateX') -ne '258') {
	throw 'The rebuilt SWF did not retain the stock regular-slider track start.'
}
if (-not $verifiedEnd -or $verifiedEnd.GetAttribute('translateX') -ne '7250') {
	throw 'The rebuilt SWF did not retain the stock regular-slider right end.'
}
$verifiedSliderFills = @($verifiedSliderShape.SelectNodes('shapes/fillStyles/fillStyles/item'))
$verifiedSearchFills = @($verifiedSearchShape.SelectNodes('shapes/fillStyles/fillStyles/item'))
$verifiedScrollTrackFills = @($verifiedScrollTrackShape.SelectNodes('shapes/fillStyles/fillStyles/item'))
$verifiedScrollThumbFills = @($verifiedScrollThumbShape.SelectNodes('shapes/fillStyles/fillStyles/item'))
if ($verifiedSliderFills[0].color.GetAttribute('alpha') -ne '51' -or
	$verifiedSliderFills[1].color.GetAttribute('alpha') -ne '0' -or
    $verifiedSliderFills[2].color.GetAttribute('alpha') -ne '0') {
    throw 'The rebuilt SWF did not retain the low-frequency regular-slider artwork.'
}
if ($verifiedSearchFills[0].color.GetAttribute('red') -ne '0' -or
    $verifiedSearchFills[0].color.GetAttribute('green') -ne '0' -or
    $verifiedSearchFills[0].color.GetAttribute('blue') -ne '0') {
    throw 'The rebuilt SWF did not retain the low-contrast search-field artwork.'
}
if ($verifiedScrollThumbFills[0].color.GetAttribute('alpha') -ne '255' -or
    $verifiedScrollThumbFills[1].color.GetAttribute('red') -ne '242' -or
    $verifiedScrollThumbFills[1].color.GetAttribute('green') -ne '242' -or
    $verifiedScrollThumbFills[1].color.GetAttribute('blue') -ne '242' -or
    $verifiedScrollThumbFills[1].color.GetAttribute('alpha') -ne '255') {
    throw 'The rebuilt SWF did not retain the low-frequency vertical scrollbar thumb artwork.'
}
if ($verifiedScrollTrackFills[0].color.GetAttribute('alpha') -ne '0' -or
    $verifiedScrollTrackFills[1].color.GetAttribute('alpha') -ne '0' -or
    $verifiedScrollTrackFills[2].color.GetAttribute('alpha') -ne '51') {
    throw 'The rebuilt SWF did not retain the low-frequency vertical scrollbar artwork.'
}
if (-not $verifiedColorField -or
    $verifiedColorField.GetAttribute('scaleX') -ne '2.25' -or
    $verifiedColorField.GetAttribute('scaleY') -ne '2.25' -or
    $verifiedColorField.GetAttribute('translateX') -ne '10240' -or
    $verifiedColorField.GetAttribute('translateY') -ne '10240') {
    throw 'The rebuilt SWF did not retain the enlarged VR ColorField transform.'
}

Remove-Item -LiteralPath $verificationScripts -Recurse -Force -ErrorAction SilentlyContinue
& $resolvedFfdec -export script $verificationScripts $resolvedOutput | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "JPEXS failed to export rebuilt ActionScript for verification (exit $LASTEXITCODE)."
}
$verifiedSearchScript = Join-Path $verificationScripts 'scripts\__Packages\skyui\components\SearchWidget.as'
$verifiedRaceMenuScript = Join-Path $verificationScripts 'scripts\__Packages\RaceMenu.as'
$verifiedRaceMenuSliderScript = Join-Path $verificationScripts 'scripts\__Packages\RaceMenuSlider.as'
if (-not (Test-Path -LiteralPath $verifiedSearchScript -PathType Leaf)) {
    throw 'The rebuilt SWF did not export the expected SearchWidget ActionScript.'
}
$verifiedSearchText = Get-Content -Raw -LiteralPath $verifiedSearchScript
if ($verifiedSearchText -match 'ShowVirtualKeyboard' -or
    $verifiedSearchText -notmatch 'BeginVRTextInput') {
    throw 'The rebuilt SWF did not retain the VR SearchWidget keyboard handlers.'
}
$verifiedRaceMenuText = Get-Content -Raw -LiteralPath $verifiedRaceMenuScript
foreach ($appearanceBoundary in @('SetupVRMenuAppearance', 'FitVRBackgroundImage', 'ColorVRTextChildren', 'menuBackgroundImage', 'MovieClipLoader', 'localToGlobal', 'removeListener', 'MountMenuBackgroundImage', 'GetMenuBackgroundImageState', 'CheckVRBackgroundMount', 'vrBackgroundMountDeadline', 'error: native texture registration timeout', 'vrBackgroundExpectedWidth', 'vrBackgroundExpectedHeight', 'error: unexpected image dimensions')) {
    if (-not $verifiedRaceMenuText.Contains($appearanceBoundary)) { throw "Rebuilt SWF is missing appearance boundary: $appearanceBoundary" }
}
if ($verifiedRaceMenuText -notmatch 'function ShowTextEntryField\(\)\s*\{\s*if\(_global\.skse\.IsVR\(\)\)\s*\{\s*this\.FinishVRCharacterCreation\(\);\s*return undefined;' -or
    $verifiedRaceMenuText -notmatch 'function FinishVRCharacterCreation\(') {
    throw 'The rebuilt SWF did not retain the VR accepted-name completion path.'
}
if ($verifiedRaceMenuText -notmatch 'SetupVRTextButtons' -or
    $verifiedRaceMenuText -notmatch 'PositionVRTextButtons' -or
    $verifiedRaceMenuText -notmatch 'this\.navPanel\.addButton' -or
    $verifiedRaceMenuText -notmatch 'BeginVRTextEntry' -or
    $verifiedRaceMenuText -notmatch 'onVRFilterButtonClicked' -or
    $verifiedRaceMenuText -notmatch 'onVRNameButtonClicked' -or
    $verifiedRaceMenuText -notmatch 'ShowTextEntryField' -or
    $verifiedRaceMenuText -notmatch 'onSearchClicked' -or
    $verifiedRaceMenuText -notmatch 'IsVRCategoryVisible') {
    throw 'The rebuilt SWF did not retain the VR text-target/category extensions.'
}
foreach ($requiredTraceFunction in @('ArmVRInputTrace', 'DisarmVRInputTrace', 'ReadVRInputTargetSnapshot', 'onPressKind', 'centerShapeHit', 'original.apply', 'RecordVRMovieInput', 'Mouse.removeListener')) {
    if (-not $verifiedRaceMenuText.Contains($requiredTraceFunction)) {
        throw "The rebuilt SWF is missing diagnostic boundary: $requiredTraceFunction"
    }
}
if (-not (Test-Path -LiteralPath $verifiedRaceMenuSliderScript -PathType Leaf) -or
    (Get-Content -Raw -LiteralPath $verifiedRaceMenuSliderScript) -match 'this\.offsetRight = 9' -or
    (Get-Content -Raw -LiteralPath $verifiedRaceMenuSliderScript) -notmatch '164\.85 : 349\.6') {
    throw 'The rebuilt SWF did not retain the VR slider post-layout clamp.'
}
foreach ($boundary in @('LayoutVRSculpt','vrWorkspaceBounds','DrawVRSculptBackground','PositionVRSculptTabs','Stage.visibleRect','UpdateVRSculptView','onVRSculptFaceClicked','onVRCanvasSizeClicked','Large canvas','Standard canvas')) {
    $vertexVerification = Get-Content -Raw -LiteralPath (Join-Path $verificationScripts 'scripts/__Packages/VertexEditor.as')
    if (-not $vertexVerification.Contains($boundary)) { throw "Rebuilt Sculpt editor is missing boundary: $boundary" }
}
$wireframeVerification = Get-Content -Raw -LiteralPath (Join-Path $verificationScripts 'scripts/__Packages/WireframeDisplay.as')
if (-not $wireframeVerification.Contains('this._parent.LayoutVRSculpt()')) { throw 'Rebuilt Sculpt canvas lacks its loaded-layout callback.' }
$modeVerification = Get-Content -Raw -LiteralPath (Join-Path $verificationScripts 'scripts/__Packages/ModeSwitcher.as')
if ($modeVerification -notmatch '!_global\.skse\.IsVR\(\) \? this\.addMode\("\$Camera"\)' -or
    $modeVerification -notmatch 'index > 2' -or $modeVerification -notmatch '_loc\d+_ >= 2' -or
    -not $modeVerification.Contains('vrLastRequestedMode') -or $modeVerification -match 'if\(index == 2\)') {
    throw 'Rebuilt mode tabs lack Camera exclusion or legacy Sculpt ID mapping.'
}
if (-not $verifiedRaceMenuText.Contains('this.racePanel.tintCount._visible = false')) { throw 'Rebuilt VR appearance lacks tint-counter suppression.' }

$outputHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedOutput).Hash
Write-Host "Verified patched VR RaceSexMenu SWF: $resolvedOutput"
Write-Host "Input SHA-256: $inputHash"
Write-Host "Output SHA-256: $outputHash"
