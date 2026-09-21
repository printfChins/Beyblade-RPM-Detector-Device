#!/usr/bin/env python3
# BRD_BBP/tests/run_host_tests.py
# [BRD_BBP 新增] 宿主驗證，不參與Arduino韌體編譯。
import pathlib,subprocess,tempfile,sys
root=pathlib.Path(__file__).resolve().parents[1]
# [修正 新增] 只訂閱的上位機也是必要回歸情境。
tests={"dual_edge":["tests/test_dual_edge.cpp"],"conformance":["tests/test_conformance.cpp","brd_bbp_protocol.cpp","brd_bbp_session.cpp"],"protocol":["tests/test_protocol.cpp","brd_bbp_protocol.cpp"],"session":["tests/test_session.cpp","brd_bbp_protocol.cpp","brd_bbp_session.cpp"],"measurement":["tests/test_measurement.cpp"],"transport":["tests/test_transport.cpp"],"subscriber":["tests/test_subscriber.cpp"]}
flags=["g++","-std=c++17","-Wall","-Wextra","-Werror","-fsanitize=address,undefined","-DCONFIG_IDF_TARGET_ESP32C3","-Itests/host_stubs","-I."]
with tempfile.TemporaryDirectory(prefix="brd-host-") as d:
 for name,sources in tests.items():
  exe=pathlib.Path(d)/name
  subprocess.run(flags+sources+["-o",str(exe)],cwd=root,check=True)
  subprocess.run([str(exe)],cwd=root,env={"ASAN_OPTIONS":"detect_leaks=0"},check=True)
  print(f"PASS {name}")
print(f"PASS {len(tests)} host test binaries")
