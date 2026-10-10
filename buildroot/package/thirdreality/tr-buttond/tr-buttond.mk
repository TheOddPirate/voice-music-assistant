################################################################################
#
# tr-buttond
#
# ThirdReality button daemon. Owns /dev/input/event0 key dispatch and
# replaces the policy role of Amlogic's input_eventd (no longer started).
#
################################################################################

TR_BUTTOND_VERSION = 1.0.0
TR_BUTTOND_SITE = $(TOPDIR)/package/thirdreality/tr-buttond
TR_BUTTOND_SITE_METHOD = local
TR_BUTTOND_LICENSE = Apache-2.0

$(eval $(cmake-package))
