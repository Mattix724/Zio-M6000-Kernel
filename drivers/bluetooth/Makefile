#
# Makefile for the Linux Bluetooth HCI device drivers.
#

obj-$(CONFIG_BT_HCIVHCI)	+= hci_vhci.o
obj-$(CONFIG_BT_HCIUART)	+= hci_uart.o
obj-$(CONFIG_BT_HCIBCM203X)	+= bcm203x.o
obj-$(CONFIG_BT_HCIBPA10X)	+= bpa10x.o
obj-$(CONFIG_BT_HCIBFUSB)	+= bfusb.o
obj-$(CONFIG_BT_HCIDTL1)	+= dtl1_cs.o
obj-$(CONFIG_BT_HCIBT3C)	+= bt3c_cs.o
obj-$(CONFIG_BT_HCIBLUECARD)	+= bluecard_cs.o
obj-$(CONFIG_BT_HCIBTUART)	+= btuart_cs.o

obj-$(CONFIG_BT_HCIBTUSB)	+= btusb.o
obj-$(CONFIG_BT_HCIBTSDIO)	+= btsdio.o

obj-$(CONFIG_BT_MRVL)		+= btmrvl.o
obj-$(CONFIG_BT_MRVL_SDIO)	+= btmrvl_sdio.o

btmrvl-y			:= btmrvl_main.o
btmrvl-$(CONFIG_DEBUG_FS)	+= btmrvl_debugfs.o

hci_uart-y				:= hci_ldisc.o
hci_uart-$(CONFIG_BT_HCIUART_H4)	+= hci_h4.o
hci_uart-$(CONFIG_BT_HCIUART_BCSP)	+= hci_bcsp.o
hci_uart-$(CONFIG_BT_HCIUART_LL)	+= hci_ll.o
hci_uart-$(CONFIG_BT_HCIUART_BRCM)	+= hci_brcm.o
hci_uart-objs				:= $(hci_uart-y)
obj-$(CONFIG_BT_MSM_SLEEP)              += msm_bt_sleep.o
msm_bt_sleep-objs                       := bluesleep.o
obj-$(CONFIG_MSM_BT_POWER)		+= bluetooth-power.o
