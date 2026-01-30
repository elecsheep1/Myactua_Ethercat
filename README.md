# MYACTUA_EtherCat

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

## 项目简介

MYACTUA_EtherCat 是一个基于 **EtherCAT** 的脉塔多电机控制库，适用于实时控制和科研工程场景。  
主要特点：

- 支持 **IGH EtherCAT Adapter**（不支持SOEM）
- 多电机管理与控制
- 支持位置、速度、力矩等控制模式
- 提供示例程序 `simple_test` 和 `debug_tool` 快速上手

---

## 目录结构

~~~
MYACTUA_EtherCat/
 ├── build/                  # 构建目录（编译产物）
 ├── include/myactua/        # 库头文件
 ├── src/                    # 源码文件
 ├── examples/               # 示例程序
 ├── CMakeLists.txt          # 构建脚本
 └── README.md               # 本文件
~~~

---

## 依赖

- C++17
- CMake >= 3.8
- Linux 系统（推荐 Ubuntu 20.04 / 22.04）
- IGH EtherCAT Adapter（路径：`../../ethercat`）
- pthread、rt 库（实时控制需要）

---

## 构建方法

1. 创建构建目录并进入：

```bash
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

## 使用示例

### 运行示例程序

```
# 简单测试
./build/simple_test

# 调试工具
./build/debug_tool
```

### 在自己项目中引用库

```
#include "myactua/motor_control.hpp"
#include "myactua/EthercatAdapterIGH.hpp" 
#include <iostream>  
#include <memory>    
#include <vector>
#include <unistd.h>

int main() {
    // 实例化
    auto adapter = std::make_shared<myactua::EthercatAdapterIGH>();
    myactua::MYACTUA controller(adapter, 1);
    std::cout << "[1/3] 正在初始化网卡: " << std::endl;
    // 连接与配置模式（填入自己使用的网卡）
    if (!controller.connect("enp12s0")) {
        std::cerr << "[错误] 无法连接到 EtherCAT 网络！" << std::endl;
        return -1;
    }
    std::cout << "[2/3] 连接成功，正在设置电机1 CSV 模式..." << std::endl;
    controller.setMode(myactua::ControlMode::CSV, 0);
    controller.setMode(myactua::ControlMode::CSV, 1);
    std::vector<double> setpoints = { 100.0 , -100.0};

    std::cout << "[3/3] 进入控制循环" << std::endl;
    std::cout << "按 Ctrl+C 停止运行" << std::endl;    
    // 控制循环
    while (true) {
        controller.update(setpoints);
        usleep(1000); // 1ms 业务周期
    }
    return 0;
}

```