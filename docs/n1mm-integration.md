# N1MM Logger+ 接入

本 GUI 可以代替同一电脑上的 N1MM Rotor，将 N1MM 的方位转向、停止请求发送到当前串口或 WebSocket 连接的 GS-232B 控制器，并向 N1MM 回传实测方位。当前支持一个设备；N1MM 只控制方位轴，停止命令使用 GS-232B `A`，不停止仰角轴。

## 设置

1. 关闭原配 N1MM Rotor。在 N1MM 的 Configurer → Antennas 中取消 **Start N1MM Rotor Program**。
2. 在 GUI 的“外部 → N1MM Logger+… → 配置名称…”中设置旋转器名称，默认 `GS232B`。名称区分大小写，必须与 N1MM 的 **Rotor Description** 完全一致。可使用 1–64 个字符，禁止逗号、`@`、制表符、换行和控制字符。保存后立即生效；取消不修改名称。
3. N1MM 的 Broadcast Data → Rotor 使用同机目标 `127.0.0.1:12040`。在 Antennas 中为相应天线填写 Rotor Description，需要显示角度时勾选 **Display Rotors Used By This Station**。
4. 在 GUI 连接控制器，再勾选“外部 → N1MM Logger+… → 启用”，将主页面模式切到“跟踪”以允许控制。启用状态和名称立即保存，重启后恢复；控制模式每次启动默认为“手动”。控制端口被其他程序占用时会提示原因，接入保持关闭。
5. N1MM 中使用 **Alt+J** 转到输入方位或目标呼号的方位，**Alt+L** 转到长路径方位，**Ctrl+Alt+J** 停止方位旋转。

GUI 自身的控制继续可用。“手动”模式继续向 N1MM 回报实测方位，但忽略它的转向及停止指令；切回“跟踪”不重放期间收到的指令。GUI 的“长径”复选框不影响 N1MM，长径和短径由 N1MM 的 Alt+L / Alt+J 选择。本功能限定同机通信，不提供远程地址或端口设置。虚拟串口透传模式和校准期间不会执行 N1MM 命令。

## 通信与角度约定

`N1mmController` 独占监听 `127.0.0.1:12040`，只接受本机报文；向 `127.0.0.1:13010` 发送 UTF-8 文本 `旋转器名称@实测方位十分之一度`，例如 `GS232B@3500` 表示 350.0°。回报单位与转向报文 `<goazi>` 的度数不同。名称精确匹配；逗号分隔的旋转器列表中包含本机名称时也可接收，用于 N1MM 的天线堆叠配置。

转向报文遵循官方格式，无 XML 声明同样可用：

```xml
<N1MMRotor>
  <rotor>GS232B</rotor>
  <goazi>55.0</goazi>
  <offset>0.0</offset>
  <bidirectional>0</bidirectional>
  <freqband>14</freqband>
</N1MMRotor>
```

停止报文的字段位于 `stop` 内：

```xml
<N1MMRotor>
  <stop>
    <rotor>GS232B</rotor>
    <freqband>14</freqband>
  </stop>
</N1MMRotor>
```

`goazi` 接受 0–450° 的有限小数。天线目标先减去报文 `offset`，再使用 GUI 已有的方位角补偿转换为设备行程，最终四舍五入为整数度。最短路径开启时从 0–450° 行程中选择最近的等价位置；关闭时优先采用直接可达的连续目标，否则采用较小的等价行程。`bidirectional=1` 时比较目标和相反方向，选择机械位移更短的一侧。报文 `offset` 不改写 GUI 的本地补偿设置。

位置反馈取控制器实际反馈的方位，加上 GUI 的本地补偿，再归一化到 0–359°，最后乘以 10 按十分之一度回报；不把目标角度当作当前位置，也不额外叠加 N1MM 的天线偏移。仰角单独反馈不会生成方位报告。有效方位读数只使用 3 秒；没有新鲜读数时拒绝转向，但在设备可控时仍可停止。断开、透传、校准期间的命令不会排队，恢复后必须先取得新的实测方位。关闭接入后释放控制端口。

协议来源：[N1MM Rotator UDP Packet Information](https://n1mmwp.hamdocs.com/setup/interfacing/#rotator-udp-packet-information)、[Antennas Tab](https://n1mmwp.hamdocs.com/setup/the-configurer/#antennas-tab)。
