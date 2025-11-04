# 📦QR Code Backup Tool

[![Version](https://img.shields.io/github/release/little-hanabi/qrb?include_prereleases)](https://github.com/little-hanabi/qrb/releases)
[![License](https://img.shields.io/github/license/little-hanabi/qrb)](LICENSE)
[![CI](https://github.com/little-hanabi/qrb/actions/workflows/release.yml/badge.svg)](https://github.com/little-hanabi/qrb/actions/workflows/release.yml)

> [!IMPORTANT]
> - 此项目应当被视为一个实用玩具而不是一个专业工具，它没有完备的测试用例、专业的文档或规范的错误处理等等。
> - 开发者不做出任何保证，请自行承担任何使用后果。

这是一个使用C++编写的利用二维码来实现自动化备份和恢复单个二进制文件的命令行工具。

## ✨特点

- 编码与解码一体的单文件、无依赖、便携式命令行程序
- 解码与奇偶校验恢复自动化，无需手动标记或设置参数，并且可以乱序识别
- 紧凑的数据组织方式，含文件名和编码时间元数据
- 可自定义页面二维码布局、二维码版本和二维码纠错等级参数
- 可自定义奇偶校验冗余等级，少量二维码整块缺失时可以恢复

## 📥安装

| 平台    | 安装方式           |
| :------ | :---------------- |
| Windows | [直接下载](https://github.com/little-hanabi/qrb/releases) |
| Linux   | 参阅 [构建](#%EF%B8%8F构建) |
| Mac OS  | 参阅 [构建](#%EF%B8%8F构建) |

## 🚀使用

### 编码文件

```
qrb -e <input_file> <output_dir> <col> <row> <qr_version> <qr_ecc> [<file_ecc>]
```

- `<input_file>` 表示待编码的文件
- `<output_dir>` 表示编码结果保存文件夹，请确保拥有写权限，且文件夹为空或不存在
- `<col>` 为整数，大于0，表示每页有几列二维码
- `<row>` 为整数，大于0，表示每页有几行二维码
- `<qr_version>` 为整数，范围`1-40`，对应二维码的版本`1-40`
- `<qr_ecc>` 为整数，范围`0-3`，对应二维码的纠错等级`L-H`
- `<file_ecc>` 为整数，范围`0-6`，表示奇偶校验冗余等级，`0`表示不使用奇偶校验，**等级越高则冗余度越低**

> [!IMPORTANT]
> - 程序不是为了高密度编码大文件而设计，建议只用于备份小文件，例如私钥
> - 请不要人为修改编码产生的图像，二维码的形状、间距或排列发生改变时，可能导致无法完全解码
> - 请不要混合`<output_dir>`中的`file`文件夹与`ecc`文件夹中的图像

> [!NOTE]
> - 以A4纸大小、版本19二维码为参考，建议每页二维码排列不超过6列9行，超过该密度的排列可能难以识别
> - 当一页图中的二维码数量超过一定值时，解码时匹配特征的耗时会大幅增加，因此单页图像二维码数量不应设置过大
> - 可编码的文件大小根据设置的二维码版本和纠错等级不同，存在动态上限，但常规用途通常不会触及上限
> - 可编码的文件名长度上限为`255`字节
> - 可在文件名中包含哈希算法校验值等附加信息，供外部处理。

### 解码文件

```
qrb -d <input_dir> <output_dir> [<ecc_dir>]
```

- `<input_dir>` 表示文件内容图像所在文件夹，不会递归处理子文件夹，自动构建的版本仅支持`PNG`、`JPG`和`BMP`格式的图像
- `<output_dir>` 表示解码结果保存文件夹，请确保拥有写权限，且文件夹为空或不存在
- `<ecc_dir>` 表示奇偶校验内容图像所在文件夹，不会递归处理子文件夹，自动构建的版本仅支持`PNG`、`JPG`和`BMP`格式的图像

> [!IMPORTANT]
> - 请确保每张图像只包含一页原始编码图像，并且无明显旋转和透视形变
> - 请确保`<input_dir>`和`<ecc_dir>`内的所有图像中的所有二维码只对应相同二维码版本和纠错等级编码的同一个文件
> - 请确保`<input_dir>`和`<ecc_dir>`中的图像与编码时对应，没有发生混合
> - 允许出现重复的二维码。例如，当原始页面图像中的二维码无法识别时，可直接在目录中追加修复后的页面图像，程序会自动处理重复

> [!NOTE]
> - 请选择边缘畸变小的镜头拍摄
> - 请确保图像光照均匀，无过曝和欠曝
> - 请确保二维码清晰且无破损
> - 请确保二维码占据图像的主要区域，否则会耗费大量时间重复尝试解码无关区域
> - 请确保二维码模块边长不少于`2px`，超小二维码或超大像素图像可能难以识别
> - 当因缺块等原因导致解码未成功完成时，`<output_dir>`中会以`file.bin`文件名保存已成功解码和已通过奇偶校验恢复的内容
> - 程序不会额外校验文件整体内容的完整性
> - 请确保终端字符集为`UTF-8`，否则元数据中的文件名可能无法正常显示，但通常不影响所保存的文件名

### 异常处理

- 当输入参数个数或指向错误，或者使用了不存在的工作模式时，会输出帮助信息
- 当存在可忽略的错误或异常，程序会忽略并继续进行剩余处理，不输出任何提示
- 当存在未考虑的错误或异常，自动构建的版本通常不输出任何内容，并直接退出
- 未按输入输出设计要求传递参数和待处理内容，可能会产生不可预测的结果

## 🛠️构建

### 要求

- CMake
- Ninja或其他CMake支持的构建系统
- 支持C++ 20的C++编译器
- [OpenCV](https://github.com/opencv/opencv/)

> [!TIP]
> - Windows平台MSVC环境未经过充分测试，建议使用MinGW环境编译

### 步骤

1. 下载代码

```bash
git clone https://github.com/little-hanabi/qrb.git
cd qrb
```

2. 按需修改`CMakeLists.txt`
3. 创建

```bash
mkdir build
cmake -B "./build" -S "." -G "Ninja" -D CMAKE_BUILD_TYPE=Release
```

> 如果你希望手动指定`OpenCV`的位置，可以执行：
> ```bash
> cmake -B "./build" -S "." -G "Ninja" -D CMAKE_BUILD_TYPE=Release -D OpenCV_DIR="<your/opencv/path>"
> ```

4. 构建

```bash
cmake --build "./build" --config Release
```

## 🤝贡献

> [!IMPORTANT]
> - 本项目不是为了作为库而设计，因此本项目**不会接受**使用类重构
> - 内嵌的`zxing-cpp`不是为了通用识别，它的识别流程经过了简化和修改，因此本项目**不会接受**直接使用`zxing-cpp`原仓库

### 新功能

为了保持本项目的简单和纯粹，原则上本项目**不会增加**以下功能：

- GUI
- PDF
- 使用打印机、摄像头或扫描仪
- 压缩与解压缩
- 加密与解密
- 多线程
- 流式传输

### 修复与改进

欢迎提出相关的issue和pr来改进本项目稳定性和性能

> [!NOTE]
> 目前存在如下事项待解决：
> - [ ] 开发者精力有限，尽管本项目理论上可以跨平台，但本文缺乏Linux平台和Mac OS平台的具体构建指导，并且`CMakeLists.txt`和自动构建目前不支持构建这些平台的产物
> - [ ] 当一页图像中存在多个二维码时，目前的开源二维码识别项目（我能找到的）总是会有遗漏。因此，本项目使用了比较取巧的方案来实现自动化解码图像中的全部二维码。如果您有更优雅且高性能、高鲁棒的解决方案，欢迎提出
> - [ ] 无论使用何种二值化方法，尽管未对`zxing-cpp`算法部分进行修改，其默认情况下的识别率与原版不一致。由于上一条所述自动化方案的存在，该问题对项目功能的影响较小，但仍希望找出原因

## 🔗相关项目

- [PaperBack](https://ollydbg.de/Paperbak/)
- [paperback](https://github.com/cyphar/paperback/)
- [qr-backup](https://github.com/za3k/qr-backup/)
- [cimbar](https://github.com/sz3/libcimbar/)
- [qrs](https://github.com/qifi-dev/qrs/)

## 📜许可

[Apache 2.0](LICENSE) © little-hanabi

本项目还使用了以下开源项目：

- [OpenCV](https://github.com/opencv/opencv/)
    - Apache 2.0许可
    - 直接使用，未修改
- [zxing-cpp](https://github.com/zxing-cpp/zxing-cpp/)
    - Apache 2.0许可
    - 存在修改，不兼容官方仓库