unit Ft.Widget.Rulers;

// Ft.Widget.Rulers
// ================
// High-precision horizontal and vertical measurement ruler widgets.
// Ideal for vector editors, digital painting suites, CAD tools, and PDF viewers.
// Features adaptive zoom-aware ticking, multiple measurement units (px, pt, mm, cm, in),
// mouse cursor hairline tracking, and interactive guide creation callbacks.

{$mode objfpc}{$H+}

interface

uses
  SysUtils, Math,
  Floria.Font,
  Ft.Canvas,
  Ft.Theme,
  Ft.Widget;

type
  TFtRulerOrientation = (ftroHorizontal, ftroVertical);
  TFtRulerUnit = (fruPixels, fruPoints, fruMillimeters, fruCentimeters, fruInches);

  TFtRuler = class;

  TFtRulerGuideEvent = procedure(Sender: TFtWidget; WorldPos: Double; UserData: Pointer); cdecl;

  { TFtRuler }
  TFtRuler = class(TFtWidget)
  private
    FOrientation: TFtRulerOrientation;
    FUnitKind: TFtRulerUnit;
    FOrigin: Double;        { Screen pixel position where World 0 is mapped }
    FScale: Double;         { Zoom factor (1.0 = 100%) }
    FPointerPos: Double;    { Current mouse cursor screen position along ruler axis }
    FShowPointer: Boolean;  { Whether to draw cursor tracking hairline }
    FDPI: Double;           { Display / document DPI (default 96.0) }
    FThickness: Integer;    { Breadth of the ruler (height if H, width if V; default 20) }
    FUserData: Pointer;
    FFont: TFtFont;

    FIsDraggingGuide: Boolean;
    FOnCreateGuide: TFtRulerGuideEvent;

    function GetPixelsPerUnit(): Double;
    function FormatNumber(AValue: Double): string;
  protected
    function GetElementType(): string; override;
  public
    constructor Create(AParent: TFtWidget; AOrientation: TFtRulerOrientation = ftroHorizontal); reintroduce;
    destructor Destroy(); override;

    procedure Draw(Canvas: TFtCanvas); override;
    procedure MouseDown(AX, AY: Integer; AButton: Integer); override;
    procedure MouseMove(AX, AY: Integer); override;
    procedure MouseUp(AX, AY: Integer; AButton: Integer); override;

    procedure SetOriginAndScale(AOrigin, AScale: Double);
    procedure SetPointerPosition(APos: Double);

    property Orientation: TFtRulerOrientation read FOrientation write FOrientation;
    property UnitKind: TFtRulerUnit read FUnitKind write FUnitKind;
    property Origin: Double read FOrigin write FOrigin;
    property Scale: Double read FScale write FScale;
    property PointerPos: Double read FPointerPos write FPointerPos;
    property ShowPointer: Boolean read FShowPointer write FShowPointer;
    property DPI: Double read FDPI write FDPI;
    property Thickness: Integer read FThickness write FThickness;
    property UserData: Pointer read FUserData write FUserData;
    property OnCreateGuide: TFtRulerGuideEvent read FOnCreateGuide write FOnCreateGuide;
  end;

implementation

constructor TFtRuler.Create(AParent: TFtWidget; AOrientation: TFtRulerOrientation);
begin
  inherited Create(AParent);
  FOrientation := AOrientation;
  FUnitKind := fruPixels;
  FOrigin := 0.0;
  FScale := 1.0;
  FPointerPos := -1.0;
  FShowPointer := True;
  FDPI := 96.0;
  FThickness := 20;
  FUserData := nil;
  FIsDraggingGuide := False;
  FOnCreateGuide := nil;
  FFont := FtFontManager().GetFont('sans-serif,8');

  if FOrientation = ftroHorizontal then
  begin
    Height := FThickness;
    PreferredHeight := FThickness;
  end
  else
  begin
    Width := FThickness;
    PreferredWidth := FThickness;
  end;
end;

destructor TFtRuler.Destroy();
begin
  inherited Destroy();
end;

function TFtRuler.GetElementType(): string;
begin
  Result := 'ruler';
end;

procedure TFtRuler.SetOriginAndScale(AOrigin, AScale: Double);
begin
  if (FOrigin <> AOrigin) or (FScale <> AScale) then
  begin
    FOrigin := AOrigin;
    if AScale < 0.0001 then
      FScale := 0.0001
    else
      FScale := AScale;
    Invalidate();
  end;
end;

procedure TFtRuler.SetPointerPosition(APos: Double);
begin
  if FPointerPos <> APos then
  begin
    FPointerPos := APos;
    Invalidate();
  end;
end;

function TFtRuler.GetPixelsPerUnit(): Double;
begin
  case FUnitKind of
    fruPixels:      Result := 1.0;
    fruPoints:      Result := FDPI / 72.0;          { 1 pt = 1/72 in }
    fruInches:      Result := FDPI;                 { 1 in = 96 px }
    fruMillimeters: Result := FDPI / 25.4;          { 1 mm = 1/25.4 in }
    fruCentimeters: Result := (FDPI / 25.4) * 10.0; { 1 cm = 10 mm }
    else            Result := 1.0;
  end;
end;

function TFtRuler.FormatNumber(AValue: Double): string;
begin
  if Abs(AValue - Round(AValue)) < 0.0001 then
    Result := IntToStr(Round(AValue))
  else
    Result := FormatFloat('0.#', AValue);
end;

procedure TFtRuler.Draw(Canvas: TFtCanvas);
var
  isDark: Boolean;
  bgR, bgG, bgB: Double;
  bdR, bdG, bdB: Double;
  tkR, tkG, tkB: Double;
  txtR, txtG, txtB: Double;
  ptrR, ptrG, ptrB: Double;
  ppu: Double;
  effScale: Double;
  minScreenSpan, minWorldSpan: Double;
  stepPower, baseStep, majorStep, minorStep: Double;
  steps: array[0..3] of Double;
  i: Integer;
  startScr, endScr: Double;
  startWld, endWld: Double;
  w, s: Double;
  tickLen: Double;
  isMajor, isHalf: Boolean;
  fnt: TFtFont;
  lbl: string;
begin
  if not Visible or not Assigned(Canvas) then Exit;

  isDark := FtGetTheme().DarkMode;
  if isDark then
  begin
    bgR := 0.12; bgG := 0.15; bgB := 0.19; // slate-850
    bdR := 0.20; bdG := 0.25; bdB := 0.32; // border
    tkR := 0.40; tkG := 0.47; tkB := 0.58; // ticks
    txtR := 0.65; txtG := 0.72; txtB := 0.82; // text
    ptrR := 0.22; ptrG := 0.74; ptrB := 0.97; // cyan-400
  end
  else
  begin
    bgR := 0.95; bgG := 0.96; bgB := 0.98; // slate-100
    bdR := 0.80; bdG := 0.84; bdB := 0.89; // border
    tkR := 0.60; tkG := 0.65; tkB := 0.72; // ticks
    txtR := 0.30; txtG := 0.35; txtB := 0.43; // text
    ptrR := 0.15; ptrG := 0.39; ptrB := 0.92; // blue-600
  end;

  Canvas.PushClipRect(X, Y, Width, Height);
  try
    // 1. Draw ruler background
    Canvas.DrawRect(X, Y, Width, Height, bgR, bgG, bgB, 1.0);

    // 2. Draw outer border divider line
    if FOrientation = ftroHorizontal then
      Canvas.DrawLine(X, Y + Height - 1, X + Width, Y + Height - 1, 1.0, bdR, bdG, bdB, 1.0)
    else
      Canvas.DrawLine(X + Width - 1, Y, X + Width - 1, Y + Height, 1.0, bdR, bdG, bdB, 1.0);

    // 3. Calculate Adaptive Step Size
    ppu := GetPixelsPerUnit();
    effScale := FScale * ppu;
    if effScale < 0.00001 then effScale := 0.00001;

    // We want major ticks to be at least 50px apart on screen
    minScreenSpan := 50.0;
    minWorldSpan := minScreenSpan / effScale;

    stepPower := Math.Power(10.0, Floor(Log10(minWorldSpan)));
    steps[0] := 1.0 * stepPower;
    steps[1] := 2.0 * stepPower;
    steps[2] := 5.0 * stepPower;
    steps[3] := 10.0 * stepPower;

    majorStep := steps[3];
    for i := 0 to 2 do
    begin
      if steps[i] >= minWorldSpan then
      begin
        majorStep := steps[i];
        Break;
      end;
    end;

    minorStep := majorStep / 5.0;
    if (minorStep * effScale) < 5.0 then
      minorStep := majorStep / 2.0;

    // 4. Determine visible world bounds
    if FOrientation = ftroHorizontal then
    begin
      startScr := 0.0;
      endScr := Width;
    end
    else
    begin
      startScr := 0.0;
      endScr := Height;
    end;

    startWld := (startScr - FOrigin) / effScale;
    endWld := (endScr - FOrigin) / effScale;
    if startWld > endWld then
    begin
      w := startWld; startWld := endWld; endWld := w;
    end;

    fnt := FFont;

    // 5. Render Ticks & Numerical Labels
    w := Floor(startWld / minorStep) * minorStep;
    while w <= endWld + minorStep do
    begin
      s := FOrigin + (w * effScale);

      isMajor := Abs(w - Round(w / majorStep) * majorStep) < (minorStep * 0.1);
      isHalf := Abs(w - Round(w / (majorStep * 0.5)) * (majorStep * 0.5)) < (minorStep * 0.1);

      if isMajor then
        tickLen := FThickness * 0.65
      else if isHalf then
        tickLen := FThickness * 0.40
      else
        tickLen := FThickness * 0.25;

      if FOrientation = ftroHorizontal then
      begin
        if (s >= 0) and (s <= Width) then
        begin
          Canvas.DrawLine(Round(X + s), Y + Height - tickLen, Round(X + s), Y + Height, 1.0, tkR, tkG, tkB, 0.9);
          if isMajor and Assigned(fnt) then
          begin
            lbl := FormatNumber(w);
            Canvas.DrawText(Round(X + s + 3.0), Round(Y + fnt.Ascent + 2.0), lbl, fnt, txtR, txtG, txtB);
          end;
        end;
      end
      else // ftroVertical
      begin
        if (s >= 0) and (s <= Height) then
        begin
          Canvas.DrawLine(X + Width - tickLen, Round(Y + s), X + Width, Round(Y + s), 1.0, tkR, tkG, tkB, 0.9);
          if isMajor and Assigned(fnt) then
          begin
            lbl := FormatNumber(w);
            // Draw vertical coordinate label rotated 90 counter-clockwise
            Canvas.DrawTextRotated(Round(X + Width - 7.0), Round(Y + s - 3.0), lbl, fnt, -90.0, txtR, txtG, txtB);
          end;
        end;
      end;

      w := w + minorStep;
    end;

    // 6. Draw Pointer Hairline Tracking Marker
    if FShowPointer and (FPointerPos >= 0.0) then
    begin
      if FOrientation = ftroHorizontal then
      begin
        if (FPointerPos >= 0.0) and (FPointerPos <= Width) then
          Canvas.DrawLine(Round(X + FPointerPos), Y, Round(X + FPointerPos), Y + Height, 1.5, ptrR, ptrG, ptrB, 1.0);
      end
      else
      begin
        if (FPointerPos >= 0.0) and (FPointerPos <= Height) then
          Canvas.DrawLine(X, Round(Y + FPointerPos), X + Width, Round(Y + FPointerPos), 1.5, ptrR, ptrG, ptrB, 1.0);
      end;
    end;

  finally
    Canvas.PopClipRect();
  end;
end;

procedure TFtRuler.MouseDown(AX, AY: Integer; AButton: Integer);
begin
  inherited MouseDown(AX, AY, AButton);
  if AButton = 1 then
    FIsDraggingGuide := True;
end;

procedure TFtRuler.MouseMove(AX, AY: Integer);
begin
  inherited MouseMove(AX, AY);
  if FOrientation = ftroHorizontal then
    FPointerPos := AX - X
  else
    FPointerPos := AY - Y;
  Invalidate();
end;

procedure TFtRuler.MouseUp(AX, AY: Integer; AButton: Integer);
var
  pos: Double;
  ppu, wldPos: Double;
begin
  inherited MouseUp(AX, AY, AButton);
  if (AButton = 1) and FIsDraggingGuide then
  begin
    FIsDraggingGuide := False;
    if Assigned(FOnCreateGuide) then
    begin
      if FOrientation = ftroHorizontal then
        pos := AY - Y
      else
        pos := AX - X;

      ppu := GetPixelsPerUnit();
      if FOrientation = ftroHorizontal then
        wldPos := (AY - FOrigin) / (FScale * ppu)
      else
        wldPos := (AX - FOrigin) / (FScale * ppu);

      FOnCreateGuide(Self, wldPos, FUserData);
    end;
  end;
end;

end.
