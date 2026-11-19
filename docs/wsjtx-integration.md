# WSJT-X / JTDX 接入

GUI 可监听 WSJT-X 或 JTDX 发出的共享 UDP `Status` 消息。收到首个非空 DX 目标，或后续 `DX Call`/`DX Grid` 与前值不同时，GUI 将旋转器指向该电台；不根据每条解码或发射轮次猜测目标。目标选择可由双击解码列表中的呼号完成，也适用于其他会更新 DX Call 的操作。

## 配置和使用

1. 在 GUI 的“外部 → WSJT-X / JTDX… → 启用”开启监听。此开关默认关闭，可保存并在重启后恢复。
2. 在 WSJT-X 或 JTDX 的 Reporting 设置中，将 **UDP Server** 设为 `127.0.0.1`，端口设为 `2237`。
3. 连接旋转器并切换到“跟踪”模式。旋转器断开、校准、透传或跟踪关闭期间收到的状态只更新当前目标，不会延迟补发转向命令。
4. 在 GUI 设置“我的网格…”。如果该项为空，控制器会回退使用 Status 消息中的本站网格。定位还需要最近三秒内的实测方位。

监听启用后，如果首个 Status 已有非空 DX Call，也会按当前选中目标转向；之后只在 DX Call 或 DX Grid 变化时请求转向。若目标就是本站呼号，则忽略。

若 UDP 2237 已被其他程序独占，GUI 会提示端口占用并保持关闭。监听只绑定 `127.0.0.1`，拒绝非本机数据报。设置另一端口无法直接用于当前固定监听。

## 定位和控制

有效 DX Grid 优先于 DX Call；没有网格时按呼号所属 DXCC 的代表坐标定位。命令沿用 GUI 的方位补偿、最短路径和长径设置，并发送 GS-232B `Mddd` 方位命令。是否轮到本站发报不影响定位：选中目标后即可提前转向，无需解码报文角色或推断 HISCALL 发射时机。

## 协议范围

监听器接受 WSJT-X 与 JTDX 共同的 schema 2/3 `Status` 字段，包括本站呼号/网格及 DX Call/Grid。只解析到共同字段为止；其他 `Status` 字段、`Decode`、心跳、QSO 记录、WSPR、控制请求和厂商扩展均不使用。额外尾随字段忽略，坏报文拒绝。

协议依据：[WSJT-X UDP 消息格式](https://github.com/saitohirga/WSJT-X/blob/master/Network/NetworkMessage.hpp)、[JTDX UDP 消息格式](https://github.com/jtdx-project/jtdx/blob/master/NetworkMessage.hpp)及 [WSJT-X 操作说明](https://wsjt.sourceforge.io/wsjtx-main_en.html#MAKING_QSOS)。
