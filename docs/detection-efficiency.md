# Detection Efficiency and Product Focus

This document summarizes **industry-style** considerations for detection efficiency: what is detected, how well, and whether detection might vary by **item type**, **price tier** (expensive vs low-cost), or **category**. It informs design choices (e.g. which defect types and categories to prioritise, how to model confidence and product metadata) before implementation. No specific company or product is referenced; this is generic domain context for the example project.

---

## 1. Typical Vendor Claims (Illustrative)

In the retail vision / SCO space, vendor materials often report:

- **Overall metrics** — e.g. high precision on fraud alerts, high recall on fraud behaviors, reduced loss in pilots. Usually **no public breakdown** by expensive vs cheap items, or by category (meat, health & beauty, alcohol, etc.).
- **What is detected** — Intentional fraud (e.g. barcode switching), unintentional errors (non-scans, wrong quantity), produce recognition at SCO, age estimation for restricted sales.
- **Gap** — No public per-category or per-price-tier detection efficiency; design should stay **agnostic** and use config/category for reporting, not assume “expensive items detected more.”

---

## 2. Industry and Research: What Shrinks, and Where Detection Is Hard

### High-shrink categories (ECR and similar)

Research (e.g. **ECR Loss**) on **high-shrink products/categories** (not specifically “what is detected best”) includes:

- **Beers, Wines and Spirits (BWS)** — consistently high-risk.
- **Health and Beauty** — significant shrinkage.
- **Fresh meat** — often cited as the **largest area of loss within food**.
- **Fresh produce** — frequently problematic at SCO due to **barcode/weighing issues** and many SKU variants.

So from a **loss impact** perspective, **expensive or high-margin categories** (BWS, health & beauty, fresh meat) and **produce** (operationally difficult) are especially relevant; vendor materials do **not** say these are “more” or “less” detected, only that they are high-shrink.

### Self-checkout loss patterns (no price-tier breakdown)

- SCO can show **higher shrink** than manned checkouts in some studies (e.g. partial shrink: pay for some but not all items; wrong code for weighed items).
- **Partial shrink** (e.g. scan 2 of 3 items, or enter a cheaper code) is a common pattern; detection systems aim to catch “items not scanned” or “wrong product”.
- **No public research** was found that explicitly breaks down **detection efficiency by item price** (expensive vs cheap) or by category (e.g. “alcohol detected better than produce”).

### Produce vs packaged goods (recognition difficulty)

- **Produce**: High variability (species, variety, ripeness), often **no barcode**; recognition is harder than scanning a barcode. Many SCO/vision systems target “fruit & veggie recognition.”
- **Packaged goods**: Can look very similar (fine-grained recognition); **multimodal (image + OCR)** can improve accuracy vs vision-only; packaging and lighting add variability.
- So: **section/category** affects **recognition difficulty** (produce and fine-grained packaged goods are harder); we can design for **category or “item type”** in our defect model.

---

## 3. Implications for Our Project (Before Code)

### What we can document and design for

| Topic | Design implication |
|-------|--------------------|
| **No “expensive vs cheap” detection data** | Do **not** claim in docs or code that “only expensive” or “only cheap” items are detected. Model **defects by type** (wrong item, wrong quantity, etc.) and optionally by **category/product ID**; **value/price** can be a downstream attribute from a product master, not a detection criterion. |
| **High-shrink categories (BWS, health & beauty, meat, produce)** | Our **defect types** and **config** can support **category** or **product group** (e.g. for prioritised alerts or reporting). Pipeline output can include **product_id** / **category** when the model or POS provides it. |
| **Produce vs packaged** | Support **different recognition paths** (e.g. produce recognition vs barcode/weight) in the **inference adapter** and **decoder**; allow **per-category or per-defect-type** confidence thresholds in config. |
| **Precision vs recall** | Design for **configurable thresholds** (e.g. confidence, NMS) so we can tune toward “fewer false alerts” (precision) or “catch more events” (recall). Store **confidence** and **defect type** in **Defect** so reporting can analyse by type/category. |

### What we should not assume without data

- That **expensive items** are detected **more** or **less** than low-cost items.
- That **only low-cost** or **only high-cost** items are in scope; the pipeline should be **category-agnostic** unless explicitly configured (e.g. “run produce model here”).
- Any specific **per-category accuracy** numbers; we can leave **efficiency metrics** as “TBD” or “configurable” and document that real numbers depend on model, training data, and deployment (retailer, category mix, lighting, etc.).

### Suggested doc/code touchpoints

- **Defect** (core): Include **defect_kind**, **confidence**, optional **product_id**, optional **category** (or product_group); no “price” or “is_expensive” in the core defect type unless we get explicit product master integration.
- **Config** (app): Optional **per-category** or **per-defect-type** thresholds; optional “high-value categories” for alerting/prioritisation (driven by retailer config, not by detection logic).
- **Docs** (this file + company-and-aim): State that detection efficiency (precision/recall) is **overall** and **by defect type** where applicable; **by category** and **by price tier** are **not** specified by vendors and are left for deployment-specific tuning and reporting.

---

## 4. Summary Table

| Question | Finding |
|----------|---------|
| Are expensive items detected more than cheap? | **No public data.** Design agnostic; use category/product_id for reporting, not “expensive vs cheap” in detection. |
| Are only low-cost items detected? | **No.** Pipeline detects **defect types** (wrong item, wrong quantity, etc.); item value is not a detection criterion. |
| Which sections/categories are “more” detected? | **Vendors don’t publish per-category efficiency.** High-shrink **categories** (BWS, health & beauty, meat, produce) are known from loss research; we can support **category** in Defect and config for prioritisation/reporting. |
| Produce vs packaged? | **Produce** is harder (no barcode, variability); **packaged** can be fine-grained. We support different backends/thresholds per use case. |
| Overall detection efficiency (vendor-style)? | Vendor materials often cite high precision/recall and reduced loss in pilots; **no per-category or per-price breakdown** is typically published. |

---

*This doc uses generic industry and loss-research context (e.g. ECR-style categories). No company or product is endorsed or referenced with permission.*
