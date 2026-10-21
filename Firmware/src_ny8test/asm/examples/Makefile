# NY8 assembler examples (target: NY8A050E1)
#
#   make                      assemble all examples -> build/<name>.hex .lst .h
#   make FINST=4000000        same, for a measured instruction clock (see clockout.asm)
#   make image PROG=blink IMAGE=path/ny8_image.h
#                             copy the programmer image of one example
#   make sim                  simulate all examples on the Verilog core and check the pins
#   make sim-blink            one example
#   make dis-blink            disassemble build/blink.hex
#   make clean
#
# Needs python3; the simulation also needs iverilog and the ny8cpu RTL (NY8CPU=..).

EXAMPLES = clockout blink timer_irq button fade morse
DEVICE   = NY8A050E1
TOOLS    = ../tools
INC      = ../inc
NYASM    = python3 $(TOOLS)/nyasm.py
BUILD    = build
NY8CPU  ?= ..

ASMFLAGS = -I $(INC) -d $(DEVICE) $(if $(FINST),-D FINST=$(FINST))

.PHONY: all image sim clean FORCE
.PRECIOUS: $(BUILD)/%.h $(BUILD)/sim/%.lst

all: $(EXAMPLES:%=$(BUILD)/%.h)

# rebuild when FINST is given on the command line
$(BUILD)/%.h: %.asm $(wildcard $(INC)/*.inc) $(if $(FINST),FORCE)
	@mkdir -p $(BUILD)
	$(NYASM) $(ASMFLAGS) $< -o $(BUILD)/$*.hex -l $(BUILD)/$*.lst -H $@

image: $(BUILD)/$(PROG).h
	@test -n "$(PROG)" -a -n "$(IMAGE)" || { echo "usage: make image PROG=<example> IMAGE=<ny8_image.h>"; exit 1; }
	cp $< $(IMAGE)
	@echo "$(PROG) -> $(IMAGE)"

dis-%: $(BUILD)/%.h
	python3 $(TOOLS)/ny8dis.py $<

# ---------------------------------------------------------------------------
# Simulation: instruction clock 100 kHz and shortened delays, so that each
# example runs for 2k..120k cycles. check_example.py derives the expected
# timing from the symbols in the listing.
SIM_FINST         = 100000
SIMDEF_blink      = -D BLINK_MS=20
SIMDEF_timer_irq  = -D BLINK_MS=200
SIMDEF_button     = -D DEBOUNCE_MS=5
SIMDEF_fade       = -D DSTEP=32
SIMDEF_morse      = -D UNIT_MS=2
CYC_clockout      = 2000
CYC_blink         = 20000
CYC_timer_irq     = 120000
CYC_button        = 22000
CYC_fade          = 80000
CYC_morse         = 40000
CHK_button        = presses=2

SIMDIR = $(BUILD)/sim
TBVVP  = $(SIMDIR)/tb.vvp

sim: $(EXAMPLES:%=sim-%)

$(TBVVP): $(wildcard $(NY8CPU)/sim/tb_example.v $(NY8CPU)/rtl/*.v)
	@test -f $(NY8CPU)/rtl/ny8_core.v || { echo "simulation needs the ny8cpu RTL, set NY8CPU=<path>"; exit 1; }
	@mkdir -p $(SIMDIR)
	iverilog -g2012 -o $@ $(NY8CPU)/sim/tb_example.v $(NY8CPU)/rtl/*.v

sim-%: %.asm $(TBVVP) FORCE
	@$(NYASM) -q -I $(INC) -d $(DEVICE) -D FINST=$(SIM_FINST) $(SIMDEF_$*) $< \
		-o $(SIMDIR)/$*.hex -l $(SIMDIR)/$*.lst
	@vvp -n $(TBVVP) +hex=$(SIMDIR)/$*.hex +maxcyc=$(CYC_$*) +edges=$(SIMDIR)/$*.edges \
		$(if $(wildcard $*.stim),+stim=$*.stim) > $(SIMDIR)/$*.log
	@python3 $(NY8CPU)/sim/check_example.py $* $(SIMDIR)/$*.lst $(SIMDIR)/$*.edges \
		log=$(SIMDIR)/$*.log $(CHK_$*)

FORCE:

clean:
	rm -rf $(BUILD)
