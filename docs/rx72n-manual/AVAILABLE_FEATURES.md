# RX72N Available Features (Not Implemented in STAR)

This file documents RX72N chip capabilities that exist in the hardware but are not
currently used in STAR firmware. It is **informational only** -- no code should be
added based on this file without an explicit decision to implement the feature.

**How to use:** When working through the manual and you see a peripheral or feature
that the chip supports but STAR does not implement, add a row to the table below.
Do NOT add any code. Just document it here.

**Format:**
```
| Manual Chapter | Feature Name | Manual Pages | Notes |
```

---

## Available Peripherals / Features

| Manual Chapter | Feature | Manual Pages | Notes |
|----------------|---------|--------------|-------|
| Ch 56 (S12AD) | Group scan mode (A/B/C) | 2819-2821, 2909-2928 | Three groups with independent triggers; ADANSB0/1, ADANSC0/1, ADSTRGR.TRSB/TRSC, ADGCTRGR, ADGSPCR |
| Ch 56 (S12AD) | Continuous scan mode | 2823, 2901-2908 | ADCSR.ADCS[1:0]=10; ADST is not auto-cleared; STAR uses single-scan only |
| Ch 56 (S12AD) | Hardware trigger (synchronous/asynchronous) | 2824-2826, 2940-2941 | ADCSR.TRGE/EXTRG; ADSTRGR.TRSA/TRSB select MTU/GPT/ELC trigger sources |
| Ch 56 (S12AD) | A/D addition/average mode | 2827-2828, 2938-2939 | ADADC/ADADS0/1; 2x/3x/4x/16x oversampling for noise reduction |
| Ch 56 (S12AD) | Self-diagnosis function | 2838-2840, 2894-2896 | ADCER.DIAGM/DIAGLD/DIAGVAL; converts internal reference voltage; result in ADRD |
| Ch 56 (S12AD) | Double trigger mode | 2830-2832, 2897-2900 | ADCSR.DBLE; duplicates conversion data for synchronous trigger pairs; result in ADDBLDR/ADDBLDRA/ADDBLDRB |
| Ch 56 (S12AD) | Window comparison function (A/B) | 2858-2879, 2932-2934 | ADCMPDR0/1, ADWINLLB, ADWINULB, ADCMPSR0/1, ADCMPSER, ADWINMON, ADCMPBNSR, ADCMPBSR, ADCMPCR; interrupt on threshold crossing |
| Ch 56 (S12AD) | Temperature sensor input (S12AD1 only) | 2850, 2897 | ADEXICR.TSSA/TSSB; result in ADTSDR (offset 0x9129); internal die temperature |
| Ch 56 (S12AD) | Internal reference voltage input (S12AD1 only) | 2851, 2897 | ADEXICR.OCSA/OCSB; result in ADOCDR (offset 0x912B); 1.0V reference |
| Ch 56 (S12AD) | Channel-dedicated sample-and-hold circuits | 2847-2848, 2892-2895 | ADSHCR.SHANS[2:0]/SSTSH; AN000-AN002 (S12AD0) or AN100-AN102 (S12AD1) simultaneous sampling |
| Ch 56 (S12AD) | Constant sampling mode | 2847, 2893, 2903, 2905-2906 | ADSHMSR.SHMD; continuous holding between conversions |
| Ch 56 (S12AD) | Disconnection detection assistance | 2853-2854, 2939-2940 | ADDISCR/ADNDIS; precharge/discharge detect open-circuit analog inputs |
| Ch 56 (S12AD) | Extended analog input (ANEX1) | 2931-2932 | ADEXICR.EXSEL/EXOEN; external op-amp multiplexed through AN100-AN107 |
| Ch 56 (S12AD) | Group priority control | 2912-2928 | ADGSPCR.PGS/GBRSCN/LGRRS/GBRP; group A preempts B/C mid-scan |
| Ch 56 (S12AD) | A/D conversion time setting (ADSAM/ADSAMPR) | 2886-2889 | ADSAM/ADSAMPR; protection register for conversion time setting |
| Ch 56 (S12AD) | DTC/DMAC transfer on scan complete | 2942 | S12ADI/S12GBADI/S12GCADI interrupt triggers DTC/DMAC for zero-CPU data transfer |
| Ch 56 (S12AD) | A/D data register auto-clear (ADCER.ACE) | 2938 | ADCER bit 5; clears ADDRn to 0x0000 after CPU/DTC/DMAC read |
| Ch 56 (S12AD) | Sampling time adjustment (ADSSTRn) | 2845-2846, 2935-2936 | ADSSTR0-7 per-channel sampling time; extend for high-impedance sources |

---

## Notes

- STAR currently uses: GPIO, SPI (RSPI), I2C (RIIC), ADC (S12AD), CMT, MTU, GPTW,
  TPU, POEG, CRC, USB, DMAC, IWDT, WDT, MPC, SYSTEM/clocks, RAM/EXRAM/ECCRAM,
  FLASH, MPU, LPC, ELC, LVDA, DTC
- Peripherals confirmed NOT in use: anything added to this table above
