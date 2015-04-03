#----------------------------------------------------------------------------
# Using Raspberry Pi and linuxspi to program ATmega
#----------------------------------------------------------------------------
# On command line:
#
# make all = Make all projects
#
# make test = Make test project.
# make base = Make base project.
# make node = Make node project.
# make radio = Make radio project.
#
# make clean = Clean out built project files.
#
# make fuses = Set device fuses, using avrdude.
#              Please customize the avrdude FUSES settings below first!
#
# make device = Get device signature and fuses, using avrdude.
#
# make progbase = Download the base hex file to the device, using avrdude.
#
# make prognode = Download the node hex file to the device, using avrdude.
#
# To rebuild project do "make clean" then "make all".
#----------------------------------------------------------------------------
# MCU name
MCU = atmega32

FUSES = -U lfuse:w:0xE4:m -U hfuse:w:0xD9:m

#---------------- Programming Options (avrdude) ----------------

AVRDUDE = avrdude

# Programming hardware: alf avr910 avrisp bascom bsd 
# dt006 pavr picoweb pony-stk200 sp12 stk200 stk500
#
# Type: avrdude -c ?
# to get a full listing.
#
AVRDUDE_PROGRAMMER = linuxspi

# com1 = serial port. Use lpt1 to connect to parallel port.
#AVRDUDE_PORT = com1    # programmer connected to serial device
AVRDUDE_PORT = /dev/spidev0.0

AVRDUDE_WRITE_FLASH = -U flash:w:$(TARGET).hex
AVRDUDE_WRITE_EEPROM = -U eeprom:w:$(TARGET).eep

# Uncomment the following if you want avrdude's erase cycle counter.
# Note that this counter needs to be initialized first using -Yn,
# see avrdude manual.
#AVRDUDE_ERASE_COUNTER = -y

# Uncomment the following if you do /not/ wish a verification to be
# performed after programming the device.
#AVRDUDE_NO_VERIFY = -V

# Increase verbosity level.  Please use this when submitting bug
# reports about avrdude. See <http://savannah.nongnu.org/projects/avrdude> 
# to submit bug reports.
#AVRDUDE_VERBOSE = -v -v

AVRDUDE_FLAGS = -p $(MCU) -P $(AVRDUDE_PORT) -c $(AVRDUDE_PROGRAMMER)
AVRDUDE_FLAGS += $(AVRDUDE_NO_VERIFY)
AVRDUDE_FLAGS += $(AVRDUDE_VERBOSE)
AVRDUDE_FLAGS += $(AVRDUDE_ERASE_COUNTER)

# Default target.
all: build

build:
	cd test; make
	cd base; make
	cd node; make
	cd radio; make

# Base station target
base:
	cd base; make

basesize:
	cd base; make size

progbase: 
	cd base; make program

# Remote node target
node:
	cd node; make

nodesize:
	cd node; make size

prognode: 	
	cd node; make program

# FM Radio target
radio:
	cd radio; make

radiosize:
	cd radio; make size

progradio:
	cd radio; make program

# Test target
test:
	cd test; make

testsize:
	cd test; make size

progtest:
	cd test; make program

# Common targets
fuses:
	$(AVRDUDE) $(AVRDUDE_FLAGS) $(FUSES)

device:
	$(AVRDUDE) $(AVRDUDE_FLAGS)

reset:
	$(AVRDUDE) $(AVRDUDE_FLAGS)

clean:
	cd test; make clean
	cd base; make clean
	cd node; make clean
	cd radio; make clean

# Listing of phony targets.
.PHONY : all build clean reset fuses \
base basesize progbase \
node nodesize prognode \
radio radiosize progradio \
test testsize progtest \