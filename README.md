# The Law Of Prey

Jul-24-2026 起项目转为开源，此前成员职责解除。

# 平台支持
优先支持 FreeBSD 、 OpenBSD 、 Linux ，尽量支持 macOS 、 Windows 。因为弄不到证书，所以当前 Windows 上编译运行可能会遇到 SmartScreen 拦截，具体情况正等待测试。

## 项目简介
正在构建程序基础，游戏内容待定。

因游戏内容待定，所以项目名称为临时设置，未来可能会修改。

## 代码及文档规范
- 交流与注释中的路径表示约定：默认从项目根目录起始，例如根目录下的 .clang-format 表示为 .clang-format ，main.c 在 项目根目录/src/main.c ，所以表示为 src/main.c 。
- 代码中尽量不要使用 emoji ，以及 12345 类似的编号，以 ASCII 、汉字为主。
- C 代码使用 Google Style ，限宽 80 字符，配置已经写在了 .clang-format 里面。

# 美术素材规范
对于全屏图片，解析度不得高于 `1280*960` ，以免影响性能。

方屏可选择的解析度：
- `320*200` CGA
- `320*240` QVGA
- `512*384` 单色 Macintosh
- `640*480` VGA, MCGA
- `800*600` SVGA
- `1024*768` XGA
- `1152*864` XGA+
- `1280*960` SXGA-

宽屏可选择的解析度：
- `256*144`
- `480*272` WQVGA
- `640*360` nHD
- `854*480` FWVGA, 480p
- `960*540` qHD
- `1024*576` WSVGA
- `1280*720` 720p, HD

# Git LFS
本仓库已经启用了 Git LFS ，全新拉取仓库以后请执行 `git lfs pull` 拉取字体、图片等二进制文件。
