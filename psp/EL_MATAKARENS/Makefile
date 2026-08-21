BUILD_PRX = 1
.DEFAULT_GOAL := all
TARGET = el_matakarens

OBJS = main.o \
	wall1_png.o wall2_png.o wall3_png.o \
	enemy_idle1_png.o enemy_idle2_png.o enemy_hit_png.o hand_gun_png.o \
	bg_music_mp3.o gunshot_pcm.o enemy_die_pcm.o

CFLAGS = -O2 -G0 -Wall -Wextra -ffast-math
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)
LIBS = -lraylib -lpng -lz -lglut -lGLU -lGL -lpspvfpu \
	-lpspaudiolib -lpspaudio -lpspmp3 -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = EL MATAKARENS
PSP_EBOOT_ICON = ICON0.PNG

PSPSDK := $(shell psp-config --pspsdk-path)

wall1_png.o: assets/wall1.png
	psp-ld -r -b binary $< -o $@
wall2_png.o: assets/wall2.png
	psp-ld -r -b binary $< -o $@
wall3_png.o: assets/wall3.png
	psp-ld -r -b binary $< -o $@
enemy_idle1_png.o: assets/enemy_idle1.png
	psp-ld -r -b binary $< -o $@
enemy_idle2_png.o: assets/enemy_idle2.png
	psp-ld -r -b binary $< -o $@
enemy_hit_png.o: assets/enemy_hit.png
	psp-ld -r -b binary $< -o $@
hand_gun_png.o: assets/hand_gun.png
	psp-ld -r -b binary $< -o $@
bg_music_mp3.o: assets/bg_music.mp3
	psp-ld -r -b binary $< -o $@
gunshot_pcm.o: assets/gunshot.pcm
	psp-ld -r -b binary $< -o $@
enemy_die_pcm.o: assets/enemy_die.pcm
	psp-ld -r -b binary $< -o $@

include $(PSPSDK)/lib/build.mak
