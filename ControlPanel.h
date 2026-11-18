#pragma once

#include "AzimuthScale.h"

#include <QWidget>
#include <QString>

class AngleReading;
class AnglePresets;
class Compass;
class MotionButtons;
class QSlider;
class QTimer;
class QWidget;

/** @brief 方位与俯仰读数、选角控件及十个方位、六个仰角预设的视图。 */
class ControlPanel final : public QWidget
{
    Q_OBJECT

public:
    /** @brief 创建零读数、载入已保存预设且命令控件禁用的面板。 */
    explicit ControlPanel(QWidget *parent = nullptr);
    /** @brief 按设备确认的型号显示仰角区；隐藏时取消待提交的仰角操作。 */
    void setElevationSupported(bool supported);
    /** @brief 按实际连接、透传及校准状态统一切换命令控件。 */
    void setCommandEnabled(bool enabled);
    /** @brief 取消待发滑条命令并停止正在点动的轴；定位、停止或断开前调用。 */
    void cancelPendingMotion();
    /** @return 是否允许桥接应用控制设备；手动模式仍保留位置反馈。 */
    bool trackingEnabled() const { return m_trackingEnabled; }
    /** @return 本地网格/DXCC 及 GridTracker 目标是否使用长径；直接角度及实测值不变。 */
    bool longPathEnabled() const { return m_longPathEnabled; }
    /** @brief 更新设备实测角度，不触发滑条定位请求。 */
    void setPosition(int azimuth, int elevation);
    /** @brief 更新显示、数字输入和预设的方位补偿；保留设备读数，不发送命令。 */
    void setAzimuthOffset(int degrees);
    /** @brief 切换方位取模显示与最近可达位置选解；取消待提交输入，不发送旋转命令。 */
    void setShortestPathEnabled(bool enabled);
    /** @brief 切换方位罗盘的大圆图显示，不影响设备连接或角度读数。 */
    void setGreatCircleMapEnabled(bool enabled);
    /** @brief 更新大圆图与目标方位计算使用的本站网格，后续提交立即使用新值。 */
    void setStationGrid(const QString &grid);
    /** @brief 更新大圆图参考波瓣的天线增益；0 dBi 时隐藏波瓣。 */
    void setAntennaGain(double gain);
    /** @return 最近一次设备方位读数，不包含补偿；供分轴反馈及 W 命令保留机械行程。 */
    int azimuth() const;
    /** @return 当前界面保留的仰角读数。 */
    int elevation() const;

signals:
    /** @brief 用户通过控制控件请求发送一条 GS-232B 命令。 */
    void commandRequested(const QString &command);
    /** @brief 大圆图生成或缓存失败，供主窗口显示状态提示。 */
    void mapError(const QString &message);
    /** @brief 角度或目标输入无效，未发送旋转命令，供主窗口展示错误。 */
    void inputError(const QString &message);
    /** @brief 用户切换桥接控制权限；宿主应立即同步到所有桥接应用。 */
    void trackingEnabledChanged(bool enabled);
    /** @brief 用户切换网格/DXCC 及 GridTracker 目标的长径选路；N1MM 不使用该设置。 */
    void longPathEnabledChanged(bool enabled);

private:
    void setupMotionButtons();
    void requestAzimuth(int mechanical);
    void requestElevation(int angle);
    void updateAzimuthInput();

    Compass *m_compass;
    QWidget *m_elevationControls;
    MotionButtons *m_azimuthMotion;
    MotionButtons *m_elevationMotion;
    AngleReading *m_azimuth;
    AngleReading *m_elevation;
    QSlider *m_slider;
    QTimer *m_elevationCommandTimer;
    AnglePresets *m_azimuthPresets;
    AnglePresets *m_elevationPresets;
    QString m_stationGrid;
    AzimuthScale m_azimuthScale;
    int m_azimuthValue = 0;
    int m_elevationValue = 0;
    bool m_shortestPathEnabled = true;
    bool m_commandEnabled = false;
    bool m_elevationSupported = true;
    bool m_trackingEnabled = false;
    bool m_longPathEnabled = false;
};
