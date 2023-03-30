##
##
## If you received this File from Marvell, you may opt to use, redistribute and/or
## modify this File in accordance with the terms and conditions of the General
## Public License Version 2, June 1991 (the "GPL License"), a copy of which is
## available along with the File in the license.txt file or by writing to the Free
## Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 or
## on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
## THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED
## WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
## DISCLAIMED. The GPL License provides additional details about this warranty
## disclaimer.
##
##

#
# Makefile for the Marvell(R) YukonXG PCI Express ethernet driver
#

obj-m := oak_pci.o

oak_pci-objs := oak.o oak_net.o oak_ctl.o oak_irq.o oak_ethtool.o oak_unimac.o oak_debug.o oak_chksum.o oak_dpm.o

EXTRA_CFLAGS += -DKERNEL

ifeq ($(CONFIG_OAK_DEBUG),y)
EXTRA_CFLAGS += -DDEBUG -DSIMULATION -DUNIMAC_MQ_RR
endif

clean:
	rm -f core *.o *.a *.s *.ko .mrvl* .oak* .*.o.cmd Module.symvers Module.markers oak_pci.mod* modules.order

clobber: clean
	rm -f *.log
	rm -rf .tmp_versions

