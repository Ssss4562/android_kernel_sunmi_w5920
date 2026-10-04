obj-$(CONFIG_MTK_I2C) := i2c.o i2c_common.o

# w5920: AOSP-compatible i2c_msg handling (auto-DMA for transfers >8B).
# Full ALPS builds define this globally; standalone builds must set it
# here or every I2C transfer longer than the 8-byte FIFO fails.
ccflags-$(CONFIG_MTK_I2C) += -DCOMPATIBLE_WITH_AOSP
