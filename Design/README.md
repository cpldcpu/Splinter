# Design

Design notes, simulations, and captured results while bringing up the Splinter hardware.

## Contents

- `SimpleBoost_V4_simple.asc` / `SimpleBoost_V5_opamp.asc` - LTspice simulations for the boost converter and op-amp control loop.
- `booster.xlsx` - sizing / calculator spreadsheet for the booster.
- `screenshots/` - captured results from bring-up experiments (`Firmware/src_pgmtest/` and `Firmware/src_boostertest/`).

## Bring-up / Verification

This section collects notes and captures from bring-up experiments across the firmware test applications.
For implementation details, see `Firmware/src_boostertest/README.md` and `Firmware/src_pgmtest/README.md`.

### Booster electrical characterization (Firmware/src_boostertest)

#### Booster ripple

Booster frequency was set so Vboost ~15V, 1 kohm load. Scope settings: 1 us/div (horizontal) and 50 mV/div (vertical). Measured ripple: 64 mV Vpp. The ripple remained similar across load conditions.

![Booster ripple](screenshots/booster_ripple.png)

#### Booster voltage vs duty cycle and load

![Booster V vs duty and load](screenshots/BoosterV_vs_DC_and_Load.png)

#### Booster current vs duty cycle and load
![Booster I vs duty and load](screenshots/BoosterI_vs_dutyc_and_Load.png)

#### VPP vs PWM_VPP and VBOOST

![VPP vs PWM_VPP and VBOOST](screenshots/Vpp_vs_PWM_VPP_and_Vboost.png)

### PDK programming (Firmware/src_pgmtest)

Bring-up notes (duplicated from the firmware README):
- Target pin mapping: PC5=ICPCK, PC6=MOSI (3-wire only), PC7=ICPDA/MISO (2-wire data or 3-wire MISO).
- Power sequencing: VDD/VPP are derived from BOOST; keep VDD/VPP at 0V while enabling BOOST, then cycle VDD/VPP per programming step.
- IO level note: without level shifting, we keep target VDD close to ~5V during bring-up for reliable GPIO logic levels.

#### PFS154 read

![PFS154 read](screenshots/read_pfs154.png)

#### PMS150C read


![PMS150C read](screenshots/read_pms150c.png)

#### PFS154 erase + write + read

![PFS154 erase/write/read](screenshots/erase_write_read_pfs154.png)

16 words were written starting at address `0x0100` (test pattern `0x0000..x000f`).
