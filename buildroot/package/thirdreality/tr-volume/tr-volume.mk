################################################################################
#
# tr-volume
#
################################################################################

TR_VOLUME_VERSION = 1.0.0
TR_VOLUME_SITE = $(TOPDIR)/package/thirdreality/tr-volume
TR_VOLUME_SITE_METHOD = local
TR_VOLUME_LICENSE = Apache-2.0
TR_VOLUME_DEPENDENCIES = pulseaudio

$(eval $(cmake-package))
