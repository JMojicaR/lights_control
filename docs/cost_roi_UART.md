# Cost, ROI & Sell Plan — UART / mmWave Option (5V USB, DIY kit)

Analysis of the recommended build: **HLK-LD2410C mmWave radar + BH1750**, running
off one **5 V / 2 A USB charger**. Full BOM is in [`BOM_UART.md`](BOM_UART.md);
full market analysis in [`market_analysis_UART.md`](market_analysis_UART.md).

Sold as a **DIY kit** → no NOM-001-SCFI certification required initially.

---

## 1. Cost summary (MXN)

| Scenario | Core kit | Complete kit |
|----------|----------|--------------|
| Small batch (100) | $1,095 | $1,415 |
| **Mass production (1,000)** | **$460** | **$560** |

The core kit is the controller (board + 2 radars + BH1750 + RJ45/Cat6); the
complete kit adds the **5 V USB LED strip** + a **5 V/2 A USB charger**.

> Going 5 V **removed** the 12 V PSU (~$220) and the 12→5 V buck/regulator from
> the BOM — the single USB charger replaces both, so the complete kit got
> *cheaper* than the old 12 V build.

---

## 2. Unit economics (mass production, direct sale)

``` 
                        Core kit        Complete kit
Retail price:            $1,499           $1,899
- COGS (mass prod):        $460             $560
- Payment processing (3%):  $45              $57
- Shipping:                $120             $130
- Support:                  $20              $20
- Marketing (CAC):          $50              $60
──────────────────────────────────────────────────
Net profit / unit:         $804           $1,072
Gross margin:              53.6%            56.5%
```

Small-batch margins are thin (~11%) but positive — the business case, as always,
is the mass-production COGS.

---

## 3. ROI & break-even

Fixed costs (Year 1, DIY kit — **no certification**): **~$77,200 MXN**
(PCB + enclosure design, instructions, website, marketing, legal/accounting).
A certified finished product would run ~$157,200 — the kit skips ~$80K.

```
Fixed costs:               $77,200
Gross profit (core kit):      $804  → break-even ≈  96 units
Gross profit (complete):    $1,072  → break-even ≈  72 units
```

| Scenario | Units/mo | Break-even | Year 1 profit/loss |
|----------|----------|------------|--------------------|
| Optimistic (50/mo) | 600/yr | ~2 months | +$405K |
| Realistic (20/mo) | 240/yr | ~5 months | +$115K |
| Pessimistic (5/mo) | 60/yr | ~19 months | –$29K |

---

## 4. Sell plan

### Positioning

> **"Build your own radar stair light — no soldering, no certification, no
> monthly fees. Plugs into any USB phone charger."**

The mmWave story is the differentiator (detects a *still* person, beats PIR), and
the **5 V USB power** is now a *simplifier* to talk up: no mains wiring, no
electrician, plug it into the charger you already own.

### Pricing

| SKU | Retail |
|-----|--------|
| **Core kit** (bring your own strip) | **$1,499** |
| **Complete kit** (strip + charger) | **$1,899** |

### Channels

| Channel | Role |
|---------|------|
| Tindie | Primary — the natural home for a no-cert kit |
| Direct / Shopify | Brand + margin, own the data |
| MercadoLibre | LATAM reach, list as "kit de partes / educativo" |
| Amazon.com.mx | Trust |
| Home Assistant / ESPHome groups | Community validation, zero-cost demand gen |

### Go-to-market

1. Build 5 kits, place with Home Assistant / ESPHome community members.
2. Pre-sell on Tindie + a landing page with ~$100 in ads.
3. If 10+ pre-orders → design the custom PCB (RJ45 + radar adapters).
4. 50-kit batch at $1,499 (core) / $1,899 (complete).
5. Scale; later offer a **certified "pro" finished version** as the upsell.

---

## 5. Verdict

| | I²C finished (baseline) | **UART/mmWave 5V kit (this branch)** |
|---|---|---|
| Prototype BOM | $1,138 | ~$1,310 |
| Mass-prod BOM | $790 | **$460 (core) / $560 (complete)** |
| Fixed costs | $157,200 (certified) | **$77,200 (no cert)** |
| Break-even | ~322 units | **~72–96 units** |
| Retail | $1,699 | **$1,499 / $1,899** |
| Still-person detection | narrow beam | **wider cone + micro-motion** |
| Power | 12 V + buck | **5 V USB (single supply)** |

**The 5 V USB kit is the most capital-efficient build yet**: it breaks even at
~a quarter of the certified product's volume, drops the 12 V PSU + buck from the
BOM, and the "plugs into your phone charger" story removes a real adoption
barrier for the maker/Home-Assistant audience.
