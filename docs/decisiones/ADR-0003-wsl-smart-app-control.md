# ADR-0003: Compilar y ejecutar en WSL por Smart App Control

- **Estado:** Aceptada (2026-11-21)
- **Contexto:** En el equipo de desarrollo (Windows 11 con Smart App Control activado) los ejecutables compilados con MSVC
  se bloqueaban de forma intermitente: “Una directiva de Control de aplicaciones bloqueó este archivo”. Smart App Control
  impide ejecutar programas sin firma de confianza y sin reputación en la nube de Microsoft, y cada compilación genera un
  binario nuevo y desconocido. El registro de Code Integrity confirmaba los bloqueos.
- **Opciones consideradas:**
    1. **Compilar y ejecutar en WSL 2 (Ubuntu).** Los binarios Linux se ejecutan dentro de WSL y no pasan por Smart App Control.
    2. Compilar el motor a WebAssembly (sin `.exe`).
    3. Desactivar Smart App Control (en muchas versiones de Windows 11 no se puede reactivar sin reinstalar Windows).
    4. Firmar los ejecutables con un certificado de confianza (de pago; un certificado autofirmado no sirve para Smart App Control).
- **Decisión:** Opción 1. El flujo principal es WSL: presets `wsl-release`/`wsl-debug`, `tools/wsl.sh`, y el servidor
  accesible desde el navegador de Windows en `localhost` (WSL 2 lo reenvía). El código sigue siendo portable y la CI compila
  también con MSVC en Windows, donde no hay Smart App Control.
- **Consecuencias:** Se mantiene la protección del sistema. Las cifras de rendimiento son de Linux. El *hook* de pre-commit
  ejecuta `text_lint` a través de WSL cuando se usa Git para Windows.
