# Pruebas e integración continua

## Pruebas locales

```sh
cmake --preset wsl-release && cmake --build --preset wsl-release
ctest --preset wsl-release           # todo
./build-wsl/engine_tests sim         # solo las pruebas cuyo nombre contiene "sim"
```

Para errores de memoria o comportamiento indefinido: `cmake --preset wsl-debug` (AddressSanitizer + UBSan).

| Grupo | Qué comprueba |
|---|---|
| `core/*` | JSON (ida y vuelta, errores con línea), RNG determinista y flujos independientes, reloj |
| `data/*` | carga del catálogo, jerarquía actividad–subestado, horarios que cruzan medianoche, **traducciones completas** |
| `world/*` | cada plantilla en cada tamaño genera una ciudad conectada con servicios; determinismo; exportar/importar; mapas personalizados mínimos e inválidos; rutas y cortes |
| `sim/*` | un día completo con todos los residentes coherentes y perfil horario realista; determinismo y **repetición con checksum**; eventos y reacciones; caos; escenarios; filtros, inspector, frame y CSV; mapas personalizados |
| `server/*` | parser HTTP y la API completa de extremo a extremo |
| `storage_benchmark`, `cli_validate_map` | herramientas de línea de comandos |
| `text_lint` | ningún archivo versionado tiene corrupción de texto |

## GitHub Actions

| Flujo | Cuándo | Qué hace |
|---|---|---|
| `.github/workflows/ci.yml` | cada *push* y *pull request* | compila y prueba en Ubuntu (GCC, Release y Debug con sanitizers) y en Windows (MSVC); ejecuta una simulación corta sin interfaz y verifica su repetición |
| `.github/workflows/docs.yml` | *push* a `main` y manual | compila, ejecuta el benchmark, regenera la referencia desde `data/`, construye esta wiki con MkDocs y la publica en **GitHub Pages**; adjunta el informe de rendimiento como artefacto |

Para activar la publicación: en GitHub, *Settings → Pages → Source: GitHub Actions*.

## Ver la wiki en local

```sh
pip install mkdocs-material
mkdocs serve        # http://127.0.0.1:8000
```
