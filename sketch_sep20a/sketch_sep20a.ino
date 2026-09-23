#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <cJSON.h>
#include <math.h>

const char* WIFI_NOMBRE = "canda-workshop";
const char* WIFI_CLAVE = "Handbook7";

// Placa: ESP32-C3 Super Mini (Arduino IDE: ESP32C3 Dev Module).
// GPIO, no posiciones físicas del conector. Servo 1 conserva GPIO 4.
constexpr int PINES_SERVOS[] = {4, 5, 6, 7};
constexpr int CANTIDAD_SERVOS = sizeof(PINES_SERVOS) / sizeof(PINES_SERVOS[0]);
constexpr int ANGULO_MIN = 0;
constexpr int ANGULO_MAX = 180;
constexpr unsigned long ESPERA_INICIAL_MS = 1000;

WebServer servidor(80);
Servo servos[CANTIDAD_SERVOS];

// delay = espera desde el keyframe anterior, antes de aplicar esta pose.
constexpr int MAX_KEYFRAMES = 32;
constexpr unsigned long MAX_DELAY_MS = 10000;
constexpr unsigned long MAX_DURACION_MS = 30000;
constexpr unsigned int MAX_JSON_BYTES = 8192;
struct Keyframe {
  int angulos[CANTIDAD_SERVOS];
  unsigned long esperaMs;
};
Keyframe animacion[MAX_KEYFRAMES];
int cantidadKeyframes = 0;
int siguienteKeyframe = 0;
unsigned long ultimoKeyframeMs = 0;
bool animando = false;

bool enteroEnRango(const cJSON* valor, int minimo, int maximo) {
  return cJSON_IsNumber(valor) && isfinite(valor->valuedouble) &&
         valor->valuedouble >= minimo && valor->valuedouble <= maximo &&
         floor(valor->valuedouble) == valor->valuedouble;
}

void actualizarAnimacion() {
  // La resta unsigned funciona incluso cuando millis() desborda.
  while (animando) {
    unsigned long ahora = millis();
    const Keyframe& frame = animacion[siguienteKeyframe];
    if (ahora - ultimoKeyframeMs < frame.esperaMs) return;
    for (int i = 0; i < CANTIDAD_SERVOS; i++) servos[i].write(frame.angulos[i]);
    ultimoKeyframeMs = ahora;
    siguienteKeyframe++;
    if (siguienteKeyframe >= cantidadKeyframes) animando = false;
  }
}

void recibirAnimacion() {
  if (animando) {
    servidor.send(409, "text/plain; charset=utf-8", "Hay una animacion en curso. No se reemplazo.");
    return;
  }
  const String cuerpo = servidor.arg("plain");
  if (cuerpo.length() == 0 || cuerpo.length() > MAX_JSON_BYTES) {
    servidor.send(413, "text/plain; charset=utf-8", "Se requiere un array JSON de hasta 8192 bytes.");
    return;
  }
  // require_null_terminated rechaza basura adicional despues del JSON.
  cJSON* json = cJSON_ParseWithLengthOpts(cuerpo.c_str(), cuerpo.length() + 1, nullptr, true);
  int cantidad = cJSON_IsArray(json) ? cJSON_GetArraySize(json) : 0;
  if (cantidad < 1 || cantidad > MAX_KEYFRAMES) {
    cJSON_Delete(json);
    servidor.send(400, "text/plain; charset=utf-8", "Se requieren entre 1 y 32 keyframes.");
    return;
  }
  // Validar la secuencia entera antes de modificar el estado o mover servos.
  Keyframe candidata[MAX_KEYFRAMES];
  unsigned long duracionMs = 0;
  bool valida = true;
  for (int k = 0; k < cantidad && valida; k++) {
    const cJSON* frame = cJSON_GetArrayItem(json, k);
    const cJSON* angulos = cJSON_GetObjectItemCaseSensitive(frame, "angulos");
    const cJSON* espera = cJSON_GetObjectItemCaseSensitive(frame, "delay");
    if (!cJSON_IsObject(frame) || !cJSON_IsArray(angulos) ||
        cJSON_GetArraySize(angulos) != CANTIDAD_SERVOS ||
        !enteroEnRango(espera, 0, MAX_DELAY_MS)) {
      valida = false;
      break;
    }
    candidata[k].esperaMs = (unsigned long)espera->valuedouble;
    duracionMs += candidata[k].esperaMs;
    if (duracionMs > MAX_DURACION_MS) { valida = false; break; }
    for (int i = 0; i < CANTIDAD_SERVOS; i++) {
      const cJSON* angulo = cJSON_GetArrayItem(angulos, i);
      if (!enteroEnRango(angulo, ANGULO_MIN, ANGULO_MAX)) { valida = false; break; }
      candidata[k].angulos[i] = (int)angulo->valuedouble;
    }
  }
  cJSON_Delete(json);
  if (!valida) {
    servidor.send(400, "text/plain; charset=utf-8",
                  "Keyframe invalido: 4 angulos enteros 0-180, delay entero 0-10000 ms y total hasta 30000 ms. No se movio ningun servo.");
    return;
  }
  for (int k = 0; k < cantidad; k++) animacion[k] = candidata[k];
  cantidadKeyframes = cantidad;
  siguienteKeyframe = 0;
  ultimoKeyframeMs = millis();
  animando = true;
  servidor.send(202, "application/json",
                String("{\"aceptada\":true,\"keyframes\":") + cantidad +
                ",\"duracionMs\":" + duracionMs + "}");
}

void estadoAnimacion() {
  servidor.send(200, "application/json",
                String("{\"animando\":") + (animando ? "true" : "false") +
                ",\"aplicados\":" + siguienteKeyframe +
                ",\"keyframes\":" + cantidadKeyframes + "}");
}

void moverTodos(int angulo) {
  for (int i = 0; i < CANTIDAD_SERVOS; i++) {
    servos[i].write(angulo);
  }
}

bool leerAngulo(const String& texto, int& angulo) {
  if (texto.length() == 0 || texto.length() > 3) {
    return false;
  }
  int valor = 0;
  for (unsigned int i = 0; i < texto.length(); i++) {
    if (texto[i] < '0' || texto[i] > '9') {
      return false;
    }
    valor = valor * 10 + (texto[i] - '0');
  }
  if (valor < ANGULO_MIN || valor > ANGULO_MAX) {
    return false;
  }
  angulo = valor;
  return true;
}

void recibirAngulos() {
  int angulos[CANTIDAD_SERVOS];
  // Validar el pedido entero antes de mover cualquiera de los servos.
  for (int i = 0; i < CANTIDAD_SERVOS; i++) {
    String parametro = String("angulo") + String(i + 1);
    if (!servidor.hasArg(parametro) ||
        !leerAngulo(servidor.arg(parametro), angulos[i])) {
      servidor.send(400, "text/plain; charset=utf-8",
                    String("Parametro ") + parametro +
                    " requerido: entero entre 0 y 180. No se movio ningun servo.");
      return;
    }
  }

  // Una orden manual valida interrumpe la animacion actual.
  animando = false;
  String respuesta = "{\"angulos\":[";
  for (int i = 0; i < CANTIDAD_SERVOS; i++) {
    servos[i].write(angulos[i]);
    if (i > 0) respuesta += ",";
    respuesta += String(angulos[i]);
  }
  respuesta += "]}";
  servidor.send(200, "application/json", respuesta);
}

void mostrarPagina() {
  servidor.send(200, "text/html; charset=utf-8", R"HTML(
    <!doctype html>
    <html lang="es">
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <meta charset="utf-8">
      <title>Control de los 4 servos</title>
      <h1>Control de los 4 servos</h1>
      <p>Ingresá los cuatro ángulos, entre 0° y 180°.</p>
      <form id="control">
        <p><label>Servo 1 (GPIO 4): <input name="angulo1" type="number" min="0" max="180" step="1" value="180" required></label></p>
        <p><label>Servo 2 (GPIO 5): <input name="angulo2" type="number" min="0" max="180" step="1" value="180" required></label></p>
        <p><label>Servo 3 (GPIO 6): <input name="angulo3" type="number" min="0" max="180" step="1" value="180" required></label></p>
        <p><label>Servo 4 (GPIO 7): <input name="angulo4" type="number" min="0" max="180" step="1" value="180" required></label></p>
        <button type="submit">Mover los cuatro servos</button>
      </form>
      <p id="estado" role="status"></p>
      <script>
        document.getElementById('control').addEventListener('submit', async (evento) => {
          evento.preventDefault();
          const parametros = new URLSearchParams(new FormData(evento.currentTarget));
          try {
            const respuesta = await fetch('/servos?' + parametros, { method: 'POST' });
            document.getElementById('estado').textContent = await respuesta.text();
          } catch {
            document.getElementById('estado').textContent = 'No se pudo contactar con la ESP32';
          }
        });
      </script>
    </html>
  )HTML");
}

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < CANTIDAD_SERVOS; i++) {
    servos[i].setPeriodHertz(50);
    // Conservar el rango de pulsos predeterminado de ESP32Servo.
    servos[i].attach(PINES_SERVOS[i]);
  }
  moverTodos(ANGULO_MIN);
  delay(ESPERA_INICIAL_MS);
  moverTodos(ANGULO_MAX);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_NOMBRE, WIFI_CLAVE);
  Serial.print("Conectando al Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Permitir que el HTML local lea las respuestas, incluidos errores HTTP.
  servidor.enableCORS(true);
  servidor.on("/servos", HTTP_OPTIONS, []() {
    // Preflight CORS: no ejecutar movimientos.
    servidor.send(204);
  });
  servidor.on("/animacion", HTTP_OPTIONS, []() { servidor.send(204); });
  servidor.on("/animacion", HTTP_POST, recibirAnimacion);
  servidor.on("/animacion", HTTP_GET, estadoAnimacion);
  servidor.on("/", HTTP_GET, mostrarPagina);
  // GET permite probar desde la barra del navegador; POST desde clientes HTTP.
  servidor.on("/servos", HTTP_GET, recibirAngulos);
  servidor.on("/servos", HTTP_POST, recibirAngulos);
  servidor.begin();

  Serial.println();
  Serial.print("Abrí en tu navegador: http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  servidor.handleClient();
  actualizarAnimacion();
}
