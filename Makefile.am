pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = pal.pc
EXTRA_DIST = $(pkgconfig_DATA)

if BUILDSYSTEM_OPENWRT
h_sources = ./stream/inc/Stream.h \
            ./stream/inc/StreamCompress.h \
            ./stream/inc/StreamInCall.h \
            ./stream/inc/StreamPCM.h \
            ./stream/inc/StreamSoundTrigger.h \
            ./device/inc/Bluetooth.h \
            ./device/inc/Device.h \
            ./device/inc/DisplayPort.h \
            ./device/inc/Handset.h \
            ./device/inc/HandsetMic.h \
            ./device/inc/HandsetVaMic.h \
            ./device/inc/Headphone.h \
            ./device/inc/HeadsetMic.h \
            ./device/inc/HeadsetVaMic.h \
            ./device/inc/RTProxy.h \
            ./device/inc/Speaker.h \
            ./device/inc/SpeakerMic.h \
            ./device/inc/SpeakerProtection.h \
            ./device/inc/USBAudio.h \
            ./plugins/codecs/bt_intf.h \
            ./session/inc/Session.h \
            ./session/inc/PayloadBuilder.h \
            ./session/inc/SessionGsl.h \
            ./session/inc/SessionAlsaPcm.h \
            ./session/inc/SessionAlsaCompress.h \
            ./session/inc/SessionAlsaVoice.h \
            ./session/inc/SessionAlsaUtils.h \
            ./session/inc/SoundTriggerEngine.h \
            ./session/inc/SoundTriggerEngineGsl.h \
            ./session/inc/SoundTriggerEngineCapi.h \
            $(TOPDIR)/src/audio/mm-audio-headers/capiv2_api/capi_v2.h \
            $(TOPDIR)/src/audio/mm-audio-headers/capiv2_api/capi_v2_events.h \
            $(TOPDIR)/src/audio/mm-audio-headers/capiv2_api/capi_v2_extn.h \
            $(TOPDIR)/src/audio/mm-audio-headers/capiv2_api/capi_v2_properties.h \
            $(TOPDIR)/src/audio/mm-audio-headers/capiv2_api/capi_v2_types.h \
            ./resource_manager/inc/ResourceManager.h \
            ./PalDefs.h \
            ./PalApi.h \
            ./PalAudioRoute.h \
            ./PalCommon.h \
            ./utils/inc/PalRingBuffer.h \
            ./plugins/codecs/bt_intf.h \
            ./utils/inc/SoundTriggerPlatformInfo.h

AM_CPPFLAGS := -I ./stream/inc
AM_CPPFLAGS += -I ./device/inc
AM_CPPFLAGS += -I ./session/inc
AM_CPPFLAGS += -I ./resource_manager/inc
AM_CPPFLAGS += -I ./utils/inc
AM_CPPFLAGS += -I ./

AM_CPPFLAGS += $(SPF_CFLAGS)/
AM_CPPFLAGS += $(ACDBDATA_CFLAGS)
AM_CPPFLAGS += -I $(TOPDIR)/src/audio/mm-audio-headers/capiv2_api/

pal_sources = ./stream/src/Stream.cpp \
              ./stream/src/StreamCompress.cpp \
              ./stream/src/StreamInCall.cpp \
              ./stream/src/StreamPCM.cpp \
              ./stream/src/StreamSoundTrigger.cpp \
              ./device/src/Bluetooth.cpp \
              ./device/src/Device.cpp \
              ./device/src/DisplayPort.cpp \
              ./device/src/Handset.cpp \
              ./device/src/HandsetMic.cpp \
              ./device/src/HandsetVaMic.cpp \
              ./device/src/Headphone.cpp \
              ./device/src/HeadsetMic.cpp \
              ./device/src/HeadsetVaMic.cpp \
              ./device/src/RTProxy.cpp \
              ./device/src/Speaker.cpp \
              ./device/src/SpeakerMic.cpp \
              ./device/src/SpeakerProtection.cpp \
              ./device/src/USBAudio.cpp \
              ./session/src/Session.cpp \
              ./session/src/PayloadBuilder.cpp \
              ./session/src/SessionAlsaUtils.cpp \
              ./session/src/SessionAlsaPcm.cpp \
              ./session/src/SessionAlsaCompress.cpp \
              ./session/src/SessionAlsaVoice.cpp \
              ./session/src/SoundTriggerEngine.cpp \
              ./session/src/SoundTriggerEngineGsl.cpp \
              ./session/src/SoundTriggerEngineCapi.cpp \
              ./resource_manager/src/ResourceManager.cpp \
              ./resource_manager/src/SndCardMonitor.cpp \
              ./Pal.cpp \
              ./utils/src/PalRingBuffer.cpp \
              ./utils/src/SoundTriggerPlatformInfo.cpp
else
h_sources = ${top_srcdir}/stream/inc/Stream.h \
            ${top_srcdir}/stream/inc/StreamCompress.h \
            ${top_srcdir}/stream/inc/StreamInCall.h \
            ${top_srcdir}/stream/inc/StreamPCM.h \
            ${top_srcdir}/stream/inc/StreamSoundTrigger.h \
            ${top_srcdir}/device/inc/Bluetooth.h \
            ${top_srcdir}/device/inc/Device.h \
            ${top_srcdir}/device/inc/DisplayPort.h \
            ${top_srcdir}/device/inc/Handset.h \
            ${top_srcdir}/device/inc/HandsetMic.h \
            ${top_srcdir}/device/inc/HandsetVaMic.h \
            ${top_srcdir}/device/inc/Headphone.h \
            ${top_srcdir}/device/inc/HeadsetMic.h \
            ${top_srcdir}/device/inc/HeadsetVaMic.h \
            ${top_srcdir}/device/inc/RTProxy.h \
            ${top_srcdir}/device/inc/Speaker.h \
            ${top_srcdir}/device/inc/SpeakerMic.h \
            ${top_srcdir}/device/inc/SpeakerProtection.h \
            ${top_srcdir}/device/inc/USBAudio.h \
            ${top_srcdir}/plugins/codecs/bt_intf.h \
            ${top_srcdir}/session/inc/Session.h \
            ${top_srcdir}/session/inc/PayloadBuilder.h \
            ${top_srcdir}/session/inc/SessionGsl.h \
            ${top_srcdir}/session/inc/SessionAlsaPcm.h \
            ${top_srcdir}/session/inc/SessionAlsaCompress.h \
            ${top_srcdir}/session/inc/SessionAlsaVoice.h \
            ${top_srcdir}/session/inc/SessionAlsaUtils.h \
            ${top_srcdir}/session/inc/SoundTriggerEngine.h \
            ${top_srcdir}/session/inc/SoundTriggerEngineGsl.h \
            ${top_srcdir}/session/inc/SoundTriggerEngineCapi.h \
            ${WORKSPACE}/audio/mm-audio-headers/capiv2_api/capi_v2.h \
            ${WORKSPACE}/audio/mm-audio-headers/capiv2_api/capi_v2_events.h \
            ${WORKSPACE}/audio/mm-audio-headers/capiv2_api/capi_v2_extn.h \
            ${WORKSPACE}/audio/mm-audio-headers/capiv2_api/capi_v2_properties.h \
            ${WORKSPACE}/audio/mm-audio-headers/capiv2_api/capi_v2_types.h \
            ${top_srcdir}/resource_manager/inc/ResourceManager.h \
            ${top_srcdir}/PalDefs.h \
            ${top_srcdir}/PalApi.h \
            ${top_srcdir}/PalAudioRoute.h \
            ${top_srcdir}/PalCommon.h \
            ${top_srcdir}/utils/inc/PalRingBuffer.h \
            ${top_srcdir}/utils/inc/SoundTriggerPlatformInfo.h

AM_CPPFLAGS := -I $(top_srcdir)/stream/inc
AM_CPPFLAGS += -I $(top_srcdir)/device/inc
AM_CPPFLAGS += -I $(top_srcdir)/session/inc
AM_CPPFLAGS += -I $(top_srcdir)/resource_manager/inc
AM_CPPFLAGS += -I $(top_srcdir)/utils/inc
AM_CPPFLAGS += -I $(top_srcdir)/plugins/codecs
AM_CPPFLAGS += -I $(top_srcdir)/
AM_CPPFLAGS += $(SPF_CFLAGS)/
AM_CPPFLAGS += $(ACDBDATA_CFLAGS)
AM_CPPFLAGS += -I $(PKG_CONFIG_SYSROOT_DIR)/usr/include/agm
AM_CPPFLAGS += -I $(PKG_CONFIG_SYSROOT_DIR)/usr/include/spf
AM_CPPFLAGS += -I $(PKG_CONFIG_SYSROOT_DIR)/usr/include/

AM_CPPFLAGS += -I ${WORKSPACE}/audio/mm-audio-headers/capiv2_api/

pal_sources = ${top_srcdir}/stream/src/Stream.cpp \
              ${top_srcdir}/stream/src/StreamCompress.cpp \
              ${top_srcdir}/stream/src/StreamInCall.cpp \
              ${top_srcdir}/stream/src/StreamPCM.cpp \
              ${top_srcdir}/stream/src/StreamSoundTrigger.cpp \
              ${top_srcdir}/device/src/Bluetooth.cpp \
              ${top_srcdir}/device/src/Device.cpp \
              ${top_srcdir}/device/src/DisplayPort.cpp \
              ${top_srcdir}/device/src/Handset.cpp \
              ${top_srcdir}/device/src/HandsetMic.cpp \
              ${top_srcdir}/device/src/HandsetVaMic.cpp \
              ${top_srcdir}/device/src/Headphone.cpp \
              ${top_srcdir}/device/src/HeadsetMic.cpp \
              ${top_srcdir}/device/src/HeadsetVaMic.cpp \
              ${top_srcdir}/device/src/RTProxy.cpp \
              ${top_srcdir}/device/src/Speaker.cpp \
              ${top_srcdir}/device/src/SpeakerMic.cpp \
              ${top_srcdir}/device/src/SpeakerProtection.cpp \
              ${top_srcdir}/device/src/USBAudio.cpp \
              ${top_srcdir}/session/src/Session.cpp \
              ${top_srcdir}/session/src/PayloadBuilder.cpp \
              ${top_srcdir}/session/src/SessionAlsaUtils.cpp \
              ${top_srcdir}/session/src/SessionAlsaPcm.cpp \
              ${top_srcdir}/session/src/SessionAlsaCompress.cpp \
              ${top_srcdir}/session/src/SessionAlsaVoice.cpp \
              ${top_srcdir}/session/src/SoundTriggerEngine.cpp \
              ${top_srcdir}/session/src/SoundTriggerEngineGsl.cpp \
              ${top_srcdir}/session/src/SoundTriggerEngineCapi.cpp \
              ${top_srcdir}/resource_manager/src/ResourceManager.cpp \
              ${top_srcdir}/resource_manager/src/SndCardMonitor.cpp \
              ${top_srcdir}/Pal.cpp \
              ${top_srcdir}/utils/src/PalRingBuffer.cpp \
              ${top_srcdir}/utils/src/SoundTriggerPlatformInfo.cpp

endif

AM_CPPFLAGS += -D__unused=__attribute__\(\(__unused__\)\) -DLINUX_ENABLED
AM_CPPFLAGS += -DCONFIG_GSL

library_include_HEADERS = $(h_sources)
library_includedir = $(includedir)/pal

lib_LTLIBRARIES     = libpal.la
libpal_la_SOURCES   = $(pal_sources)
if BUILDSYSTEM_OPENWRT
libpal_la_LIBADD    = $(GLIB_LIBS) -ltinyalsa -laudioroute -lar_osal -lexpat -ltinycompress -lagmclientwrapper
libpal_la_LIBADD   += -lar_gsl -lacdbdata
else
libpal_la_LIBADD    = $(GLIB_LIBS) -ltinyalsa -laudioroute -lar_osal -lexpat -ltinycompress
endif
if IS_SDXLEMUR
libpal_la_LIBADD    += -lagmclientwrapper
endif
if IS_SA525M
libpal_la_LIBADD    += -lagmclientwrapper
endif
libpal_la_CPPFLAGS := $(AM_CPPFLAGS)
libpal_la_CPPFLAGS += -std=c++14 -DHAVE_PTHREADS -DHAVE_ANDROID_OS
libpal_la_CPPFLAGS += -DDEBUG_REFS_CALLSTACK_ENABLED=0
libpal_la_CPPFLAGS += -DNDEBUG
libpal_la_LDFLAGS   = -shared -avoid-version
libpal_la_CPPFLAGS += $(GLIB_CFLAGS) -include glib.h
if USE_G_STRLCPY
libpal_la_CPPFLAGS += -Dstrlcat=g_strlcat -Dstrlcpy=g_strlcpy
endif

if BUILDSYSTEM_OPENWRT
libpal_la_LIBADD += -lglib-2.0
endif
if COMPILE_COMPRESS
libpal_la_CPPFLAGS += -DSND_COMPRESS_DEC_HDR
endif
