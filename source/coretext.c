// Texto: CGFont/CTFont/UIFont + NSString(UIStringDrawing) sobre FreeType con una sola fuente
// (font.ttf, NanumGothic: latín + coreano). Los IDs de glifo son los de FreeType, así que todo lo que el
// juego mide (CTFontGetGlyphsForCharacters, advances, cmap de CGFontCopyTableForTag) es coherente con lo
// que se dibuja. Clientes en Zenonia 5: CCImage::initWithString de cocos2d-x (UIKit drawInRect:withFont:,
// glifo con borde vía CTFontCreatePathForGlyph), el gestor de fuentes C++ de CCGX (métricas y advances)
// y FontLabel/ZFont (CGContextShowGlyphsAtPoint). Six Guns: GlyphSource (atlas dinámico) con CTFontCreateWithName,
// CTLine/CTRun y CTFontDrawGlyphs. Copia maestra en Ferrari (viene de Zenonia5-iOS-vita 9c1683a). Sin font.ttf
// (app0: o DATA_PATH) la tabla no se registra (coretext_ok = 0) y UIFont queda con los stubs de uikit.c.
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_STROKER_H
#include FT_TRUETYPE_TABLES_H
#include FT_ADVANCES_H
#include <math.h>
#include <psp2/kernel/processmgr.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "foundation.h"
#include "hle.h"
#include "imports.h"
#include "log.h"
#include "objc.h"

typedef struct {
    float x, y;
} CGPoint;
typedef struct {
    float w, h;
} CGSize;
typedef struct {
    CGPoint o;
    CGSize s;
} CGRect;

int cg_ctx_map(void *ctx, float ux, float uy, float *col, float *row); // uikit.c
void cg_ctx_cover(void *ctx, int col, int row, int cov);
void *ui_current_ctx(void);

static FT_Library ft;
static FT_Face face, latin; // latin: font_latin.ttf (tildes/ñ que NanumGothic no trae), solo en layout()
static FT_Stroker stroker;
static pthread_mutex_t ft_lock = PTHREAD_MUTEX_INITIALIZER; // cocos2d arma texturas también en hilos de carga
static float cur_px = -1;
int coretext_ok; // imports.c: sin fuente, esta tabla no existe

static objc_class *c_CGFont, *c_CTFont, *c_CGPath, *c_UIFont;
static struct {
    objc_class *isa;
} cgfont; // única CGFont: sin cabecera de objc_alloc, retain/release no la liberan
typedef struct {
    objc_class *isa;
    float size;
} ct_font; // CTFont y UIFont
typedef struct {
    objc_class *isa;
    int glyph;
    float size;
} cg_path; // CTFontCreatePathForGlyph: el glifo, se rasteriza en FillPath/DrawPath

static void set_px(float px) {
    if (px != cur_px) {
        FT_Set_Char_Size(face, 0, (FT_F26Dot6)(px * 64), 72, 72);
        cur_px = px;
    }
}
static float units(int v, float px) { return v * px / face->units_per_EM; }

// Dibuja un glifo (relleno o solo el borde de grosor stroke) con su origen en (ux, uy) del contexto.
// CG (ydown = 0): el bitmap baja en memoria desde el origen. ponytail: ahí el CTM solo traslada/voltea.
// UIKit (ydown = 1): el glifo crece hacia +y del usuario y cada píxel pasa por el CTM, como en iOS: con el CTM
// volteado (cocos2d) queda igual que CG; sin voltear (EA, Sims 3 Ambitions) sale invertido en memoria, que es lo
// que espera su textura (antes: letras al revés y cortadas, corrida 007).
// Caché de glifos rasterizados (sin borde): sin ella cada sizeWithFont:/drawAtPoint: recargaba y rasterizaba el
// contorno de cada letra; EA mide/dibuja sus textos en cada frame (corrida 008: menú a 3 fps).
typedef struct {
    FT_Face f;
    int gid, px64, left, top, w, h;
    float adv;
    uint8_t *bmp;
} gslot;
static gslot gcache[2048];
static const gslot *glyph_cached(FT_Face f, int gid, float px) {
    int px64 = (int)(px * 64);
    unsigned hsh = ((unsigned)gid * 2654435761u ^ (unsigned)px64 * 40503u ^ (f == face ? 0 : 0x9e37u)) & 2047;
    gslot *g = &gcache[hsh];
    if (g->f == f && g->gid == gid && g->px64 == px64)
        return g;
    if (f == face)
        set_px(px);
    else
        FT_Set_Char_Size(f, 0, (FT_F26Dot6)px64, 72, 72);
    if (FT_Load_Glyph(f, gid, FT_LOAD_NO_BITMAP | FT_LOAD_RENDER))
        return NULL;
    FT_Bitmap *b = &f->glyph->bitmap;
    uint8_t *bmp = b->width && b->rows ? malloc(b->width * b->rows) : NULL;
    for (unsigned y = 0; bmp && y < b->rows; y++)
        memcpy(bmp + y * b->width, b->buffer + y * b->pitch, b->width);
    free(g->bmp);
    *g = (gslot){f, gid, px64, f->glyph->bitmap_left, f->glyph->bitmap_top, bmp ? b->width : 0, bmp ? b->rows : 0,
                 f->glyph->advance.x / 64.0f, bmp};
    return g;
}
static uint64_t ct_us; // perfil: tiempo en texto (gles.c lo imprime en PERF)
uint64_t coretext_prof_us(void) {
    uint64_t r = ct_us;
    ct_us = 0;
    return r;
}

static float draw_glyph_f(FT_Face f, void *ctx, int gid, float px, float ux, float uy, float stroke, int ydown) {
    float col, row;
    if (stroke <= 0) {
        const gslot *g = glyph_cached(f, gid, px);
        if (!g)
            return 0;
        if (!ctx || !g->bmp || !cg_ctx_map(ctx, ux, uy, &col, &row))
            return g->adv;
        if (!ydown) {
            int x0 = (int)lroundf(col) + g->left, y0 = (int)lroundf(row) - g->top;
            for (int y = 0; y < g->h; y++)
                for (int x = 0; x < g->w; x++)
                    cg_ctx_cover(ctx, x0 + x, y0 + y, g->bmp[y * g->w + x]);
            return g->adv;
        }
        // UIKit: cada píxel por el CTM (lineal: origen + derivadas, calculadas una vez por glifo)
        float c0, r0, cx, rx, cy, ry;
        float gx0 = ux + g->left + 0.5f, gy0 = uy - g->top + 0.5f;
        cg_ctx_map(ctx, gx0, gy0, &c0, &r0);
        cg_ctx_map(ctx, gx0 + 1, gy0, &cx, &rx);
        cg_ctx_map(ctx, gx0, gy0 + 1, &cy, &ry);
        cx -= c0, rx -= r0, cy -= c0, ry -= r0;
        for (int y = 0; y < g->h; y++)
            for (int x = 0; x < g->w; x++) {
                int cov = g->bmp[y * g->w + x];
                if (cov)
                    cg_ctx_cover(ctx, (int)floorf(c0 + cx * x + cy * y), (int)floorf(r0 + rx * x + ry * y), cov);
            }
        return g->adv;
    }
    if (f == face)
        set_px(px);
    else
        FT_Set_Char_Size(f, 0, (FT_F26Dot6)(px * 64), 72, 72);
    if (FT_Load_Glyph(f, gid, FT_LOAD_NO_BITMAP))
        return 0;
    float adv = f->glyph->advance.x / 64.0f;
    if (!ctx || !cg_ctx_map(ctx, ux, uy, &col, &row))
        return adv;
    FT_Glyph g;
    if (FT_Get_Glyph(f->glyph, &g))
        return adv;
    if (stroke > 0) {
        FT_Stroker_Set(stroker, (FT_Fixed)(stroke * 32), FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
        FT_Glyph_Stroke(&g, stroker, 1);
    }
    if (!FT_Glyph_To_Bitmap(&g, FT_RENDER_MODE_NORMAL, NULL, 1)) {
        FT_BitmapGlyph b = (FT_BitmapGlyph)g;
        int x0 = (int)lroundf(col) + b->left, y0 = (int)lroundf(row) - b->top;
        for (unsigned y = 0; y < b->bitmap.rows; y++)
            for (unsigned x = 0; x < b->bitmap.width; x++) {
                int cov = b->bitmap.buffer[y * b->bitmap.pitch + x];
                if (!ydown) {
                    cg_ctx_cover(ctx, x0 + x, y0 + y, cov);
                } else if (cov && cg_ctx_map(ctx, ux + b->left + x + 0.5f, uy - b->top + y + 0.5f, &col, &row)) {
                    cg_ctx_cover(ctx, (int)floorf(col), (int)floorf(row), cov);
                }
            }
    }
    FT_Done_Glyph(g);
    return adv;
}

static float draw_glyph(void *ctx, int gid, float px, float ux, float uy, float stroke) {
    return draw_glyph_f(face, ctx, gid, px, ux, uy, stroke, 0);
}
static FT_Face face_for(int c) {
    return latin && !FT_Get_Char_Index(face, c) && FT_Get_Char_Index(latin, c) ? latin : face;
}

static int utf8_next(const unsigned char **p) {
    const unsigned char *s = *p;
    int c = *s++, n = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    c &= n == 3 ? 7 : n == 2 ? 15 : n == 1 ? 31 : 127;
    while (n-- && (*s & 0xC0) == 0x80)
        c = c << 6 | (*s++ & 63);
    *p = s;
    return c;
}

// Layout de UIKit: líneas por '\n' y ajuste a maxw (por palabra; si no hay espacio, por carácter).
// Con ctx dibuja en r (origen arriba-izquierda, alineación 0 izq / 1 centro / 2 der). Devuelve el tamaño.
static void layout(CGSize *ret, const char *text, float px, float maxw, float maxh, int align, void *ctx, CGRect r) {
    pthread_mutex_lock(&ft_lock);
    uint64_t t0 = sceKernelGetProcessTimeWide();
    set_px(px);
    float asc = face->size->metrics.ascender / 64.0f, lh = face->size->metrics.height / 64.0f, maxlw = 0;
    int n = 0, cap = strlen(text) + 1, lines = 0;
    int *cp = malloc(cap * sizeof(int));
    float *adv = malloc(cap * sizeof(float));
    for (const unsigned char *p = (const unsigned char *)text; *p;) {
        cp[n] = utf8_next(&p);
        adv[n] = cp[n] == '\n' ? 0 : draw_glyph_f(face_for(cp[n]), NULL, FT_Get_Char_Index(face_for(cp[n]), cp[n]), px, 0, 0, 0, 1);
        n++;
    }
    for (int i = 0; i < n || (i == 0 && n == 0);) {
        int start = i, end = i, brk = -1;
        float w = 0;
        while (end < n && cp[end] != '\n') {
            if (maxw > 0 && w + adv[end] > maxw + 0.5f && end > start)
                break;
            if (cp[end] == ' ')
                brk = end;
            w += adv[end++];
        }
        int next = end;
        if (end < n && cp[end] != '\n' && brk > start)
            end = brk, next = brk + 1; // corta en el último espacio
        else if (end < n && cp[end] == '\n')
            next = end + 1;
        w = 0;
        for (int k = start; k < end; k++)
            w += adv[k];
        for (int k = end - 1; k >= start && cp[k] == ' '; k--)
            w -= adv[k];
        if (maxh > 0 && lines > 0 && (lines + 1) * lh > maxh + 0.5f)
            break; // UIKit no dibuja líneas que no entran
        if (w > maxlw)
            maxlw = w;
        if (ctx) {
            float x = r.o.x + (align == 1 ? (r.s.w - w) / 2 : align == 2 ? r.s.w - w : 0);
            for (int k = start; k < end; k++)
                x += cp[k] == ' ' ? adv[k] : draw_glyph_f(face_for(cp[k]), ctx, FT_Get_Char_Index(face_for(cp[k]), cp[k]), px, x,
                                                          r.o.y + asc + lines * lh, 0, 1);
        }
        lines++;
        if (n == 0 || next >= n)
            break;
        i = next;
    }
    free(cp);
    free(adv);
    ct_us += sceKernelGetProcessTimeWide() - t0;
    pthread_mutex_unlock(&ft_lock);
    *ret = (CGSize){ceilf(maxlw), ceilf(lines * lh)};
}

// ================================================================ CGFont (unidades de la fuente)

static void *cg_FontCreateWithFontName(id name) {
    static int logged;
    if (!logged++)
        l_info("CGFontCreateWithFontName(%s) -> font.ttf (todas las fuentes)", ns_cstr(name));
    return &cgfont;
}
static void *cg_DataProviderCreateWithURL(id url) { return (void *)1; }
static void *cg_FontCreateWithDataProvider(void *p) { return &cgfont; }
static void *cg_FontRetain(void *f) { return f; }
static void cg_FontRelease(void *f) {}
static int cg_FontGetUnitsPerEm(void *f) { return face->units_per_EM; }
static int cg_FontGetAscent(void *f) { return face->ascender; }
static int cg_FontGetDescent(void *f) { return face->descender; } // negativo, como en CG
static int os2(int cap) {
    TT_OS2 *t = FT_Get_Sfnt_Table(face, FT_SFNT_OS2);
    int v = t && t->version >= 2 ? (cap ? t->sCapHeight : t->sxHeight) : 0;
    return v ? v : face->units_per_EM * (cap ? 7 : 5) / 10;
}
static int cg_FontGetCapHeight(void *f) { return os2(1); }
static int cg_FontGetXHeight(void *f) { return os2(0); }
static int cg_FontGetGlyphAdvances(void *f, const uint16_t *g, size_t n, int *adv) {
    pthread_mutex_lock(&ft_lock);
    for (size_t i = 0; i < n; i++) {
        FT_Fixed a = 0;
        FT_Get_Advance(face, g[i], FT_LOAD_NO_SCALE, &a);
        adv[i] = a;
    }
    pthread_mutex_unlock(&ft_lock);
    return 1;
}
static int cg_FontGetGlyphBBoxes(void *f, const uint16_t *g, size_t n, CGRect *bb) {
    pthread_mutex_lock(&ft_lock);
    for (size_t i = 0; i < n; i++) {
        bb[i] = (CGRect){{0, 0}, {0, 0}};
        if (!FT_Load_Glyph(face, g[i], FT_LOAD_NO_SCALE)) {
            FT_Glyph_Metrics *m = &face->glyph->metrics;
            bb[i] = (CGRect){{m->horiBearingX, m->horiBearingY - m->height}, {m->width, m->height}};
        }
    }
    cur_px = -1; // NO_SCALE no toca el tamaño, pero por las dudas
    pthread_mutex_unlock(&ft_lock);
    return 1;
}
static id cg_FontCopyTableForTag(void *f, uint32_t tag) {
    FT_ULong len = 0;
    if (FT_Load_Sfnt_Table(face, tag, 0, NULL, &len) || !len)
        return NULL;
    void *b = malloc(len);
    FT_Load_Sfnt_Table(face, tag, 0, b, &len);
    id d = ns_data_new(b, len);
    free(b);
    return d;
}

// ================================================================ CTFont (píxeles al tamaño de la fuente)

static ct_font *font_new(objc_class *c, float size) {
    ct_font *f = objc_alloc(c);
    f->size = size > 0 ? size : 12;
    return f;
}
static ct_font *ct_FontCreateWithGraphicsFont(void *cg, float size, void *matrix, void *attrs) {
    return font_new(c_CTFont, size);
}
static ct_font *ct_FontCreateWithName(id name, float size, void *matrix) { return font_new(c_CTFont, size); }
static ct_font *ct_FontCreateForString(ct_font *cur, id s, int loc, int len) { return objc_retain(cur); }
static id ct_FontCopyDisplayName(ct_font *f) { return ns_string_new("NanumGothic"); }
static float ct_FontGetSize(ct_font *f) { return f->size; }
static float ct_FontGetAscent(ct_font *f) { return units(face->ascender, f->size); }
static float ct_FontGetDescent(ct_font *f) { return units(-face->descender, f->size); }
static float ct_FontGetLeading(ct_font *f) {
    float l = units(face->height - face->ascender + face->descender, f->size);
    return l > 0 ? l : 0;
}
static float ct_FontGetCapHeight(ct_font *f) { return units(os2(1), f->size); }
static float ct_FontGetXHeight(ct_font *f) { return units(os2(0), f->size); }
static float ct_FontGetUnderlinePosition(ct_font *f) { return units(face->underline_position, f->size); }
// ponytail: UTF-16 sin pares sustitutos (el juego no usa caracteres fuera del BMP)
static int ct_FontGetGlyphsForCharacters(ct_font *f, const uint16_t *ch, uint16_t *g, int n) {
    int all = 1;
    for (int i = 0; i < n; i++)
        all &= (g[i] = FT_Get_Char_Index(face, ch[i])) != 0;
    return all;
}
static double ct_FontGetAdvancesForGlyphs(ct_font *f, int orient, const uint16_t *g, CGSize *adv, int n) {
    double total = 0;
    pthread_mutex_lock(&ft_lock);
    set_px(f->size);
    for (int i = 0; i < n; i++) {
        float a = FT_Load_Glyph(face, g[i], FT_LOAD_NO_BITMAP) ? 0 : face->glyph->advance.x / 64.0f;
        if (adv)
            adv[i] = (CGSize){a, 0};
        total += a;
    }
    pthread_mutex_unlock(&ft_lock);
    return total;
}
// CGRect por sret; rects en píxeles con origen en la línea base (y hacia arriba, como CoreText)
static void ct_FontGetBoundingRectsForGlyphs(CGRect *ret, ct_font *f, int orient, const uint16_t *g, CGRect *rects, int n) {
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    pthread_mutex_lock(&ft_lock);
    set_px(f->size);
    for (int i = 0; i < n; i++) {
        CGRect r = {{0, 0}, {0, 0}};
        if (!FT_Load_Glyph(face, g[i], FT_LOAD_NO_BITMAP)) {
            FT_Glyph_Metrics *m = &face->glyph->metrics;
            r = (CGRect){{m->horiBearingX / 64.0f, (m->horiBearingY - m->height) / 64.0f}, {m->width / 64.0f, m->height / 64.0f}};
        }
        if (rects)
            rects[i] = r;
        x0 = fminf(x0, r.o.x), y0 = fminf(y0, r.o.y), x1 = fmaxf(x1, r.o.x + r.s.w), y1 = fmaxf(y1, r.o.y + r.s.h);
    }
    pthread_mutex_unlock(&ft_lock);
    *ret = (CGRect){{x0, y0}, {x1 - x0, y1 - y0}};
}
static void ct_FontDrawGlyphs(ct_font *f, const uint16_t *g, const CGPoint *pos, int n, void *ctx) {
    pthread_mutex_lock(&ft_lock);
    for (int i = 0; i < n; i++)
        draw_glyph(ctx, g[i], f->size, pos[i].x, pos[i].y, 0);
    pthread_mutex_unlock(&ft_lock);
}

// CFAttributedString + CTLine/CTRun: una corrida con un glifo FreeType por carácter (sin ligaduras ni bidi)
typedef struct {
    objc_class *isa;
    id str, attrs;
} ct_astr;
typedef struct {
    objc_class *isa;
    int n;
    uint16_t *glyphs;
    int32_t *idx;
    id attrs;
} ct_run;
typedef struct {
    objc_class *isa;
    id runs;
} ct_line;
static objc_class *c_CFAttributedString, *c_CTRun, *c_CTLine;
static id cf_AttributedStringCreate(void *a, id str, id attrs) {
    ct_astr *s = objc_alloc(c_CFAttributedString);
    s->str = objc_retain(str), s->attrs = objc_retain(attrs);
    return s;
}
static id ct_LineCreateWithAttributedString(ct_astr *s) {
    const unsigned char *u = (const unsigned char *)ns_cstr(s ? s->str : NULL);
    ct_run *r = objc_alloc(c_CTRun);
    int cap = strlen((const char *)u) + 1;
    r->glyphs = malloc(cap * 2), r->idx = malloc(cap * 4), r->attrs = objc_retain(s ? s->attrs : NULL);
    for (int ci = 0; *u; ci++) {
        int c = utf8_next(&u);
        r->glyphs[r->n] = FT_Get_Char_Index(face, c);
        r->idx[r->n++] = ci;
        if (c > 0xFFFF)
            ci++; // par sustituto en UTF-16
    }
    ct_line *l = objc_alloc(c_CTLine);
    l->runs = ns_array_new();
    ns_array_add(l->runs, r);
    objc_release(r);
    return l;
}
static id ct_LineGetGlyphRuns(ct_line *l) { return l ? l->runs : NULL; }
static int ct_RunGetGlyphCount(ct_run *r) { return r ? r->n : 0; }
static const uint16_t *ct_RunGetGlyphsPtr(ct_run *r) { return r ? r->glyphs : NULL; }
static const int32_t *ct_RunGetStringIndicesPtr(ct_run *r) { return r ? r->idx : NULL; }
static void ct_RunGetGlyphs(ct_run *r, int loc, int len, uint16_t *out) { // CFRange por valor: r1:r2
    memcpy(out, r->glyphs + loc, (len ? len : r->n - loc) * 2);
}
static void ct_RunGetStringIndices(ct_run *r, int loc, int len, int32_t *out) {
    memcpy(out, r->idx + loc, (len ? len : r->n - loc) * 4);
}
static id ct_RunGetAttributes(ct_run *r) { return r ? r->attrs : NULL; }
static int ct_RunGetStatus(ct_run *r) { return 0; }

static cg_path *ct_FontCreatePathForGlyph(ct_font *f, uint16_t g, void *matrix) {
    cg_path *p = objc_alloc(c_CGPath);
    p->glyph = g, p->size = f->size;
    return p;
}

// ================================================================ CGContext: texto y trazos de glifo

static cg_path *pending; // CGContextAddPath -> FillPath/DrawPath (ponytail: un camino, sin contexto por hilo)
static float line_w = 1, text_px = 12;
static void cg_BeginPath(void *c) { pending = NULL; }
static void cg_AddPath(void *c, cg_path *p) { pending = objc_is_kind(p, c_CGPath) ? p : NULL; }
static void cg_SetLineWidth(void *c, float w) { line_w = w; }
static void path_draw(void *c, float stroke) {
    if (!pending)
        return;
    pthread_mutex_lock(&ft_lock);
    draw_glyph(c, pending->glyph, pending->size, 0, 0, stroke);
    pthread_mutex_unlock(&ft_lock);
}
static void cg_FillPath(void *c) { path_draw(c, 0); }
static void cg_DrawPath(void *c, int mode) { // 0 fill 1 eoFill 2 stroke 3 fillStroke 4 eoFillStroke
    if (mode != 2)
        path_draw(c, 0);
    if (mode >= 2)
        path_draw(c, line_w);
}
// ponytail: UIColor no guarda color en este runtime: relleno y borde usan el color de relleno del contexto
// (cocos2d pinta el texto en blanco y lo tiñe en GL).
static void cg_noop(void) {}
static void cg_SetFont(void *c, void *f) {}
static void cg_SetFontSize(void *c, float px) { text_px = px; }
static void cg_ShowGlyphsAtPoint(void *c, float x, float y, const uint16_t *g, size_t n) {
    pthread_mutex_lock(&ft_lock);
    for (size_t i = 0; i < n; i++)
        x += draw_glyph(c, g[i], text_px, x, y, 0);
    pthread_mutex_unlock(&ft_lock);
}


const import_entry coretext_imports[] = {
    F(CGFontCreateWithFontName, cg_FontCreateWithFontName), F(CGDataProviderCreateWithURL, cg_DataProviderCreateWithURL),
    F(CGFontCreateWithDataProvider, cg_FontCreateWithDataProvider), F(CGFontRetain, cg_FontRetain),
    F(CGFontRelease, cg_FontRelease), F(CGFontGetUnitsPerEm, cg_FontGetUnitsPerEm), F(CGFontGetAscent, cg_FontGetAscent),
    F(CGFontGetDescent, cg_FontGetDescent), F(CGFontGetCapHeight, cg_FontGetCapHeight),
    F(CGFontGetXHeight, cg_FontGetXHeight), F(CGFontGetGlyphAdvances, cg_FontGetGlyphAdvances),
    F(CGFontGetGlyphBBoxes, cg_FontGetGlyphBBoxes), F(CGFontCopyTableForTag, cg_FontCopyTableForTag),
    F(CTFontCreateWithGraphicsFont, ct_FontCreateWithGraphicsFont), F(CTFontCreateForString, ct_FontCreateForString),
    F(CTFontCopyDisplayName, ct_FontCopyDisplayName), F(CTFontCopyFullName, ct_FontCopyDisplayName),
    F(CTFontCreateWithName, ct_FontCreateWithName), F(CTFontGetBoundingRectsForGlyphs, ct_FontGetBoundingRectsForGlyphs),
    F(CTFontDrawGlyphs, ct_FontDrawGlyphs), F(CFAttributedStringCreate, cf_AttributedStringCreate),
    F(CTLineCreateWithAttributedString, ct_LineCreateWithAttributedString), F(CTLineGetGlyphRuns, ct_LineGetGlyphRuns),
    F(CTRunGetGlyphCount, ct_RunGetGlyphCount), F(CTRunGetGlyphsPtr, ct_RunGetGlyphsPtr),
    F(CTRunGetGlyphs, ct_RunGetGlyphs), F(CTRunGetStringIndicesPtr, ct_RunGetStringIndicesPtr),
    F(CTRunGetStringIndices, ct_RunGetStringIndices), F(CTRunGetAttributes, ct_RunGetAttributes),
    F(CTRunGetStatus, ct_RunGetStatus), F(CTFontGetSize, ct_FontGetSize),
    F(CTFontGetAscent, ct_FontGetAscent), F(CTFontGetDescent, ct_FontGetDescent), F(CTFontGetLeading, ct_FontGetLeading),
    F(CTFontGetCapHeight, ct_FontGetCapHeight), F(CTFontGetXHeight, ct_FontGetXHeight),
    F(CTFontGetUnderlinePosition, ct_FontGetUnderlinePosition),
    F(CTFontGetGlyphsForCharacters, ct_FontGetGlyphsForCharacters),
    F(CTFontGetAdvancesForGlyphs, ct_FontGetAdvancesForGlyphs), F(CTFontCreatePathForGlyph, ct_FontCreatePathForGlyph),
    F(CGContextBeginPath, cg_BeginPath), F(CGContextAddPath, cg_AddPath), F(CGContextSetLineWidth, cg_SetLineWidth),
    F(CGContextFillPath, cg_FillPath), F(CGContextDrawPath, cg_DrawPath), F(CGContextSetFont, cg_SetFont),
    F(CGContextSetFontSize, cg_SetFontSize), F(CGContextShowGlyphsAtPoint, cg_ShowGlyphsAtPoint),
    F(CGContextSetTextMatrix, cg_noop), F(CGContextSetAllowsFontSmoothing, cg_noop),
    F(CGContextSetFillColorWithColor, cg_noop), F(CGContextSetStrokeColorWithColor, cg_noop),
};
TABLE_END(coretext)

// ================================================================ UIFont + NSString(UIStringDrawing)

#define AR(x) objc_autorelease((id)(x))
static id uf_fontWithNameSize(objc_class *cls, SEL _c, id name, float size) { return AR(font_new(c_UIFont, size)); }
static id uf_systemFontOfSize(objc_class *cls, SEL _c, float size) { return AR(font_new(c_UIFont, size)); }
static id uf_empty(objc_class *cls, SEL _c) { return AR(ns_array_new()); }
static id uf_fontWithSize(ct_font *f, SEL _c, float size) { return AR(font_new(c_UIFont, size)); }
static id uf_fontName(ct_font *f, SEL _c) { return ns_str("NanumGothic"); }
static float uf_pointSize(ct_font *f, SEL _c) { return f->size; }
static float uf_systemFontSize(objc_class *cls, SEL _c) { return 14; } // iOS: 14 pt
static float uf_ascender(ct_font *f, SEL _c) { return ct_FontGetAscent(f); }
static float uf_descender(ct_font *f, SEL _c) { return -ct_FontGetDescent(f); }
static float uf_lineHeight(ct_font *f, SEL _c) { return units(face->height, f->size); }
static float uf_capHeight(ct_font *f, SEL _c) { return ct_FontGetCapHeight(f); }
static float uf_xHeight(ct_font *f, SEL _c) { return ct_FontGetXHeight(f); }
static float uf_leading(ct_font *f, SEL _c) { return ct_FontGetLeading(f); }
static const nmethod uifont_c[] = {
    NM("fontWithName:size:", uf_fontWithNameSize), NM("systemFontOfSize:", uf_systemFontOfSize),
    NM("boldSystemFontOfSize:", uf_systemFontOfSize), NM("italicSystemFontOfSize:", uf_systemFontOfSize),
    NM("systemFontSize", uf_systemFontSize), NM("labelFontSize", uf_systemFontSize),
    NM("familyNames", uf_empty), NM("fontNamesForFamilyName:", uf_empty), {0}};
static const nmethod uifont_i[] = {
    NM("fontWithSize:", uf_fontWithSize), NM("fontName", uf_fontName), NM("familyName", uf_fontName),
    NM("pointSize", uf_pointSize), NM("ascender", uf_ascender), NM("descender", uf_descender),
    NM("lineHeight", uf_lineHeight), NM("capHeight", uf_capHeight), NM("xHeight", uf_xHeight),
    NM("leading", uf_leading), {0}};

static float fsize(ct_font *f) { return f && (objc_is_kind(f, c_UIFont) || objc_is_kind(f, c_CTFont)) ? f->size : 12; }
static void s_sizeWithFont(CGSize *ret, id self, SEL _c, ct_font *f) {
    layout(ret, ns_cstr(self), fsize(f), 0, 0, 0, NULL, (CGRect){{0, 0}, {0, 0}});
}
static void s_sizeWithFontConstrained(CGSize *ret, id self, SEL _c, ct_font *f, CGSize max) {
    layout(ret, ns_cstr(self), fsize(f), max.w, max.h, 0, NULL, (CGRect){{0, 0}, {0, 0}});
}
static void s_sizeWithFontConstrainedLBM(CGSize *ret, id self, SEL _c, ct_font *f, CGSize max, int lbm) {
    s_sizeWithFontConstrained(ret, self, _c, f, max);
}
static void s_drawInRectFontLBMAlign(CGSize *ret, id self, SEL _c, CGRect r, ct_font *f, int lbm, int align) {
    layout(ret, ns_cstr(self), fsize(f), r.s.w, r.s.h, align, ui_current_ctx(), r);
}
static void s_drawInRectFontLBM(CGSize *ret, id self, SEL _c, CGRect r, ct_font *f, int lbm) {
    s_drawInRectFontLBMAlign(ret, self, _c, r, f, lbm, 0);
}
static void s_drawInRectFont(CGSize *ret, id self, SEL _c, CGRect r, ct_font *f) {
    s_drawInRectFontLBMAlign(ret, self, _c, r, f, 0, 0);
}
static void s_drawAtPointFont(CGSize *ret, id self, SEL _c, CGPoint p, ct_font *f) {
    layout(ret, ns_cstr(self), fsize(f), 0, 0, 0, ui_current_ctx(), (CGRect){p, {0, 0}});
}
static const nmethod nsstring_draw[] = {
    NM("sizeWithFont:", s_sizeWithFont), NM("sizeWithFont:constrainedToSize:", s_sizeWithFontConstrained),
    NM("sizeWithFont:constrainedToSize:lineBreakMode:", s_sizeWithFontConstrainedLBM),
    NM("drawInRect:withFont:lineBreakMode:alignment:", s_drawInRectFontLBMAlign),
    NM("drawInRect:withFont:lineBreakMode:", s_drawInRectFontLBM), NM("drawInRect:withFont:", s_drawInRectFont),
    NM("drawAtPoint:withFont:", s_drawAtPointFont), {0}};

// Agrega métodos a una clase nativa ya definida (categoría UIStringDrawing de UIKit sobre NSString).
static void add_methods(objc_class *c, const nmethod *add) {
    int n = 0, k = 0;
    for (const nmethod *m = c->vtable; m && m->sel; m++)
        n++;
    while (add[k].sel)
        k++;
    nmethod *t = calloc(n + k + 1, sizeof(nmethod));
    memcpy(t, c->vtable, n * sizeof(nmethod));
    for (int i = 0; i < k; i++)
        t[n + i] = (nmethod){sel_registerName(add[i].sel), add[i].imp};
    c->vtable = t;
}

static void set_methods(objc_class *c, const nmethod *m) {
    c->vtable = NULL;
    add_methods(c, m);
}

void coretext_init(void) {
    if (FT_Init_FreeType(&ft) || (FT_New_Face(ft, "app0:font.ttf", 0, &face) && FT_New_Face(ft, DATA_PATH "font.ttf", 0, &face))) {
        l_info("coretext: sin font.ttf (app0: ni " DATA_PATH "): CoreText/UIFont quedan en stubs");
        return;
    }
    coretext_ok = 1;
    if (FT_New_Face(ft, DATA_PATH "font_latin.ttf", 0, &latin) && FT_New_Face(ft, "app0:font_latin.ttf", 0, &latin))
        latin = NULL;
    FT_Stroker_New(ft, &stroker);
    l_info("coretext: font.ttf %s, %ld glifos, %d unidades/em; font_latin.ttf %s", face->family_name, face->num_glyphs,
           face->units_per_EM, latin ? "sí" : "no (sin tildes/ñ)");
    c_CGFont = objc_define("CGFont", "NSObject", sizeof(cgfont), NULL, NULL);
    cgfont.isa = c_CGFont;
    c_CTFont = objc_define("CTFont", "NSObject", sizeof(ct_font), NULL, NULL);
    c_CGPath = objc_define("CGPath", "NSObject", sizeof(cg_path), NULL, NULL);
    c_CFAttributedString = objc_define("NSAttributedString", "NSObject", sizeof(ct_astr), NULL, NULL);
    c_CTRun = objc_define("CTRun", "NSObject", sizeof(ct_run), NULL, NULL);
    c_CTLine = objc_define("CTLine", "NSObject", sizeof(ct_line), NULL, NULL);
    c_UIFont = objc_getClass("UIFont"); // uikit.c la define (sizeof isa + float) con stubs: se reemplazan
    set_methods(c_UIFont, uifont_i);
    set_methods(c_UIFont->isa, uifont_c);
    add_methods(objc_getClass("NSString"), nsstring_draw);
}
