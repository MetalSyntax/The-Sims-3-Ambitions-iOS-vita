// Parches de The Sims 3 Ambitions, heredados de The Sims 3 / Medieval (mismo framework EA). Solo lo que el análisis
// imports sin implementar sale en el log como "unimpl" y se ataca con evidencia de consola.
#include <stdlib.h>
#include <string.h>

#include "imports.h"
#include "log.h"
#include "macho.h"
#include "objc.h"
#include "patch.h"

const char *darwin_path(const char *in, char *out, size_t outlen); // darwin.c

// ---------------------------------------------------------------- zlib (z_stream = 56 bytes en los dos)
void crc32(void), deflate(void), deflateEnd(void), deflateInit2_(void), get_crc_table(void), inflate(void),
    inflateEnd(void), inflateInit2_(void);

// ---------------------------------------------------------------- sqlite3 (SceSqlite, misma API)
extern int sqlite3_open(const char *, void **);
void sqlite3_bind_int(void), sqlite3_changes(void), sqlite3_column_int(void), sqlite3_last_insert_rowid(void),
    sqlite3_prepare(void), sqlite3_reset(void);
static int p_sqlite3_open(const char *path, void **db) {
    char p[512];
    int r = sqlite3_open(darwin_path(path, p, sizeof(p)), db);
    l_info("sqlite3_open(%s) -> %d", p, r);
    return r;
}

// ---------------------------------------------------------------- libxml2 (libxml2.a del VitaSDK)
// Sin headers de libxml: solo se toman las direcciones.
void xmlCopyNode(void), xmlDocDumpFormatMemory(void), xmlDocDumpMemory(void), xmlDocGetRootElement(void),
    xmlFreeDoc(void), xmlFreeNode(void), xmlGetLastError(void), xmlNodeDumpOutput(void), xmlNodeListGetString(void),
    xmlOutputBufferClose(void), xmlOutputBufferFlush(void), xmlParseDoc(void), xmlReadMemory(void),
    xmlResetLastError(void), xmlXPathEvalExpression(void), xmlXPathFreeContext(void), xmlXPathFreeObject(void),
    xmlXPathNewContext(void);
extern void *xmlFree; // variable (puntero a free) que el juego llama directo
extern void *xmlOutputBufferCreateIO(void *iowrite, void *ioclose, void *ioctx, void *encoder);
// Los callbacks de escritura son código del juego: se llaman con call_guest (r9) desde libxml.
typedef struct {
    void *wr, *cl, *ctx;
} xio;
static int xio_write(xio *x, const char *buf, int len) {
    return (int)call_guest(x->wr, (uint32_t)x->ctx, (uint32_t)buf, (uint32_t)len, 0);
}
static int xio_close(xio *x) {
    int r = x->cl ? (int)call_guest(x->cl, (uint32_t)x->ctx, 0, 0, 0) : 0;
    free(x);
    return r;
}
static void *p_xmlOutputBufferCreateIO(void *wr, void *cl, void *ctx, void *enc) {
    xio *x = malloc(sizeof(*x));
    *x = (xio){wr, cl, ctx};
    void *b = xmlOutputBufferCreateIO(wr ? (void *)xio_write : NULL, (void *)xio_close, x, enc);
    if (!b)
        free(x);
    return b;
}

// ---------------------------------------------------------------- libc
// newlib: FILE con el mismo _p/_r que Darwin (macro getc inline del juego -> __srget al vaciarse el buffer).
// En el hosttest el FILE es de glibc (otra disposición): ahí no hay equivalente y se devuelve EOF.
#ifdef HOSTTEST
static int p_srget(void *f) { return -1; }
#else
void __srget(void);
#endif
static int p_zero(void) { return 0; }
// statfs de Darwin 32 bits (sin INODE64): f_bsize@4 f_iosize@8 f_blocks@12 f_bfree@16 f_bavail@20
static int p_statfs(const char *path, uint32_t *st) {
    memset(st, 0, 64);
    st[1] = st[2] = 4096;
    st[3] = 1 << 20;
    st[4] = st[5] = 1 << 18; // 1 GB libre
    return 0;
}

const import_entry patch_imports[] = {
    D(crc32), D(deflate), D(deflateEnd), D(deflateInit2_), D(get_crc_table), D(inflate), D(inflateEnd),
    D(inflateInit2_),
    F(sqlite3_open, p_sqlite3_open), D(sqlite3_bind_int), D(sqlite3_changes), D(sqlite3_column_int),
    D(sqlite3_last_insert_rowid), D(sqlite3_prepare), D(sqlite3_reset),
    D(xmlCopyNode), D(xmlDocDumpFormatMemory), D(xmlDocDumpMemory), D(xmlDocGetRootElement), D(xmlFreeDoc),
    D(xmlFreeNode), D(xmlGetLastError), D(xmlNodeDumpOutput), D(xmlNodeListGetString), D(xmlOutputBufferClose),
    D(xmlOutputBufferFlush), D(xmlParseDoc), D(xmlReadMemory), D(xmlResetLastError), D(xmlXPathEvalExpression),
    D(xmlXPathFreeContext), D(xmlXPathFreeObject), D(xmlXPathNewContext), V(xmlFree, &xmlFree),
    F(xmlOutputBufferCreateIO, p_xmlOutputBufferCreateIO),
#ifdef HOSTTEST
    F(__srget, p_srget),
#else
    D(__srget),
#endif
    F(flockfile, p_zero), F(funlockfile, p_zero), F(statfs, p_statfs),
};
TABLE_END(patch)

// ---------------------------------------------------------------- GLES1 forzado
// El juego trae dos renderers (cadenas "OpenGLES1"/"OpenGLES2", shaders GLSL embebidos e imports
// de los dos). El runtime solo tiene probado GLES1 (incluida la paleta de matrices OES que usa el
// skinning de ES1) y no exporta GLES2: initWithAPI:2 -> nil para que el juego caiga al camino ES1.
static id (*ea_init)(id, SEL, int);
static id (*ea_init_share)(id, SEL, int, id);
static id p_initWithAPI(id self, SEL c, int api) {
    if (api != 1) {
        l_info("EAGLContext initWithAPI:%d -> nil (forzado GLES1)", api);
        return NULL;
    }
    return ea_init(self, c, api);
}
static id p_initWithAPIShare(id self, SEL c, int api, id share) {
    if (api != 1) {
        l_info("EAGLContext initWithAPI:%d sharegroup: -> nil (forzado GLES1)", api);
        return NULL;
    }
    return ea_init_share(self, c, api, share);
}

void patch_init(void) {
    // vtable de una clase nativa = copia en heap con SEL internados (objc.c intern_table): se edita en el lugar.
    objc_class *c = objc_getClass("EAGLContext");
    SEL s1 = sel_registerName("initWithAPI:"), s2 = sel_registerName("initWithAPI:sharegroup:");
    for (nmethod *m = c ? c->vtable : NULL; m && m->sel; m++) {
        if (m->sel == s1)
            ea_init = m->imp, m->imp = (void *)p_initWithAPI;
        else if (m->sel == s2)
            ea_init_share = m->imp, m->imp = (void *)p_initWithAPIShare;
    }
    if (!ea_init || !ea_init_share)
        l_error("patch: EAGLContext sin initWithAPI: nativo, GLES1 no forzado");
}
