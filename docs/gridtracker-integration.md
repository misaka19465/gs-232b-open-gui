# GridTracker 接入

GUI 接收同一电脑上 GridTracker / GridTracker 2 的旋转器定位请求，通过当前串口或 WebSocket 连接控制 GS-232B，并向 GridTracker 2 回复实测方位和仰角。使用 CatRotator / PstRotator UDP 协议，不需要另外运行 PstRotator。当前支持一个控制器，固定使用本机地址和默认端口。

## 配置和使用

1. 在 GUI 连接 GS-232B，勾选“外部 → GridTracker… → 启用”，将主页面模式切到“跟踪”以允许定位。接入开关立即保存，重启恢复监听；默认关闭，控制模式每次启动默认为“手动”。端口被其他程序占用时提示原因并保持关闭，不覆盖已保存偏好。
2. GridTracker 2 打开 **Settings → Features → Service → Rotor Control**，启用 **Rotor Control**，取消 **Connect** 后选择 **CatRotator / PstRotator**，Host 填 `127.0.0.1`，UDP Port 填 `12000`，再勾选 **Connect**。旧版 GridTracker 在 **CatRotator / PstRotator** 行启用服务，IP 和 UDP Port 使用相同值。
3. 在 Call Roster 中右键呼号选择 **Aim Rotator**，或按住 **Ctrl** 点击地图目标。GridTracker 自行根据它的 QTH 计算方位，无需在 GUI 再设置本站网格。
4. GridTracker 2 自动查询位置，位置回复端口为 `12001`。它的 **Follow Satellite** 可发送方位和仰角；不需要跟踪时在 GridTracker 取消该选项。

GUI 的控制仍可使用，N1MM 桥接使用不同端口，也可同时启用。虚拟串口透传、断线、校准确认弹窗和校准执行期间不执行请求或回复位置。恢复后必须先收到新的实测读数，期间命令不排队重放。关闭接入会释放 `12000`。

“手动”模式继续应答实测方位和仰角查询，但忽略所有定位请求；切回“跟踪”不重放期间收到的请求。主页面勾选“长径”后，将后续 GridTracker 方位目标反转 180°，再应用本地方位补偿和最短路径设置。例如无补偿时，目标 10° 按 190° 定位；仅仰角定位和位置查询保持原义。切换“长径”本身不发起定位。

## 通信约定

`GridTrackerController` 独占监听 `127.0.0.1:12000`，只接受本机报文，向固定的 `127.0.0.1:12001` 回复查询，不使用发送方的临时源端口。

旧版及新版的方位定位报文：

```xml
<PST><AZIMUTH>10</AZIMUTH></PST>
```

新版的双轴定位报文：

```xml
<PST><AZIMUTH>10</AZIMUTH><ELEVATION>45</ELEVATION></PST>
```

`AZIMUTH` 接受 0–360°，`ELEVATION` 接受 0–180°，有限小数四舍五入到整数度。仅方位请求发送 `Mddd`，保留仰角；含仰角时发送 `Wddd eee`。仅仰角请求在 `W` 中保持最近实测机械方位。方位目标扣除 GUI 本地方位补偿，再根据“最短路径旋转”偏好选择 0–450° 内的可达行程。例如无补偿时，实测 350°、请求 10°，开启最短路径发送 `M370`，关闭发送 `M010`。定位前实测方位必须在三秒内，过期或缺失时拒绝执行并提示。

GridTracker 2 将两个查询放在一个 UDP 数据报内：

```xml
<PST>AZ?</PST><PST>EL?</PST>
```

两轴分别回复 UTF-8 文本，例如 `AZ:10`、`EL:45`，每轴独立检查三秒有效期；缺失、过期或越界的轴不回复。方位回复为“实测机械角度 + GUI 补偿”归一化到 0–359°；仰角直接使用实测值。发送目标不会刷新或替代位置读数，单独的仰角反馈也不会刷新方位有效期。

仅支持上述 GridTracker 使用的定位及查询子集，不实现完整 PstRotator 协议。每个数据报上限 8192 字节，所有请求验证通过后才执行；损坏 XML、重复字段、未知元素、混合查询与定位、非有限数和越界角度均整体丢弃。

协议依据：[GridTracker 原版发送实现](https://github.com/mattmelling/gridtracker/blob/master/package.nw/lib/services/pstrotator.js)、[GridTracker 2 旋转器协议实现](https://gitlab.com/gridtracker.org/gridtracker2/-/blob/main/src/renderer/lib/services/rotor.js)、[GridTracker 2 配置说明](https://gitlab.com/gridtracker.org/gridtracker2/-/blob/main/docs/Rig-and-Rotor-Control.md#rotor-control)。
