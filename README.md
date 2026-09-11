# fan-minitool

fan-minitool 是一个用 C 编写的 Windows 服务。它读取 NVIDIA GPU 核心温度，并通过 Libre Hardware Monitor 的本机 REST 接口控制主板 PWM 风扇。

本机目标是 Libre Hardware Monitor 中的 ITE IT8665E / Fans #4。示例配置使用 /lpc/it8665e/0/control/3，但 LHM 的 SensorId 会随主板检测结果变化，启动服务前请打开 http://127.0.0.1:8085/data.json 确认它。

温度曲线默认如下：

- 低于 50 °C：PWM 0%
- 50 °C：PWM 30%，风扇启动
- 50 至 85 °C：从 30% 线性增加到 100%
- 85 °C 及以上：PWM 100%

## 依赖

1. Windows 10 或更新版本，64 位 NVIDIA 显卡驱动。
2. Libre Hardware Monitor（https://github.com/LibreHardwareMonitor/LibreHardwareMonitor），以管理员权限运行，并启用 Remote Web Server。其默认地址为 http://127.0.0.1:8085。主板 PWM 控制所需的硬件访问驱动也必须按 LHM 的说明安装。
3. Visual Studio 的 MSVC x64 C 工具链。

服务通过运行时加载 NVIDIA 驱动中的 nvapi64.dll 读取温度，不会把 NVIDIA SDK 二进制文件提交到仓库。主板 Super I/O 由 LHM 访问，避免在本项目中重复实现各家主板寄存器和内核驱动。

## 构建

在 Windows 的 Visual Studio Developer Command Prompt 中运行：

    build-msvc.bat

也可以在 WSL 中通过互操作运行：

    /mnt/c/Windows/System32/cmd.exe /c "$(wslpath -w "$PWD")\build-msvc.bat"

输出在 build\fan-minitool.exe，脚本也会复制示例配置为 build\fan-minitool.ini。

## 配置与运行

将 fan-minitool.ini.example 复制为 fan-minitool.ini，放在 exe 同一目录。pwm_sensor_id 是 LHM 的 Control SensorId；对于本机 IT8665E Fans #4 通常是：

    pwm_sensor_id=/lpc/it8665e/0/control/3

先用前台模式验证配置和 LHM 接口：

    build\fan-minitool.exe --console

确认日志中的 GPU 温度和 PWM 值正确后，以管理员权限安装服务：

    build\fan-minitool.exe install
    sc start fan-minitool

卸载：

    sc stop fan-minitool
    build\fan-minitool.exe uninstall

日志文件为 exe 旁的 fan-minitool.log。服务停止时会向 LHM 发送 value=null，释放软件 PWM 控制，让主板默认风扇曲线接管。

## 许可证

本项目以 GNU GPL v3 或更高版本发布，详见 COPYING。
