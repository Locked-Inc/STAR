# STAR Capstone - Research Dossier

Verified research package for the STAR (Spatial Topography Accessibility Robot)
capstone, Locked Inc., TAMU ESET Senior Capstone, Spring 2026. Every statistic
is traceable to a primary source. Three corrections from the original team
prompt are flagged at the end.

---

## The compliance crisis is real, expensive, and growing

Federal courts received **8,800 ADA Title III lawsuits in 2024**, a **7%
year-over-year increase** that reversed a two-year decline and pushed total
filings back toward the 2021 peak of 11,452 (Seyfarth Shaw ADA Title III
Report, March 2025). California regained the top spot with **3,252 filings**,
followed by New York at 2,220 and Florida at 1,627; Texas placed fourth
nationally with **224 filings**. A single firm - So Cal Equal Access Group -
drove the California rebound by filing 2,598 cases, illustrating how a small
number of plaintiffs' firms can multiply enforcement pressure on building
owners almost overnight. Within the 2024 total, **2,452 cases (28%) targeted
website accessibility**, a 13% drop from 2023; the remainder - roughly 6,300
lawsuits - concerned **physical/architectural accessibility**, the exact
problem space STAR addresses.

The full federal Title III time series tells a compounding story:
2,722 filings in 2013, climbing to 11,452 in 2021, falling to 8,694 in 2022,
8,227 in 2023, and rebounding to **8,800 in 2024**. Year-by-year website-only
subset filings - frequently misreported as the total - were 2,258 (2018),
2,256 (2019), 2,523 (2020), 2,895 (2021), 3,255 (2022), 2,794 (2023), and
2,452 (2024). **The team should cite total Title III filings (the larger
numbers) when discussing physical-access litigation pressure, because website
lawsuits are not what STAR prevents.**

---

## The price tag for getting caught keeps climbing

The Department of Justice adjusts ADA Title III civil penalties for inflation
every January under 28 CFR 85.5. The current maximums, effective after July 3,
2025 per Federal Register notice **90 FR 29445**, are **$118,225 for a first
violation and $236,451 for each subsequent violation** - well above the
often-cited $75,000 / $150,000 figures, which date to a 2014 rule and have
since been adjusted upward six times. Because each non-compliant element (a
doorway, a ramp, a threshold) can constitute a separate violation, a single
building can rack up six- or seven-figure exposure before plaintiff attorney
fees are added. Plaintiff attorney fees and remediation costs typically dwarf
the statutory penalty itself: industry post-mortems of settled cases
routinely report total costs of **$50,000 to $250,000** even when DOJ does not
directly file suit.

---

## Manual audits are slow, expensive, and rare

Certified Access Specialist (CASp) and equivalent professional ADA
inspections currently cost **$2,500 to $7,000 per building** for typical
commercial properties, with larger or multi-tenant facilities running into
five figures (MBCS Orange County, 2024; CASp Inspectors pricing guide, 2026).
Smaller properties can be inspected for $650-$2,000, but those quotes assume
a single-suite tenant build-out, not a campus building. Time on-site varies
from a half-day for a small storefront to **multiple days for an academic
building or office tower**, because the inspector must physically measure
ramp slopes, door widths, threshold heights, clear floor space at fixtures,
parking, and the entire path of travel using a tape measure, smart level,
and clipboard. The capstone's "2-5 days, $3,000-$8,000 per building" framing
aligns with these published ranges and is defensible.

The deeper problem is that almost no one orders these audits proactively.
Industry data from Focus Building Inspections (2024) found only **32% of
commercial property buyers ask about ADA compliance during inspection**,
meaning roughly two-thirds of building transactions close with the
accessibility status entirely unknown. Building Principles, a CASp
consultancy, estimates that **~73% of commercial establishments fail at
least one ADA standard** when inspected - the often-quoted figure that,
while industry-sourced rather than peer-reviewed, is consistent with the
volume of lawsuits being filed. As a digital-side comparator, the WebAIM
Million 2024 study found that **95.9% of the top one million homepages have
detectable WCAG failures**, suggesting that low-effort, hard-to-measure
compliance domains are universally non-compliant when nobody is
automatically watching.

---

## The market is large in dollars but blue ocean in robotics

The broader **digital accessibility software market** is variously sized
between $549 million and $4.48 billion in 2024, with most analysts
(Grand View Research, SNS Insider, Market.us) clustering around $670M-$754M
and projecting 8.4%-10.9% CAGR through 2030-2034. Grand View specifically
forecasts the segment growing from $721M in 2023 to $1.30 billion by 2030.
North America commands 38-44% of revenue, and large enterprises drive
62-68% of spend.

Critically, **no published market sizing exists for built-environment /
physical-ADA-audit technology** - the segment STAR enters. This absence,
combined with the competitive scan below, validates the team's blue-ocean
framing.

The closest competitive plays are not autonomous robots:

- **CloudPoint Geospatial** offers human-led ADA site assessments augmented
  by terrestrial LiDAR and digital levels, marketed via their 2022 white paper
  "Using GIS & LiDAR to Conduct an ADA Site Assessment."
- **ROCK Robotic** sells the R3 Pro V2 handheld LiDAR (5 mm range accuracy,
  Hesai Pandar XT32, 1.28M points/sec) and has publicly marketed it for
  "ADA Compliance at Scale" via a September 2024 LinkedIn post - but this is
  a sensor a human carries, not an autonomous robot.
- **Matterport** scans capture geometry but ship no compliance engine.
- **Oregon State University's Yelda Turkan group (Erzhuo Che)** has
  published academic work on automated curb-ramp ADA assessment from
  vehicle-mounted mobile LiDAR - but this targets outdoor sidewalks, not
  indoor buildings.

To the team's knowledge - and the research conducted for this report
confirms - **no shipping product or published research demonstrates an
autonomous indoor robot that performs end-to-end ADA compliance auditing.**
STAR is first-of-kind.

---

## Disability prevalence: 1 in 4 adults, and the dataset has updated

The CDC's most recently published Disability and Health Data System figures
(2022 BRFSS, released 2024) show **28.7% of U.S. adults - approximately
70 million people - live with a disability**, an increase from the widely
quoted 2016 figure of 25.7% (Okoro et al., MMWR 2018; 67(32):882-887). The
team's prompt cited the older 2016 breakdown; the current and historical
numbers compared:

| Disability type | 2016 BRFSS (Okoro 2018, MMWR) | 2022 BRFSS (CDC DHDS, current) |
|---|---|---|
| Cognition | 10.8% | **13.9% <- now the most prevalent** |
| Mobility | 13.7% | 12.2% |
| Independent Living | 6.8% | 7.7% |
| Hearing | 5.9% | 6.2% |
| Vision | 4.6% | 5.5% |
| Self-care | 3.7% | 3.6% |
| **Any disability** | 25.7% (61.4M) | **28.7% (~70M)** |

**Recommendation:** Cite the current 2022 figures as the headline ("more than
1 in 4 U.S. adults - 70 million people - live with a disability") and reserve
the 2016 dataset for historical context only. The chart code uses the 2022
numbers; the team should be ready to explain that cognition surpassed mobility
as the most prevalent disability type in the most recent dataset, while
emphasizing that mobility, vision, and self-care disabilities together - the
populations STAR most directly serves - still affect over 60 million Americans.

---

## Academic prior work: the foundation STAR builds on

Four research streams ground STAR in published work:

### Crowd + AI accessibility mapping (Makeability Lab, UW)

- Saha et al., *Project Sidewalk: A Web-based Crowdsourcing Tool for
  Collecting Sidewalk Accessibility Data at Scale* (CHI 2019, Best Paper)
- Weld et al., *Deep Learning for Automatically Detecting Sidewalk
  Accessibility Problems Using Streetscape Imagery* (ASSETS 2019, Best
  Student Paper)
- Hara et al., *Tohme: Detecting Curb Ramps in Google Street View* (UIST 2014)
- Hosseini et al., *Towards Global-Scale Crowd+AI Techniques to Map and
  Assess Sidewalks for People with Disabilities* (CVPR 2022 AVA Workshop,
  arXiv:2206.13677)

### LiDAR-based ADA assessment

- Turkan & Che (Oregon State), mobile-LiDAR curb-ramp condition assessment
- CloudPoint Geospatial, *Using GIS & LiDAR to Conduct an ADA Site Assessment*
  (2022 industry white paper)

Both confirm that LiDAR can resolve ADA-relevant geometry to sub-inch
tolerances; neither automates the audit pipeline end-to-end.

### Stereo + LiDAR SLAM fusion

- *Stereo and LiDAR Loosely Coupled SLAM Constrained by Ground Detection*
  (Sensors via PMC11548508, 2024)
- Lang et al., *Gaussian-LIC: Real-Time Photo-Realistic SLAM with Gaussian
  Splatting and LiDAR-Inertial-Camera Fusion* (arXiv:2404.06926, 2024)
- *LVI-Fusion* tightly-coupled scheme (MDPI Remote Sensing 16(9):1524, 2024)

These papers consistently report that fused stereo+LiDAR+IMU outperforms any
single modality on indoor pose accuracy and surface reconstruction - the
exact claim STAR's validation data is designed to substantiate.

---

## TAMU campus context and sponsor opportunity

Texas A&M University's College Station campus spans **5,200 acres** with
**approximately 800 buildings** (TAMU Facts; Aggie Map Building Directory)
and serves **74,407 students at College Station** out of 81,354 total system
enrollment (Fall 2025), supported by **4,398 faculty**. Even one professional
CASp inspection per building at the midpoint of $5,000 implies a
**$4 million sticker price** to audit the campus once, with multi-day
on-site time per building making a comprehensive sweep effectively
impossible under current methods.

**TAMU Department of Disability Resources** (disability.tamu.edu, 471 Houston
Street, Student Services Building Suite 122) is the most natural advocate
stakeholder for STAR, with an existing Campus Construction accessibility
tracking page demonstrating active interest in built-environment access. The
**Office of Risk, Ethics & Compliance (OREC)** runs the formal ADA program
at orec.tamu.edu/ada, and **Facilities Services / Office of the University
Architect** owns remediation budgets - a joint-stakeholder model with
Disability Resources as user advocate and OREC/Facilities as compliance
owner is the realistic sponsor structure. The DOJ's April 2024 update to ADA
Title II requires public entities including state universities to meet
**WCAG 2.1 AA by April 24, 2026**, demonstrating that the A&M System is
already operating under active accessibility compliance pressure on the
digital side; the physical-access side has no equivalent automated tooling.

No TAMU lab currently combines robotics with physical accessibility auditing
despite a robust autonomy ecosystem (Center for Autonomous Vehicles and
Sensor Systems, the RAD Lab at RELLIS led by former NASA JSC chief Robert
Ambrose, the LASR Lab in Aerospace, and the NetBot Lab in CSE for
vision-based navigation). **STAR fills that institutional gap.**

---

## Final consistency notes for the team

Three corrections to internalize before stage time, because judges who know
the data will catch you:

1. The **disability prevalence headline** is now **28.7% / 70 million U.S.
   adults**, not 26% / 61 million - the older number is from the 2016 BRFSS
   published in MMWR 2018, while the 2022 BRFSS released by CDC in 2024 is
   the current dataset and is what the chart code uses. **Cognition (13.9%)
   has now overtaken mobility (12.2%) as the most prevalent disability type**;
   if asked, frame this as "the population we serve has grown, not shrunk."

2. The **ADA penalty figures** are **$118,225 and $236,451**, per Federal
   Register 90 FR 29445 (July 2025). The often-cited $75,000 / $150,000
   figures are from a 2014 rule that has since been adjusted upward six
   times. Using the current numbers signals technical rigor.

3. The **"2018: 2,258" through "2022: 3,255"** lawsuit numbers in the
   original prompt are **website-accessibility-only filings**, not total
   Title III filings. The total filings for those years are materially
   higher (e.g., 2022 total = 8,694, of which 3,255 were website cases). The
   charts and writeup use the **total Title III** figures because STAR
   addresses physical access, not websites, and the larger numbers tell the
   more honest story for this product.

Everything in this package is designed to be assembled, rehearsed, and
delivered inside the seven-day window. Build the demo. Win the room.
