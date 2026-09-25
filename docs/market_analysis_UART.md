# Market Analysis — UART/mmWave DIY Kit (HLK-LD2410C radar + BH1750)

Analysis of the `uart_option` build sold as a **DIY kit** (no certification
required initially). Prices in MXN, mid-range MercadoLibre México / Amazon MX
(Sept 2026, ~1 USD = 18 MXN).

---

## 1. Product — the DIY kit

A **"build-it-yourself" radar stair-light kit**: two HLK-LD2410C mmWave radars
(top/bottom), a BH1750 ambient-light sensor, and a main controller board, wired
together with **standard Cat6 patch cables** via **RJ45 jacks** (no soldering,
no I²C pull-up tuning). The customer mounts it, plugs it in, and gets a
staircase that lights itself when someone steps on it — and stays on while they
stand still.

**Sold as a kit, not a finished appliance** → in Mexico this generally sidesteps
NOM-001-SCFI (finished-product electrical safety) and IFETEL (the ESP32 module
is already homologated by its manufacturer). This removes the ~$60K certification
cliff from the earlier analyses and is the whole point of the kit strategy.

### Two SKUs

| SKU | Contents | Retail |
|-----|----------|--------|
| **Core kit** | main board + 2 radars + BH1750 + RJ45 adapters + Cat6 cables + instructions | **$1,499** |
| **Complete kit** | core kit + 12 V LED strip + 12 V PSU + enclosures + hardware | **$1,899** |

---

## 2. Market analysis

### 2a. Target market

| Question | Answer |
|----------|--------|
| Who buys it? | Makers/hobbyists, Home Assistant / ESPHome users, semi-pro installers, tech-savvy homeowners |
| Problem solved | Safety + convenience on stairs, without paying for a "smart home" hub, and without a custom build |
| Why a kit? | Lower price than finished products, the satisfaction of DIY, and **no certification** = available now |
| Addressable (MX) | ~600K automation-interested households + the maker community + LATAM ESPHome/HA users |
| Obtainable Year 1 | 150–400 kits (realistic) |

### 2b. Competitors (kit/DIY framing)

| Competitor | Price (MXN) | What it is | Weakness vs. us |
|------------|-------------|------------|-----------------|
| Battery PIR stair lights (ML/Amazon) | $150–250 | Stick-on, battery, motion-only | No still-person detection, recharging, no API |
| Smart-home hub kits (Tuya/Aqara) | $800–2,000 | Hub + motion sensor + bulbs | Needs hub, single-sensor, cloud dependency |
| Finished radar presence (Aqara FP2, etc.) | $1,200–2,000 | mmWave presence, off-the-shelf | Single sensor, closed ecosystem, not stair-specific |
| DIY-from-scratch (ESPHome/Tindie tutorials) | $800–1,300 (parts) | Build it yourself from a blog post | No curated kit, no instructions, fiddly I²C |
| Finished stair-light products | $1,699–2,999 | Our own other variants (finished) | Requires certification to sell legally as a product |
| "Do nothing" | $0 | manual switch | The eternal competitor |

**Key differentiator:** we are the only **purpose-built staircase radar kit**
with dual still-person detection + a zero-solder Cat6/RJ45 build. The "kit"
framing also *unlocks the market now* (no certification lead time).

### 2c. SWOT

| Strengths | Weaknesses |
|-----------|------------|
| Dual mmWave detects still people (better than PIR/ToF) | Kit = customer does the assembly (support load) |
| Cat6/RJ45 — no soldering, foolproof wiring | Smaller "maker" market vs mass consumer |
| No certification needed to sell (kit) | Lower perceived polish vs finished product |
| Real lux (BH1750) + web dashboard + API | Requires a 12 V feed + wiring near stairs |

| Opportunities | Threats |
|---------------|---------|
| Growing Home Assistant / ESPHome community in LATAM | Cheap mmWave modules + free tutorials |
| "No-cert" kit → first-to-market speed | Regulatory interpretation of "kit" could shift |
| Upsell: complete kit, install service, pro SKU | Copycats on Tindie/AliExpress |
| Partnerships with maker stores (Tindie, local) | USD/MXN volatility on radar modules |

---

## 3. Bill of Materials (with Cat6/RJ45 PCB connectors)

The Cat6 connection is handled by **RJ45 PCB-mount jacks** on the main board and
a **radar adapter PCB** (RJ45 + 5-pin radar header) at each radar — so the
customer just plugs in standard Ethernet patch cables.

### 3a. Core kit (small batch, ~100 units)

| # | Component | Qty | Unit | Total |
|---|-----------|-----|------|-------|
| 1 | ESP32-S3 SuperMini | 1 | $200 | $200 |
| 2 | HLK-LD2410C mmWave radar | 2 | $180 | $360 |
| 3 | BH1750 ambient light sensor | 1 | $75 | $75 |
| 4 | IRLZ44N MOSFET (TO-220) | 1 | $30 | $30 |
| 5 | **RJ45 PCB-mount jack (Cat6)** | 2 | $15 | $30 |
| 6 | **Radar adapter PCB (RJ45 + 5-pin header)** | 2 | $30 | $60 |
| 7 | **Cat6 patch cable 3 m** | 2 | $30 | $60 |
| 8 | Screw terminals + passives (resistors, 5 V reg) | 1 set | $30 | $30 |
| 9 | Main enclosure | 1 | $80 | $80 |
| 10 | Radar enclosures | 2 | $40 | $80 |
| 11 | Instructions + packaging | 1 | $60 | $60 |
| 12 | Mounting hardware | 1 | $30 | $30 |
| | **Core kit subtotal** | | | **$1,095** |

### 3b. Complete kit (adds)

| # | Component | Qty | Unit | Total |
|---|-----------|-----|------|-------|
| 13 | 12 V LED strip 5 m (5050) | 1 | $250 | $250 |
| 14 | 12 V PSU 6 A | 1 | $220 | $220 |
| | **Complete kit total** | | | **$1,565** |

### 3c. Mass production (custom PCB + bulk radar)

| Scenario | Core kit | Complete kit |
|----------|----------|--------------|
| Small batch (100) | $1,095 | $1,565 |
| **Mass production (1,000)** | **~$460** | **~$650** |

At scale the custom PCB integrates the ESP32-S3 module, BH1750, 5 V regulator,
MOSFET and the two RJ45 jacks on one board (~$150 assembled); radar modules drop
to ~$80 each in bulk.

---

## 4. Unit cost & unit economics

### 4a. Unit cost (COGS)

| | Core kit | Complete kit |
|---|---|---|
| Small batch | $1,095 | $1,565 |
| Mass production | $460 | $650 |

### 4b. Unit economics (direct sale)

```
                        Core kit        Complete kit
Retail price:            $1,499           $1,899
- COGS (mass prod):        $460             $650
- Payment processing (3%):  $45              $57
- Shipping:                $120             $130
- Support:                  $20              $20
- Marketing (CAC):          $50              $60
──────────────────────────────────────────────────
Net profit / unit:         $804             $982
Gross margin:              53.6%            51.7%
```

Small-batch margins are thin (~11% core / ~5% complete) — the business case,
as always, is the mass-production COGS.

---

## 5. Sell analysis — ROI & break-even

### 5a. Fixed costs (Year 1, DIY kit — NO certification)

| Item | Cost (MXN) | Recurring? |
|------|-----------|------------|
| PCB design + prototypes | $6,000 | Once |
| Enclosure design + 3D prints | $4,000 | Once |
| Instruction manual (design + print) | $6,000 | Once |
| Website (Shopify) | $6,000 | Annual |
| Domain + email | $1,200 | Annual |
| Packaging design | $5,000 | Once |
| Photography + video | $3,000 | Once |
| Marketing (Meta/Google + community) | $36,000 | $3,000/mo |
| Legal (trademark only) | $4,000 | Once |
| Accounting / SAT | $6,000 | Annual |
| **Total fixed (Year 1)** | **~$77,200** | |

> vs. **~$157,200** for the certified finished product — the kit strategy cuts
> fixed costs **~$80K** by skipping NOM/IFETEL.

### 5b. Break-even (mass-production COGS)

```
Fixed costs:             $77,200
Gross profit (core kit):    $804  → break-even ≈  96 units
Gross profit (complete):    $982  → break-even ≈  79 units
```

| Scenario | Units/mo | Break-even | Year 1 profit/loss |
|----------|----------|------------|--------------------|
| Optimistic (50/mo) | 600/yr | ~2 months | +$405K |
| Realistic (20/mo) | 240/yr | ~5 months | +$115K |
| Pessimistic (5/mo) | 60/yr | ~19 months | –$29K |

---

## 6. Sell plan (DIY kit, no-cert strategy)

### 6a. Positioning

> **"Build your own radar stair light — no soldering, no certification, no
> monthly fees."** A curated kit: plug Cat6 into RJ45, flash, mount, done.

The "kit" framing is the strategy — it ships *now* (no cert lead time), targets
the growing maker/Home-Assistant audience, and sidesteps the regulatory cliff.

### 6b. Channels

| Channel | Role | Notes |
|---------|------|-------|
| **Tindie** | Primary (maker marketplace) | The natural home for a no-cert kit |
| Direct / Shopify | Brand + margin | 3–5% fees, own the data |
| MercadoLibre | LATAM reach | list as "kit de partes / educativo" |
| Amazon.com.mx | Trust | list as "kit/componentes" |
| Facebook/Reddit HA & ESPHome groups | Community + validation | zero-cost demand gen |

### 6c. Go-to-market

1. Build 5 kits, place with Home Assistant / ESPHome community members.
2. Pre-sell on Tindie + a landing page ("radar stair kit") with $100 in ads.
3. If 10+ pre-orders → design the custom PCB (RJ45 + radar adapters).
4. 50-kit batch at $1,499 (core) / $1,899 (complete).
5. Scale; later offer a **certified "pro" finished version** as the upsell once
   the kit proves demand.

---

## 7. Verdict

| | I²C finished (baseline) | **UART/mmWave DIY kit (this branch)** |
|---|---|---|
| BOM (mass prod) | $790 | **$460 (core) / $650 (complete)** |
| Fixed costs | $157,200 (certified) | **$77,200 (no cert)** |
| Break-even | ~322 units | **~79–96 units** |
| Retail | $1,699 | $1,499 / $1,899 |
| Still-person detection | narrow beam | **wider cone + micro-motion** |
| Wiring | I²C (address + pull-ups) | **Cat6/RJ45, no solder** |

**The DIY kit is the most capital-efficient way to market**: it drops the
certification cost, breaks even at ~a quarter of the certified product's volume,
and reaches the maker/Home-Assistant audience *now* — while keeping the radar
"still-person detection" differentiator. The trade-off is a smaller market and
more support per sale; the mitigation is a certified "pro" SKU as the eventual
upsell.
