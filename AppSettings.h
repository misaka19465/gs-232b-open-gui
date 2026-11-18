#pragma once

#include <QSettings>
#include <QStringList>

/** @brief 应用本地偏好的统一入口，集中管理存储键、默认值和校验。 */
class AppSettings final
{
public:
    /** @brief 角度预设所属的旋转器轴。 */
    enum class Axis { Azimuth, Elevation };
    /** @brief 返回指定轴的预设数量：方位 10 个，仰角 6 个。 */
    static constexpr int presetCount(Axis axis)
    {
        return axis == Axis::Azimuth ? 10 : 6;
    }

    /** @brief 使用当前用户的系统配置存储，沿用现有角度预设的位置。 */
    AppSettings();
    /** @brief 使用指定的独立 INI 文件，不影响默认配置存储。 */
    explicit AppSettings(const QString &fileName);

    /** @brief 从扫描列表恢复已保存串口；不存在时返回首项，空列表返回空字符串，不改写存储。 */
    QString serialPort(const QStringList &availablePorts) const;
    /** @brief 去除首尾空白并立即保存串口号；空字符串清除选择。 */
    bool setSerialPort(const QString &port);
    /** @brief 读取连接波特率；仅支持 1200、2400、4800、9600，缺失或无效时返回 9600。 */
    int baudRate() const;
    /** @return 波特率受支持且立即同步到存储成功时返回 true；非法参数不写入。 */
    bool setBaudRate(int baudRate);
    /** @brief 读取网络连接地址；未设置时返回 192.168.1.50，已保存的空地址保持为空。 */
    QString connectionAddress() const;
    /** @brief 去除首尾空白并立即保存地址；连接时仍由传输后端校验地址。 */
    bool setConnectionAddress(const QString &address);
    /** @brief 读取网络连接模式；缺失或无效时默认使用串口。 */
    bool networkMode() const;
    /** @return 立即同步网络连接模式到存储是否成功。 */
    bool setNetworkMode(bool enabled);

    /** @brief 读取控制台显示偏好；缺失或无效时默认隐藏。 */
    bool consoleVisible() const;
    /** @return 立即同步控制台显示偏好到存储是否成功。 */
    bool setConsoleVisible(bool visible);

    /** @brief 读取窗口置顶偏好；缺失或无效时默认关闭。 */
    bool alwaysOnTop() const;
    /** @return 立即同步窗口置顶偏好到存储是否成功。 */
    bool setAlwaysOnTop(bool enabled);

    /** @brief 读取罗盘大圆图模式；缺失或无效时默认关闭。 */
    bool greatCircleMapEnabled() const;
    /** @return 立即同步大圆图模式偏好到存储是否成功。 */
    bool setGreatCircleMapEnabled(bool enabled);

    /** @brief 读取方位角补偿；完整支持 int 范围的正负整数度，缺失或无效时返回 0。 */
    int azimuthOffset() const;
    /** @return 立即同步方位角补偿到存储是否成功。 */
    bool setAzimuthOffset(int degrees);

    /** @brief 读取方位最短路径模式；缺失或无效时默认开启。 */
    bool shortestPathEnabled() const;
    /** @return 立即同步最短路径模式到存储是否成功。 */
    bool setShortestPathEnabled(bool enabled);

    /** @brief 读取同机 N1MM 接入偏好；缺失或无效时默认关闭。 */
    bool n1mmEnabled() const;
    /** @return 立即同步 N1MM 接入偏好到存储是否成功。 */
    bool setN1mmEnabled(bool enabled);
    /** @brief 读取 N1MM 旋转器名；缺失或无效时返回 GS232B。 */
    QString n1mmRotorName() const;
    /** @brief 去除首尾空白并立即保存合法旋转器名；非法值不写入。 */
    bool setN1mmRotorName(const QString &name);

    /** @brief 读取同机 GridTracker 接入偏好；缺失或无效时默认关闭。 */
    bool gridTrackerEnabled() const;
    /** @return 立即同步 GridTracker 接入偏好到存储是否成功。 */
    bool setGridTrackerEnabled(bool enabled);

    /** @brief 读取同机 WSJT-X/JTDX 接入偏好；缺失或无效时默认关闭。 */
    bool wsjtXEnabled() const;
    /** @return 立即同步 WSJT-X/JTDX 接入偏好到存储是否成功。 */
    bool setWsjtXEnabled(bool enabled);

    /** @brief 读取本站 Maidenhead 网格；返回大写，缺失或无效时返回空字符串。 */
    QString stationGrid() const;
    /**
     * @brief 保存本站网格，自动去除首尾空白并转为大写；空字符串表示清除。
     * @return 网格合法且立即同步到存储成功时返回 true；非法参数不写入。
     */
    bool setStationGrid(const QString &grid);
    /** @brief 校验 4、6、8 位 Maidenhead 网格，忽略大小写和首尾空白；空值无效。 */
    static bool isValidStationGrid(const QString &grid);

    /**
     * @brief 读取角度预设；缺失或无效时返回该位置的默认角度。
     * @param index 从 0 开始的预设位置；越界时返回 0。
     */
    int presetAngle(Axis axis, int index) const;
    /**
     * @brief 保存角度预设，方位范围为 0–450 度，仰角范围为 0–180 度。
     * @param index 从 0 开始的预设位置，必须小于 presetCount(axis)。
     * @return 参数合法且立即同步到存储成功时返回 true；非法参数不写入。
     */
    bool setPresetAngle(Axis axis, int index, int angle);

private:
    bool storeValue(const QString &key, const QVariant &value);

    QSettings m_settings;
};
