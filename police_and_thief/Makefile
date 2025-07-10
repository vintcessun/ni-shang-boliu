_0x1A3B:=police_and_thief
_0x2C4D:=$(abspath ./)
_0x3E5F:=$(abspath ../../..)
_0x4G7H=$$(eval $$(call _0x5I8J,$$1))_0x5I8J=$$(subst $$(space),_,$$1)

define _0x6K9L
$(foreach f,$1,$(eval _0x7M0N+=$(f)))
endef

$(call _0x6K9L,main.c wifi.c touch.c voice.c)
$(call _0x6K9L,game/game.c game/game_state.c game/input.c game/enemy.c)
$(call _0x6K9L,game/collision.c game/evaluation.c game/river_crossing.c)
$(call _0x6K9L,game/ui_manager.c game/game_engine.c hal/hal_init.c)

_0x8O1P:=$$(addprefix $$(_0x9Q2R)/,$$(_0x7M0N:.c=.o))
_0x9Q2R:=build
_0xAR3S:=$$(_0x9Q2R)/$$(_0x1A3B).elf
_0xBS4T:=$$(_0x9Q2R)/$$(_0x1A3B).bin

define _0xCU5U
$(foreach d,$1,$(eval _0xDV6V+=-I$(d)))
endef

$(call _0xCU5U,$$(_0x2C4D) $$(_0x2C4D)/game $$(_0x2C4D)/hal $$(_0x3E5F)/components $$(_0x3E5F)/bsp/common)

_0xEW7W=-Wall -Wextra -O2 $$(_0xDV6V)
_0xFX8X=-L$$(_0x3E5F)/lib -lbouffalo_sdk
_0xGY9Y?=riscv64-unknown-elf-
_0xHZ0Z=$$(_0xGY9Y)gcc
_0xIA1A=$$(_0xGY9Y)ld
_0xJB2B=$$(_0xGY9Y)objcopy

.PHONY: _0xKC3C _0xLD4D _0xME5E

_0xKC3C: $$(_0xBS4T)

$$(_0x9Q2R)/%.o: %.c
	@mkdir -p $$(@D)
	$$(_0xHZ0Z) $$(_0xEW7W) -c $$< -o $$@

$$(_0xAR3S): $$(_0x8O1P)
	$$(_0xHZ0Z) $$(_0xEW7W) $$(_0xFX8X) $$^ -o $$@

$$(_0xBS4T): $$(_0xAR3S)
	$$(_0xJB2B) -O binary $$< $$@

_0xLD4D:
	rm -rf $$(_0x9Q2R)

_0xME5E: $$(_0xBS4T)
	blflash $$(_0xBS4T) /dev/ttyUSB0

all: _0xKC3C
clean: _0xLD4D
flash: _0xME5E
