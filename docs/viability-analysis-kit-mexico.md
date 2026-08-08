# Análisis de Viabilidad Comercial — Kit de Iluminación Inteligente para Escaleras

## Mercado Mexicano: Steren Smart Home

**Fecha:** Agosto 2026
**Proyecto:** `lights_control` — Controlador de iluminación para escaleras con ESP32-S3
**Objetivo:** Evaluar la viabilidad de comercializar el proyecto como kit en México, compitiendo/complementando la línea Smart Home de Steren.

---

## 1. Panorama del Mercado

### 1.1 Tamaño y Crecimiento

| Indicador | Valor |
|-----------|-------|
| Mercado domótica México 2024 | **USD $1.09B** |
| Proyección 2033 | **USD $3.41B** |
| CAGR 2025–2033 | **12.1%** |
| Mercado smart homes 2024 | **USD $1.7B** |
| Proyección smart homes 2033 | **USD $4.8B** |
| Crecimiento real anual México | **~25%** |
| Segmento dominante | Apps y plataformas de control (47.8%) |

**Fuentes:** IMARC Group, Grupo Milenio, ABI Research.

**Conclusión:** El mercado mexicano de automatización del hogar crece a doble dígito, impulsado por la accesibilidad de dispositivos, la penetración de smartphones y la conciencia de eficiencia energética. Existe un espacio claro para productos de nicho (iluminación inteligente para escaleras) que los grandes retailers no están cubriendo con soluciones dedicadas.

### 1.2 Steren — El Competidor de Referencia

Steren es la cadena de electrónica más grande de México con más de 400 tiendas físicas, presencia en línea, y una línea Smart Home en crecimiento bajo la marca **"Sterren Home"** (app propia, compatible con Alexa y Google Assistant).

**Productos Steren Smart Home relevantes:**

| Producto | Precio aprox. (MXN) | Tecnología |
|----------|---------------------|------------|
| Mini sensor PIR para techo | ~$180–250 | PIR pasivo (solo movimiento), sin WiFi |
| Socket con sensor de movimiento y luminosidad | ~$250–350 | PIR + fotorresistencia, 360°, temporizador ajustable |
| Foco LED 10W con sensor de movimiento | ~$200–300 | PIR integrado en foco |
| Tira LED WiFi RGB+W 2m | ~$400–500 | WiFi, app Steren Home, RGB+blanco cálido |
| Tira LED Neón WiFi RGB 5m exterior | ~$500–700 | WiFi, RGB, 5m, exterior |
| Interruptor WiFi | ~$300–400 | WiFi, control por app |

**Lo que Steren NO ofrece:**
- ❌ Sensor de presencia ToF (detección de personas aunque estén quietas)
- ❌ Sincronización automática con hora de atardecer
- ❌ Fade-in / fade-out PWM suave (evita cambios bruscos)
- ❌ Dashboard web integrado (sin necesidad de app)
- ❌ Solución específica para escaleras (2 sensores, arriba + abajo)
- ❌ IP estática configurable (no depender de DHCP/descubrimiento)

---

## 2. El Producto: Kit de Iluminación Inteligente para Escaleras

### 2.1 Componentes del Kit

**Opción A — Kit Controlador (sin tira LED ni fuente):**

| Componente | Cant. | Costo unitario (MXN) — lote pequeño (AliExpress, 10–50u) | Costo unitario (MXN) — lote grande (100–500u, directo fábrica) |
|------------|-------|----------------------------------------------------------|---------------------------------------------------------------|
| ESP32-S3 SuperMini | 1 | $80 | $50 |
| VL53L0X ToF sensor | 2 | $60 c/u → $120 | $30 c/u → $60 |
| BH1750 sensor luz | 1 | $40 | $16 |
| IRLZ44N MOSFET | 1 | $16 | $6 |
| PCB personalizada + pasivos | 1 | $60 | $40 |
| Carcasa 3D (4 piezas) | 1 kit | $100 | $30 (inyección) |
| Cableado, jumper, headers | 1 kit | $40 | $20 |
| Empaque + manual | 1 | $40 | $20 |
| **Subtotal BOM controlador** | | **$496** | **$242** |

**Opción B — Kit Completo (con tira LED y fuente):**

| Añadir a Opción A | Cant. | Lote pequeño | Lote grande |
|-------------------|-------|-------------|-------------|
| Fuente 12V 5A certificada | 1 | $120 | $60 |
| Tira LED 12V 5m blanco cálido | 1 | $100 | $50 |
| Conectores DC + montaje | 1 kit | $30 | $15 |
| **Subtotal kit completo** | | **$746** | **$367** |

### 2.2 Costos por Unidad (lote de 100 unidades — punto de equilibrio realista)

**Opción A — Controlador:**

| Concepto | Costo (MXN) |
|----------|-------------|
| BOM (lote 100u, promedio entre pequeño y grande) | $350 |
| Ensamble + prueba (mano de obra MX) | $50 |
| Envío/fulfillment (Mercado Envíos) | $60 |
| Comisión plataforma (MercadoLibre ~14.5%) | $101 |
| Marketing/ads por unidad | $50 |
| Reserva garantía (3%) | $21 |
| **Costo total por unidad** | **$632** |

**Opción B — Kit Completo:**

| Concepto | Costo (MXN) |
|----------|-------------|
| BOM (lote 100u) | $530 |
| Ensamble + prueba | $60 |
| Envío/fulfillment | $100 |
| Comisión plataforma (14.5%) | $145 |
| Marketing/ads | $60 |
| Reserva garantía (3%) | $30 |
| **Costo total por unidad** | **$925** |

### 2.3 Precio de Venta Sugerido

| Opción | Precio sugerido (MXN) | Margen bruto | Margen neto | Posicionamiento vs Steren |
|--------|----------------------|-------------|-------------|--------------------------|
| **A — Controlador** | **$899** | $549 (61%) | $267 (30%) | Premium: más caro que sensor PIR, más barato que tira WiFi Steren |
| **B — Kit Completo** | **$1,299** | $769 (59%) | $374 (29%) | Comparable a tira LED WiFi Steren 5m ($500–700) + sensor (~$250) = ~$750–950, pero con más funcionalidad |

### 2.4 Comparativa de Valor vs Steren

| Característica | Kit Propuesto | Sensor PIR Steren | Tira LED WiFi Steren |
|---------------|---------------|-------------------|----------------------|
| Detección con persona quieta | ✅ (ToF) | ❌ (solo movimiento) | ❌ |
| Lux umbral inteligente | ✅ (BH1750) | ✅ (fotorresistencia básica) | ❌ |
| Sincronización atardecer | ✅ (API internet) | ❌ | ❌ |
| Fade PWM suave | ✅ (8-bit, 5kHz) | ❌ (on/off brusco) | ✅ (app) |
| Dashboard web integrado | ✅ (sin app) | ❌ | Solo por app Steren |
| Doble sensor (arriba/abajo) | ✅ | ❌ (un solo punto) | ❌ |
| IP estática configurable | ✅ | N/A | DHCP |
| Open source / modificable | ✅ | ❌ | ❌ |
| Compatible Alexa/Google | ❌ (sin skill) | ❌ | ✅ |
| Garantía / soporte México | Por definir | ✅ Steren | ✅ Steren |

---

## 3. Inversión Inicial Requerida

| Rubro | Costo (MXN) | Costo (USD) | Notas |
|-------|------------|-------------|-------|
| Diseño PCB + prototipado | $12,000 | ~$600 | 2 iteraciones de prototipo |
| Certificación NOM (seguridad eléctrica) | $20,000 | ~$1,000 | Obligatorio para venta en México |
| Certificación IFT (WiFi/Bluetooth) | $30,000 | ~$1,500 | Obligatorio para equipos con WiFi |
| Producción inicial (100 unidades Opción A) | $35,000 | ~$1,750 | BOM del primer lote |
| Carcasa — molde de inyección (opcional) | $25,000 | ~$1,250 | Solo si se escala; inicialmente 3D |
| Diseño empaque + impresión primer tiraje | $8,000 | ~$400 | Caja, inserto, manual impreso |
| E-commerce (fotos, listings, contenido) | $10,000 | ~$500 | MercadoLibre, Shopify, landing |
| Marketing inicial (3 meses) | $18,000 | ~$900 | Ads MercadoLibre + Meta + Google |
| Desarrollo firmware complementario | $0 | $0 | Ya está desarrollado (open source) |
| Legal / constitución / registro marca | $12,000 | ~$600 | Acta constitutiva + registro IMPI |
| **TOTAL INVERSIÓN** | **$170,000** | **~$8,500** | |

> **Nota sobre certificaciones:** NOM-001-SCFI (seguridad) e IFT-008 (telecom) son obligatorias legalmente en México para comercializar productos electrónicos con WiFi. Existen laboratorios autorizados como NYCE, ANCE, y UL México. Sin estas certificaciones el producto solo puede venderse de manera informal (clasificados, grupos de Facebook, etc.), con riesgos legales y de credibilidad.
>
> **Alternativa de entrada rápida (bajo riesgo):** Vender inicialmente como kit DIY/educativo (sin certificación) en MercadoLibre y grupos de maker/IoT, a un precio menor (~$599 MXN), para validar el mercado antes de invertir en certificaciones.

---

## 4. Retorno de Inversión (ROI)

### 4.1 Escenario Conservador — Opción A (Controlador a $899 MXN)

| Métrica | Valor |
|---------|-------|
| Precio de venta | $899 MXN |
| Costo total por unidad | $632 MXN |
| Utilidad neta por unidad | $267 MXN |
| Inversión total | $170,000 MXN |
| **Unidades para break-even** | **637 unidades** |

| Ventas (unidades) | Ingreso bruto | Utilidad neta | ROI |
|-------------------|---------------|---------------|-----|
| 500 | $449,500 | $133,500 | 78% |
| 637 | $572,663 | $170,079 | **100% (break-even)** |
| 1,000 | $899,000 | $267,000 | 157% |
| 2,000 | $1,798,000 | $534,000 | 314% |

**Tiempo estimado para break-even:** 12–18 meses con ventas de ~40–55 unidades/mes.

### 4.2 Escenario Optimista — Opción A (con certificación + MercadoLibre Full)

| Métrica | Valor |
|---------|-------|
| Precio de venta | $899 MXN |
| Utilidad neta por unidad (mayor volumen → menor BOM) | $370 MXN |
| Inversión total | $170,000 MXN |
| **Break-even** | **460 unidades** |

| Ventas (unidades) | Utilidad neta | ROI |
|-------------------|---------------|-----|
| 460 | $170,200 | **100%** |
| 1,000 | $370,000 | 218% |
| 3,000 | $1,110,000 | 653% |

### 4.3 Escenario Kit Completo — Opción B ($1,299 MXN)

| Métrica | Valor |
|---------|-------|
| Precio de venta | $1,299 MXN |
| Costo total por unidad | $925 MXN |
| Utilidad neta por unidad | $374 MXN |
| Inversión total (más alta por inventario LED+PSU) | $210,000 MXN |
| **Break-even** | **562 unidades** |

| Ventas (unidades) | Utilidad neta | ROI |
|-------------------|---------------|-----|
| 562 | $210,188 | **100%** |
| 1,000 | $374,000 | 178% |
| 2,000 | $748,000 | 356% |

---

## 5. Canales de Venta y Estrategia

### 5.1 Canales Primarios

| Canal | Ventaja | Comisión | Alcance |
|-------|---------|----------|---------|
| **MercadoLibre** | Mayor tráfico e-commerce MX, confianza del comprador, Mercado Envíos | 12–16% | Nacional |
| **Amazon México** | Tráfico creciente, clientes tech-savvy | 15% | Nacional |
| **Marketplace Steren** | Sinergia directa con consumidores Smart Home | Negociable | Oportunidad futura |

### 5.2 Canales Complementarios

| Canal | Estrategia |
|-------|-----------|
| **Grupos Facebook IoT/Arduino MX** | Comunidad maker, precio reducido versión DIY |
| **Ferias de electrónica** (TecnoMóvil, Expo Electrónica) | Demostraciones en vivo |
| **YouTube / TikTok** | Demostración instalación real, tutorial |
| **Instagram** | Contenido visual del fade y sensores |

---

## 6. Riesgos y Mitigaciones

| Riesgo | Probabilidad | Impacto | Mitigación |
|--------|-------------|---------|------------|
| **Costo y tiempo de certificación NOM+IFT** | Alta | Alto | Iniciar sin certificación como kit DIY; certificar después de validar demanda |
| **Steren lanza producto similar** | Media | Alto | Diferenciarse por ToF, código abierto, y nicho (escaleras); Steren es generalista |
| **Soporte técnico post-venta** | Alta | Medio | Documentación clara, videos tutoriales, WhatsApp Business para soporte |
| **Problemas de calidad primer lote** | Media | Alto | Lote piloto de 20 unidades para beta testers antes del lanzamiento |
| **Componentes agotados / volatilidad AliExpress** | Media | Medio | Stock de seguridad de 50 unidades; migrar a LCSC/Mouser para producción |
| **Devoluciones / garantía** | Media | Bajo | Reserva del 3%, política clara de 30 días |
| **Competencia de marcas chinas (Sonoff, Tuya)** | Alta | Medio | Enfatizar "Hecho en México", soporte local, personalización, open source |
| **Seguridad eléctrica (12V LED strip)** | Baja | Alto | Usar fuentes certificadas, incluir fusible en PCB, instructivo claro |

---

## 7. Ventajas Competitivas Clave

### 7.1 Frente a Steren

1. **ToF vs PIR:** El VL53L0X detecta presencia aunque la persona esté completamente quieta (leyendo en las escaleras, sentado). El PIR solo detecta movimiento. Esto es crítico en escaleras donde la gente puede detenerse.

2. **Doble sensor:** Dos sensores (arriba y abajo) es una solución diseñada específicamente para escaleras. Steren no ofrece nada similar.

3. **Atardecer automático:** La luz solo se activa de noche, con ajuste automático estacional via API — sin necesidad de reconfigurar.

4. **Fade PWM suave:** El fade-in/fade-out es más placentero y seguro que un encendido brusco. Transmite calidad premium.

5. **Sin app requerida:** Dashboard web integrado accesible desde cualquier navegador. No requiere instalar apps de terceros ni crear cuentas.

6. **Open source / hackeable:** El comprador puede modificar el firmware. Atractivo para el mercado maker y técnico.

### 7.2 Frente a Marcas Chinas (Sonoff, Tuya, Shelly)

1. **Soporte local en español.**
2. **No depende de nube china:** Toda la lógica corre localmente en el ESP32.
3. **Personalización por proyecto:** Adaptable a cualquier escalera (umbrales, tiempos, sensibilidad).

---

## 8. Conclusión y Recomendación

### Viabilidad: ✅ **VIABLE CON ENFOQUE GRADUAL**

El proyecto tiene un nicho claro y desatendido (iluminación inteligente para escaleras) en un mercado que crece al 25% anual. La competencia directa (Steren) ofrece productos genéricos que no igualan las capacidades técnicas del kit propuesto.

### Plan de acción recomendado (3 fases):

**Fase 1 — Validación DIY (3 meses, ~$15,000 MXN):**
- Producir 20 unidades como kits DIY sin certificación
- Vender en grupos de Facebook IoT/Arduino México a $499–599 MXN
- Recopilar feedback, pulir diseño, crear comunidad
- Meta: vender 20 unidades, validar interés

**Fase 2 — Lanzamiento formal (6 meses, ~$100,000 MXN adicionales):**
- Certificación NOM + IFT
- Primer lote de producción (100 unidades)
- Listados profesionales en MercadoLibre y Amazon
- Marketing digital
- Meta: vender 100 unidades en 6 meses (~17/mes)

**Fase 3 — Escalamiento (12 meses, reinversión de utilidades):**
- Subir a 500+ unidades por lote
- Explorar convenio con Steren como proveedor o marca complementaria
- Agregar variantes: pasillos, cocheras, jardín
- Meta: 500+ unidades/año, utilidad neta >$185,000 MXN/año

### Respuesta rápida:

| Pregunta | Respuesta |
|----------|-----------|
| **¿Inversión necesaria?** | $170,000 MXN (~$8,500 USD) para lanzamiento formal; $15,000 MXN para validación DIY |
| **¿Precio de venta?** | $899 MXN (controlador) / $1,299 MXN (kit completo) |
| **¿Unidades para break-even?** | 460–637 unidades (dependiendo del volumen y canal) |
| **¿ROI a 1,000 unidades?** | 157–218% (recuperas la inversión y ganas $267,000–370,000 MXN) |
| **¿Tiempo estimado?** | 12–18 meses para break-even vendiendo ~40–55 unidades/mes |
| **¿Mayor riesgo?** | Certificaciones NOM+IFT (~$50,000 MXN, 3–6 meses de trámite) |
| **¿Ventaja clave vs Steren?** | ToF (presencia real), doble sensor escalera, fade, dashboard web, open source |

---

## 9. Fuentes

- IMARC Group — Mexico Home Automation Market Report 2024–2033
- IMARC Group — Mexico Smart Homes Market Report 2024–2033
- Grupo Milenio — "Demanda de casas inteligentes crecerá más de 80% en un lustro" (2024)
- Muy Digital — "El Crecimiento de la Domótica en México en 2025"
- Steren Tienda en Línea — Catálogo Smart Home (steren.com.mx/smart-home)
- AliExpress — Precios mayoristas de componentes electrónicos (2024–2025)
- MercadoLibre México — Precios de venta al público de componentes y kits
- Raloro Tech — "¿Cuánto Cuesta la Impresión 3D en México? Guía de Precios 2025"
- DHL México — Guía de Servicios y Precios 2025
- NYCE / ANCE — Procesos de certificación NOM e IFT para dispositivos electrónicos
