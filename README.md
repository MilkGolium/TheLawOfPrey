# The Law Of Prey

Jul-24-2026 起项目转为开源，此前成员职责解除。

`README.md` 未来会分为中英双份，现在只有中文，等待处理，doc/ 目录下面的文档也会分成中英两份。

# 平台支持
优先支持 FreeBSD 、 OpenBSD 、 Linux ，尽量支持 macOS 、 Windows 。因为弄不到证书，所以当前 Windows 上编译运行可能会遇到 SmartScreen 拦截，具体情况正等待测试。

## 项目简介
正在构建程序基础，游戏内容待定。

因游戏内容待定，所以项目名称为临时设置，未来可能会修改。

## 代码及文档规范
- 代码中的注释请使用 ASCII ，不然 tty 里面没法看注释。文档可以用不在 tty 里面看，但是代码不行。
- C 代码使用 Google Style ，限宽 80 字符。

# Git LFS
本仓库已经启用了 Git LFS ，拉取仓库以后请执行 `git lfs pull` 拉取字体等二进制文件。
