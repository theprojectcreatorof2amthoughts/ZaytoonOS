CC = gcc
CFLAGS = -std=gnu11 -nostdlib -m32 -ffreestanding -O2 -Wall -Wextra -fno-common -fno-builtin -fno-pic -fno-pie -fno-stack-protector -mno-sse -mno-sse2 -mno-mmx -I.
LDFLAGS = -m32 -Wl,-m,elf_i386 -ffreestanding -nostdlib -no-pie -static -T linker.ld
BUILD_DIR = build
APPCOMPILER_DIR = appcompiler
GENERATED_CAPPS = kernel/apps/generated_capps.c
PACKAGED_CAPPS = $(APPCOMPILER_DIR)/commandline.capp $(APPCOMPILER_DIR)/number_guess.capp $(APPCOMPILER_DIR)/zayflight.capp
HDD_IMAGE = zaytoonos-data.img
BOOT_DISK_IMAGE = zaytoonos-hdd.img
SRC_DIRS = . kernel kernel/arch kernel/irq kernel/mm kernel/drivers kernel/kernel kernel/user kernel/apps kernel/fs kernel/shell

SRC_C = $(sort $(wildcard $(addsuffix /*.c,$(SRC_DIRS))))
SRC_S = $(sort $(wildcard $(addsuffix /*.S,$(SRC_DIRS))))

OBJ = $(patsubst %.S,$(BUILD_DIR)/%.o,$(SRC_S)) $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRC_C))

DEP = $(OBJ:.o=.d)
OUT = krnl.elf
ISO_IMAGE = zaytoonos.iso
ISO_STAGING_DIR = $(BUILD_DIR)/iso

all: $(GENERATED_CAPPS) $(OUT)

$(GENERATED_CAPPS): $(PACKAGED_CAPPS) tools/embed_capps.sh
	./tools/embed_capps.sh $@ $(PACKAGED_CAPPS)

$(APPCOMPILER_DIR)/commandline.capp: $(APPCOMPILER_DIR)/commandline.c $(APPCOMPILER_DIR)/lib.c $(APPCOMPILER_DIR)/lib.h
	$(MAKE) -C $(APPCOMPILER_DIR) commandline.capp

$(APPCOMPILER_DIR)/number_guess.capp: $(APPCOMPILER_DIR)/number_guess.c $(APPCOMPILER_DIR)/lib.c $(APPCOMPILER_DIR)/lib.h
	$(MAKE) -C $(APPCOMPILER_DIR) number_guess.capp

$(APPCOMPILER_DIR)/zayflight.capp: $(APPCOMPILER_DIR)/zayflight.c $(APPCOMPILER_DIR)/lib.c $(APPCOMPILER_DIR)/lib.h
	$(MAKE) -C $(APPCOMPILER_DIR) zayflight.capp

$(HDD_IMAGE): tools/create_data_disk.py
	@mkdir -p $(BUILD_DIR)
	@test -e $@ || python3 tools/create_data_disk.py $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT): $(GENERATED_CAPPS) $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $(BUILD_DIR)/kernel/arch/boot.o $(filter-out $(BUILD_DIR)/kernel/arch/boot.o, $(OBJ)) -o $@

$(ISO_IMAGE): $(OUT) ../iso/boot/grub/grub.cfg
	mkdir -p $(ISO_STAGING_DIR)/boot/grub
	cp $(OUT) $(ISO_STAGING_DIR)/boot/krnl.elf
	cp ../iso/boot/grub/grub.cfg $(ISO_STAGING_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(ISO_STAGING_DIR)

$(BOOT_DISK_IMAGE): $(OUT) ../iso/boot/grub/grub.cfg tools/create_boot_disk.py
	python3 tools/create_boot_disk.py --overwrite $@ $(OUT) ../iso/boot/grub/grub.cfg

install-image: $(ISO_IMAGE)

disk-image: $(BOOT_DISK_IMAGE)

run: $(OUT) $(HDD_IMAGE)
	qemu-system-x86_64 -machine accel=tcg -serial stdio -display none -kernel $(OUT) \
		-drive file=$(HDD_IMAGE),format=raw,if=ide,index=0

run-iso: $(ISO_IMAGE) $(HDD_IMAGE)
	qemu-system-x86_64 -machine accel=tcg -serial stdio -display none -boot d \
		-cdrom $(ISO_IMAGE) -drive file=$(HDD_IMAGE),format=raw,if=ide,index=0

run-disk-image: $(BOOT_DISK_IMAGE)
	qemu-system-x86_64 -machine accel=tcg -serial stdio -display none -boot c \
		-drive file=$(BOOT_DISK_IMAGE),format=raw,if=ide,index=0

clean:
	rm -rf $(BUILD_DIR) $(OUT) $(ISO_IMAGE) $(BOOT_DISK_IMAGE)
	$(MAKE) -C $(APPCOMPILER_DIR) clean

.PHONY: all install-image disk-image run run-iso run-disk-image clean

-include $(DEP)
