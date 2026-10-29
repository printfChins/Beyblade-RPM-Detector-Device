#!/usr/bin/env python3
"""Host-side validation for integration. Not a replacement for ESP32-C3 Arduino build."""
from pathlib import Path
import os, subprocess
root=Path(__file__).resolve().parents[1]
flags=['g++','-std=c++17','-Wall','-Wextra','-Werror','-Itests/stubs','-I.']
tests={
 'rpm_adc':['tests/test_rpm_adc.cpp','brd_rpm_adc.cpp'],
 'load_adc':['tests/test_load_adc.cpp','brd_load_adc.cpp'],
 'adc_shared':['tests/test_adc_shared.cpp','brd_adc_shared.cpp'],
 'model':['tests/test_model.cpp','brd_settings_model.cpp'],
 'dual_edge_ble':['tests/test_dual_edge_ble_selection.cpp','brd_bbp_session.cpp','brd_bbp_protocol.cpp'],
 'dual_edge_rpm':['tests/test_dual_edge_measurement.cpp','brd_measurement.cpp','brd_shot_capture.cpp','brd_ui_curve.cpp'],
 'ui_input_curve':['tests/test_ui_input_curve.cpp','brd_ui_input.cpp','brd_ui_curve.cpp'],
 'shot_capture':['tests/test_shot_capture.cpp','brd_shot_capture.cpp','brd_ui_curve.cpp','brd_bbp_session.cpp','brd_bbp_protocol.cpp'],
 'ui_navigation':['tests/test_ui_navigation.cpp'],
 'runtime':['tests/test_runtime.cpp','brd_measurement.cpp','brd_shot_capture.cpp','brd_ui_curve.cpp','brd_settings.cpp','brd_settings_model.cpp','brd_bbp_session.cpp','brd_bbp_protocol.cpp'],
 'ble':['tests/test_ble.cpp','brd_settings_ble.cpp','brd_settings_model.cpp'],
 'auto':['tests/test_auto_baseline.cpp','brd_measurement.cpp','brd_shot_capture.cpp','brd_ui_curve.cpp'],
 'ota_protocol':['tests/test_ota_protocol.cpp','brd_ota_protocol.cpp'],
 'ota_runtime':['tests/test_ota_runtime.cpp','brd_ota.cpp','brd_ota_protocol.cpp','brd_settings_model.cpp'],
 'ota_history':['tests/test_ota_history.cpp','brd_bbp.cpp','brd_bbp_session.cpp','brd_bbp_protocol.cpp']
}
bin_dir=root/'tests'/'bin';bin_dir.mkdir(exist_ok=True)
for name, files in tests.items():
    output=bin_dir/('motion_'+name)
    link=['-lcrypto'] if name=='ota_runtime' else []
    subprocess.run(flags+files+link+['-o',str(output)], cwd=root, check=True)
    subprocess.run([str(output)],cwd=root,check=True)

for f in ['brd_ui_legible_text.cpp','brd_ui_core.cpp','brd_ui_motion.cpp','brd_tft.cpp','brd_ui_screen.cpp', 'brd_ui_input.cpp', 'brd_bbp.cpp', 'brd_ui_page_ota.cpp', 'brd_ui_style_scene.cpp', 'brd_ui_warning_pages.cpp', 'brd_ui_dirty_presenter.cpp','brd_ui_transition.cpp']:
    subprocess.run(flags+['-fsyntax-only',f],cwd=root,check=True)
subprocess.run(flags+['-x','c++','-fsyntax-only','BRD_BPX_PRO.ino'], cwd=root,check=True)
for title,sources in {
    'dirty_tiles':['tests/test_tile_dirty.cpp','brd_ui_dirty_presenter.cpp'],
    'u8g2_fonts':['tests/test_legible_text.cpp','brd_ui_legible_text.cpp'],
    'torque_ui':['tests/test_torque_style.cpp','brd_ui_style_scene.cpp','brd_ui_warning_pages.cpp'],
    'low_battery_blink':['tests/test_low_battery_blink.cpp','brd_ui_motion.cpp','brd_ui_style_scene.cpp','brd_ui_warning_pages.cpp','brd_ui_dirty_presenter.cpp'],
    'torque_ble_icon':['tests/test_ui_ble_icon.cpp','brd_ui_style_scene.cpp','brd_ui_warning_pages.cpp'],
    'page_transition':['tests/test_ui_transition.cpp','brd_ui_transition.cpp','brd_ui_style_scene.cpp','brd_ui_warning_pages.cpp','brd_ui_dirty_presenter.cpp'],
    'max_on_main':['tests/render_main_max.cpp','brd_ui_style_scene.cpp','brd_ui_warning_pages.cpp'],
    'indicator_full_scale':['tests/test_ui_indicator_full_scale.cpp','brd_ui_style_scene.cpp','brd_ui_warning_pages.cpp'],
    '25fps_schedule':['tests/test_25fps_schedule.cpp','brd_ui_motion.cpp','brd_ui_style_scene.cpp','brd_ui_warning_pages.cpp','brd_ui_dirty_presenter.cpp'],
    'boot_fill_animation':['tests/test_boot_fill_animation.cpp','brd_ui_motion.cpp','brd_ui_style_scene.cpp','brd_ui_warning_pages.cpp','brd_ui_dirty_presenter.cpp'],
    # [保留] TORQUE MAIN / IR TEST 無閃電充電動畫回歸測試。
    'charging_ui':['tests/test_charge_ui.cpp','brd_ui_style_scene.cpp','brd_ui_warning_pages.cpp'],
    'w1_l2_layout':['tests/test_w1_l2_layout.cpp','brd_ui_style_scene.cpp','brd_ui_warning_pages.cpp'],
}.items():
    if 'brd_ui_style_scene.cpp' in sources:
        sources=sources+['brd_ui_legible_text.cpp']
    output=bin_dir/('exact_'+title)
    subprocess.run(flags+sources+['-o',str(output)],cwd=root,check=True)
    args=[str(output)]
    if title=='max_on_main': args.append(str(bin_dir))
    subprocess.run(args,cwd=root,check=True)
subprocess.run(['python3', 'tests/test_arduino_pi_compat.py'], cwd=root, check=True)
print('PASS: 15 regression modules + TORQUE UI tests + firmware syntax checks.')
