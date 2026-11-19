# 目标方位计算

连接设备后，在方位角输入框中可输入整数角度、4/6/8 位 Maidenhead 网格、对方呼号或 DXCC 前缀。默认开启[最短路径旋转](application-settings.md)，数字角度按 360° 取余为 0–359°，支持负数和多圈整数；关闭后使用“方位角补偿值”至“补偿值 + 450°”的连续角度范围。输入忽略大小写和首尾空白，按回车发送旋转命令；未按回车不会移动设备。网格或呼号需要先通过“设置 → 我的网格…”设置本站位置，修改后下一次提交立即生效。

网格使用几何中心；呼号和前缀使用所属 DXCC 的代表坐标，不查询对方的实际位置。计算结果是真北起算、顺时针增加的短路径大圆初始方位，四舍五入到 0–359 度，再扣除补偿并从 0–450° 中选择最近一次设备读数附近的可达机械角度，发送 `Mddd`，保留当前仰角。重叠区域等距时选较小机械角度；数字输入在最短路径开启时使用同样的选解，关闭时明确指定连续行程。未设置本站网格、无法识别目标、同点或对跖点等无法确定唯一方位的情况会提示原因，保留输入供修改，不发送命令。编辑预设时，这些目标解析规则相同，但回车保存预设且不发送命令；按 Esc 或离开编辑框取消。

主页面勾选“长径”后，本地网格、呼号或 DXCC 前缀算出的目标方位反转 180°，再应用方位补偿并选择最近的可达机械行程。例如无补偿时，从 `PM95AA` 定位 `JA` 的短径方位约为 11°，长径为 191°。右键方位预设可输入角度、网格、呼号或 DXCC 前缀；回车只保存预设，不会立即旋转。输入目标时使用当前长径/短径状态，并以该预设原机械位置为参考保存等价的可达位置；之后点击预设按钮才旋转，且按当时的设备读数重新选最近位置。直接输入的数字角度、罗盘选角及已保存预设按指定角度定位，不会因之后切换长径而改变；实测读数、仰角和控制台原始命令同样不受影响。长径仍作用于 GridTracker 桥接的目标，N1MM 的长径/短径由 N1MM 自行选择。

有效网格优先于呼号解析，例如 `AA00` 按网格处理。呼号查询先匹配完整呼号例外，再匹配最长前缀，使用内置的 [CTY.DAT](../targeting/data/README.md)，无需网络。支持 `EA8/K1ABC`、`K1ABC/EA8` 及常见 `/P`、`/M`、`/QRP` 后缀；单个数字异地后缀按区号替换处理，完整呼号例外仍优先。`/MM`、`/AM` 无固定 DXCC，两端都是完整呼号或无法识别的异地前缀也会拒绝解析。内置数据只表示对应快照中的归属，不推断未来或未列出的特殊台位置。

## 在其他接口中复用

将调用方链接到 CMake 目标 `gs232b_targeting`，包含 `targeting/TargetBearing.h`。该静态库只依赖 Qt Core，自带数据资源，不依赖 Widgets、`AppSettings` 或通信层；调用方显式提供本站网格并自行决定是否发送设备命令。

```cpp
#include "targeting/TargetBearing.h"

const auto result = TargetBearing::calculate(stationGrid, targetText);
if (result.solution) {
    const double bearing = result.solution->degrees;
    const int rotorAngle = result.solution->roundedDegrees();
    const auto &target = result.solution->target;
}
```

`calculate()` 的 `Error` 区分 `InvalidStationGrid`、`UnknownTarget` 和 `UndefinedBearing`；仅成功时 `solution` 有值。错误枚举由调用方通过自身的 i18n 系统转换为提示，计算层不携带界面文案。此接口只解析网格或呼号，直接角度由调用方按设备轴的范围校验。

`resolve()` 可单独获取规范化输入、目标类型、坐标，以及呼号对应的 DXCC 名称和主前缀；`initialBearing()` 可直接接受两组坐标。所有坐标均以 `QPointF(x=东经, y=北纬)` 表示，单位为度。非法坐标、同点、对跖点以及本站位于极点时返回空值。`DxccDatabase::instance().find()` 还可用于只查询呼号归属，不参与网格识别。
