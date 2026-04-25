#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import tkinter as tk
from tkinter import ttk

GRID_W = 5
GRID_H = 8
CELL = 42
PADDING = 12

# True: bit0 在顶部(row=0)，False: bit7 在顶部(row=0)
LSB_TOP = True


class Font5x8App:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("5x8 可视化取模工具")

        # 像素数据: pixels[y][x] -> 0/1
        self.pixels = [[0 for _ in range(GRID_W)] for _ in range(GRID_H)]

        self.canvas = tk.Canvas(
            root,
            width=GRID_W * CELL + PADDING * 2,
            height=GRID_H * CELL + PADDING * 2,
            bg="#f2f4f7",
            highlightthickness=0
        )
        self.canvas.grid(row=0, column=0, columnspan=6, padx=10, pady=10)

        self.canvas.bind("<Button-1>", self.on_click)

        self.char_var = tk.StringVar(value="4")
        ttk.Label(root, text="注释字符:").grid(row=1, column=0, sticky="e", padx=6, pady=6)
        ttk.Entry(root, textvariable=self.char_var, width=8).grid(row=1, column=1, sticky="w", padx=6, pady=6)

        default_idx = ord(self.char_var.get()[0]) - ord(' ')
        self.idx_var = tk.StringVar(value=str(default_idx))
        ttk.Label(root, text="序号:").grid(row=1, column=2, sticky="e", padx=6, pady=6)
        ttk.Entry(root, textvariable=self.idx_var, width=8).grid(row=1, column=3, sticky="w", padx=6, pady=6)
        ttk.Button(root, text="-1", command=lambda: self.adjust_index(-1)).grid(
            row=1, column=4, padx=4, pady=6, sticky="we"
        )
        ttk.Button(root, text="+1", command=lambda: self.adjust_index(1)).grid(
            row=1, column=5, padx=4, pady=6, sticky="we"
        )

        self.out_var = tk.StringVar()
        ttk.Entry(root, textvariable=self.out_var, width=90).grid(
            row=2, column=0, columnspan=6, sticky="we", padx=10, pady=6
        )

        ttk.Button(root, text="清空", command=self.clear_all).grid(row=3, column=0, padx=6, pady=8, sticky="we")
        ttk.Button(root, text="反相", command=self.invert_all).grid(row=3, column=1, padx=6, pady=8, sticky="we")
        ttk.Button(root, text="复制数组", command=self.copy_line).grid(row=3, column=2, padx=6, pady=8, sticky="we")
        ttk.Button(root, text="退出", command=root.destroy).grid(row=3, column=3, padx=6, pady=8, sticky="we")

        for i in range(6):
            root.grid_columnconfigure(i, weight=1)

        self.char_var.trace_add("write", lambda *_: self.update_output())
        self.idx_var.trace_add("write", lambda *_: self.update_output())

        self.draw_grid()
        self.update_output()

    def bit_mask(self, row: int) -> int:
        if LSB_TOP:
            return 1 << row
        return 1 << (7 - row)

    def encode_columns(self):
        # 输出 5 个字节，每列一个字节
        cols = []
        for x in range(GRID_W):
            v = 0
            for y in range(GRID_H):
                if self.pixels[y][x]:
                    v |= self.bit_mask(y)
            cols.append(v)
        return cols

    def get_index(self) -> int:
        raw = self.idx_var.get().strip()
        if raw == "":
            return 0
        try:
            return int(raw)
        except ValueError:
            return 0

    def adjust_index(self, delta: int):
        idx = self.get_index() + delta
        idx = max(0, min(94, idx))
        self.idx_var.set(str(idx))
        self.update_output()

    def format_line(self):
        cols = self.encode_columns()
        hex_list = ",".join(f"0x{v:02X}" for v in cols)

        raw = self.char_var.get().strip() or "?"
        ch = raw[0]  # 只取第一个字符
        show = ch.replace("\\", "\\\\").replace('"', '\\"')

        idx = self.get_index()
        return "{" + hex_list + f'}},/*"{show}",{idx}*/'

    def update_output(self):
        self.out_var.set(self.format_line())
        self.draw_grid()

    def clear_all(self):
        for y in range(GRID_H):
            for x in range(GRID_W):
                self.pixels[y][x] = 0
        self.update_output()

    def invert_all(self):
        for y in range(GRID_H):
            for x in range(GRID_W):
                self.pixels[y][x] ^= 1
        self.update_output()

    def copy_line(self):
        line = self.out_var.get()
        self.root.clipboard_clear()
        self.root.clipboard_append(line)
        self.root.update()

    def on_click(self, event):
        x = (event.x - PADDING) // CELL
        y = (event.y - PADDING) // CELL
        if 0 <= x < GRID_W and 0 <= y < GRID_H:
            self.pixels[y][x] ^= 1
            self.update_output()

    def draw_grid(self):
        self.canvas.delete("all")
        for y in range(GRID_H):
            for x in range(GRID_W):
                x0 = PADDING + x * CELL
                y0 = PADDING + y * CELL
                x1 = x0 + CELL - 2
                y1 = y0 + CELL - 2
                fill = "#111827" if self.pixels[y][x] else "#ffffff"
                self.canvas.create_rectangle(
                    x0, y0, x1, y1,
                    fill=fill,
                    outline="#9ca3af",
                    width=1
                )


def main():
    root = tk.Tk()
    Font5x8App(root)
    root.mainloop()


if __name__ == "__main__":
    main()