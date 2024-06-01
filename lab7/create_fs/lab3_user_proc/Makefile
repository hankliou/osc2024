ARMGNU ?= aarch64-linux-gnu

BUILD_DIR = build
#---------------------------------------------------------------------------------------

ASM_FILES = $(wildcard *.S)
OBJ_FILES += $(ASM_FILES:%.S=$(BUILD_DIR)/%_s.o)

DEP_FILES = $(OBJ_FILES:%.o=%.d)
-include $(DEP_FILES)

$(BUILD_DIR)/%_s.o: %.S
	@mkdir -p $(@D)
	$(ARMGNU)-gcc -c $< -o $@

u_proc.img: linker.ld $(OBJ_FILES)
	$(ARMGNU)-ld -T linker.ld -o $(BUILD_DIR)/u_proc.elf  $(OBJ_FILES)
	$(ARMGNU)-objcopy $(BUILD_DIR)/u_proc.elf -O binary u_proc.img

all: u_proc.img

clean:
	rm -rf $(BUILD_DIR) *.img