unit Ft.Widget.Scrubbers;

// Ft.Widget.Scrubbers
// ====================
// Modern interactive numeric scrubber control (Photoshop / Blender / Figma style).
// Features label-drag horizontal value scrubbing with Shift (10x) and Alt (0.1x) modifiers,
// direct keyboard value editing, min/max clamping, custom units/suffixes, and CSS styling.

{$mode objfpc}{$H+}

interface

uses
  SysUtils, Math,
  Floria.Font,
  Ft.Canvas,
  Ft.Theme,
  Ft.Widget;

type
  TFtNumberScrubber = class;

  TFtScrubberChangeEvent = procedure(Sender: TFtWidget; NewValue: Double; UserData: Pointer); cdecl;

  { TFtNumberScrubber }
  TFtNumberScrubber = class(TFtWidget)
  private
    FLabelText: string;
    FValue: Double;
    FMinValue: Double;
    FMaxValue: Double;
    FStep: Double;
    FDecimals: Integer;
    FSuffix: string;
    FPrefix: string;
    FUserData: Pointer;

    { Interaction state }
    FIsScrubbing: Boolean;
    FDragStartX: Integer;
    FDragStartValue: Double;
    FIsEditingText: Boolean;
    FEditText: string;
    FCaretPos: Integer;

    FOnChange: TFtScrubberChangeEvent;

    function FormatValueString(AVal: Double): string;
    procedure CommitEditText();
    procedure CancelEditText();
  protected
    function GetElementType(): string; override;
    function GetStatePseudoClass(): string; override;
  public
    constructor Create(AParent: TFtWidget); override;
    destructor Destroy(); override;

    procedure Draw(Canvas: TFtCanvas); override;
    function GetCursor(): Integer; override;

    procedure MouseDown(AX, AY: Integer; AButton: Integer); override;
    procedure MouseMove(AX, AY: Integer); override;
    procedure MouseUp(AX, AY: Integer; AButton: Integer); override;
    procedure KeyDown(AKeySym: Cardinal; AState: Cardinal; const AChar: string); override;
    procedure LostFocus(); override;

    procedure SetValue(AValue: Double);
    procedure SetRange(AMin, AMax: Double);

    property LabelText: string read FLabelText write FLabelText;
    property Value: Double read FValue write SetValue;
    property MinValue: Double read FMinValue write FMinValue;
    property MaxValue: Double read FMaxValue write FMaxValue;
    property Step: Double read FStep write FStep;
    property Decimals: Integer read FDecimals write FDecimals;
    property Suffix: string read FSuffix write FSuffix;
    property Prefix: string read FPrefix write FPrefix;
    property UserData: Pointer read FUserData write FUserData;
    property OnChange: TFtScrubberChangeEvent read FOnChange write FOnChange;
  end;

implementation

constructor TFtNumberScrubber.Create(AParent: TFtWidget);
begin
  inherited Create(AParent);
  FFocusable := True;
  Width := 140;
  Height := 28;
  PreferredWidth := 140;
  PreferredHeight := 28;

  FLabelText := 'X';
  FValue := 0.0;
  FMinValue := -100000.0;
  FMaxValue := 100000.0;
  FStep := 1.0;
  FDecimals := 0;
  FSuffix := 'px';
  FPrefix := '';
  FUserData := nil;

  FIsScrubbing := False;
  FDragStartX := 0;
  FDragStartValue := 0.0;
  FIsEditingText := False;
  FEditText := '';
  FCaretPos := 0;
  FOnChange := nil;
end;

destructor TFtNumberScrubber.Destroy();
begin
  inherited Destroy();
end;

function TFtNumberScrubber.GetElementType(): string;
begin
  Result := 'numberscrubber';
end;

function TFtNumberScrubber.GetStatePseudoClass(): string;
begin
  if not FEnabled then
    Result := ':disabled'
  else if FIsScrubbing or FIsEditingText then
    Result := ':active'
  else
    Result := inherited GetStatePseudoClass();
end;

function TFtNumberScrubber.GetCursor(): Integer;
begin
  if FIsEditingText then
    Result := FT_CURSOR_IBEAM
  else
    Result := FT_CURSOR_SIZE_H;
end;

function TFtNumberScrubber.FormatValueString(AVal: Double): string;
var
  fmt: string;
begin
  if FDecimals <= 0 then
    fmt := '0'
  else
    fmt := '0.' + StringOfChar('#', FDecimals);

  Result := FPrefix + FormatFloat(fmt, AVal);
  if FSuffix <> '' then
    Result := Result + ' ' + FSuffix;
end;

procedure TFtNumberScrubber.SetValue(AValue: Double);
var
  clamped: Double;
begin
  clamped := EnsureRange(AValue, FMinValue, FMaxValue);
  if FDecimals <= 0 then
    clamped := Round(clamped)
  else
    clamped := RoundTo(clamped, -FDecimals);

  if Abs(FValue - clamped) > 0.000001 then
  begin
    FValue := clamped;
    Invalidate();
    if Assigned(FOnChange) then
      FOnChange(Self, FValue, FUserData);
  end;
end;

procedure TFtNumberScrubber.SetRange(AMin, AMax: Double);
begin
  FMinValue := AMin;
  FMaxValue := AMax;
  SetValue(FValue);
end;

procedure TFtNumberScrubber.CommitEditText();
var
  cleaned: string;
  parsedVal: Double;
  fs: TFormatSettings;
begin
  if not FIsEditingText then Exit;
  FIsEditingText := False;

  cleaned := Trim(FEditText);
  // Strip suffix if present
  if (FSuffix <> '') and (Pos(LowerCase(FSuffix), LowerCase(cleaned)) > 0) then
    cleaned := Trim(Copy(cleaned, 1, Pos(LowerCase(FSuffix), LowerCase(cleaned)) - 1));

  fs := DefaultFormatSettings;
  fs.DecimalSeparator := '.';
  cleaned := StringReplace(cleaned, ',', '.', [rfReplaceAll]);

  if TryStrToFloat(cleaned, parsedVal, fs) then
    SetValue(parsedVal);

  Invalidate();
end;

procedure TFtNumberScrubber.CancelEditText();
begin
  FIsEditingText := False;
  Invalidate();
end;

procedure TFtNumberScrubber.LostFocus();
begin
  inherited LostFocus();
  if FIsEditingText then
    CommitEditText();
end;

procedure TFtNumberScrubber.Draw(Canvas: TFtCanvas);
var
  isDark: Boolean;
  bgR, bgG, bgB: Double;
  bdR, bdG, bdB: Double;
  lblR, lblG, lblB: Double;
  valR, valG, valB: Double;
  fntLbl, fntVal: TFtFont;
  dispStr: string;
  labelW: Double;
  valW: Double;
  radius: Double;
begin
  if not Visible or not Assigned(Canvas) then Exit;

  isDark := FtGetTheme().DarkMode;
  if isDark then
  begin
    if FIsScrubbing then
    begin
      bgR := 0.20; bgG := 0.26; bgB := 0.35; // slate-750 active
      bdR := 0.23; bdG := 0.51; bdB := 0.96; // blue-500 accent
    end
    else
    begin
      bgR := 0.14; bgG := 0.17; bgB := 0.22; // slate-800
      bdR := 0.25; bdG := 0.30; bdB := 0.38; // border
    end;
    lblR := 0.58; lblG := 0.65; lblB := 0.75; // slate-400 label
    valR := 0.93; valG := 0.95; valB := 0.98; // slate-100 value
  end
  else
  begin
    if FIsScrubbing then
    begin
      bgR := 0.92; bgG := 0.95; bgB := 0.99; // blue-50 active
      bdR := 0.15; bdG := 0.39; bdB := 0.92; // blue-600 accent
    end
    else
    begin
      bgR := 0.97; bgG := 0.98; bgB := 0.99; // slate-50
      bdR := 0.80; bdG := 0.84; bdB := 0.89; // border
    end;
    lblR := 0.40; lblG := 0.46; lblB := 0.54; // slate-500 label
    valR := 0.09; valG := 0.12; valB := 0.18; // slate-900 value
  end;

  radius := 6.0;

  Canvas.PushClipRect(X, Y, Width, Height);
  try
    // Background plate
    Canvas.DrawRoundedRect(X, Y, Width, Height, radius, bgR, bgG, bgB, 1.0);
    // 1px Border outline
    Canvas.DrawRoundedRectOutline(X, Y, Width, Height, radius, 1.0, bdR, bdG, bdB, 1.0);

    fntLbl := FtFontManager().GetFont('sans-serif,9,bold');
    fntVal := FtFontManager().GetFont('sans-serif,9');

    // 1. Draw Label text on the left
    labelW := 0.0;
    if (FLabelText <> '') and Assigned(fntLbl) then
    begin
      Canvas.DrawText(X + 8.0, Y + (Height - fntLbl.Size * 1.3) * 0.5, FLabelText, fntLbl, lblR, lblG, lblB);
      labelW := fntLbl.GetTextWidth(FLabelText) + 12.0;
    end;

    // 2. Draw Value text or editing caret on the right
    if FIsEditingText then
    begin
      dispStr := FEditText;
      Canvas.DrawText(X + Max(24.0, labelW), Y + (Height - fntVal.Size * 1.3) * 0.5, dispStr, fntVal, valR, valG, valB);
      // Blinking or static caret line
      valW := fntVal.GetTextWidth(Copy(dispStr, 1, FCaretPos));
      Canvas.DrawLine(X + Max(24.0, labelW) + valW + 1.0, Y + 5.0,
                      X + Max(24.0, labelW) + valW + 1.0, Y + Height - 5.0,
                      1.5, bdR, bdG, bdB, 1.0);
    end
    else
    begin
      dispStr := FormatValueString(FValue);
      if Assigned(fntVal) then
      begin
        valW := fntVal.GetTextWidth(dispStr);
        // Draw right-aligned or comfortably centered value
        Canvas.DrawText(X + Width - valW - 8.0, Y + (Height - fntVal.Size * 1.3) * 0.5, dispStr, fntVal, valR, valG, valB);
      end;
    end;

  finally
    Canvas.PopClipRect();
  end;
end;

procedure TFtNumberScrubber.MouseDown(AX, AY: Integer; AButton: Integer);
begin
  inherited MouseDown(AX, AY, AButton);
  if not FEnabled or (AButton <> 1) then Exit;

  SetFocus();

  // If clicked on the right-hand value portion, enter text edit mode!
  // If clicked on the left label portion, enter drag scrubbing mode!
  if (AX - X) > (Width * 0.45) then
  begin
    FIsEditingText := True;
    FEditText := FormatFloat('0.###', FValue);
    FCaretPos := Length(FEditText);
    Invalidate();
  end
  else
  begin
    if FIsEditingText then
      CommitEditText();

    FIsScrubbing := True;
    FDragStartX := AX;
    FDragStartValue := FValue;
    Invalidate();
  end;
end;

procedure TFtNumberScrubber.MouseMove(AX, AY: Integer);
var
  deltaX: Integer;
  multiplier: Double;
  stepVal: Double;
begin
  inherited MouseMove(AX, AY);
  if not FIsScrubbing then Exit;

  deltaX := AX - FDragStartX;

  multiplier := 1.0;
  // Hold Shift for 10x faster scrubbing
  if (GCurrentKeyboardModifiers and 1) <> 0 then
    multiplier := 10.0
  // Hold Alt or Control for 0.1x fine scrubbing
  else if (GCurrentKeyboardModifiers and (4 or 8)) <> 0 then
    multiplier := 0.1;

  stepVal := FStep * multiplier;
  SetValue(FDragStartValue + (deltaX * stepVal));
end;

procedure TFtNumberScrubber.MouseUp(AX, AY: Integer; AButton: Integer);
begin
  inherited MouseUp(AX, AY, AButton);
  if (AButton = 1) and FIsScrubbing then
  begin
    FIsScrubbing := False;
    Invalidate();
  end;
end;

procedure TFtNumberScrubber.KeyDown(AKeySym: Cardinal; AState: Cardinal; const AChar: string);
var
  multiplier: Double;
begin
  inherited KeyDown(AKeySym, AState, AChar);

  // Return / Enter -> commit text
  if (AKeySym = 65293) or (AKeySym = 65421) then
  begin
    if FIsEditingText then
      CommitEditText();
    Exit;
  end;

  // Escape -> cancel text
  if AKeySym = 65307 then
  begin
    if FIsEditingText then
      CancelEditText();
    Exit;
  end;

  if FIsEditingText then
  begin
    // Backspace
    if (AKeySym = 65288) and (FCaretPos > 0) then
    begin
      Delete(FEditText, FCaretPos, 1);
      Dec(FCaretPos);
      Invalidate();
    end
    // Delete
    else if (AKeySym = 65535) and (FCaretPos < Length(FEditText)) then
    begin
      Delete(FEditText, FCaretPos + 1, 1);
      Invalidate();
    end
    // Left arrow
    else if (AKeySym = 65361) and (FCaretPos > 0) then
    begin
      Dec(FCaretPos);
      Invalidate();
    end
    // Right arrow
    else if (AKeySym = 65363) and (FCaretPos < Length(FEditText)) then
    begin
      Inc(FCaretPos);
      Invalidate();
    end
    // Printable characters
    else if (Length(AChar) = 1) and (AChar[1] in ['0'..'9', '.', '-', '+']) then
    begin
      Insert(AChar, FEditText, FCaretPos + 1);
      Inc(FCaretPos);
      Invalidate();
    end;
  end
  else
  begin
    multiplier := 1.0;
    if (AState and 1) <> 0 then multiplier := 10.0
    else if (AState and 4) <> 0 then multiplier := 0.1;

    // Up / Right Arrow
    if (AKeySym = 65362) or (AKeySym = 65363) then
      SetValue(FValue + (FStep * multiplier))
    // Down / Left Arrow
    else if (AKeySym = 65364) or (AKeySym = 65361) then
      SetValue(FValue - (FStep * multiplier))
    // Page Up
    else if AKeySym = 65365 then
      SetValue(FValue + (FStep * 10.0 * multiplier))
    // Page Down
    else if AKeySym = 65366 then
      SetValue(FValue - (FStep * 10.0 * multiplier));
  end;
end;

end.
