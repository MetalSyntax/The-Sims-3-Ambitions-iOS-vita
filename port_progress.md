# The Sims 3 Ambitions — Progreso del Port (iOS Mach-O)

Documentación completa y arquitectura en `PORTING_PLAN.md`.

## Resumen de Estado

- **Juego:** The Sims 3 Ambitions
- **Plataforma Original:** iOS (Mach-O ARMv7 32-bit en binario universal FAT)
- **Motor:** EA Mobile / Ideaworks 3D Engine (`m3g` / Mobile 3D Graphics format)
- **Title ID:** `PSVSIMS3A`
- **Slug:** `sims3ambitions`
- **Build Actual:** `build/sims3ambitions.vpk` (1.0 MB) & `build/eboot.bin` (954 KB) compilados exitosamente.
- **Estado:** Listo para despliegue y pruebas directas en PS Vita física.

---

## Fases del Port

### Phase 1: Repo and tools ✅
- [x] IPA extraído (`sims3ambitions_extract/Payload/sims3dp_iphone.app`), symlink `game_app` funcional.
- [x] Binario descifrado verificado (`cryptid 0`).
- [x] `rebase.bin` generado con 2970 palabras de relocalización sobre la slice armv7 (`docs/macho_report.txt`).
- [x] Scripts de compilación y empaquetado `build.sh` y `CMakeLists.txt` operativos.

### Phase 2: Decompilation and inventory ✅
- [x] Dumps generados en `decompiled/macho/`:
  - `frameworks.txt`: `OpenGLES`, `OpenAL`, `AVFoundation`, `UIKit`, `Foundation`, `QuartzCore`, `CoreGraphics`, etc.
  - `objc_classes.txt`: `RuntimeAppDelegate`, `EAGLView`, `AudioQueueStructObject`, etc.
  - `symbols.txt`: Tabla de símbolos extraída.
  - `imports.txt`: 499 imports únicos clasificados.
- [x] Funciones OpenGL ES 1.1 mapeadas a vitaGL: `glDrawElements`, `glAlphaFuncx`, `glClearColorx`, `glColorPointer`, `glCullFace`, `glDepthRangex`, `glFrontFace`, `glGetError`, `glHint`, `glMaterialx`, `glMaterialxv`, `glNormalPointer`.

### Phase 3: Real engine analysis ✅
- [x] Motor identificado: **EA Mobile / Ideaworks 3D Engine**:
  - Formato de modelos 3D y animaciones: `.m3g` (Mobile 3D Graphics).
  - Archivos de datos: `simsdata.bin`, `simsui.bin`.
  - Punto de entrada: `LC_MAIN` con `entryoff = 1040149` (`0xFD715` en Thumb-2).
  - Ciclo de vida: `RuntimeAppDelegate` (`applicationDidFinishLaunching:`, `applicationDidBecomeActive:`, `applicationWillResignActive:`, `applicationWillTerminate:`).
  - Vista gráfica: `EAGLView` (`getGLESMajorVersion`, `layoutSubviews`, `viewRenderbuffer`, `viewFramebuffer`).
  - Audio: `AudioQueueStructObject` (`initWithAudioQueue:`) + `AudioFile*` + `OpenAL`.

### Phase 4: Loader and ObjC bootstrap ✅
- [x] `macho_load()` mapea segmentos en `MACHO_SLIDE = 0x98000000`, procesa `rebase.bin` y enlaza símbolos indirectos.
- [x] Soporte para `LC_MAIN` y `LC_UNIXTHREAD` integrado en `macho.c` para resolver el punto de entrada incluso en binarios stripteados (`macho_entry()`).
- [x] Trampolines `ml_align_call` en `asm.S` para realinear la pila de 4 a 8 bytes de forma segura.
- [x] Protección de símbolos de datos (`is_data_symbol`): asigna bloques de memoria cero (`make_unimpl_data`) a símbolos `_OBJC_CLASS_$_*`, `_OBJC_METACLASS_$_*`, `_kCF*` y RTTI vtables.
- [x] Shims POSIX y runtime C++ en `source/patch.c`: `open`, `read`, `write`, `lseek`, `fstat`, `remove`, `getenv`, `setenv`, `printf`, `fprintf`, `operator new`, `operator delete`.
- [x] Gestor de logs rotativos (`ux0:data/sims3ambitions/logs/sims3ambitions_NNN.log`).

### Phase 5: Graphics (Build ✅, Prueba de hardware en progreso)
- [x] `vendor/vitaGL` compilado en modo `SOFTFP_ABI=1` con redirección de viewport float a VFP (`sceGxmSetViewport_sfp`).
- [x] Inicialización con `vglInitExtended` en resolución 960x544.
- [x] 12 llamadas adicionales de GLES1 vinculadas en `source/gles.c`.
- [ ] Verificación en pantalla del renderizado de la ciudad y sims.

### Phase 6: Touch ✅
- [x] Capa de entrada en `source/input.c` con escalado y traslación de coordenadas táctiles frontales a espacio lógico.
- [ ] Verificación de interacción en menús in-game y acciones de sims.

### Phase 7: Physical controls ✅
- [x] Esquema de mapeo diseñado:
  - Stick Analógico Izquierdo: paneo de cámara / navegación de ciudad.
  - Stick Derecho / Cruceta: zoom in / zoom out.
  - ✕ (Cruz): Confirmar / interactuar con Sims u objetos.
  - ○ (Círculo): Cancelar / Atrás.
  - START: Pausa / Menú del juego.
- [ ] Ajuste fino de respuesta analógica en hardware físico.

### Phase 8: Audio
- [x] Rutas de `AudioQueue` y `AudioFile` identificadas.
- [x] Enrutamiento a `sceAudioOut` en `source/coreaudio.c`.
- [ ] Prueba de música y voces en Simlish en la consola.

### Phase 9: Videos / FMV
- [x] Símbolo `MPMoviePlayerController` registrado.
- [ ] Salto seguro en caso de ausencia de vídeos.

### Phase 10: Assets, data, language and saves ✅
- [x] Datos de la app redirigidos a `ux0:data/sims3ambitions/sims3dp_iphone.app/`.
- [x] Partidas guardadas y configuración a `ux0:data/sims3ambitions/saves/`.
- [x] Detección de idioma del sistema mediante `sceAppUtilSystemParamGetInt`.
- [ ] Validación de guardado persistente.

### Phase 11: Performance ✅
- [x] Overclock configurado por defecto en `main.c` (CPU 444 MHz, GPU 222 MHz).
- [ ] Monitoreo de tasa de cuadros en hardware real.

### Phase 12: Configurator and options
- [ ] Configuración persistente opcional para ajustes visuales y controles.

### Phase 13: Trophies
- [ ] Mapeo opcional de logros a `sceNpTrophy`.

### Phase 14: LiveArea and VPK ✅
- [x] Configuración de `param.sfo` con TITLEID `PSVSIMS3A`.
- [x] Assets de LiveArea listos.
- [x] Generación confirmada de `eboot.bin` y paquete `sims3ambitions.vpk`.

### Phase 15: Real-hardware testing and release (Siguiente paso)
- [ ] Desplegar datos: `psvita-toolkit deploy --data`
- [ ] Desplegar VPK: `psvita-toolkit deploy`
- [ ] Ejecutar en consola PS Vita y revisar `sims3ambitions_001.log`.

### Revisión 2026-11-16: migrado al runtime maestro (Ferrari)
Universal armv7/armv7s/arm64 (SDK 8.3, clang): se usa el slice armv7 (`PORT_IOS_PREP_ARGS "--arch armv7"`), PIE →
rebase exacto (22735 palabras, 2648 binds). `patch.c` heredado de The Sims 3 / Medieval (mismo framework EA).
Arreglos (también en el maestro Ferrari): `MainWindow~iphone.nib` (sufijo de dispositivo y nib en carpeta del SDK 6+
con `runtime.nib`), CFString `GetCharacterAtIndex` / `CreateCopy` / `CreateWithSubstring`, `__srget` de newlib.
Hosttest: main → RuntimeAppDelegate (nib) → EAGLContext GLES1 320x480 → OpenAL → acelerómetro → crash con puntero
inválido devuelto por 0xb3ec (binario sin símbolos). Ojo: el hosttest no emula el `getc` inline del juego (FILE de
glibc ≠ Darwin; en la Vita newlib sí coincide), así que este crash puede no repetirse en consola.

### Corridas #003–#005 (2026-11-18)
- **#003**: crash en 0x1172a (getter `this->m8` con NULL) al comparar el idioma con "en": la lista de idiomas sale de
  `CFBundleCopyBundleLocalizations` (sin implementar → vacía → elemento NULL). Ahora lista las `.lproj` reales del
  bundle; también `kCFLocaleLanguageCode` y `kCFAllocatorNull`.
- **#004**: sin crash, pero sin textos y sin toques. (a) Toques "SIN VISTA": `-[RuntimeAppDelegate setWindow:]` es
  sintetizado con `objc_setProperty_nonatomic` (sin implementar) → el outlet `window` del nib quedaba nil y el
  `addSubview:`/`makeKeyAndVisible` del juego iban a nil. Implementado (orden de args: self, _cmd, val, offset).
  También `-[NSString compare:options:]` (NSNumericSearch: el juego compara `systemVersion`).
  (b) Textos: faltaba `font.ttf` en DATA_PATH. Traídos de The-Sims-3-iOS-vita (mismo motor EA, `GraphicsIPhoneCG`)
  `font.ttf` + `font_latin.ttf` (respaldo de tildes/ñ en `layout()` de coretext.c), `+[UIFont systemFontSize]`,
  `%C`/`%S` en darwin_printf (anchos por carácter), CoreGraphics: Save/RestoreGState, SetFillColor, mezcla
  source-over, trazados con relleno y modos Clear(16)/Copy(17) (`clear()` de los textos), CGContextRetain,
  `CGDataProviderCreateSequential` + `CGImageCreateWithPNGDataProvider` (texturas de los Sims), animaciones de
  UIView con `animationDidStop` diferido, `fstat` ino64 (`PORT_STAT_INO64`: si no, `sim3set` se resetea).
  (c) Pantalla completa: pantalla de iPhone 5 (320x568, `iPhone5,1`) como Zenonia 5; el juego trae LaunchImage-568h
  y assets `*_568`.
- Hosttest: llega a 320x568 con fuentes y luego cae en el crash conocido del `getc` inline (solo hosttest).
- **#005**: pantalla completa OK. Texto: las fuentes no estaban subidas a la consola (`coretext: sin font.ttf`).
  Toque: llegaba a una `UIView` genérica: `setRootViewController:` monta la vista por defecto del controller y el
  juego luego hace `[rootVC setView:eaglView]` + `sendSubviewToBack:`; la vieja quedaba encima. `vc_setView` ahora
  reemplaza la vista montada en su superview.
- **#006**: toque OK (llega a `EAGLView`, el juego avanza de pantalla). Las fuentes seguían sin subir: subidas con
  `deploy_data.py`. Agregados `CFStringCreateWithFormat(AndArguments)` (EA mide cada letra con `%C`) y
  `-[NSFileManager attributesOfFileSystemForPath:error:]` (tamaños fijos). eboot.bin subido directo a ux0:app/PSVSIMS3A.
- **#007**: textos visibles pero al revés y cortados. EA dibuja `drawAtPoint:withFont:` en su CGBitmapContext sin
  voltear el CTM; coretext.c solo transformaba el origen y copiaba el bitmap hacia abajo (válido con el CTM volteado
  de cocos2d). Ahora la ruta UIKit (`layout`) pasa cada píxel del glifo por el CTM, como The-Sims-3-iOS-vita.
- **#008**: textos legibles. Menú a ~3 fps (300–450 ms de "resto"): bajó de ~18 fps al activarse las fuentes
  (007), mismos draws, sin subidas glTexImage2D → EA mide/dibuja texto cada frame y coretext recargaba y
  rasterizaba el contorno de cada letra en cada llamada. Caché de glifos (2048 entradas por fuente/glifo/tamaño),
  mapeo por CTM con derivadas por glifo, clear de FillPath por tramo, PORT_GL_DIAG 0. Perfil nuevo en el log:
  `PERF+ texto / CoreGraphics / glTexSubImage2D` para ver qué queda de los ~52 ms base (18 fps sin texto).
