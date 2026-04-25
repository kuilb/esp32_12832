#!/usr/bin/env python3
"""Build/read a binary font package from UTF-8 index and glyph text files.

Input formats this script supports:
1) index txt like:  並(0) 丟(1) ...
2) glyph txt like: {0x..,0x..,...},/*"並",0*/

Binary layout (all offsets are absolute in output file):
- header at --info-offset
- index table at --index-offset (or auto)
- bitmap bytes at --bitmap-offset (or auto)
"""

from __future__ import annotations

import argparse
import os
import re
import struct
from dataclasses import dataclass
from typing import Dict, List, Sequence, Tuple


MAGIC = b"UTF8FNT\0"
VERSION = 1
HEADER_STRUCT = struct.Struct("<8sHHHHIIIIIIII16s")  # 64 bytes
INDEX_ENTRY_STRUCT = struct.Struct("<4sI")  # utf8 bytes (zero padded), glyph_offset


@dataclass(frozen=True)
class GlyphRecord:
	char: str
	idx: int
	data: bytes


@dataclass(frozen=True)
class PackedEntry:
	utf8_key: bytes
	char: str
	idx: int
	data: bytes


def align_up(value: int, alignment: int) -> int:
	if alignment <= 1:
		return value
	return (value + alignment - 1) // alignment * alignment


def ensure_size(buf: bytearray, size: int) -> None:
	if len(buf) < size:
		buf.extend(b"\x00" * (size - len(buf)))


def write_at(buf: bytearray, offset: int, payload: bytes) -> None:
	if offset < 0:
		raise ValueError("offset must be >= 0")
	ensure_size(buf, offset + len(payload))
	buf[offset : offset + len(payload)] = payload


def read_text_auto(path: str) -> str:
	raw = open(path, "rb").read()
	encodings = ["utf-8-sig", "utf-8", "gb18030", "gbk", "utf-16"]
	for enc in encodings:
		try:
			return raw.decode(enc)
		except UnicodeDecodeError:
			continue
	raise ValueError(f"cannot decode text file with known encodings: {path}")


def parse_index_text(path: str) -> List[Tuple[str, int]]:
	text = read_text_auto(path)
	items: List[Tuple[str, int]] = []
	for m in re.finditer(r"(.)\((\d+)\)", text):
		ch = m.group(1)
		idx = int(m.group(2))
		items.append((ch, idx))
	if not items:
		raise ValueError(f"no index entries parsed from: {path}")
	return items


def parse_glyph_text(path: str) -> Dict[int, GlyphRecord]:
	text = read_text_auto(path)
	pattern = re.compile(r"\{([^}]*)\}\s*,\s*/\*\"(.*?)\"\s*,\s*(\d+)\s*\*/", re.S)

	out: Dict[int, GlyphRecord] = {}
	for m in pattern.finditer(text):
		raw_bytes = m.group(1)
		ch = m.group(2)
		idx = int(m.group(3))

		vals: List[int] = []
		for token in raw_bytes.split(","):
			token = token.strip()
			if not token:
				continue
			vals.append(int(token, 0))

		if idx in out:
			raise ValueError(f"duplicate glyph index in glyph file: {idx}")
		out[idx] = GlyphRecord(char=ch, idx=idx, data=bytes(vals))

	if not out:
		raise ValueError(f"no glyph entries parsed from: {path}")
	return out


def utf8_key_of_char(ch: str) -> bytes:
	encoded = ch.encode("utf-8")
	if len(encoded) > 4:
		raise ValueError(f"char utf-8 length > 4 bytes: {ch!r}")
	return encoded.ljust(4, b"\x00")


def decode_utf8_key(utf8_key: bytes) -> str:
	try:
		return utf8_key.rstrip(b"\x00").decode("utf-8")
	except UnicodeDecodeError:
		return "?"


def build_package(args: argparse.Namespace) -> None:
	index_items = parse_index_text(args.index_file)
	glyph_map = parse_glyph_text(args.glyph_file)

	glyph_bytes = args.glyph_bytes
	if glyph_bytes is None:
		glyph_bytes = args.width * ((args.height + 7) // 8)

	entries: List[PackedEntry] = []
	missing: List[int] = []

	for ch, idx in index_items:
		glyph = glyph_map.get(idx)
		if glyph is None:
			missing.append(idx)
			continue

		if len(glyph.data) != glyph_bytes:
			raise ValueError(
				f"glyph bytes mismatch at idx={idx}: got {len(glyph.data)}, expect {glyph_bytes}"
			)

		entries.append(PackedEntry(utf8_key=utf8_key_of_char(ch), char=ch, idx=idx, data=glyph.data))

	if missing:
		raise ValueError(f"glyph data missing for indexes: {missing[:10]} (total {len(missing)})")

	entries.sort(key=lambda entry: entry.utf8_key)

	index_blob = bytearray()
	bitmap_blob = bytearray()
	for out_i, entry in enumerate(entries):
		glyph_offset = out_i * glyph_bytes
		index_blob.extend(INDEX_ENTRY_STRUCT.pack(entry.utf8_key, glyph_offset))
		bitmap_blob.extend(entry.data)

	glyph_count = len(entries)
	index_bytes = len(index_blob)
	bitmap_bytes = len(bitmap_blob)

	info_offset = args.info_offset
	index_offset = args.index_offset
	bitmap_offset = args.bitmap_offset

	if index_offset is None:
		index_offset = align_up(info_offset + HEADER_STRUCT.size, args.align)
	if bitmap_offset is None:
		bitmap_offset = align_up(index_offset + index_bytes, args.align)

	if not (info_offset + HEADER_STRUCT.size <= index_offset <= bitmap_offset):
		raise ValueError("offset conflict: check info/index/bitmap offsets")

	flags = 0
	if args.column_major:
		flags |= 0x01
	if args.lsb_top:
		flags |= 0x02
	flags |= 0x04  # index and bitmap are ordered by utf8_key for binary search

	font_name = (args.font_name or "utf8_font").encode("ascii", errors="ignore")[:16]
	font_name = font_name.ljust(16, b"\x00")

	header = HEADER_STRUCT.pack(
		MAGIC,
		VERSION,
		HEADER_STRUCT.size,
		args.width,
		args.height,
		glyph_bytes,
		glyph_count,
		INDEX_ENTRY_STRUCT.size,
		index_offset,
		index_bytes,
		bitmap_offset,
		bitmap_bytes,
		flags,
		font_name,
	)

	if args.base_file and os.path.exists(args.base_file):
		out = bytearray(open(args.base_file, "rb").read())
	else:
		out = bytearray()

	write_at(out, info_offset, header)
	write_at(out, index_offset, bytes(index_blob))
	write_at(out, bitmap_offset, bytes(bitmap_blob))

	with open(args.output, "wb") as f:
		f.write(out)

	print("build ok")
	print(f"output       : {args.output}")
	print(f"font count   : {glyph_count}")
	print(f"glyph bytes  : {glyph_bytes}")
	print(f"header off   : 0x{info_offset:X}")
	print(f"index  off   : 0x{index_offset:X}, size={index_bytes}")
	print(f"bitmap off   : 0x{bitmap_offset:X}, size={bitmap_bytes}")
	print("index order  : utf8_key ascending")


def read_info(args: argparse.Namespace) -> None:
	with open(args.input, "rb") as f:
		f.seek(args.info_offset)
		raw = f.read(HEADER_STRUCT.size)
	if len(raw) != HEADER_STRUCT.size:
		raise ValueError("cannot read full header")

	(
		magic,
		version,
		header_size,
		width,
		height,
		glyph_bytes,
		glyph_count,
		index_entry_size,
		index_offset,
		index_bytes,
		bitmap_offset,
		bitmap_bytes,
		flags,
		font_name,
	) = HEADER_STRUCT.unpack(raw)

	if magic != MAGIC:
		raise ValueError(f"bad magic at offset 0x{args.info_offset:X}: {magic!r}")

	name = font_name.split(b"\x00", 1)[0].decode("ascii", errors="ignore")
	print("font info")
	print(f"version      : {version}")
	print(f"header size  : {header_size}")
	print(f"size         : {width}x{height}")
	print(f"glyph bytes  : {glyph_bytes}")
	print(f"glyph count  : {glyph_count}")
	print(f"index entry  : {index_entry_size}")
	print(f"index offset : 0x{index_offset:X}, bytes={index_bytes}")
	print(f"bitmap off   : 0x{bitmap_offset:X}, bytes={bitmap_bytes}")
	print(f"flags        : 0x{flags:X}")
	print(f"font name    : {name}")
	print(f"binary search: {'yes' if (flags & 0x04) else 'no'}")

	if args.show_first > 0:
		with open(args.input, "rb") as f:
			f.seek(index_offset)
			max_n = min(args.show_first, index_bytes // INDEX_ENTRY_STRUCT.size)
			for i in range(max_n):
				raw_entry = f.read(INDEX_ENTRY_STRUCT.size)
				if len(raw_entry) != INDEX_ENTRY_STRUCT.size:
					break
				utf8_key, glyph_off = INDEX_ENTRY_STRUCT.unpack(raw_entry)
				ch = decode_utf8_key(utf8_key)
				print(f"idx[{i}] utf8={utf8_key.hex()} ch={ch} glyph_off={glyph_off}")


def find_glyph(args: argparse.Namespace) -> None:
	needle = utf8_key_of_char(args.char)
	with open(args.input, "rb") as f:
		f.seek(args.info_offset)
		raw = f.read(HEADER_STRUCT.size)
	if len(raw) != HEADER_STRUCT.size:
		raise ValueError("cannot read full header")

	(
		magic,
		_version,
		_header_size,
		_width,
		_height,
		glyph_bytes,
		glyph_count,
		index_entry_size,
		index_offset,
		_index_bytes,
		bitmap_offset,
		_bitmap_bytes,
		flags,
		_font_name,
	) = HEADER_STRUCT.unpack(raw)

	if magic != MAGIC:
		raise ValueError(f"bad magic at offset 0x{args.info_offset:X}: {magic!r}")
	if index_entry_size != INDEX_ENTRY_STRUCT.size:
		raise ValueError("unexpected index entry size")
	if not (flags & 0x04):
		raise ValueError("font index is not marked as utf8-sorted")

	left = 0
	right = glyph_count - 1
	with open(args.input, "rb") as f:
		while left <= right:
			mid = (left + right) // 2
			f.seek(index_offset + mid * INDEX_ENTRY_STRUCT.size)
			raw_entry = f.read(INDEX_ENTRY_STRUCT.size)
			if len(raw_entry) != INDEX_ENTRY_STRUCT.size:
				raise ValueError("failed to read index entry")
			utf8_key, glyph_off = INDEX_ENTRY_STRUCT.unpack(raw_entry)

			if needle == utf8_key:
				absolute = bitmap_offset + glyph_off
				print(f"found        : {args.char}")
				print(f"utf8 key     : {utf8_key.hex()}")
				print(f"index        : {mid}")
				print(f"glyph offset : 0x{glyph_off:X}")
				print(f"bitmap addr  : 0x{absolute:X}")
				print(f"glyph bytes  : {glyph_bytes}")
				return

			if needle < utf8_key:
				right = mid - 1
			else:
				left = mid + 1

	raise ValueError(f"char not found by binary search: {args.char!r}")


def build_parser() -> argparse.ArgumentParser:
	parser = argparse.ArgumentParser(
		description="Build/read UTF-8 index + glyph binary package with fixed info offset."
	)
	sub = parser.add_subparsers(dest="cmd", required=True)

	p_build = sub.add_parser("build", help="build binary package")
	p_build.add_argument("--index-file", required=True, help="path to index txt")
	p_build.add_argument("--glyph-file", required=True, help="path to glyph txt")
	p_build.add_argument("--output", required=True, help="output binary file")
	p_build.add_argument("--width", type=int, required=True, help="font width")
	p_build.add_argument("--height", type=int, required=True, help="font height")
	p_build.add_argument(
		"--glyph-bytes",
		type=int,
		default=None,
		help="bytes per glyph (default: width * ceil(height/8))",
	)
	p_build.add_argument("--info-offset", type=lambda s: int(s, 0), default=0, help="header offset")
	p_build.add_argument(
		"--index-offset", type=lambda s: int(s, 0), default=None, help="index table offset"
	)
	p_build.add_argument(
		"--bitmap-offset", type=lambda s: int(s, 0), default=None, help="bitmap data offset"
	)
	p_build.add_argument("--align", type=int, default=16, help="auto offset alignment")
	p_build.add_argument("--base-file", default=None, help="optional base binary to patch")
	p_build.add_argument("--font-name", default="utf8_font", help="ascii name in header")
	p_build.add_argument("--column-major", action="store_true", default=True)
	p_build.add_argument("--lsb-top", action="store_true", default=True)
	p_build.set_defaults(func=build_package)

	p_info = sub.add_parser("info", help="read header from binary")
	p_info.add_argument("--input", required=True, help="binary file")
	p_info.add_argument("--info-offset", type=lambda s: int(s, 0), default=0, help="header offset")
	p_info.add_argument("--show-first", type=int, default=5, help="print first n index entries")
	p_info.set_defaults(func=read_info)

	p_find = sub.add_parser("find", help="binary search a char in the sorted index")
	p_find.add_argument("--input", required=True, help="binary file")
	p_find.add_argument("--info-offset", type=lambda s: int(s, 0), default=0, help="header offset")
	p_find.add_argument("--char", required=True, help="single character to search")
	p_find.set_defaults(func=find_glyph)

	return parser


def main(argv: Sequence[str] | None = None) -> int:
	parser = build_parser()
	args = parser.parse_args(argv)
	args.func(args)
	return 0


if __name__ == "__main__":
	raise SystemExit(main())

