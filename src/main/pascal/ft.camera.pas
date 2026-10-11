unit Ft.Camera;

// Ft.Camera
// =========
// Pure 2D affine camera math helper for creative and document viewports.
// Provides bidirectional World <-> Screen coordinate transformations,
// cursor-pivot anchored exponential zoom, panning, and view fitting.

{$mode objfpc}{$H+}
{$modeswitch advancedrecords}

interface

uses
  SysUtils, Math;

type
  PFtCamera2D = ^TFtCamera2D;

  { 2D Camera for infinite or bounded viewports }
  TFtCamera2D = record
    PanX: Double;       { Horizontal pan offset in screen coordinates }
    PanY: Double;       { Vertical pan offset in screen coordinates }
    Zoom: Double;       { Zoom scale factor (1.0 = 100%) }
    MinZoom: Double;    { Minimum zoom threshold (e.g. 0.02 = 2%) }
    MaxZoom: Double;    { Maximum zoom threshold (e.g. 64.0 = 6400%) }
    Rotation: Double;   { Viewport rotation in degrees (default 0.0) }

    procedure Init(AMinZoom: Double = 0.02; AMaxZoom: Double = 64.0);
    procedure Reset();
    procedure WorldToScreen(AWldX, AWldY: Double; out AScrX, AScrY: Double);
    procedure ScreenToWorld(AScrX, AScrY: Double; out AWldX, AWldY: Double);
    procedure ZoomAt(AScreenPivotX, AScreenPivotY: Double; ADeltaFactor: Double);
    procedure SetZoomAt(AScreenPivotX, AScreenPivotY: Double; ANewZoom: Double);
    procedure PanBy(ADeltaScrX, ADeltaScrY: Double);
    procedure FitRect(AWldX, AWldY, AWldW, AWldH: Double; AViewW, AViewH: Double; AMargin: Double = 30.0);
  end;

function FtCameraCreate(AMinZoom: Double = 0.02; AMaxZoom: Double = 64.0): TFtCamera2D;

implementation

function FtCameraCreate(AMinZoom: Double; AMaxZoom: Double): TFtCamera2D;
begin
  Result.Init(AMinZoom, AMaxZoom);
end;

procedure TFtCamera2D.Init(AMinZoom: Double; AMaxZoom: Double);
begin
  PanX := 0.0;
  PanY := 0.0;
  Zoom := 1.0;
  MinZoom := AMinZoom;
  MaxZoom := AMaxZoom;
  Rotation := 0.0;
end;

procedure TFtCamera2D.Reset();
begin
  PanX := 0.0;
  PanY := 0.0;
  Zoom := 1.0;
  Rotation := 0.0;
end;

procedure TFtCamera2D.WorldToScreen(AWldX, AWldY: Double; out AScrX, AScrY: Double);
var
  rad, cosA, sinA: Double;
  rx, ry: Double;
begin
  if Abs(Rotation) > 0.0001 then
  begin
    rad := DegToRad(Rotation);
    cosA := Cos(rad);
    sinA := Sin(rad);
    rx := AWldX * cosA - AWldY * sinA;
    ry := AWldX * sinA + AWldY * cosA;
    AScrX := rx * Zoom + PanX;
    AScrY := ry * Zoom + PanY;
  end
  else
  begin
    AScrX := AWldX * Zoom + PanX;
    AScrY := AWldY * Zoom + PanY;
  end;
end;

procedure TFtCamera2D.ScreenToWorld(AScrX, AScrY: Double; out AWldX, AWldY: Double);
var
  effZoom: Double;
  dx, dy: Double;
  rad, cosA, sinA: Double;
begin
  effZoom := Zoom;
  if effZoom < 0.00001 then effZoom := 0.00001;

  dx := (AScrX - PanX) / effZoom;
  dy := (AScrY - PanY) / effZoom;

  if Abs(Rotation) > 0.0001 then
  begin
    rad := DegToRad(-Rotation);
    cosA := Cos(rad);
    sinA := Sin(rad);
    AWldX := dx * cosA - dy * sinA;
    AWldY := dx * sinA + dy * cosA;
  end
  else
  begin
    AWldX := dx;
    AWldY := dy;
  end;
end;

procedure TFtCamera2D.ZoomAt(AScreenPivotX, AScreenPivotY: Double; ADeltaFactor: Double);
begin
  SetZoomAt(AScreenPivotX, AScreenPivotY, Zoom * ADeltaFactor);
end;

procedure TFtCamera2D.SetZoomAt(AScreenPivotX, AScreenPivotY: Double; ANewZoom: Double);
var
  clampedZoom: Double;
  wPivotX, wPivotY: Double;
begin
  clampedZoom := EnsureRange(ANewZoom, MinZoom, MaxZoom);
  if Abs(clampedZoom - Zoom) < 0.000001 then Exit;

  // 1. Convert pivot screen point to world coordinates using current transform
  ScreenToWorld(AScreenPivotX, AScreenPivotY, wPivotX, wPivotY);

  // 2. Apply new zoom
  Zoom := clampedZoom;

  // 3. Adjust Pan so that wPivot still projects to AScreenPivotX, AScreenPivotY
  // When Rotation is 0: AScreenPivotX = wPivotX * Zoom + PanX  =>  PanX = AScreenPivotX - wPivotX * Zoom
  if Abs(Rotation) <= 0.0001 then
  begin
    PanX := AScreenPivotX - (wPivotX * Zoom);
    PanY := AScreenPivotY - (wPivotY * Zoom);
  end
  else
  begin
    // For rotated views:
    PanX := 0.0;
    PanY := 0.0;
    WorldToScreen(wPivotX, wPivotY, PanX, PanY);
    PanX := AScreenPivotX - PanX;
    PanY := AScreenPivotY - PanY;
  end;
end;

procedure TFtCamera2D.PanBy(ADeltaScrX, ADeltaScrY: Double);
begin
  PanX := PanX + ADeltaScrX;
  PanY := PanY + ADeltaScrY;
end;

procedure TFtCamera2D.FitRect(AWldX, AWldY, AWldW, AWldH: Double; AViewW, AViewH: Double; AMargin: Double);
var
  availW, availH: Double;
  scaleX, scaleY, newZoom: Double;
  centerWldX, centerWldY: Double;
begin
  if (AWldW <= 0) or (AWldH <= 0) or (AViewW <= 0) or (AViewH <= 0) then Exit;

  availW := AViewW - (AMargin * 2.0);
  availH := AViewH - (AMargin * 2.0);
  if availW < 10.0 then availW := 10.0;
  if availH < 10.0 then availH := 10.0;

  scaleX := availW / AWldW;
  scaleY := availH / AWldH;
  newZoom := EnsureRange(Min(scaleX, scaleY), MinZoom, MaxZoom);

  Zoom := newZoom;
  Rotation := 0.0;

  centerWldX := AWldX + (AWldW * 0.5);
  centerWldY := AWldY + (AWldH * 0.5);

  PanX := (AViewW * 0.5) - (centerWldX * Zoom);
  PanY := (AViewH * 0.5) - (centerWldY * Zoom);
end;

end.
