#!/usr/bin/env python3
"""Asset consistency check: icon path case, path rules, dangling references.

Standard library only. Exit code: 1 when errors exist, 0 when only warnings do.

Key convention: this script deliberately avoids os.path.exists to decide whether
an icon exists. On a case-insensitive filesystem such as macOS,
os.path.exists('a/bone.png') also returns True when the disk only holds
'Bone.png', which would hide exactly the case-mismatch class of error. Instead
the real file names are read with os.listdir() and compared byte for byte.
"""

import csv
import os
import sys

# Same meaning as TOP_DIR in scripts/build.sh: the parent of this script is the repo root.
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

ITEMS_CSV = os.path.join(ROOT, 'assets', 'databases', 'items.csv')
CONSUMABLES_CSV = os.path.join(ROOT, 'assets', 'databases', 'consumables.csv')
ICON_DIR = os.path.join(ROOT, 'assets', 'sprites', 'item_icons')
ICON_PREFIX = 'assets' + os.sep + 'sprites' + os.sep
PNG_SUFFIX = '.png'

# At most this many rows are listed per category; the rest are only counted.
DETAIL_LIMIT = 10


def load_table(path, label, required):
    """Read a CSV, return (rows, errors). Missing columns are errors; the caller decides."""
    errors = []
    if not os.path.isfile(path):
        return [], ['file not found: %s' % label]

    with open(path, newline='', encoding='utf-8-sig') as handle:
        reader = csv.DictReader(handle)
        if reader.fieldnames is None:
            return [], ['file is empty: %s' % label]
        missing = [c for c in required if c not in reader.fieldnames]
        if missing:
            return [], ['%s is missing column(s): %s' % (label, ', '.join(missing))]
        rows = list(reader)

    return rows, errors


def list_icon_dir():
    """Read real file names with os.listdir. Returns (exact set, folded map, conflicts)."""
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


def show(title, count, rows, render, unit='entries'):
    """Print one category's count and its first few rows."""
    print('  %s: %d %s' % (title, count, unit))
    if count == 0:
        return
    shown = rows[:DETAIL_LIMIT]
    for row in shown:
        print('    - %s' % render(row))
    if count > len(shown):
        print('    ...%d more not listed' % (count - len(shown)))


def main():
    print('Asset consistency check')
    print('repository root: %s' % ROOT)
    print()

    items, item_errors = load_table(
        ITEMS_CSV, 'assets/databases/items.csv', ['id', 'texture'])
    consumables, consumable_errors = load_table(
        CONSUMABLES_CSV, 'assets/databases/consumables.csv', ['item_id'])

    hard_errors = list(item_errors) + list(consumable_errors)

    # Group by texture path; rows reusing the same icon are not listed twice.
    order = []
    usage = {}
    for row in items:
        texture = (row.get('texture') or '').strip()
        if texture not in usage:
            usage[texture] = []
            order.append(texture)
        usage[texture].append((row.get('id') or '').strip())

    print('[tables]')
    print('  assets/databases/items.csv: %d rows, %d distinct texture paths'
          % (len(items), len(order)))
    print('  assets/databases/consumables.csv: %d rows'
          % len(consumables))
    if hard_errors:
        print('  data table errors:')
        for message in hard_errors:
            print('    - %s' % message)
        print()
        print('Result: data tables are unreadable, cannot continue.')
        return 1
    print()

    def render_texture(texture):
        ids = ', '.join(i for i in usage[texture] if i) or '?'
        return '%s (item id=%s)' % (texture, ids)

    # Path rules: relative to the repo root, under assets/sprites/, lowercase .png.
    bad_abs = [t for t in order if os.path.isabs(t) or t.startswith('~')]
    bad_escape = [t for t in order
                  if '..' in os.path.normpath(t).split(os.sep)]
    bad_prefix = [t for t in order
                  if not t.replace('/', os.sep).startswith(ICON_PREFIX)]
    bad_ext = [t for t in order if not t.endswith(PNG_SUFFIX)]

    print('[texture paths] texture path rules (repo-relative, under assets/sprites/, lowercase .png)')
    show('not a relative path (error)', len(bad_abs), bad_abs, render_texture)
    show('escapes with .. (error)', len(bad_escape), bad_escape, render_texture)
    show('outside assets/sprites/ (error)', len(bad_prefix), bad_prefix,
         render_texture)
    show('extension is not lowercase .png (error)', len(bad_ext), bad_ext, render_texture)
    norm_errors = len(bad_abs) + len(bad_escape) + len(bad_prefix) + len(bad_ext)
    print('  subtotal: %d error(s)' % norm_errors)
    print()

    # Existence: byte-exact comparison against real file names, see the module docstring.
    exact, folded, conflicts = list_icon_dir()
    matched, cased, absent = [], [], []
    if exact is None:
        hard_errors.append('icon directory not found: assets/sprites/item_icons')
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

    print('[icon existence] icon file existence (case sensitive)')
    if exact is None:
        print('  icon directory missing; none of the %d paths can be checked' % len(order))
    else:
        for old, new in conflicts:
            print('    - error: %s and %s both exist on disk, differing only in case' % (old, new))
        print('  files actually on disk: %d' % len(exact))
        show('exact match', len(matched), matched, render_texture)
        show('case mismatch only (error)', len(cased), cased,
             lambda p: 'CSV says %s, disk has %s (%s)'
                       % (p[0], p[1], render_texture(p[0])))
        show('missing on disk (warning, art not delivered yet)', len(absent), absent, render_texture)
    case_errors = len(cased) + len(conflicts)
    print('  subtotal: %d error(s) (case mismatch or duplicate on disk), %d warning(s) (missing file)'
          % (case_errors, len(absent)))
    print()

    # Every item_id in consumables.csv must exist as an id in items.csv.
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

    print('[consumables] consumables.csv dangling references')
    show('item_id not found in items.csv (error)', len(dangling), dangling,
         lambda i: 'item_id=%s' % i)
    print()

    total_errors = norm_errors + case_errors + len(dangling) + len(hard_errors)
    total_warnings = len(absent)

    print('Result: %d error(s), %d warning(s)' % (total_errors, total_warnings))
    if total_errors:
        print('Errors present, exit code 1.')
        return 1
    print('No errors (warnings do not block), exit code 0.')
    return 0


if __name__ == '__main__':
    sys.exit(main())