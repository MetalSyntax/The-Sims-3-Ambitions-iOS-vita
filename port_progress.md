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
