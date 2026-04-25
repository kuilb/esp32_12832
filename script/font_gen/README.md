# UTF-8 字库生成与索引格式说明

本文档说明 `script/font_gen/index.py` 生成的字库二进制格式、索引排序规则、二分查找原理，以及开发时的常用命令。

## 1. 目标

当前字库工具用于把：

1. 索引文本文件
2. 字模文本文件

打包成一个可在固定偏移读取头信息、可按 UTF-8 键二分查找的二进制字库文件。

适用场景：

1. 外部 Flash 字库存储
2. 固件内嵌字库资源
3. MCU 端按字符快速查字模

## 2. 输入文件格式

### 2.1 索引文件

索引文件格式示例：

```txt
並(0) 丟(1) 轅(2)
```

含义：

1. `並` 是字符本身
2. `0` 是该字符在原始字模文本中的逻辑编号

脚本会从文件中解析 `(字符, idx)` 对。

### 2.2 字模文件

字模文件格式示例：

```txt
{0x00,0x00,0x00,0x20,...},/*"並",0*/
```

含义：

1. 花括号中的内容是该字符的字模字节数组
2. 注释中的 `"並"` 是字符注释
3. 注释中的 `0` 是逻辑编号 `idx`

脚本会解析为 `idx -> glyph_bytes` 的映射。

## 3. 输出文件布局

输出二进制由三部分组成：

1. 头部 `header`
2. 索引区 `index table`
3. 字模区 `bitmap data`

三者都支持放在指定偏移。

如果构建时未指定索引区和字模区偏移，则自动按对齐值顺延。

布局示意：

```text
+----------------------+  <- info_offset
| header (64 bytes)    |
+----------------------+  <- index_offset
| index table          |
| entry[0]             |
| entry[1]             |
| ...                  |
+----------------------+  <- bitmap_offset
| glyph bitmap[0]      |
| glyph bitmap[1]      |
| ...                  |
+----------------------+
```

## 4. 头部格式

头部固定 64 字节，对应 Python 结构：

```python
HEADER_STRUCT = struct.Struct("<8sHHHHIIIIIIII16s")
```

字段含义如下：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| magic | 8 bytes | 固定值 `UTF8FNT\0` |
| version | uint16 | 格式版本，当前为 `1` |
| header_size | uint16 | 头部大小，当前固定 `64` |
| width | uint16 | 字模宽度 |
| height | uint16 | 字模高度 |
| glyph_bytes | uint32 | 每个字占用的字节数 |
| glyph_count | uint32 | 字符总数 |
| index_entry_size | uint32 | 单条索引项大小，当前为 `8` |
| index_offset | uint32 | 索引区偏移 |
| index_bytes | uint32 | 索引区总字节数 |
| bitmap_offset | uint32 | 字模区偏移 |
| bitmap_bytes | uint32 | 字模区总字节数 |
| flags | uint32 | 取模和索引标志 |
| font_name | 16 bytes | ASCII 字体名，右侧补零 |

### 4.1 flags 定义

当前定义：

1. `0x01`：按列取模
2. `0x02`：bit0 在顶部
3. `0x04`：索引和字模按 UTF-8 键升序排列，可用于二分查找

## 5. 索引项格式

索引项结构：

```python
INDEX_ENTRY_STRUCT = struct.Struct("<4sI")
```

每条索引共 8 字节：

1. `utf8_key`：4 字节 UTF-8 键
2. `glyph_offset`：4 字节字模偏移

### 5.1 UTF-8 键定义

字符会先编码为 UTF-8，然后右侧补 `0x00` 到 4 字节。

示例：

| 字符 | UTF-8 原始字节 | 存储键 |
| --- | --- | --- |
| `A` | `41` | `41 00 00 00` |
| `並` | `E4 B8 A6` | `E4 B8 A6 00` |

### 5.2 glyph_offset 含义

`glyph_offset` 不是文件绝对地址，而是相对字模区起始地址的偏移。

实际字模地址计算方式：

```text
glyph_addr = bitmap_offset + glyph_offset
```

## 6. 排序与二分查找原理

### 6.1 为什么要排序

若索引区只是按原始 `idx` 顺序写入，则字符键无序，MCU 端只能线性扫描，时间复杂度为：

$$
O(n)
$$

当前脚本会先把每个字符转换成 UTF-8 键，再按 `utf8_key` 升序排序，然后同时写入：

1. 排序后的索引区
2. 与索引顺序一致的字模区

这样就能直接按 UTF-8 键做二分查找，时间复杂度为：

$$
O(\log n)
$$

### 6.2 构建时的核心过程

1. 解析索引文件，得到 `(char, idx)`
2. 解析字模文件，得到 `idx -> glyph bytes`
3. 组装为 `(utf8_key, char, idx, data)`
4. 按 `utf8_key` 升序排序
5. 排序后重新写入索引区和字模区
6. 索引中的 `glyph_offset` 指向排序后的字模位置

### 6.3 查找时的核心过程

1. 读取头部，拿到 `index_offset`、`glyph_count`、`bitmap_offset`
2. 把目标字符转成 4 字节 `utf8_key`
3. 在索引区用二分查找比较 `utf8_key`
4. 命中后读取 `glyph_offset`
5. 使用 `bitmap_offset + glyph_offset` 定位字模数据

## 7. 常用命令

### 7.1 构建字库

```bash
python script/font_gen/index.py build \
  --index-file script/kanji_16x16_index.TXT \
  --glyph-file script/kanji_16x16.TXT \
  --output script/kanji_font_utf8.bin \
  --width 16 \
  --height 16 \
  --info-offset 0x1000 \
  --font-name kanji16
```

Windows `cmd` 下续行可写成：

```bat
python script/font_gen/index.py build ^
  --index-file script/kanji_16x16_index.TXT ^
  --glyph-file script/kanji_16x16.TXT ^
  --output script/kanji_font_utf8.bin ^
  --width 16 ^
  --height 16 ^
  --info-offset 0x1000 ^
  --font-name kanji16
```

### 7.2 查看头信息

```bash
python script/font_gen/index.py info \
  --input script/kanji_font_utf8.bin \
  --info-offset 0x1000 \
  --show-first 5
```

输出重点：

1. `index offset`
2. `bitmap off`
3. `flags`
4. `binary search: yes`

### 7.3 验证二分查找

```bash
python script/font_gen/index.py find \
  --input script/kanji_font_utf8.bin \
  --info-offset 0x1000 \
  --char 並
```

输出包含：

1. 命中的字符
2. UTF-8 键值
3. 排序后的索引位置
4. 字模相对偏移和绝对地址

## 8. MCU 端建议结构体

建议在 C 端使用定长结构体读取：

```c
typedef struct {
    char magic[8];
    uint16_t version;
    uint16_t header_size;
    uint16_t width;
    uint16_t height;
    uint32_t glyph_bytes;
    uint32_t glyph_count;
    uint32_t index_entry_size;
    uint32_t index_offset;
    uint32_t index_bytes;
    uint32_t bitmap_offset;
    uint32_t bitmap_bytes;
    uint32_t flags;
    char font_name[16];
} font_header_t;

typedef struct {
    uint8_t utf8_key[4];
    uint32_t glyph_offset;
} font_index_entry_t;
```

二分查找比较逻辑建议直接按 4 字节字典序比较。

## 9. 开发约束

1. 单个字符 UTF-8 编码长度必须不超过 4 字节
2. 索引文件和字模文件中的 `idx` 必须一致
3. 每个字模的字节数必须等于 `glyph_bytes`
4. 当前格式假设一个索引键只对应一个字符
5. 若修改排序规则，必须同步更新 `flags` 和 MCU 端查找逻辑

## 10. 调试建议

出现问题时优先检查：

1. `magic` 是否正确
2. `info_offset` 是否与固件读取地址一致
3. `flags` 是否包含 `0x04`
4. `glyph_bytes` 是否与字体实际取模一致
5. `utf8_key` 是否与 MCU 端编码结果一致

若需要快速确认某个字是否存在，优先使用：

```bash
python script/font_gen/index.py find --input script/kanji_font_utf8.bin --info-offset 0x1000 --char 並
```
