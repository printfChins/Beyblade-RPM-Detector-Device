/* File: BRD_BPX_PRO/brd_ui_style_scene.h
 * [修改] 純 GFX 單一 TORQUE 場景；維持既有 draw API 及硬體介面。
 */
#ifndef BRD_UI_STYLE_SCENE_H
#define BRD_UI_STYLE_SCENE_H
#include <stdint.h>
#include "brd_ui_types.h"
#include "brd_ui_curve.h"
#include <Adafruit_GFX.h>
namespace brd_ui { namespace style_scene {
void draw(Adafruit_GFX& target,uint8_t theme,Page page,uint8_t frame,
          const Model& model,const Curve& curve);
} }
#endif
