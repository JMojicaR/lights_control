# Cost, ROI & Sell Plan — UART / mmWave Option

Analysis of the recommended build from `options.md`: **HLK-LD2410B mmWave radar
+ LDR** (no I²C). Full BOM is in [`BOM_UART.md`](BOM_UART.md).

---

## 1. Cost summary (MXN)

| Scenario | COGS / unit | vs I²C baseline (~$1,138) |
|----------|-------------|---------------------------|
| Prototype (singles) | ~$1,410 | +$270 |
| Small batch (100) | ~$1,280 | +$140 |
| **Mass production (1,000)** | **~$690** | **–$448** |

The prototype premium comes from the two radars ($180 each). It **flips to a
cost advantage at scale** because the radar is a single self-contained module
(no breakout, no pull-ups, no node MCU), and the LDR is $65 cheaper than the
BH1750.

---

## 2. Unit economics

### Mass production, direct sale @ $1,899

```
Retail price:              $1,899
- COGS (1,000-unit batch):   $690
- Payment processing (3%):    $57
- Shipping to customer:      $120
- Customer support:           $15
- Marketing (CAC):            $60
─────────────────────────────────────
Net profit / unit:           $957
Gross margin:                50.4%
```

### Small batch, direct sale @ $1,899 (validation phase)

```
Retail price:              $1,899
- COGS (small batch):      $1,280
- Fees + shipping + CAC:     $252
─────────────────────────────────────
Net profit / unit:           $367   (19.3% margin)
```

> Small-batch margins are thin but positive; the business case is built on the
> mass-production COGS, as with every electronics product.

---

## 3. ROI & break-even

Fixed costs (Year 1) carry over unchanged from the base analysis
(certification, tooling, marketing, etc.): **~$157,200 MXN**.

| Metric | Value |
|--------|-------|
| Gross profit / unit (mass prod) | $957 |
| **Break-even** | **~164 units** |
| Optimistic (50/mo → 600/yr) | break-even in ~3.3 months, +$417K Year 1 |
| Realistic (20/mo → 240/yr) | break-even in ~8 months, +$72K Year 1 |
| Pessimistic (5/mo → 60/yr) | break-even ~33 months |

Compared to the I²C baseline (~322 units to break even), the **radar build
breaks even at ~half the volume** — a direct consequence of the higher price and
the lower mass-production COGS.

---

## 4. Sell plan

### Positioning

> **"24 GHz radar presence detection."** Lights the stairs the moment a person
> steps on them — and keeps them on while they stand still, because the radar
> sees the person's presence (not just motion), day or night.

The mmWave story is the differentiator: it beats PIR (motion-only), matches ToF
(still-detection) with a **wider, more forgiving coverage cone**, and sounds
premium ("radar", not "laser distance sensor").

### Pricing

| Method | Price (MXN) |
|--------|-------------|
| Competitor-based (below smart-home kits) | $1,499–1,799 |
| Value-based (safety + radar premium) | **$1,899–1,999** |
| **Recommended retail** | **$1,899** |

$1,899 is +$200 over the I²C baseline ($1,699) — justified by "radar presence"
and absorbed by the lower scale COGS.

### Channels (unchanged from base plan)

| Channel | Role |
|---------|------|
| Direct / Shopify | Primary (best margin) |
| MercadoLibre | Volume + reach |
| Amazon.com.mx | Trust + Prime |
| Electrician / installer partnerships | High-trust installs |
| Facebook Marketplace | Validation / early sales |

### Go-to-market

1. Build 5 units, place with friends/family.
2. Pre-sell a landing page ("radar stair light") with $100 Meta ads.
3. If 10+ pre-orders → design custom PCB (radar + ESP32 on one board).
4. 50-unit batch at $1,699 intro price; $1,899 after validation.
5. Certify (NOM + IFETEL) and scale.

---

## 5. Verdict

| | I²C baseline | **UART / mmWave** |
|---|---|---|
| Prototype BOM | $1,138 | $1,410 |
| Mass-prod BOM | $790 | **$690** |
| Break-even | ~322 units | **~164 units** |
| Retail | $1,699 | **$1,899** |
| Still-person detection | narrow beam | **wider cone + micro-motion** |
| Wiring complexity | I²C (address + pull-ups) | **plain GPIO/UART** |

**The mmWave + LDR build is the most profitable option**: it costs more to
prototype but less at scale, breaks even at half the volume, and commands a
higher price on a stronger "radar presence" story — while being simpler to wire.
