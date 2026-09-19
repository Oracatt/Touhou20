# 东方锦上京 TH20 C++ 逆向工程

**完整独立 C++ 源码尚未恢复完成。** 用户已明确要求完整源码，原版机器码的局部替换版不满足该目标。`source_reconstruction/` 正在按此要求恢复独立模块；`dist/playable` 保留为此前的局部替换实验，不是最终交付。

已从指定的 `th20.exe` / `th20.dat` / `thbgm.dat` 恢复真实资源、脚本、机器码证据，并实现可编译的 C++ 解析模块和部分引擎函数。原游戏目录保持不变。

## 当前独立源码入口

见 [源码构建说明](SOURCE_BUILD.md) 和 [模块记录](source_reconstruction/README.md)。整套源码已成功链接为 `dist/source_game/th20_source.exe`，构建不使用原 EXE，运行时读取两个原资源 DAT。已修复加载时回调过早启用及敌机释放后写入，实际运行第 1、3 关演示并进入非演示游戏。**整局流程及 1:1 逐帧验收仍未完成。**

从源码重新编译并打包：

```powershell
Set-Location 'D:\AIWorkspace\Touhou20Reconstruction'
.\build_source_game.ps1
```

源码和运行问题见 [源码工程](source_reconstruction/README.md) 与 [源码版运行记录](reports/SOURCE_GAME_RUNTIME.md)。`dist/source_game/build_manifest.json` 记录实际编译源文件、EXE 和资源哈希；`Play.cmd` 使用包内隔离存档。以下 `dist/playable` 是此前的机器码局部替换实验。

## 历史实验：原版机器码局部替换包

双击 `D:\AIWorkspace\Touhou20Reconstruction\dist\playable\Play.cmd`。方向键移动，Z 确认/射击，X 炸弹，Shift 低速，Esc 暂停；画面及手柄设置使用同目录的 `Configure.cmd`。请保留整个目录，游戏需要同目录 DLL 和两个 DAT 资源文件。通过这两个脚本运行时，存档保存到包内 `userdata/ShanghaiAlice/th20`，不会写入原版存档。

正式启动脚本已实测并选择 960×720 窗口。运行测试和剩余验证范围见 `reports/PLAYABLE_BUILD.md`、`reports/playable_runtime_validation.json`；自动按键未被游戏稳定识别，手动移动、射击和暂停尚未完成实操验收。

按用户“保持一致与原版”的优先级，本次采用增量替换：新 EXE 保留原入口、渲染、输入、关卡脚本执行、音频、碰撞、存档和回放机器码，七个随机数/计时器函数转入真正由 C++ 编译的 `th20_native_bridge.dll`。它直接运行保留的引擎，不会启动原 `th20.exe`。构建清单 `reports/playable_build.json` 记录源 EXE、新 EXE、DLL、C++ 源文件及资源哈希。

重新编译并打包：

```powershell
Set-Location 'D:\AIWorkspace\Touhou20Reconstruction'
.\build_playable.ps1
```

这项构建需要本机 VS2019 的 x86 C++ 工具和 Windows SDK、Python，以及此前准备的 `.cache/binary_python/pefile`。`incremental/native_bridge/README.md` 包含 ABI 与测试说明。直接双击 EXE 会沿用进程的 APPDATA；要保持独立存档，请用 `Play.cmd`。

## 从哪里看

| 路径 | 内容与验证范围 |
|---|---|
| `dist/playable/` | 新 EXE、C++ DLL、原版数据及独立存档启动脚本 |
| `incremental/native_bridge/` | 七个实际游戏 ABI 的 C++ DLL 导出及硬件对照测试 |
| `tools/build_playable.py` | 校验原文件哈希，扩展 PE 导入与重定位，链接七个 C++ 入口并打包 |
| `native_recovered/native_core.hpp` | 手工重建的随机数核心、帧计时器；每个函数标出原程序地址 |
| `src/`、`include/th20/binary.hpp` | 可编译 C++17 PE/ECL 解析器，保留指令地址、时刻、参数和未知字段 |
| `scripts/recovered/ecl/` | 21 个原版 ECL 的可读反编译文本；这是专用脚本语言，不是 C++ |
| `scripts/recovered/ecl_raw/` | 用于回编译检查的 ECL 文本 |
| `assets/raw/` | 从 th20.dat 提取的原始文件，包含场景、动画、弹幕、音效和自机数据 |
| `assets/bgm/` | 19 首原始 PCM 音乐；`loops.json` 保存精确循环帧位置 |
| `analysis/binary/` | 完整线性反汇编、候选函数、导入、调用、字符串和地址引用 |
| `analysis/ghidra/pseudocode/` | Ghidra 识别的 6,928 个函数的 C 风格伪代码；已知存在类型/调用约定恢复不准确，须继续人工整理 |
| `analysis/ghidra/source_module_index.md` | 原源码路径线索与函数地址索引，供继续恢复引擎 |
| `reports/` | 文件哈希、资源恢复、C++ 解析、机器码差分验证等结果 |

完整机器码自动反编译的结果与限制见 `reports/binary_analysis.md`。首次导出有一个 ECL 大函数超时，单独提高时限后成功；最终 6,928 个已识别函数均有伪代码文本。这是恢复类型和控制流的参考，不能当作已可编译的原始源码，也不能证明不存在未识别的函数。

## 已验证内容

- 指定原版 `th20.exe` 的 SHA-256 为 `a274b45fe6ec53511718bb328c2ff169a74e67f95d1b0c74d97d348b955a0897`，PE32/x86，首选映像基址 `0x00400000`。随附 readme 写 1.00a，但 EXE 内嵌标识为 `ver 1.00c`，本工程以实际 EXE 哈希为依据。
- C++ ECL 模块解析全部 **21 个文件、730 个子程序、23,760 条指令**，逐条重编码后保持原容器字节一致。这证明解析和保存可靠，不代表指令已在新引擎中实现。
- **7 个重建函数、39,216 次差分比较通过**：在 Unicorn 中执行原 EXE 的实际 x86 指令，与编译出的 C++ DLL 比较返回值和对象的完整字节。测试不替换原机器指令；覆盖极值、计时器标志、分数速度、NaN/Inf 等。范围限于这些函数及默认 SSE/x87 环境。
- **29,856 次真实 CPU 对照通过**：另建 32 位测试程序，映射原 PE 并只执行指定随机数/计时器函数，与同进程的 C++ 实现比较。`reports/native_cpu_validation.json` 保存实测结果，测试没有运行原游戏入口。
- **实际游戏 ABI 的 100,116 次真实 CPU 对照通过**：装载发布 DLL，与原函数比较完整对象、返回值、栈平衡和非易失寄存器。计时器修改函数的 `EAX=self` 已保留。报告为 `incremental/native_bridge/bridge_abi_validation.json`。
- **增量 PE 结构校验通过**：保留 211 个原导入的 IAT 地址和 29,196 个原重定位，新增七个入口间接跳转及重定位；只修改原文件内 70 个字节，均属于已列出的 PE 头字段或函数入口。原六节布局、原入口、安全元数据和其余字节保留。
- **最终 EXE 链接的 162,784 次硬件对照通过**：在两个不同映像基址下，直接调用发布 EXE 的七个原入口，经 `FF25 → IAT → C++ DLL` 执行，验证重定位、完整对象、返回值、栈和寄存器。见 `reports/playable_link_validation.json`。首选基址在测试进程内被 Windows 代码页映射占用，报告如实标记跳过；两个非首选基址以及真实游戏的 ASLR 加载均已测试。
- **实际运行回归**：新 EXE 已进入主菜单及内置演示关卡，实测约 60 FPS，DLL 计数证明实际执行了 C++ 替换函数。原版与重建版的 `demo2.rpy` 第一关，在 1,916 个共享回放帧号的稳定采样中，20 个指定状态字段合计 38,320 次比较无差异，包括分数、灵力、擦弹、生命/炸弹资源和输入位掩码。见 `reports/playable_demo_comparison.json`。这是异步字段采样，不是完整逐帧锁步；未比较全部实体、图像和音频。
- **19 首 BGM** 解出后重新读取 WAV，PCM 与原归档逐字节相同；所有 PCM 区间连续覆盖 thbgm.dat 的 16 字节头之后的内容。
- `source_manifest.py --verify` 检查原目录 7 个文件是否仍与初始哈希相同。

资源脚本的“文本反编译 → 重新编译”是另一项验证：**21 ECL + 87 MSG + 10 STD，共 118 个脚本已全部重新编译且 SHA-256 与原文件相同**。为此修正了上游工具的 TH20 格式、文本编码和序列化问题。完整清单保存在 `reports/assets_recovery.json`，不能与上述 C++ 容器重编码检查混为一谈。

73 个 ANM 动画包已导出脚本与 660 个纹理文件，随后从脚本和原压缩纹理重建，也全部与原文件逐字节相同。四类合计 **191 个脚本档案** 通过无损重建验证。归档中 285 条记录对应 283 个唯一文件名；两个同名重复项已按各自索引独立提取并确认相同。

## 构建与运行检查

本机已使用 VS2019/MSVC 和 Windows SDK 编译。PowerShell：

```powershell
Set-Location 'D:\AIWorkspace\Touhou20Reconstruction'
.\build.ps1 -Validate
```

构建出的程序是分析工具：

```powershell
.\build\Release\th20_inspect.exe ecl-summary assets\raw\st01.ecl
.\build\Release\th20_inspect.exe ecl assets\raw\st01.ecl reports\st01_instructions.json
.\build\Release\th20_inspect.exe ecl-scan assets\raw reports\ecl_binary_scan.json
```

二进制检查可单独用 CMake 构建，不依赖 thtk 或 Python。`-Validate` 的机器码对照测试需要本工程 `.cache/binary_python` 中的 pefile，以及 `.cache/emulation_python` 中的 Unicorn；当前机器已准备好。

恢复音乐的可复现命令：

```powershell
python tools\recover_bgm.py
```

thtk 构建和完整资源恢复命令如下；版本和本地兼容补丁见 [资源恢复报告](reports/assets_recovery.md)。原目录中的零字节 `st01.ecl` 不是实际第一关脚本，工程使用从归档恢复的版本。

```powershell
.\tools\asset_build_thtk.ps1
python tools\asset_recover.py --game-dir 'E:\BaiduNetdiskDownload\PCGAME-Touhou.Kinjoukyou.Fossilized.Wonders-P2P\Touhou.Kinjoukyou.Fossilized.Wonders'
```

独立核对实际落盘的 191 个重建档案及 19 首音乐：

```powershell
python tools\verify_artifacts.py
```

结果保存在 `reports/artifact_verification.json`。它重新读取文件计算哈希，而不是直接相信恢复工具写出的成功标志。

真实 CPU 差分验证可单独复跑：

```powershell
.\tests\validate_native_cpu.ps1
```

补充的任意浮点位模式仿真测试发现 330 个 NaN payload/符号位差异，因此 `reports/native_edges_validation.json` 如实保留 `failed`。真实 CPU 的 29,856 次对照没有差异；全部 330 个相同失败输入的四方比较确认原机器码、本机 x86 C++、x64 C++ 相同，而 Unicorn 的 NaN 传播不同，见 `reports/native_nan_oracle_comparison.json`。这不代表所有 CPU、编译器或浮点控制状态都已验证；项目不修改正确的原机行为来迎合仿真器。

## 距离完整纯 C++ 重建与 1:1 证明的剩余工作

1. 对已链接的源码 EXE 做实际运行验证，修复跨模块初始化、线程调度和生命周期问题。
2. 审核逐函数覆盖，补齐尚未验证的原程序路径；明确区分真实空函数、无效输入和未实现逻辑。
3. 覆盖两个角色、各难度、关卡、通关、暂停、录像与存档流程。
4. 使用原版相同回放/输入流，在相同随机种子和帧进度下逐帧比较位置、弹幕、碰撞、分数及随机状态，再比较图像与音频。

`dist/source_game` 使用源码实现上述子系统。各模块的局部对照不能推出整款游戏玩法相同；完整覆盖和实际运行仍须逐项核对。自动反编译文本与工程中的可编译源码分别保存，不能混为一谈。

## 参考来源

- [Touhou Toolkit 上游](https://github.com/thpatch/thtk)：本工程资源恢复工具，固定提交 `892114a0fcaa0bbdaaecf3cb4ad56f758683fb40`。
- [thprac 的 TH20 模块](https://github.com/touhouworldcup/thprac/blob/585fae1aad4b720f350655a44e3f2ec7fc0dfa25/thprac/src/thprac/thprac_th20.cpp)：作为计时器入口地址的查找线索；重建函数内容以指定原 EXE 的汇编及差分结果为准。
- [thcrap BGM 实现](https://github.com/thpatch/thcrap/blob/master/thcrap_tsa/src/bgm.cpp)：音乐格式字段与循环点含义的参考。

外部工具与参考代码的许可证保存在各自目录；原作资源没有被当作本工程新代码。所有资源恢复都在本地进行。
