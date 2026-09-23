# Animaciones por keyframes

Cargar `sketch_sep20a/sketch_sep20a.ino` en el ESP32-C3 y abrir `index.html` en un navegador. LM Studio debe tener su servidor y CORS habilitados. La LLM elige una cara y un gesto; la página convierte el gesto en una secuencia editable y la manda completa al ESP32.

## Contrato HTTP

`POST http://192.168.68.51/animacion`

`Content-Type: application/json`

El cuerpo es un array, sin objeto envolvente:

```json
[
  { "angulos": [90, 90, 35, 60], "delay": 0 },
  { "angulos": [90, 90, 90, 90], "delay": 400 },
  { "angulos": [90, 90, 35, 60], "delay": 400 },
  { "angulos": [90, 90, 90, 90], "delay": 700 }
]
```

Cada `delay` es la espera en milisegundos **antes de aplicar esa posición**, contada desde la anterior. La primera espera se cuenta desde la aceptación del pedido. En el ejemplo se aplican poses aproximadamente a los 0, 400, 800 y 1500 ms. Se aplican los cuatro ángulos juntos; no hay interpolación entre poses. La duración es nominal y puede aumentar por el tiempo de procesamiento de otros pedidos HTTP.

Orden de `angulos`:

1. Hombro izquierdo: servo 1 / GPIO 4.
2. Codo izquierdo: servo 2 / GPIO 5.
3. Hombro derecho: servo 3 / GPIO 6.
4. Codo derecho: servo 4 / GPIO 7.

Los valores iniciales requieren ajuste al montaje real. Al terminar, queda aplicada la última pose; las animaciones iniciales incluyen el retorno a reposo explícitamente.

Respuesta `202 Accepted`:

```json
{ "aceptada": true, "keyframes": 4, "duracionMs": 1500 }
```

Confirma la aceptación de la secuencia, no su finalización ni una medición física de los servos. `GET /animacion` permite consultar el progreso:

```json
{ "animando": true, "aplicados": 2, "keyframes": 4 }
```

- Se valida toda la secuencia antes de mover: entre 1 y 32 keyframes, exactamente 4 ángulos enteros de 0 a 180, esperas enteras de 0 a 10000 ms y suma máxima de 30000 ms.
- JSON o valores inválidos: `400`; cuerpo vacío o superior a 8192 bytes: `413`. No comienza ningún movimiento.
- Si otra animación está activa: `409`, sin reemplazarla ni encolarla.
- El endpoint anterior `/servos?angulo1=…&angulo2=…&angulo3=…&angulo4=…` sigue disponible. Una orden manual válida interrumpe la animación y aplica la pose; una inválida no la interrumpe.
- `OPTIONS /animacion` atiende el preflight CORS sin mover servos.
- La secuencia se ejecuta desde `loop()` con `millis()`, sin usar `delay()` durante la animación. El servidor sigue atendiendo pedidos.
- No se reintentan movimientos automáticamente ante errores de red.

## Editor y persistencia

En “Conexión y animaciones de los brazos” se editan los cuatro ángulos y la espera de cada paso, y se agregan o quitan keyframes. La configuración y el historial se guardan en el navegador. Las poses guardadas por la versión anterior se migran a las animaciones iniciales conservando sus ángulos.

## Verificación

Desde la raíz del proyecto:

```sh
node tests/animation.test.cjs
python3 tests/scheduler.test.py
```

La primera prueba usa respuestas HTTP simuladas y verifica el contrato, el historial, la persistencia, la migración y los errores. La segunda requiere un compilador C++ y prueba la función de temporización real con reloj y servos simulados, incluido el desbordamiento de `millis()`. No sustituyen una prueba del movimiento físico.
