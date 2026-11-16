#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
用工程里的真实字模渲染 OLED 菜单页面，生成 README 用的 PNG。

不依赖任何第三方库：字模直接从 OLED_Fonts.c 解析，PNG 用 zlib + struct 手写。
布局参数（行高、文字坐标、滚动条、光标）与 OLED_UI.c 保持一致，
所以出来的图和实物屏幕逐像素相同。

用法（在仓库根目录或任意位置都可以）：
    python tools/render_oled.py                 # 全部页面，4 倍放大，输出 docs/images
    python tools/render_oled.py --scale 6       # 放大 6 倍
    python tools/render_oled.py --only main,pwm # 只重出指定页面
    python tools/render_oled.py --out D:/tmp    # 换个输出目录
    python tools/render_oled.py --list          # 列出所有页面名
"""
import argparse
import os
import re
import struct
import sys
import zlib
from pathlib import Path

# 仓库根目录：本文件在 tools/ 下
ROOT = Path(__file__).resolve().parents[1]
FONT_SRC = ROOT / "Core" / "Usercode" / "OLEDUI" / "OLED_Fonts.c"

W, H = 128, 32          # SSD1306 分辨率
PAD = 10                # 图片四周的透明留白
BG = (7, 9, 14)         # 面板底色
FG = (232, 242, 255)    # 点亮像素
BORDER = (52, 56, 66)   # 面板边框


# ---------------------------------------------------------------- 字模解析
def parse_hex_array(text, header):
    """取出 `const uint8_t XXX[...] = { ... };` 里的所有 0xNN"""
    i = text.index(header)
    j = text.index("{", i)
    k = text.index("};", j)
    return [int(x, 16) for x in re.findall(r"0[xX]([0-9A-Fa-f]{1,2})", text[j + 1:k])]


def parse_chinese8x8(text):
    """OLED_CF8x8：{ {"□"}, {8 字节} }"""
    i = text.index("const ChineseCell8x8_t OLED_CF8x8[]")
    j = text.index("{", i)
    k = text.index("};", j)
    table = {}
    for m in re.finditer(r'\{\{\s*"([^"]*)"\s*\}\s*,\s*\{([^}]*)\}', text[j + 1:k]):
        data = [int(x, 16) for x in re.findall(r"0[xX]([0-9A-Fa-f]{1,2})", m.group(2))]
        if len(data) == 8:
            table[m.group(1)] = data
    return table


def load_fonts():
    if not FONT_SRC.exists():
        sys.exit("找不到字模文件：%s" % FONT_SRC)
    # 源文件是 GBK，注释里有中文，解不开的字节直接替换掉，不影响数字
    src = FONT_SRC.read_bytes().decode("gbk", "replace")

    f6x8 = parse_hex_array(src, "const uint8_t OLED_F6x8[][6]")
    if len(f6x8) != 95 * 6:
        sys.exit("OLED_F6x8 长度异常：%d（应为 570）" % len(f6x8))
    f6x8 = [f6x8[i * 6:(i + 1) * 6] for i in range(95)]

    icons = {}
    for name, header in (("PWM", "const uint8_t IconPwm[90]"),
                         ("Sweep", "const uint8_t IconSweep[90]"),
                         ("Settings", "const uint8_t IconSettings[90]"),
                         ("About", "const uint8_t IconAbout[90]")):
        data = parse_hex_array(src, header)
        if len(data) != 90:
            sys.exit("%s 长度异常：%d（应为 90）" % (name, len(data)))
        icons[name] = data

    cf8 = parse_chinese8x8(src)
    if "\u25a1" not in cf8 or "\u25a0" not in cf8:
        sys.exit("OLED_CF8x8 里找不到 □ / ■")

    return f6x8, icons, cf8


F6X8, ICONS, CF8 = load_fonts()


# ---------------------------------------------------------------- 画布
class Screen:
    """1 bit / 像素的显存，坐标原点在左上角"""

    def __init__(self):
        self.px = [[0] * W for _ in range(H)]

    def set(self, x, y):
        if 0 <= x < W and 0 <= y < H:
            self.px[y][x] = 1

    def xor(self, x, y):
        if 0 <= x < W and 0 <= y < H:
            self.px[y][x] ^= 1

    def rect(self, x, y, w, h):
        """对应 OLED_DrawRectangle(FILLED)，越界自动裁剪"""
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                self.set(xx, yy)

    def line(self, x0, y0, x1, y1):
        """只用到横线和竖线"""
        if y0 == y1:
            for x in range(min(x0, x1), max(x0, x1) + 1):
                self.set(x, y0)
        elif x0 == x1:
            for y in range(min(y0, y1), max(y0, y1) + 1):
                self.set(x0, y)

    def reverse(self, x, y, w, h):
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                self.xor(xx, yy)

    def reverse_round(self, x, y, w, h):
        """REVERSE_ROUNDRECTANGLE：四角先反一次、整块再反一次，角上等于没动"""
        if w >= 3 and h >= 3:
            for cx, cy in ((x, y), (x + w - 1, y), (x, y + h - 1), (x + w - 1, y + h - 1)):
                self.xor(cx, cy)
        self.reverse(x, y, w, h)

    # ---- 文字 ----
    def glyph(self, x, y, cols, gh, clip=None):
        """按列存、bit0 在上的字模，clip 是 (x, y, w, h) 裁剪区"""
        for i, byte in enumerate(cols):
            for bit in range(gh):
                if not byte & (1 << bit):
                    continue
                px, py = x + i, y + bit
                if clip is not None:
                    rx, ry, rw, rh = clip
                    if not (rx <= px < rx + rw and ry <= py < ry + rh):
                        continue
                self.set(px, py)

    def text(self, x, y, s, clip=None):
        """ASCII 走 6x8，其余走 8x8 汉字表（只有 □ 和 ■）"""
        for ch in s:
            if ord(ch) < 128:
                idx = ord(ch) - 32
                if 0 <= idx < 95:
                    self.glyph(x, y, F6X8[idx], 8, clip)
                x += 6
            else:
                data = CF8.get(ch)
                if data:
                    self.glyph(x, y, data, 8, clip)
                x += 8

    def image(self, x, y, w, h, data):
        """对应 OLED_ShowImageArea：3 页 x 30 列，bit0 是当前页顶部"""
        pages = (h - 1) // 8 + 1
        for j in range(pages):
            for i in range(w):
                byte = data[j * w + i]
                for bit in range(8):
                    if byte & (1 << bit):
                        self.set(x + i, y + j * 8 + bit)


# ---------------------------------------------------------------- 页面绘制
ROW_H = 10          # General_FontSize(8) + General_LineSpace(2)
TEXT_X = 2          # List_StartPointX
TEXT_Y = 2          # List_StartPointY
CHINESE_W = 8       # 8x8 中文字宽，单选框占位用
ASCII_W = 6         # 6x8 ASCII 字宽
VALUE_PAD = 10      # OLED_UI_VALUE_RIGHT_PAD


class Row:
    """列表的一行：text 是标题，value 右对齐显示，radio 是单选框状态"""

    def __init__(self, text, value=None, radio=None):
        self.text = text
        self.value = value
        self.radio = radio


def draw_list(scr, rows, active, total=None):
    if total is None:
        total = len(rows)

    for i, row in enumerate(rows):
        y = TEXT_Y + i * ROW_H

        # 右侧控件：单选框优先，其次是右对齐数值
        if row.radio is not None:
            radio_w = CHINESE_W + 2
            scr.text(TEXT_X + W - radio_w - 9, y,
                     "\u25a0" if row.radio else "\u25a1", clip=(0, 0, W - 6, H))
        elif row.value is not None:
            vw = len(row.value) * ASCII_W
            radio_w = CHINESE_W + vw + 4
            scr.text(TEXT_X + W - radio_w, y, row.value, clip=(0, 0, W - 6, H))
        else:
            radio_w = 0

        # 标题区域被右侧控件挤压，超长会自动裁掉（固件里是横向滚动）
        area_w = W - 6 - TEXT_X - 2 - radio_w
        scr.text(TEXT_X, y, row.text, clip=(0, 0, area_w, H))

    # 滚动条 + 中心线
    bar_h = min(H, int(H * (active + 1) / total))
    scr.rect(W - 5, 0, 5, bar_h)
    scr.line(W - 3, 0, W - 3, H - 1)

    # 光标：圆角反色，宽度跟着标题走
    cur = rows[active]
    text_w = len(cur.text) * ASCII_W
    if cur.radio is not None:
        radio_w = CHINESE_W + 2
    elif cur.value is not None:
        radio_w = CHINESE_W + len(cur.value) * ASCII_W + 4
    else:
        radio_w = 0
    w = min(text_w + 2, W - TEXT_X - 6 - radio_w)
    scr.reverse_round(TEXT_X - 1, TEXT_Y + active * ROW_H - 1, w, 8 + 2)


def draw_tiles(scr, active):
    """磁贴页：选中项永远居中，整行按 active 左移"""
    names = ["PWM", "Sweep", "Settings", "About"]
    x = W // 2 - 30 // 2 - active * (30 + 16)
    for name in names:
        if not (x + 30 < 0 or x > W):
            scr.image(x, 0, 30, 24, ICONS[name])
        x += 30 + 16
    scr.reverse_round(W // 2 - 15, 0, 30, 24)


# ---------------------------------------------------------------- PNG 输出
def save_png(scr, path, scale):
    cw = W * scale + PAD * 2
    ch = H * scale + PAD * 2

    # 简易光晕：相邻点按比例提亮，放大后不会是一堆硬方块
    glow = [[0.0] * W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            if not scr.px[y][x]:
                continue
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    if dx == 0 and dy == 0:
                        continue
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < W and 0 <= ny < H:
                        k = 0.22 if dx == 0 or dy == 0 else 0.10
                        if glow[ny][nx] < k:
                            glow[ny][nx] = k

    rows = []
    for py in range(ch):
        row = bytearray()
        for px in range(cw):
            if px < PAD or px >= cw - PAD or py < PAD or py >= ch - PAD:
                row += bytes((0, 0, 0, 0))          # 四周透明
                continue
            if (px < PAD + 1 or px >= cw - PAD - 1
                    or py < PAD + 1 or py >= ch - PAD - 1):
                row += bytes(BORDER + (255,))       # 1 px 边框
                continue
            ox = (px - PAD) // scale
            oy = (py - PAD) // scale
            level = 1.0 if scr.px[oy][ox] else glow[oy][ox]
            r = int(BG[0] + (FG[0] - BG[0]) * level)
            g = int(BG[1] + (FG[1] - BG[1]) * level)
            b = int(BG[2] + (FG[2] - BG[2]) * level)
            row += bytes((r, g, b, 255))
        rows.append(bytes(row))

    raw = b"".join(b"\x00" + r for r in rows)

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", cw, ch, 8, 6, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 9))
           + chunk(b"IEND", b""))

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(png)
    print("%-16s %4d x %-4d %6d B" % (path.name, cw, ch, len(png)))


# ---------------------------------------------------------------- 各页面
def shot_main():
    scr = Screen()
    # 选中第 2 项：左右各有一个磁贴，画面比开机状态（PWM 居中、左侧空）完整
    draw_tiles(scr, 1)
    return scr


def shot_pwm():
    scr = Screen()
    draw_list(scr, [
        Row("[PWM]"),
        Row("Ch3&4 Freq", value="1 kHz"),
        Row("Ch3 Duty%", value="50"),
        Row("Ch3 Out", value="OFF"),
        Row("Ch4 Duty", value="50"),
        Row("Ch4 Out", value="OFF"),
    ], active=1)
    return scr


def shot_sweep():
    scr = Screen()
    draw_list(scr, [
        Row("[Sweep]"),
        Row("FreqSw", radio=False),
        Row("Freq Setup"),
        Row("Ch3 DtySw", radio=False),
        Row("Ch4 DtySw", radio=False),
        Row("Duty Setup"),
        Row("Ch3 Out", value="OFF"),
        Row("Ch4 Out", value="OFF"),
    ], active=1)
    return scr


def shot_sweep_freq():
    scr = Screen()
    draw_list(scr, [
        Row("[Freq Set]"),
        Row("From", value="100 Hz"),
        Row("To", value="20 kHz"),
        Row("Time", value="10s"),
    ], active=1)
    return scr


def shot_sweep_duty():
    scr = Screen()
    draw_list(scr, [
        Row("[Duty Set]"),
        Row("Ch3 From", value="0"),
        Row("Ch3 To", value="100"),
        Row("Ch4 From", value="0"),
        Row("Ch4 To", value="100"),
        Row("Time", value="10s"),
    ], active=1)
    return scr


def shot_settings():
    scr = Screen()
    draw_list(scr, [
        Row("[Seting]"),
        Row("Brightness", value="10"),
        Row("Anti-burn", radio=True),
        Row("Show Stat", radio=False),
    ], active=1)
    return scr


def shot_about():
    """About 共 15 行，这里滚到底部，显示 TEMP / VDD / CPU"""
    scr = Screen()
    rows = [
        Row("[About]"), Row("Device:PWM Tool"),
        Row("MCU:STM32F103C8"), Row("OLED:SSD1306"), Row("UI: OLED_UI"),
        Row("ROM: 64K"), Row("RAM: 20K"),
        Row("HW:  1.0"), Row("FW:  1.0.0"), Row("BL:  0.0"),
        Row("Build:  1"), Row("-------------------"),
        Row("TEMP(C):", value="26.50"), Row("VDD(V):", value="3.28"),
        Row("CPU(%):", value="12.4"), Row("RAM(%):", value="38.6"),
        Row("ROM(%):", value="73.2"),
    ]

    # 光标停在 CPU 行，此时槽位到底，页面上移，上面两行正好是 TEMP / VDD
    active = 14
    page_y = TEXT_Y + 2 * ROW_H - active * ROW_H

    for i, row in enumerate(rows):
        y = page_y + i * ROW_H
        if y < -8 or y > H:
            continue
        if row.value is not None:
            scr.text(TEXT_X + W - len(row.value) * ASCII_W - VALUE_PAD, y,
                     row.value, clip=(0, 0, W - 6, H))
        scr.text(TEXT_X, y, row.text, clip=(0, 0, W - 6, H))

    scr.rect(W - 5, 0, 5, H)
    scr.line(W - 3, 0, W - 3, H - 1)
    scr.reverse_round(TEXT_X - 1, TEXT_Y + 2 * ROW_H - 1,
                      len(rows[active].text) * ASCII_W + 2, 10)
    return scr


PAGES = [
    ("main", shot_main),
    ("pwm", shot_pwm),
    ("sweep", shot_sweep),
    ("sweep_freq", shot_sweep_freq),
    ("sweep_duty", shot_sweep_duty),
    ("settings", shot_settings),
    ("about", shot_about),
]


def main():
    ap = argparse.ArgumentParser(description="渲染 OLED 菜单页面为 PNG")
    ap.add_argument("--scale", type=int, default=4, help="放大倍数，默认 4")
    ap.add_argument("--out", default=str(ROOT / "docs" / "images"), help="输出目录")
    ap.add_argument("--only", default="", help="只渲染指定页面，逗号分隔")
    ap.add_argument("--list", action="store_true", help="列出所有页面名")
    args = ap.parse_args()

    if args.list:
        print("\n".join(name for name, _ in PAGES))
        return

    if args.scale < 1:
        sys.exit("--scale 至少为 1")

    wanted = [s.strip() for s in args.only.split(",") if s.strip()]
    if wanted:
        unknown = [s for s in wanted if s not in dict(PAGES)]
        if unknown:
            sys.exit("未知页面：%s（可用：%s）"
                     % (", ".join(unknown), ", ".join(n for n, _ in PAGES)))
        pages = [(n, f) for n, f in PAGES if n in wanted]
    else:
        pages = PAGES

    out_dir = Path(args.out)
    print("字模：%s" % FONT_SRC)
    print("输出：%s（%d 倍放大）" % (out_dir, args.scale))
    for name, build in pages:
        save_png(build(), out_dir / (name + ".png"), args.scale)


if __name__ == "__main__":
    main()
