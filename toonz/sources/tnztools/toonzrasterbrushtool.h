#pragma once

#ifndef TOONZRASTERBRUSHTOOL_H
#define TOONZRASTERBRUSHTOOL_H

#include <tgeometry.h>
#include <tproperty.h>
#include <trasterimage.h>
#include <ttoonzimage.h>
#include <tstroke.h>
#include <toonz/strokegenerator.h>
#include <toonz/rasterstrokegenerator.h>
#include "toonz/preferences.h"
#include <tools/tool.h>
#include <tools/cursors.h>

#include <tools/inputmanager.h>
#include <tools/modifiers/modifierline.h>
#include <tools/modifiers/modifiertangents.h>
#include <tools/modifiers/modifierassistants.h>
#include <tools/modifiers/modifiersegmentation.h>
#include <tools/modifiers/modifiersmooth.h>
#ifndef NDEBUG
#include <tools/modifiers/modifiertest.h>
#endif

#include "bluredbrush.h"
#include "mypainttoonzbrush.h"

#include <QCoreApplication>
#include <QRadialGradient>
#include <QElapsedTimer>

//--------------------------------------------------------------

//  Forward declarations

class TTileSetCM32;
class TTileSaverCM32;
class RasterStrokeGenerator;
class BluredBrush;
class ToonzRasterBrushToolNotifier;

//--------------------------------------------------------------

//************************************************************************
//  Toonz Raster Brush Data declaration
//************************************************************************

struct BrushData final : public TPersist {
  PERSIST_DECLARATION(BrushData)
  // frameRange, snapSensitivity and snap are not included
  // Those options are not really a part of the brush settings,
  // just the overall tool.

  std::wstring m_name;
  double m_min, m_max, m_smooth, m_hardness, m_opacityMin, m_opacityMax;
  bool m_pencil, m_pressure;
  int m_drawOrder;
  double m_modifierSize, m_modifierOpacity;
  bool m_modifierEraser, m_modifierLockAlpha;
  bool m_assistants;

  BrushData();
  BrushData(const std::wstring &name);

  bool operator<(const BrushData &other) const { return m_name < other.m_name; }

  void saveData(TOStream &os) override;
  void loadData(TIStream &is) override;
};

//************************************************************************
//   Toonz Raster Brush Preset Manager declaration
//************************************************************************

class BrushPresetManager {
  TFilePath m_fp;                 //!< Presets file path
  std::set<BrushData> m_presets;  //!< Current presets container

public:
  BrushPresetManager() {}

  void load(const TFilePath &fp);
  void save();

  const TFilePath &path() { return m_fp; };
  const std::set<BrushData> &presets() const { return m_presets; }

  void addPreset(const BrushData &data);
  void removePreset(const std::wstring &name);
};

//************************************************************************
//   Toonz Raster Brush Tool declaration
//************************************************************************

class ToonzRasterBrushTool final : public TTool,
                                   public RasterController,
                                   public TInputHandler {
  Q_DECLARE_TR_FUNCTIONS(ToonzRasterBrushTool)

  void updateCurrentStyle();
  double restartBrushTimer();

public:
  ToonzRasterBrushTool(std::string name, int targetType);

  ToolType getToolType() const override { return TTool::LevelWriteTool; }
  unsigned int getToolHints() const override;

  ToolOptionsBox *createOptionsBox() override;

  void updateTranslation() override;

  void onActivate() override;
  void onDeactivate() override;

  bool preLeftButtonDown() override;
  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) override;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;

  void inputMouseMove(const TPointD &position,
                      const TInputState &state) override;
  void inputSetBusy(bool busy) override;
  void inputPaintTrackPoint(const TTrackPoint &point, const TTrack &track,
                            bool firstTrack, bool preview) override;
  void inputInvalidateRect(const TRectD &bounds) override {
    invalidate(bounds);
  }
  TTool *inputGetTool() override { return this; };

  void draw() override;

  void onEnter() override;
  void onLeave() override;

  int getCursorId() const override {
      return Preferences::instance()->isUseStrokeEndCursor()
          ? ToolCursor::CURSOR_NONE
          : ToolCursor::PenCursor;
  }

  TPropertyGroup *getProperties(int targetType) override;
  bool onPropertyChanged(std::string propertyName) override;
  void onImageChanged() override;
  void setWorkAndBackupImages();
  void updateWorkAndBackupRasters(const TRect &rect);

  void initPresets();
  void loadPreset();
  void addPreset(QString name);
  void removePreset();

  void loadLastBrush();

  // return true if the pencil mode is active in the Brush / PaintBrush / Eraser
  // Tools.
  bool isPencilModeActive() override;

  void onColorStyleChanged();
  bool askRead(const TRect &rect) override;
  bool askWrite(const TRect &rect) override;
  bool isMyPaintStyleSelected() { return m_isMyPaintStyleSelected; }

private:
  void updateModifiers();

  enum MouseEventType { ME_DOWN, ME_DRAG, ME_UP, ME_MOVE };
  void handleMouseEvent(MouseEventType type, const TPointD &pos,
                        const TMouseEvent &e);

protected:
  TInputManager m_inputmanager;
  TSmartPointerT<TModifierLine> m_modifierLine;
  TSmartPointerT<TModifierTangents> m_modifierTangents;
  TSmartPointerT<TModifierAssistants> m_modifierAssistants;
  TSmartPointerT<TModifierSegmentation> m_modifierSegmentation;
  TSmartPointerT<TModifierSegmentation> m_modifierSmoothSegmentation;
  TSmartPointerT<TModifierSmooth> m_modifierSmooth[3];
#ifndef NDEBUG
  TSmartPointerT<TModifierTest> m_modifierTest;
#endif
  TInputModifier::List m_modifierReplicate;

  class MyPaintStroke : public TTrackHandler {
  public:
    MyPaintToonzBrush brush;

    inline MyPaintStroke(const TRaster32P &ras, RasterController &controller,
                         const mypaint::Brush &brush,
                         bool interpolation = false)
        : brush(ras, controller, brush, interpolation) {}
  };

  class PencilStroke : public TTrackHandler {
  public:
    RasterStrokeGenerator brush;

    inline PencilStroke(const TRasterCM32P &raster, Tasks task,
                        ColorType colorType, int styleId, const TThickPoint &p,
                        bool selective, int selectedStyle, bool lockAlpha,
                        bool keepAntialias, bool isPaletteOrder = false)
        : brush(raster, task, colorType, styleId, p, selective, selectedStyle,
                lockAlpha, keepAntialias, isPaletteOrder) {}
  };

  class BluredStroke : public TTrackHandler {
  public:
    BluredBrush brush;

    inline BluredStroke(const TRaster32P &ras, int size,
                        const QRadialGradient &gradient, bool doDynamicOpacity)
        : brush(ras, size, gradient, doDynamicOpacity) {}
  };

  struct Painting {
    // initial painting input
    bool active = false;
    int styleId = 0;
    bool smooth = false;
    // 作業中のFrameIdをクリック時に保存し、マウスリリース時（Undoの登録時）に別のフレームに
    // 移動していたときの不具合を修正する。
    TFrameId frameId;

    // common variables
    TTileSetCM32 *tileSet     = nullptr;
    TTileSaverCM32 *tileSaver = nullptr;
    TRect affectedRect;

    struct Pencil {
      bool isActive   = false;
      bool realPencil = false;
    } pencil;

    struct Blured {
      bool isActive = false;
    } blured;

    struct MyPaint {
      bool isActive = false;
      mypaint::Brush baseBrush;
      TRect strokeSegmentRect;
    } myPaint;
  } m_painting;

  TPropertyGroup m_prop[2];

  TDoublePairProperty m_rasThickness;
  TDoubleProperty m_smooth;
  TDoubleProperty m_hardness;
  TEnumProperty m_preset;
  TEnumProperty m_drawOrder;
  TBoolProperty m_pencil;
  TBoolProperty m_pressure;
  TDoubleProperty m_modifierSize;
  TBoolProperty m_modifierLockAlpha;
  TBoolProperty m_assistants;

  double m_minThick, m_maxThick;

  int m_targetType;
  TPointD m_dpiScale,
      m_mousePos,  //!< Current mouse position, in world coordinates.
      m_brushPos;  //!< World position the brush will be painted at.

  QRadialGradient m_brushPad;

  TRasterCM32P m_backupRas;
  TRaster32P m_workRas;
  TRect m_workBackupRect;

  BrushPresetManager
      m_presetsManager;  //!< Manager for presets of this tool instance

  bool m_enabled,
      m_isPrompting,  //!< Whether the tool is prompting for spline
                      //! substitution.
      m_firstTime, m_presetsLoaded;

  ToonzRasterBrushToolNotifier *m_notifier;
  bool m_isMyPaintStyleSelected = false;
  QElapsedTimer m_brushTimer;
  int m_minCursorThick, m_maxCursorThick;

  bool m_propertyUpdating = false;

protected:
  static void drawLine(const TPointD &point, const TPointD &centre,
                       bool horizontal, bool isDecimal);
  static void drawEmptyCircle(TPointD point, int thick, bool isLxEven,
                              bool isLyEven, bool isPencil);

  TPointD getCenteredCursorPos(const TPointD &originalCursorPos);
};

//------------------------------------------------------------

class ToonzRasterBrushToolNotifier final : public QObject {
  Q_OBJECT

  ToonzRasterBrushTool *m_tool;

public:
  ToonzRasterBrushToolNotifier(ToonzRasterBrushTool *tool);

protected slots:
  // void onCanvasSizeChanged() { m_tool->onCanvasSizeChanged(); }
  void onColorStyleChanged() { m_tool->onColorStyleChanged(); }
};

#endif  // TOONZRASTERBRUSHTOOL_H
