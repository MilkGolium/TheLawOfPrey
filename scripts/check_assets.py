#!/usr/bin/env python3
"""素材一致性核查：图标路径大小写、路径规范与悬空引用。

只用标准库。退出码：存在错误返回 1，只有警告返回 0。

关键约定：本脚本刻意不使用 os.path.exists 判断图标是否存在。macOS 等
大小写不敏感的文件系统上，os.path.exists('a/bone.png') 在磁盘只有
'Bone.png' 时同样返回 True，会漏报大小写不符这一类错误。改为先用
os.listdir() 取回目录下的真实文件名，再做逐字节精确比对。
"""

import csv
import os
import sys

# 与 scripts/build.sh 的 TOP_DIR 同义：脚本所在目录的上一级即仓库根。
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

ITEMS_CSV = os.path.join(ROOT, 'assets', 'databases', 'items.csv')
CONSUMABLES_CSV = os.path.join(ROOT, 'assets', 'databases', 'consumables.csv')
ICON_DIR = os.path.join(ROOT, 'assets', 'sprites', 'item_icons')
ICON_PREFIX = 'assets' + os.sep + 'sprites' + os.sep
PNG_SUFFIX = '.png'

# 每类明细最多列出的条数，其余只给计数。
DETAIL_LIMIT = 10


def load_table(path, label, required):
    """读 CSV，返回 (行列表, 错误信息列表)。缺列属于错误，退出码由调用方决定。"""
    errors = []
    if not os.path.isfile(path):
        return [], ['文件不存在：%s' % label]

    with open(path, newline='', encoding='utf-8-sig') as handle:
        reader = csv.DictReader(handle)
        if reader.fieldnames is None:
            return [], ['文件为空：%s' % label]
        missing = [c for c in required if c not in reader.fieldnames]
        if missing:
            return [], ['%s 缺少字段：%s' % (label, '、'.join(missing))]
        rows = list(reader)

    return rows, errors


def list_icon_dir():
    """用 os.listdir 取真实文件名。返回 (精确名集合, 折叠大小写后的映射, 冲突列表)。"""
    if not os.path.isdir(ICON_DIR):
        return None, {}, []

    exact = set()
    folded = {}
    conflicts = []
    for name in os.listdir(ICON_DIR):
        if name.startswith('.'):
            continue
        exact.add(name)
        key = name.lower()
        if key in folded:
            conflicts.append((folded[key], name))
        else:
            folded[key] = name
    return exact, folded, conflicts


def show(title, count, rows, render, unit='条'):
    """打印某一类的计数与前若干条明细。"""
    print('  %s：%d %s' % (title, count, unit))
    if count == 0:
        return
    shown = rows[:DETAIL_LIMIT]
    for row in shown:
        print('    - %s' % render(row))
    if count > len(shown):
        print('    ...另有 %d 条未列出' % (count - len(shown)))


def main():
    print('素材一致性核查')
    print('仓库根目录：%s' % ROOT)
    print()

    items, item_errors = load_table(
        ITEMS_CSV, 'assets/databases/items.csv', ['id', 'texture'])
    consumables, consumable_errors = load_table(
        CONSUMABLES_CSV, 'assets/databases/consumables.csv', ['item_id'])

    hard_errors = list(item_errors) + list(consumable_errors)

    # 按 texture 路径归并，重复引用同一图标的行不重复计入明细。
    order = []
    usage = {}
    for row in items:
        texture = (row.get('texture') or '').strip()
        if texture not in usage:
            usage[texture] = []
            order.append(texture)
        usage[texture].append((row.get('id') or '').strip())

    print('[数据表]')
    print('  assets/databases/items.csv：%d 条记录，%d 个不同 texture 路径'
          % (len(items), len(order)))
    print('  assets/databases/consumables.csv：%d 条记录'
          % len(consumables))
    if hard_errors:
        print('  数据表读取错误：')
        for message in hard_errors:
            print('    - %s' % message)
        print()
        print('结论：数据表不可读，无法继续核查。')
        return 1
    print()

    def render_texture(texture):
        ids = '、'.join(i for i in usage[texture] if i) or '?'
        return '%s（item id=%s）' % (texture, ids)

    # 路径规范：仓库根相对、位于 assets/sprites/ 下、扩展名为小写 .png。
    bad_abs = [t for t in order if os.path.isabs(t) or t.startswith('~')]
    bad_escape = [t for t in order
                  if '..' in os.path.normpath(t).split(os.sep)]
    bad_prefix = [t for t in order
                  if not t.replace('/', os.sep).startswith(ICON_PREFIX)]
    bad_ext = [t for t in order if not t.endswith(PNG_SUFFIX)]

    print('[检查一] texture 路径规范（相对仓库根、assets/sprites/ 下、小写 .png）')
    show('非相对路径（错误）', len(bad_abs), bad_abs, render_texture)
    show('含 .. 越界（错误）', len(bad_escape), bad_escape, render_texture)
    show('不在 assets/sprites/ 下（错误）', len(bad_prefix), bad_prefix,
         render_texture)
    show('扩展名非小写 .png（错误）', len(bad_ext), bad_ext, render_texture)
    norm_errors = len(bad_abs) + len(bad_escape) + len(bad_prefix) + len(bad_ext)
    print('  小计：%d 条错误' % norm_errors)
    print()

    # 存在性：真实文件名精确比对，见模块 docstring 的说明。
    exact, folded, conflicts = list_icon_dir()
    matched, cased, absent = [], [], []
    if exact is None:
        hard_errors.append('图标目录不存在：assets/sprites/item_icons')
        matched, cased, absent = [], [], order
    else:
        for texture in order:
            base = os.path.basename(texture.replace('/', os.sep))
            if base in exact:
                matched.append(texture)
            elif base.lower() in folded:
                cased.append((texture, folded[base.lower()]))
            else:
                absent.append(texture)

    print('[检查二] 图标文件存在性（大小写敏感）')
    if exact is None:
        print('  图标目录不存在，全部 %d 个路径无法核查' % len(order))
    else:
        for old, new in conflicts:
            print('    - 错误：磁盘同时存在 %s 与 %s，仅大小写不同' % (old, new))
        print('  磁盘实际文件数：%d' % len(exact))
        show('完全匹配', len(matched), matched, render_texture)
        show('仅大小写不符（错误）', len(cased), cased,
             lambda p: 'CSV 写 %s，磁盘实际为 %s（%s）'
                       % (p[0], p[1], render_texture(p[0])))
        show('文件不存在（警告，美术未交付）', len(absent), absent, render_texture)
    case_errors = len(cased) + len(conflicts)
    print('  小计：%d 条错误（大小写不符或磁盘重名），%d 条警告（文件不存在）'
          % (case_errors, len(absent)))
    print()

    # consumables.csv 的 item_id 必须能在 items.csv 的 id 中找到。
    item_ids = {(row.get('id') or '').strip() for row in items}
    dangling = []
    seen_ids = set()
    for row in consumables:
        item_id = (row.get('item_id') or '').strip()
        if item_id in seen_ids:
            continue
        seen_ids.add(item_id)
        if item_id not in item_ids:
            dangling.append(item_id)

    print('[检查三] consumables.csv 悬空引用')
    show('item_id 在 items.csv 中找不到（错误）', len(dangling), dangling,
         lambda i: 'item_id=%s' % i)
    print()

    total_errors = norm_errors + case_errors + len(dangling) + len(hard_errors)
    total_warnings = len(absent)

    print('结论：错误 %d 条，警告 %d 条' % (total_errors, total_warnings))
    if total_errors:
        print('有错误，退出码 1。')
        return 1
    print('无错误（警告不阻断），退出码 0。')
    return 0


if __name__ == '__main__':
    sys.exit(main())