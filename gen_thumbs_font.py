import freetype, math

WOFF  = r"j:\ESP32_P4\SonosESP-main\.pio\libdeps\esp32-p4\lvgl\scripts\built_in_font\FontAwesome5-Solid+Brands+Regular.woff"
OUT   = r"j:\ESP32_P4\SonosESP-main\src\fonts\lv_font_fa_thumbs_32.c"
SIZE  = 32
CPS   = [(0xF164, "thumb-up"), (0xF165, "thumb-down")]

face = freetype.Face(WOFF)
face.set_pixel_sizes(0, SIZE)

def render_4bpp(face, cp):
    face.load_char(cp, freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_NORMAL)
    g    = face.glyph
    bm   = g.bitmap
    w, h = bm.width, bm.rows
    buf  = bytes(bm.buffer)
    pitch = bm.pitch
    packed = []
    for row in range(h):
        for col in range(0, w, 2):
            hi = buf[row * pitch + col] >> 4
            lo = (buf[row * pitch + col + 1] >> 4) if (col + 1 < w) else 0
            packed.append((hi << 4) | lo)
    return dict(w=w, h=h,
        adv_w = g.advance.x >> 6,
        ofs_x = g.bitmap_left,
        ofs_y = g.bitmap_top - h,
        data  = bytes(packed))

glyphs = [(cp, nm, render_4bpp(face, cp)) for cp, nm in CPS]

ascender  = face.ascender  * SIZE // face.units_per_EM
descender = face.descender * SIZE // face.units_per_EM
line_h    = ascender - descender
base_l    = -descender

out = []

out.append("/*")
out.append(" * FontAwesome 5 Solid -- thumbs-up (U+F164) + thumbs-down (U+F165)")
out.append(" * Size: 32px, Bpp: 4 -- generated for LVGL")
out.append(" */")
out.append("")
out.append("#ifdef LV_LVGL_H_INCLUDE_SIMPLE")
out.append("#include \"lvgl.h\"")
out.append("#else")
out.append("#include \"lvgl/lvgl.h\"")
out.append("#endif")
out.append("")
out.append("#ifndef LV_FONT_FA_THUMBS_32")
out.append("#define LV_FONT_FA_THUMBS_32 1")
out.append("#endif")
out.append("")
out.append("#if LV_FONT_FA_THUMBS_32")
out.append("")

# Bitmaps
out.append("static const uint8_t glyph_bitmap[] = {")
bitmap_offsets = []
offset = 0
for cp, nm, g in glyphs:
    bitmap_offsets.append(offset)
    out.append("    /* U+%04X %s */" % (cp, nm))
    row = []
    for b in g['data']:
        row.append("0x%02x" % b)
        if len(row) == 16:
            out.append("    " + ", ".join(row) + ",")
            row = []
    if row:
        out.append("    " + ", ".join(row) + ",")
    offset += len(g['data'])
out.append("};")
out.append("")

# Glyph descriptors
out.append("static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {")
out.append("    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},")
for i, (cp, nm, g) in enumerate(glyphs):
    out.append("    {.bitmap_index = %d, .adv_w = %d, .box_w = %d, .box_h = %d, .ofs_x = %d, .ofs_y = %d},"
        % (bitmap_offsets[i], g['adv_w']*16, g['w'], g['h'], g['ofs_x'], g['ofs_y']))
out.append("};")
out.append("")

# Unicode cmap
range_start = min(cp for cp, _, _ in glyphs)
ofs_list = ", ".join("0x%04X" % (cp - range_start) for cp, _, _ in glyphs)
out.append("static const uint16_t unicode_list_0[] = { %s };" % ofs_list)
out.append("")
out.append("static const lv_font_fmt_txt_cmap_t cmaps[] =")
out.append("{")
out.append("    {")
out.append("        .range_start = %d, .range_length = %d," % (range_start, max(cp for cp,_,_ in glyphs)-range_start+1))
out.append("        .glyph_id_start = 1, .unicode_list = unicode_list_0, .glyph_id_ofs_list = NULL,")
out.append("        .list_length = %d, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY" % len(glyphs))
out.append("    }")
out.append("};")
out.append("")

out.append("#if defined(__cplusplus)")
out.append("extern \"C\" {")
out.append("#endif")
out.append("")
out.append("static const lv_font_fmt_txt_dsc_t font_dsc =")
out.append("{")
out.append("    .glyph_bitmap  = glyph_bitmap,")
out.append("    .glyph_dsc     = glyph_dsc,")
out.append("    .cmaps         = cmaps,")
out.append("    .kern_dsc      = NULL,")
out.append("    .kern_scale    = 0,")
out.append("    .cmap_num      = 1,")
out.append("    .bpp           = 4,")
out.append("    .kern_classes  = 0,")
out.append("    .bitmap_format = 0,")
out.append("};")
out.append("")
out.append("const lv_font_t lv_font_fa_thumbs_32 =")
out.append("{")
out.append("    .get_glyph_dsc    = lv_font_get_glyph_dsc_fmt_txt,")
out.append("    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,")
out.append("    .line_height = %d," % line_h)
out.append("    .base_line   = %d," % base_l)
out.append("    .subpx               = LV_FONT_SUBPX_NONE,")
out.append("    .underline_position  = 0,")
out.append("    .underline_thickness = 0,")
out.append("    .dsc       = &font_dsc,")
out.append("    .fallback  = NULL,")
out.append("    .user_data = NULL,")
out.append("};")
out.append("")
out.append("#if defined(__cplusplus)")
out.append("}")
out.append("#endif")
out.append("")
out.append("#endif  /* LV_FONT_FA_THUMBS_32 */")

with open(OUT, 'w', encoding='utf-8') as f:
    f.write('\n'.join(out))

print("OK: %s  (%d bitmap bytes, line_h=%d, base_l=%d)" % (OUT, offset, line_h, base_l))
