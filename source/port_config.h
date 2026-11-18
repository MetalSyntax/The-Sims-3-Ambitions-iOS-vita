#pragma once
// Configuración propia de este juego (el resto de source/ es común a los ports iOS de la familia).
#define PORT_NAME "The Sims 3 Ambitions"
#define PORT_SLUG "sims3ambitions"
#define PORT_EXEC "sims3dp_iphone"      // v1.1.84 universal (armv7/armv7s/arm64), cryptid 0: se usa armv7
#define PORT_IOS_PREP_ARGS "--arch armv7"
#define PORT_ROTATION 1                 // Info.plist: LandscapeRight
#define PORT_STRETCH 0
#define PORT_SYSTEM_VERSION "8.3"       // MinimumOSVersion 6.1, SDK 8.3
#define PORT_DEVICE_MACHINE "iPhone5,1" // 4" 16:9 como la Vita
#define PORT_STAT_INO64 1
#define GLES_POOL_MB 32
#define PORT_ACCEL_FROM_STICK 0
#define PORT_SCREEN_W 320
#define PORT_SCREEN_H 568 // iPhone 5 (trae LaunchImage-568h y assets *_568): pantalla completa sin estirar, como Zenonia 5
#define PORT_SCREEN_SCALE 1.0f
#define PORT_GL_DIAG 0 // la imagen ya se ve: sin capturas TGA ni trazas de frame
