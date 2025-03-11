/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Changes from Qualcomm Innovation Center are provided under the following
 * license:
 *
 * Copyright (c) 2023, 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */


#define LOG_TAG "PAL: SessionAlsaVoice"

#include "SessionAlsaVoice.h"
#include "SessionAlsaUtils.h"
#include "Stream.h"
#include "ResourceManager.h"
#include "apm_api.h"
#include <sstream>
#include <string>
#include <agm_api.h>
#ifdef FEATURE_IPQ_OPENWRT
#include "audio_route.h"
#else
#include "audio_route/audio_route.h"
#endif

#define PAL_PADDING_8BYTE_ALIGN(x)  ((((x) + 7) & 7) ^ 7)
#define MAX_VOL_INDEX 5
#define MIN_VOL_INDEX 0
#define percent_to_index(val, min, max) \
            ((val) * ((max) - (min)) * 0.01 + (min) + .5)

#define NUM_OF_CAL_KEYS 2



void SessionAlsaVoice::HandleDtmfCallBack(uint64_t hdl, uint32_t event_id,
                                          void *data, uint32_t event_size)
{
    dtmf_event_data event_data;
    pal_stream_callback cb;
    struct dtmf_detect_event_t *dtmf_info = nullptr;
    Stream *s = NULL;

    PAL_ERR(LOG_TAG, "Enter");

    if ((hdl == 0) || !data || !event_size) {
        PAL_ERR(LOG_TAG, "Invalid stream handle or event data or event size");
        return;
    }
    PAL_ERR(LOG_TAG, "Enter, event detected on SPF, event id = 0x%x", event_id);
    if (event_id != EVENT_ID_DTMF_DETECTION) {
        return;
    }
    PAL_ERR(LOG_TAG, "EVENT_ID_DTMF_DETECTION detected on SPF, event id = 0x%x", event_id);
    dtmf_info = (struct dtmf_detect_event_t *)data;
    PAL_ERR(LOG_TAG, "high_freq: %d, low_freq: %d",
            dtmf_info->tone_high_freq, dtmf_info->tone_low_freq);
    s = reinterpret_cast<Stream *>(hdl);
    //payload_size = sizeof(struct dtmf_detect_event_t);
    event_data.dtmf_high_freq = dtmf_info->tone_high_freq;
    event_data.dtmf_low_freq = dtmf_info->tone_low_freq;
    PAL_ERR(LOG_TAG, "high_freq: %d, low_freq: %d",
            event_data.dtmf_high_freq, event_data.dtmf_low_freq);

    if (s->getCallBack(&cb) == 0) {
        if (cb) {
            PAL_ERR(LOG_TAG, "found callback");
             cb(reinterpret_cast<pal_stream_handle_t *>(s), PAL_DTMF_CBK_EVENT, (uint32_t *)&event_data,
                event_size, s->cookie);
        }
    }

    PAL_ERR(LOG_TAG, "Exit");
    return;
}

SessionAlsaVoice::SessionAlsaVoice(std::shared_ptr<ResourceManager> Rm)
{
   rm = Rm;
   builder = new PayloadBuilder();
   pcmRx = NULL;
   pcmTx = NULL;
   customPayload = NULL;
   customPayloadSize = 0;
   sessionCb = NULL;
   mState = SESSION_IDLE;
   this->cbCookie = 0;
}

SessionAlsaVoice::~SessionAlsaVoice()
{
   delete builder;

}

bool SessionAlsaVoice::isActive()
{
    PAL_VERBOSE(LOG_TAG, "state = %d", mState);
    return mState == SESSION_STARTED;
}

uint32_t SessionAlsaVoice::getMIID(const char *backendName, uint32_t tagId, uint32_t *miid)
{
    int status = 0;
    int device = 0;

    switch (tagId) {
    case DEVICE_HW_ENDPOINT_TX:
        device = pcmDevTxIds.at(0);
        break;
    case DEVICE_HW_ENDPOINT_RX:
        device = pcmDevRxIds.at(0);
        break;
    case RAT_RENDER:
        if(strstr(backendName,"TX"))
            device = pcmDevTxIds.at(0);
        else
            device = pcmDevRxIds.at(0);
        break;
    case DTMF_GENERATOR:
        device = pcmDevRxIds.at(0);
        break;
    case DTMF_DETECTOR:
        device = pcmDevRxIds.at(0);
        break;
    default:
        PAL_INFO(LOG_TAG, "Unsupported tag info %x",tagId);
        return -EINVAL;
    }

    status = SessionAlsaUtils::getModuleInstanceId(mixer, device,
                                                   backendName,
                                                   tagId, miid);
    if (0 != status)
        PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", tagId, status);

    return status;
}


int SessionAlsaVoice::prepare(Stream * s __unused)
{
   return 0;
}

int SessionAlsaVoice::open(Stream * s)
{
    int status = -EINVAL;
    struct pal_stream_attributes sAttr;
    std::vector<std::shared_ptr<Device>> associatedDevices;
    PAL_DBG(LOG_TAG,"Enter \n");

    status = s->getStreamAttributes(&sAttr);
    if(0 != status) {
        PAL_ERR(LOG_TAG,"getStreamAttributes Failed \n");
        goto exit;
    }

    status = s->getAssociatedDevices(associatedDevices);
    if(0 != status) {
        PAL_ERR(LOG_TAG,"getAssociatedDevices Failed \n");
        goto exit;
    }

    if (sAttr.direction != (PAL_AUDIO_INPUT|PAL_AUDIO_OUTPUT)) {
        PAL_ERR(LOG_TAG,"Voice session dir must be input and output");
        goto exit;
    }

    pcmDevRxIds = rm->allocateFrontEndIds(sAttr, RXDIR);
    pcmDevTxIds = rm->allocateFrontEndIds(sAttr, TXDIR);

    vsid = sAttr.info.voice_call_info.VSID;
    ttyMode = sAttr.info.voice_call_info.tty_mode;

    rm->getBackEndNames(associatedDevices, rxAifBackEnds, txAifBackEnds);

    if (txAifBackEnds.empty()) {
        status = -EINVAL;
        PAL_ERR(LOG_TAG, "no TX backend specified for this stream\n");
        goto exit;
    }

    if (rxAifBackEnds.empty()) {
        status = -EINVAL;
        PAL_ERR(LOG_TAG, "no RX backend specified for this stream\n");
        goto exit;
    }

    status = rm->getVirtualAudioMixer(&mixer);
    if (status) {
        PAL_ERR(LOG_TAG,"mixer error");
        goto exit;
    }

    status = SessionAlsaUtils::open(s, rm, pcmDevRxIds, pcmDevTxIds,
                                    rxAifBackEnds, txAifBackEnds);

    if (status) {
        PAL_ERR(LOG_TAG, "session alsa open failed with %d", status);
        rm->freeFrontEndIds(pcmDevRxIds, sAttr, RXDIR);
        rm->freeFrontEndIds(pcmDevTxIds, sAttr, TXDIR);
        goto exit;
    }

    if (((sAttr.type == PAL_STREAM_VOICE_CALL) ||
        (sAttr.type == PAL_STREAM_VOICE_CALL_RX_TX)) &&
        (rm->dtmf_enabled)) {
        PAL_DBG(LOG_TAG, "before registerMixerEventCallback");
        if (!sessionCb) {
            PAL_ERR(LOG_TAG, "SessionCb is null, registerCallback");
            registerCallBack(HandleDtmfCallBack, (uint64_t)s);
        }
        status = rm->registerMixerEventCallback(pcmDevRxIds,
            sessionCb, cbCookie, true);
        if (status != 0) {
            PAL_ERR(LOG_TAG, "Failed to register callback to rm for RX");
        }
        status = rm->registerMixerEventCallback(pcmDevTxIds,
            sessionCb, cbCookie, true);
        if (status != 0) {
            PAL_ERR(LOG_TAG, "Failed to register callback to rm for TX");
        }
        PAL_DBG(LOG_TAG, "after registerMixerEventCallback for DTMF RX/TX");
    }

exit:
    PAL_DBG(LOG_TAG,"Exit \n");
    return status;
}

int SessionAlsaVoice::setSessionParameters(Stream *s, int dir)
{
    int status = 0;
    int pcmId = 0;
    uint8_t *payload = NULL;
    size_t payloadSize = 0;

    if (dir == RXDIR) {
        pcmId = pcmDevRxIds.at(0);
        status = populate_rx_mfc_payload(s, &payload, &payloadSize);
        if (0 != status) {
            PAL_ERR(LOG_TAG,"populating vsid payload for RX Failed:%d", status);
            goto exit;
        }

        // populate_vsid_payload, appends to the existing payload
        status = populate_vsid_payload(s, &payload, &payloadSize);
        if (0 != status) {
            PAL_ERR(LOG_TAG,"populating vsid payload for RX Failed:%d", status);
            goto exit;
        }
    } else {
        pcmId = pcmDevTxIds.at(0);
        status = populate_vsid_payload(s, &payload, &payloadSize);
        if (0 != status) {
            PAL_ERR(LOG_TAG,"populating vsid payload for TX Failed:%d", status);
            goto exit;
        }
    }

    status = SessionAlsaUtils::setMixerParameter(mixer, pcmId,
                                                 payload, payloadSize);
    if (status != 0) {
        PAL_ERR(LOG_TAG,"setMixerParameter failed:%d for dir:%s",
                status, (dir == RXDIR)?"RX":"TX");
        goto exit;
    }

exit:
    if (payload) {
        free(payload);
    }
    return status;
}

int SessionAlsaVoice::populate_vsid_payload(Stream *s __unused, uint8_t **payload,
                                            size_t *payloadSize)
{
    int status = 0;
    apm_module_param_data_t* header;
    uint8_t* vsidPayload = NULL;
    size_t vsidpayloadSize = 0, padBytes = 0;
    uint8_t *vsid_pl;
    vcpm_param_vsid_payload_t vsid_payload;

    vsidpayloadSize = sizeof(struct apm_module_param_data_t)+
                  sizeof(vcpm_param_vsid_payload_t);
    padBytes = PAL_PADDING_8BYTE_ALIGN(vsidpayloadSize);

    vsidPayload =  (uint8_t *) realloc((void *)*payload,
                                       (*payloadSize + vsidpayloadSize + padBytes));
    if (!vsidPayload) {
        PAL_ERR(LOG_TAG, "payloadInfo realloc failed %s", strerror(errno));
        return -EINVAL;
    }
    //set base out pointer to new address
    *payload = vsidPayload;
    //update payloadinfo so vsid can be added
    vsidPayload = vsidPayload + (*payloadSize);
    //update overall payload size
    *payloadSize += (vsidpayloadSize + padBytes);

    header = (apm_module_param_data_t*)vsidPayload;
    header->module_instance_id = VCPM_MODULE_INSTANCE_ID;
    header->param_id = VCPM_PARAM_ID_VSID;
    header->error_code = 0x0;
    header->param_size = vsidpayloadSize - sizeof(struct apm_module_param_data_t);

    vsid_payload.vsid = vsid;
    vsid_pl = (uint8_t*)vsidPayload + sizeof(apm_module_param_data_t);
    ar_mem_cpy(vsid_pl,  sizeof(vcpm_param_vsid_payload_t),
                     &vsid_payload,  sizeof(vcpm_param_vsid_payload_t));

    return status;
}

int SessionAlsaVoice::populate_rx_mfc_payload(Stream *s, uint8_t **payload, size_t *payloadSize)
{
    int status = 0;
    std::vector<std::shared_ptr<Device>> associatedDevices;
    struct pal_device dAttr;
    struct sessionToPayloadParam deviceData;
    uint32_t miid = 0;
    int dev_id = 0;

    memset(&dAttr, 0, sizeof(struct pal_device));
    status = s->getAssociatedDevices(associatedDevices);
    if (0 != status) {
        PAL_ERR(LOG_TAG,"getAssociatedDevices Failed \n");
        return status;
    }

    rm->getBackEndNames(associatedDevices, rxAifBackEnds, txAifBackEnds);
    if (rxAifBackEnds.empty() && txAifBackEnds.empty()) {
        status = -EINVAL;
        PAL_ERR(LOG_TAG, "no backend specified for this stream");
        return status;
    }

    status = SessionAlsaUtils::getModuleInstanceId(mixer, pcmDevRxIds.at(0),
                                                   rxAifBackEnds[0].second.c_str(),
                                                   TAG_DEVICE_PP_MFC, &miid);
    if (status != 0) {
        PAL_ERR(LOG_TAG,"getModuleInstanceId failed status:%d", status);
        return status;
    }

    for (int i = 0; i < associatedDevices.size(); i++) {
        dev_id = associatedDevices[i]->getSndDeviceId();
        if (rm->isOutputDevId(dev_id)) {
            status = associatedDevices[i]->getDeviceAttributes(&dAttr);
            break;
        }
    }
    deviceData.bitWidth = dAttr.config.bit_width;
    deviceData.sampleRate = dAttr.config.sample_rate;
    deviceData.numChannel = dAttr.config.ch_info.channels;
    deviceData.ch_info = nullptr;
    builder->payloadMFCConfig((uint8_t**)payload, payloadSize, miid, &deviceData);

    return status;
}

int SessionAlsaVoice::start(Stream * s)
{
    struct pcm_config config;
    struct pal_stream_attributes sAttr;
    int32_t status = 0;
    std::vector<std::shared_ptr<Device>> associatedDevices;
    pal_param_payload *palPayload = NULL;
    int txDevId = PAL_DEVICE_NONE;
    uint8_t* payload = NULL;
    size_t payloadSize = 0;
    struct pal_volume_data *volume = NULL;
    bool isTxStarted = false, isRxStarted = false;

    status = s->getStreamAttributes(&sAttr);
    if (status != 0) {
        PAL_ERR(LOG_TAG,"stream get attributes failed");
        return status;
    }

    if (mState == SESSION_IDLE) {
        s->getBufInfo(&in_buf_size,&in_buf_count,&out_buf_size,&out_buf_count);
        memset(&config, 0, sizeof(config));

        config.rate = sAttr.out_media_config.sample_rate;
        if (sAttr.out_media_config.bit_width == 32)
            config.format = PCM_FORMAT_S32_LE;
        else if (sAttr.out_media_config.bit_width == 24)
            config.format = PCM_FORMAT_S24_3LE;
        else if (sAttr.out_media_config.bit_width == 16)
            config.format = PCM_FORMAT_S16_LE;
        config.channels = sAttr.out_media_config.ch_info.channels;
        config.period_size = out_buf_size;
        config.period_count = out_buf_count;
        config.start_threshold = 0;
        config.stop_threshold = 0;
        config.silence_threshold = 0;

        pcmRx = pcm_open(rm->getVirtualSndCard(), pcmDevRxIds.at(0), PCM_OUT, &config);
        if (!pcmRx) {
            PAL_ERR(LOG_TAG, "pcm-rx open failed");
            status = -EINVAL;
            goto err_pcm_open;
        }

        if (!pcm_is_ready(pcmRx)) {
            PAL_ERR(LOG_TAG, "pcm-rx open not ready");
            status = -EINVAL;
            goto err_pcm_open;
        }

        config.rate = sAttr.in_media_config.sample_rate;
        if (sAttr.in_media_config.bit_width == 32)
            config.format = PCM_FORMAT_S32_LE;
        else if (sAttr.in_media_config.bit_width == 24)
            config.format = PCM_FORMAT_S24_3LE;
        else if (sAttr.in_media_config.bit_width == 16)
            config.format = PCM_FORMAT_S16_LE;
        config.channels = sAttr.in_media_config.ch_info.channels;
        config.period_size = in_buf_size;
        config.period_count = in_buf_count;

        pcmTx = pcm_open(rm->getVirtualSndCard(), pcmDevTxIds.at(0), PCM_IN, &config);
        if (!pcmTx) {
            PAL_ERR(LOG_TAG, "pcm-tx open failed");
            status = -EINVAL;
            goto err_pcm_open;
        }

        if (!pcm_is_ready(pcmTx)) {
            PAL_ERR(LOG_TAG, "pcm-tx open not ready");
            status = -EINVAL;
            goto err_pcm_open;
        }
    }
    mState = SESSION_OPENED;

    SessionAlsaVoice::setConfig(s, MODULE, VSID, RXDIR);
    /*if no volume is set set a default volume*/
    if ((s->getVolumeData(volume))) {
        PAL_INFO(LOG_TAG, "no volume set, setting default vol to %f",
                 default_volume);
        volume = (struct pal_volume_data *)malloc(sizeof(uint32_t) +
                                                  (sizeof(struct pal_channel_vol_kv)));
        if (!volume) {
            status = -ENOMEM;
            PAL_ERR(LOG_TAG, "volume malloc failed %s", strerror(errno));
            goto err_pcm_open;
        }
        volume->no_of_volpair = 1;
        volume->volume_pair[0].channel_mask = 1;
        volume->volume_pair[0].vol = default_volume;
        /*call will cache the volume but not apply it as stream has not moved to start state*/
        s->setVolume(volume);
        /*call to apply volume*/
        setConfig(s, CALIBRATION, TAG_STREAM_VOLUME, RXDIR);


    };

    /*set tty mode*/
    if (ttyMode) {
        palPayload = (pal_param_payload *)calloc(1,
                                 sizeof(pal_param_payload) + sizeof(ttyMode));
        if (!palPayload) {
            status = -ENOMEM;
            PAL_ERR(LOG_TAG,"Failed to allocate memory for palPayload \n");
            goto err_pcm_open;
        }
        palPayload->payload_size = sizeof(ttyMode);
        *(palPayload->payload) = ttyMode;
        setParameters(s, TTY_MODE, PAL_PARAM_ID_TTY_MODE, palPayload);
    }


    status = populate_rx_mfc_payload(s, &payload, &payloadSize);
    if (status != 0) {
        PAL_ERR(LOG_TAG,"Configuring RX MFC failed");
        goto err_pcm_open;
    }
    status = SessionAlsaUtils::setMixerParameter(mixer, pcmDevRxIds.at(0),
                                                 payload, payloadSize);
    if (status != 0) {
        PAL_ERR(LOG_TAG,"setMixerParameter failed");
        goto err_pcm_open;
    }

    status = pcm_start(pcmRx);
    if (status) {
        PAL_ERR(LOG_TAG, "pcm_start rx failed %d", status);
        goto err_pcm_open;
    }
   isRxStarted = true;

    status = pcm_start(pcmTx);
    if (status) {
        PAL_ERR(LOG_TAG, "pcm_start tx failed %d", status);
        goto err_pcm_open;
    }
    isTxStarted = true;

    mState = SESSION_STARTED;
    /*set sidetone*/
    status = getTXDeviceId(s, &txDevId);
    if (status){
        PAL_ERR(LOG_TAG, "could not find TX device associated with this stream cannot set sidetone");
    } else {
        status = setSidetone(txDevId,s,1);
        if(0 != status) {
            PAL_ERR(LOG_TAG,"enabling sidetone failed \n");
        }
    }
    status = 0;
    goto exit;

err_pcm_open:
    if (pcmRx) {
        if (isRxStarted)
            pcm_stop(pcmRx);
        pcm_close(pcmRx);
        pcmRx = NULL;
    }
    if (pcmTx) {
        if (isTxStarted)
            pcm_stop(pcmTx);
        pcm_close(pcmTx);
        pcmTx = NULL;
    }
exit:
    if (payload)
        free(payload);
    if (palPayload) {
        free(palPayload);
    }
    if (volume)
        free(volume);
    return status;
}

int SessionAlsaVoice::stop(Stream * s __unused)
{
    int status = 0;
    int txDevId = PAL_DEVICE_NONE;

    /*disable sidetone*/
    status = getTXDeviceId(s, &txDevId);
    if (status){
        PAL_ERR(LOG_TAG, "could not find TX device associated with this stream cannot set sidetone");
    } else {
        status = setSidetone(txDevId,s,0);
        if(0 != status) {
            PAL_ERR(LOG_TAG,"disabling sidetone failed");
        }
    }
    if (pcmRx && isActive()) {
        status = pcm_stop(pcmRx);
        if (status) {
            PAL_ERR(LOG_TAG, "pcm_stop - rx failed %d", status);
        }
    }

    if (pcmTx && isActive()) {
        status = pcm_stop(pcmTx);
        if (status) {
            PAL_ERR(LOG_TAG, "pcm_stop - tx failed %d", status);
        }
    }

    mState = SESSION_STOPPED;

    return status;
}

int SessionAlsaVoice::close(Stream * s)
{
    int status = 0;
    struct pal_stream_attributes sAttr;
    status = s->getStreamAttributes(&sAttr);
    if (status != 0) {
        PAL_ERR(LOG_TAG,"stream get attributes failed");
        return status;
    }

    if (((sAttr.type == PAL_STREAM_VOICE_CALL) ||
        (sAttr.type == PAL_STREAM_VOICE_CALL_RX_TX)) &&
        (rm->dtmf_enabled)) {
        PAL_DBG(LOG_TAG, "before deregisterMixerEventCallback");
        status = rm->registerMixerEventCallback(pcmDevRxIds,
            sessionCb, cbCookie, false);
        if (status != 0) {
            PAL_ERR(LOG_TAG, "Failed to deregister callback to rm for RX");
        }
        status = rm->registerMixerEventCallback(pcmDevTxIds,
            sessionCb, cbCookie, false);
        if (status != 0) {
            PAL_ERR(LOG_TAG, "Failed to deregister callback to rm for TX");
        }
        PAL_DBG(LOG_TAG, "after deregisterMixerEventCallback for DTMF RX/TX");
        status = 0;
    }

    status = SessionAlsaUtils::close(s, rm, pcmDevRxIds, pcmDevTxIds,
             rxAifBackEnds, txAifBackEnds);

    if (pcmRx) {
        status = pcm_close(pcmRx);
        if (status) {
            PAL_ERR(LOG_TAG, "pcm_close - rx failed %d", status);
        }
    }
    rm->freeFrontEndIds(pcmDevRxIds, sAttr, 0);
    if (pcmTx) {
        status = pcm_close(pcmTx);
        if (status) {
            PAL_ERR(LOG_TAG, "pcm_close - tx failed %d", status);
        }
    }
    rm->freeFrontEndIds(pcmDevTxIds, sAttr, 1);
    pcmRx = NULL;
    pcmTx = NULL;

    mState = SESSION_IDLE;
    return status;
}

int SessionAlsaVoice::setParameters(Stream *s, int tagId, uint32_t param_id , void *payload)
{
    int status = 0;
    int device = pcmDevRxIds.at(0);
    uint8_t* paramData = NULL;
    size_t paramSize = 0;
    uint32_t miid = 0;
    pal_param_module_enable_t* dtmf_detect_payload;

    uint32_t tty_mode;
    int mute_dir = RXDIR;
    int mute_tag = DEVICE_UNMUTE;
    pal_param_payload *PalPayload = (pal_param_payload *)payload;

    switch (static_cast<uint32_t>(tagId)) {

        case VOICE_VOLUME_BOOST:
            device = pcmDevRxIds.at(0);
            volume_boost = *((bool *)PalPayload->payload);
            status = payloadCalKeys(s, &paramData, &paramSize);
            if (!paramData) {
                status = -ENOMEM;
                PAL_ERR(LOG_TAG, "failed to get payload status %d", status);
                goto exit;
            }
            status = setVoiceMixerParameter(s, mixer, paramData, paramSize,
                                            RXDIR);
            if (status) {
                PAL_ERR(LOG_TAG, "Failed to set voice params status = %d",
                        status);
            }
            break;

        case VOICE_SLOW_TALK_OFF:
        case VOICE_SLOW_TALK_ON:
            device = pcmDevRxIds.at(0);
            slow_talk = *((bool *)PalPayload->payload);
            status = payloadTaged(s, MODULE, tagId, device, RXDIR);
            if (status) {
                PAL_ERR(LOG_TAG, "Failed to set voice slow_Talk params status = %d",
                        status);
            }
            break;

        case MODULE_ENABLE:
        case MODULE_DISABLE:
            PAL_ERR(LOG_TAG, "Enter MODULE_ENABLE/Disable");
            dtmf_detect_payload = (pal_param_module_enable_t*) payload;
            device = pcmDevRxIds.at(0);
            enable = dtmf_detect_payload->enable;
            dir = dtmf_detect_payload->dir;
            PAL_ERR(LOG_TAG, "Dtmf detect params, Enable= %d, Dir = %d",
                        enable, dir);
            status = registerDtmfEvent(tagId, dir);
            if (status != 0) {
                PAL_ERR(LOG_TAG,"registerDtmfEvent failed");
            }
            status = payloadTaged(s, MODULE, tagId, device, dir);
            if (status) {
                PAL_ERR(LOG_TAG, "Failed to set Dtmf detect params status = %d",
                        status);
            }
            PAL_ERR(LOG_TAG, "Exit MODULE_ENABLE, Disable case");
            break;

        case TTY_MODE:
            tty_mode = *((uint32_t *)PalPayload->payload);
            device = pcmDevRxIds.at(0);
            status = payloadSetTTYMode(&paramData, &paramSize,
                                       tty_mode);
            status = setVoiceMixerParameter(s, mixer, paramData, paramSize,
                                            RXDIR);
            if (status) {
                PAL_ERR(LOG_TAG, "Failed to set voice tty params status = %d",
                        status);
                break;
            }

            if (!paramData) {
                status = -ENOMEM;
                PAL_ERR(LOG_TAG, "failed to get tty payload status %d", status);
                goto exit;
            }
            break;

        case DTMF_GEN:
            if (param_id == PAL_PARAM_ID_DTMF_GEN_WITH_PARAM)
            {
                pal_param_dtmf_gen_tone_cfg_t *dtmf_payload = (pal_param_dtmf_gen_tone_cfg_t *)payload;
                std::vector<std::shared_ptr<Device>> associatedDevices;

                status = s->getAssociatedDevices(associatedDevices);
                if (0 != status) {
                    PAL_ERR(LOG_TAG,"getAssociatedDevices Failed \n");
                    goto exit;
                }

                rm->getBackEndNames(associatedDevices, rxAifBackEnds, txAifBackEnds);

                if (rxAifBackEnds.empty()) {
                    status = -EINVAL;
                    PAL_ERR(LOG_TAG, "no TX backend specified for this stream\n");
                    goto exit;
                }
                status = SessionAlsaUtils::getModuleInstanceId(mixer, pcmDevRxIds.at(0),
                                   rxAifBackEnds[0].second.data(), DTMF_GENERATOR, &miid);
                builder->payloadDTMFGenConfig(&paramData, &paramSize, miid, dtmf_payload);
                if (paramSize) {
                    status = SessionAlsaUtils::setMixerParameter(mixer, pcmDevRxIds.at(0),
                                                    paramData, paramSize);
                    if (status != 0) {
                        PAL_ERR(LOG_TAG,"setMixerParameter failed");
                        return status;
                    }
                } else {
                    PAL_ERR(LOG_TAG,"payloadDTMFGenConfig failed");
                }
            } else {
                device = pcmDevRxIds.at(0);
                status = payloadDtmfGenTaged(s, tagId, payload, RXDIR);
                if (status) {
                    PAL_ERR(LOG_TAG, "Failed to get dtmf gen params status = %d",
                            status);
                    goto exit;
                }
            }
            break;
        case DEVICE_MUTE:
          dev_mute = *((pal_device_mute_t *)PalPayload->payload);
          if (dev_mute.dir == PAL_AUDIO_INPUT) {
              mute_dir = TXDIR;
          }
          if (dev_mute.mute == 1) {
              mute_tag = DEVICE_MUTE;
          }
          PAL_DBG(LOG_TAG, "setting device mute dir %d mute flag %d", mute_dir, mute_tag);
          status = payloadTaged(s, MODULE, mute_tag, device, mute_dir);
          if (status) {
              PAL_ERR(LOG_TAG, "Failed to set device mute params status = %d",
                      status);
          }
          break;
       default:
            PAL_ERR(LOG_TAG,"Failed unsupported tag type %d \n",
                    static_cast<uint32_t>(tagId));
            status = -EINVAL;
            break;
    }

    if (0 != status) {
        PAL_ERR(LOG_TAG,"Failed to set config data");
        goto exit;
    }

    PAL_VERBOSE(LOG_TAG, "%pK - payload and %zu size", paramData , paramSize);

exit:
if (paramData) {
    free(paramData);
}
    PAL_DBG(LOG_TAG,"exit status:%d ", status);
    return status;

}

int SessionAlsaVoice::setConfig(Stream * s, configType type, int tag)
{
    int status = 0;
    int device = pcmDevRxIds.at(0);
    uint8_t* paramData = NULL;
    size_t paramSize = 0;

    switch (static_cast<uint32_t>(tag)) {
        case TAG_STREAM_VOLUME:
            device = pcmDevRxIds.at(0);
            status = payloadCalKeys(s, &paramData, &paramSize);
            status = SessionAlsaVoice::setVoiceMixerParameter(s, mixer,
                                                              paramData,
                                                              paramSize,
                                                              RXDIR);
            if (status) {
                PAL_ERR(LOG_TAG, "Failed to set voice params status = %d",
                        status);
            }
            if (!paramData) {
                status = -ENOMEM;
                PAL_ERR(LOG_TAG, "failed to get payload status %d", status);
                goto exit;
            }
            break;
        case MUTE_TAG:
        case UNMUTE_TAG:
            device = pcmDevTxIds.at(0);
            status = payloadTaged(s, type, tag, device, TXDIR);
            break;

        default:
            PAL_ERR(LOG_TAG,"Failed unsupported tag type %d", static_cast<uint32_t>(tag));
            status = -EINVAL;
            break;
    }
    if (0 != status) {
        PAL_ERR(LOG_TAG,"Failed to set config data");
        goto exit;
    }

    PAL_VERBOSE(LOG_TAG, "%pK - payload and %zu size", paramData , paramSize);

exit:
if (paramData) {
    free(paramData);
}
    PAL_DBG(LOG_TAG,"exit status:%d ", status);
    return status;
}

int SessionAlsaVoice::setConfig(Stream * s, configType type __unused, int tag, int dir)
{
    int status = 0;
    int device = pcmDevRxIds.at(0);
    uint8_t* paramData = NULL;
    size_t paramSize = 0;

    switch (static_cast<uint32_t>(tag)) {

       case TAG_STREAM_VOLUME:
            device = pcmDevRxIds.at(0);
            status = payloadCalKeys(s, &paramData, &paramSize);
            status = SessionAlsaVoice::setVoiceMixerParameter(s, mixer,
                                                              paramData,
                                                              paramSize,
                                                              dir);
            if (status) {
                PAL_ERR(LOG_TAG, "Failed to set voice params status = %d",
                        status);
            }
            if (!paramData) {
                status = -ENOMEM;
                PAL_ERR(LOG_TAG, "failed to get payload status %d", status);
                goto exit;
            }
            break;

        case MUTE_TAG:
        case UNMUTE_TAG:
            device = pcmDevTxIds.at(0);
            status = payloadTaged(s, type, tag, device, TXDIR);
            break;

        case VSID:
            device = pcmDevRxIds.at(0);
            status = payloadSetVSID(&paramData, &paramSize);
            status = SessionAlsaVoice::setVoiceMixerParameter(s, mixer,
                                                              paramData,
                                                              paramSize,
                                                              dir);
            if (status) {
                PAL_ERR(LOG_TAG, "Failed to set voice params status = %d",
                        status);
                break;
            }

            if (!paramData) {
                status = -ENOMEM;
                PAL_ERR(LOG_TAG, "failed to get payload status %d", status);
                goto exit;
            }

            break;

        default:
            PAL_ERR(LOG_TAG,"Failed unsupported tag type %d", static_cast<uint32_t>(tag));
            status = -EINVAL;
            break;
    }
    if (0 != status) {
        PAL_ERR(LOG_TAG,"Failed to set config data\n");
        goto exit;
    }

    PAL_VERBOSE(LOG_TAG, "%x - payload and %zu size", *paramData , paramSize);

exit:
if (paramData) {
    free(paramData);
}
    PAL_DBG(LOG_TAG,"exit status:%d ", status);
    return status;
}

int SessionAlsaVoice::setDtmfGenTKV(Stream * s, std::vector <std::pair<int,int>> &tkv, int index, int size, uint32_t* gsltag)
{
    int status = 0;
    int i = 0;

    PAL_ERR(LOG_TAG,"enter, index: %d", index);

    const Key_DTMF_GEN taglist[] = {DTMF_GEN_1, DTMF_GEN_2, DTMF_GEN_3,
                                   DTMF_GEN_4, DTMF_GEN_5, DTMF_GEN_6,
                                   DTMF_GEN_7, DTMF_GEN_8, DTMF_GEN_9,
                                   DTMF_GEN_10, DTMF_GEN_11, DTMF_GEN_12,
                                   DTMF_GEN_13, DTMF_GEN_14, DTMF_GEN_15,
                                   DTMF_GEN_16};

    std::vector<Key_DTMF_GEN> dtmfGenTagList(taglist, taglist+size);
    tkv.push_back(std::make_pair(TAG_KEY_DTMF_GEN_TONE, dtmfGenTagList[index]));
    *gsltag = DTMF_GENERATOR;

    PAL_ERR(LOG_TAG, "exit, status: %d", status);
    return status;
}

int SessionAlsaVoice::populateFreqPair() {
    int size_lFreq = 0;
    int size_hFreq = 0;
    int totalSize = 0;
    int highFreq[] = {1209, 1336, 1477, 1633};
    int lowFreq[] = {697, 770, 852, 941};

    size_hFreq = sizeof(highFreq)/sizeof(highFreq[0]);
    size_lFreq = sizeof(lowFreq)/sizeof(lowFreq[0]);
    totalSize = size_hFreq * size_lFreq;

    for (int i=0; i<size_hFreq; i++){
        for (int j=0;j<size_lFreq;j++) {
            freqPair.push_back(std::make_pair(highFreq[i],lowFreq[j]));
        }
    }

    return totalSize;
}

int SessionAlsaVoice::payloadDtmfGenTaged(Stream *s, int tag, void *pData, int dir){
    int status = 0;
    int totalSize = 0;
    int index = 0;
    int tkv_size = 0;
    uint32_t tagsent;
    struct mixer_ctl *ctl;
    struct agm_tag_config* tagConfig;
    const char *setParamTagControl = "setParamTag";
    std::ostringstream tagCntrlName;
    const char *stream = SessionAlsaVoice::getMixerVoiceStream(s, dir);

    pal_param_dtmf_gen_tone_cfg_t* dtmf_gen_payload =
                                (pal_param_dtmf_gen_tone_cfg_t*) pData;
    totalSize = populateFreqPair();
    for (int i=0; i<totalSize; i++) {
        if (freqPair[i].first == dtmf_gen_payload->high_freq) {
            if (freqPair[i].second == dtmf_gen_payload->low_freq) {
                index = i+1;
                PAL_ERR(LOG_TAG, "found freq at index: %d", index);
                break;
            }
        }
    }
    status = setDtmfGenTKV(s, tkv, index-1, totalSize, &tagsent);
    if (0 != status) {
        PAL_ERR(LOG_TAG,"Failed to set the tkv for index: %d \n", index);
    }

    if (tkv.size() == 0) {
        status = -EINVAL;
        PAL_ERR(LOG_TAG,"invalid tkv size\n");
        goto done;
    }
    tagConfig = (struct agm_tag_config*)malloc (sizeof(struct agm_tag_config) +
                 (tkv.size() * sizeof(agm_key_value)));
    if(!tagConfig) {
        status = -EINVAL;
        PAL_ERR(LOG_TAG,"invalid tagConfig\n");
        goto done;
    }
    status = SessionAlsaUtils::getTagMetadata(tagsent, tkv, tagConfig);
    if (0 != status) {
        PAL_ERR(LOG_TAG,"getTagMetadata failed\n");
        goto done;
    }
    tagCntrlName<<stream<<" "<<setParamTagControl;
    ctl = mixer_get_ctl_by_name(mixer, tagCntrlName.str().data());
    if (!ctl) {
        PAL_ERR(LOG_TAG, "Invalid mixer control: %s\n", tagCntrlName.str().data());
        return -ENOENT;
    }

    tkv_size = tkv.size()*sizeof(struct agm_key_value);
    status = mixer_ctl_set_array(ctl, tagConfig, sizeof(struct agm_tag_config) + tkv_size);
    if (status != 0) {
         PAL_ERR(LOG_TAG,"failed to set the tag calibration %d", status);
         goto done;
    }
    ctl = NULL;
    tkv.clear();
    if (tagConfig) {
        free(tagConfig);
    }

done:
    PAL_ERR(LOG_TAG, "Exit");
    return status;
}

int SessionAlsaVoice::registerDtmfEvent(int tagId, int dir) {
    int status = 0;
    int payload_size = 0;
    struct agm_event_reg_cfg *event_cfg;

    PAL_DBG(LOG_TAG, "Enter");

    payload_size = sizeof(struct agm_event_reg_cfg);
    event_cfg = (struct agm_event_reg_cfg *)calloc(1, payload_size);
    if (!event_cfg) {
        PAL_ERR(LOG_TAG, "Failed to allocate memory for event_cfg");
        status = -ENOMEM;
    } else {
        event_cfg->event_id = EVENT_ID_DTMF_DETECTION;
        event_cfg->event_config_payload_size = 0;

        if (tagId == MODULE_ENABLE) {
            PAL_ERR(LOG_TAG, "Enter with tagID:%d", tagId);
            event_cfg->is_register = 1;
        } else {
            PAL_ERR(LOG_TAG, "Enter with tagID:%d", tagId);
            event_cfg->is_register = 0;
        }

        if (dir == TXDIR) {
            status = SessionAlsaUtils::registerMixerEvent(mixer, pcmDevTxIds.at(0),
            txAifBackEnds[0].second.data(), DTMF_DETECTOR, (void *)event_cfg,
            payload_size);
        } else {
            status = SessionAlsaUtils::registerMixerEvent(mixer, pcmDevRxIds.at(0),
            rxAifBackEnds[0].second.data(), DTMF_DETECTOR, (void *)event_cfg,
            payload_size);
        }
        if (status != 0) {
            PAL_ERR(LOG_TAG,"registerMixerEvent failed");
        }
    }
    PAL_DBG(LOG_TAG, "Exit");
    return status;
}

int SessionAlsaVoice::payloadTaged(Stream * s, configType type, int tag,
                                   int device __unused, int dir){
    int status = 0;
    uint32_t tagsent;
    struct agm_tag_config* tagConfig;
    const char *setParamTagControl = "setParamTag";
    struct mixer_ctl *ctl;
    std::ostringstream tagCntrlName;
    int tkv_size = 0;
    const char *stream = SessionAlsaVoice::getMixerVoiceStream(s, dir);
    switch (type) {
        case MODULE:
            tkv.clear();
            status = builder->populateTagKeyVector(s, tkv, tag, &tagsent);
            if (0 != status) {
                PAL_ERR(LOG_TAG,"Failed to set the tag configuration\n");
                goto exit;
            }

            if (tkv.size() == 0) {
                status = -EINVAL;
                goto exit;
            }

            tagConfig = (struct agm_tag_config*)malloc (sizeof(struct agm_tag_config) +
                            (tkv.size() * sizeof(agm_key_value)));

            if(!tagConfig) {
                status = -EINVAL;
                goto exit;
            }

            status = SessionAlsaUtils::getTagMetadata(tagsent, tkv, tagConfig);
            if (0 != status) {
                goto exit;
            }
            tagCntrlName<<stream<<" "<<setParamTagControl;
            ctl = mixer_get_ctl_by_name(mixer, tagCntrlName.str().data());
            if (!ctl) {
                PAL_ERR(LOG_TAG, "Invalid mixer control: %s\n", tagCntrlName.str().data());
                return -ENOENT;
            }
            PAL_DBG(LOG_TAG, " mixer control: %s\n", tagCntrlName.str().data());

            tkv_size = tkv.size()*sizeof(struct agm_key_value);
            status = mixer_ctl_set_array(ctl, tagConfig, sizeof(struct agm_tag_config) + tkv_size);
            if (status != 0) {
                PAL_ERR(LOG_TAG,"failed to set the tag calibration %d", status);
                goto exit;
            }
            ctl = NULL;
            tkv.clear();
            if (tagConfig) {
                free(tagConfig);
            }
            break;
        default:
            PAL_ERR(LOG_TAG,"invalid type ");
            status = -EINVAL;
    }

exit:
    PAL_DBG(LOG_TAG,"exit status:%d ", status);
    return status;
}

int SessionAlsaVoice::payloadSetVSID(uint8_t **payload, size_t *size){
    int status = 0;
    apm_module_param_data_t* header;
    uint8_t* payloadInfo = NULL;
    size_t payloadSize = 0, padBytes = 0;
    uint8_t *vsid_pl;
    vcpm_param_vsid_payload_t vsid_payload;

    payloadSize = sizeof(struct apm_module_param_data_t)+
                  sizeof(vcpm_param_vsid_payload_t);
    padBytes = PAL_PADDING_8BYTE_ALIGN(payloadSize);

    payloadInfo = new uint8_t[payloadSize + padBytes]();
    if (!payloadInfo) {
        PAL_ERR(LOG_TAG, "payloadInfo malloc failed %s", strerror(errno));
        return -EINVAL;
    }
    header = (apm_module_param_data_t*)payloadInfo;
    header->module_instance_id = VCPM_MODULE_INSTANCE_ID;
    header->param_id = VCPM_PARAM_ID_VSID;
    header->error_code = 0x0;
    header->param_size = payloadSize - sizeof(struct apm_module_param_data_t);

    vsid_payload.vsid = vsid;
    vsid_pl = (uint8_t*)payloadInfo + sizeof(apm_module_param_data_t);
    ar_mem_cpy(vsid_pl,  sizeof(vcpm_param_vsid_payload_t),
                     &vsid_payload,  sizeof(vcpm_param_vsid_payload_t));

    *size = payloadSize + padBytes;
    *payload = payloadInfo;


    return status;
}

int SessionAlsaVoice::payloadCalKeys(Stream * s, uint8_t **payload, size_t *size)
{
    int status = 0;
    apm_module_param_data_t* header;
    uint8_t* payloadInfo = NULL;
    size_t payloadSize = 0, padBytes = 0;
    uint8_t *vol_pl;
    vcpm_param_cal_keys_payload_t cal_keys;
    vcpm_ckv_pair_t cal_key_pair[NUM_OF_CAL_KEYS];
    float volume = 0.0;
    int vol;
    struct pal_volume_data *voldata = NULL;

    voldata = (struct pal_volume_data *)calloc(1, (sizeof(uint32_t) +
                      (sizeof(struct pal_channel_vol_kv) * (0xFFFF))));
    if (!voldata) {
        status = -ENOMEM;
        goto exit;
    }
    status = s->getVolumeData(voldata);
    if(0 != status) {
        PAL_ERR(LOG_TAG,"getVolumeData Failed");
        goto exit;
    }

    PAL_VERBOSE(LOG_TAG,"volume sent:%f", (voldata->volume_pair[0].vol));
    volume = (voldata->volume_pair[0].vol);

    payloadSize = sizeof(apm_module_param_data_t) +
                  sizeof(vcpm_param_cal_keys_payload_t) +
                  sizeof(vcpm_ckv_pair_t)*NUM_OF_CAL_KEYS;
    padBytes = PAL_PADDING_8BYTE_ALIGN(payloadSize);

    payloadInfo = new uint8_t[payloadSize + padBytes]();
    if (!payloadInfo) {
        PAL_ERR(LOG_TAG, "payloadInfo malloc failed %s", strerror(errno));
        return -EINVAL;
    }
    header = (apm_module_param_data_t*)payloadInfo;
    header->module_instance_id = VCPM_MODULE_INSTANCE_ID;
    header->param_id = VCPM_PARAM_ID_CAL_KEYS;
    header->error_code = 0x0;
    header->param_size = payloadSize - sizeof(struct apm_module_param_data_t);
    cal_keys.vsid = vsid;
    cal_keys.num_ckv_pairs = 2;
    if (volume < 0.0) {
            volume = 0.0;
    } else if (volume > 1.0) {
        volume = 1.0;
    }

    vol = lrint(volume * 100.0);

    // Voice volume levels from android are mapped to driver volume levels as follows.
    // 0 -> 5, 20 -> 4, 40 ->3, 60 -> 2, 80 -> 1, 100 -> 0
    // So adjust the volume to get the correct volume index in driver
    vol = 100 - vol;

    /*volume key*/
    cal_key_pair[0].cal_key_id = VCPM_CAL_KEY_ID_VOLUME_LEVEL;
    cal_key_pair[0].value = percent_to_index(vol, MIN_VOL_INDEX, MAX_VOL_INDEX);

    /*cal key for volume boost*/
    cal_key_pair[1].cal_key_id = VCPM_CAL_KEY_ID_VOL_BOOST;
    cal_key_pair[1].value = volume_boost;

    vol_pl = (uint8_t*)payloadInfo + sizeof(apm_module_param_data_t);
    ar_mem_cpy(vol_pl, sizeof(vcpm_param_cal_keys_payload_t),
                     &cal_keys, sizeof(vcpm_param_cal_keys_payload_t));

    vol_pl += sizeof(vcpm_param_cal_keys_payload_t);
    ar_mem_cpy(vol_pl, sizeof(vcpm_ckv_pair_t)*NUM_OF_CAL_KEYS,
                     &cal_key_pair, sizeof(vcpm_ckv_pair_t)*NUM_OF_CAL_KEYS);


    *size = payloadSize + padBytes;
    *payload = payloadInfo;
    PAL_VERBOSE(LOG_TAG, "payload %pK size %zu", *payload, *size);

exit:
    if (voldata) {
        free(voldata);
    }
    return status;
}

int SessionAlsaVoice::payloadSetTTYMode(uint8_t **payload, size_t *size, uint32_t mode){
    int status = 0;
    apm_module_param_data_t* header;
    uint8_t* payloadInfo = NULL;
    size_t payloadSize = 0, padBytes = 0;
    uint8_t *phrase_pl;
    vcpm_param_id_tty_mode_t tty_payload;

    payloadSize = sizeof(struct apm_module_param_data_t)+
                  sizeof(tty_payload);
    padBytes = PAL_PADDING_8BYTE_ALIGN(payloadSize);

    payloadInfo = new uint8_t[payloadSize + padBytes]();
    if (!payloadInfo) {
        PAL_ERR(LOG_TAG, "payloadInfo malloc failed %s", strerror(errno));
        return -EINVAL;
    }
    header = (apm_module_param_data_t*)payloadInfo;
    header->module_instance_id = VCPM_MODULE_INSTANCE_ID;
    header->param_id = VCPM_PARAM_ID_TTY_MODE;
    header->error_code = 0x0;
    header->param_size = payloadSize - sizeof(struct apm_module_param_data_t);

    tty_payload.vsid = vsid;
    tty_payload.mode = mode;
    phrase_pl = (uint8_t*)payloadInfo + sizeof(apm_module_param_data_t);
    ar_mem_cpy(phrase_pl,  sizeof(vcpm_param_id_tty_mode_t),
                     &tty_payload,  sizeof(vcpm_param_id_tty_mode_t));

    *size = payloadSize + padBytes;
    *payload = payloadInfo;
    return status;
}

int SessionAlsaVoice::setSidetone(int deviceId,Stream * s, bool enable){
    int status = 0;
    sidetone_mode_t mode;

    status = rm->getSidetoneMode((pal_device_id_t)deviceId, PAL_STREAM_VOICE_CALL, &mode);
    if(status) {
            PAL_ERR(LOG_TAG, "get sidetone mode failed");
    }
    if (mode == SIDETONE_HW) {
        PAL_DBG(LOG_TAG, "HW sidetone mode being set");
        if (enable) {
            status = setHWSidetone(s,1);
        } else {
            status = setHWSidetone(s,0);
        }
    }
    /*if SW mode it will be set via kv in graph open*/
    return status;
}

int SessionAlsaVoice::setHWSidetone(Stream * s, bool enable){
    int status = 0;
    std::vector<std::shared_ptr<Device>> associatedDevices;
    struct audio_route *audioRoute;
    bool set = false;

    status = s->getAssociatedDevices(associatedDevices);
    status = rm->getAudioRoute(&audioRoute);

    status = s->getAssociatedDevices(associatedDevices);
    for(int i =0; i < associatedDevices.size(); i++) {
        switch(associatedDevices[i]->getSndDeviceId()){
            case PAL_DEVICE_IN_HANDSET_MIC:
                if(enable)
                    audio_route_apply_and_update_path(audioRoute, "sidetone-handset");
                else
                    audio_route_reset_and_update_path(audioRoute, "sidetone-handset");
                set = true;
                break;
            case PAL_DEVICE_IN_WIRED_HEADSET:
                if(enable)
                    audio_route_apply_and_update_path(audioRoute, "sidetone-headphones");
                else
                    audio_route_reset_and_update_path(audioRoute, "sidetone-headphones");
                set = true;
                break;
            default:
                PAL_DBG(LOG_TAG,"codec sidetone not supported on device %d",associatedDevices[i]->getSndDeviceId());
                break;

        }
        if(set)
            break;
    }
    return status;
}

int SessionAlsaVoice::disconnectSessionDevice(Stream *streamHandle,
                                              pal_stream_type_t streamType,
                                              std::shared_ptr<Device> deviceToDisconnect)
{
    std::vector<std::shared_ptr<Device>> deviceList;
    std::vector<std::string> aifBackEndsToDisconnect;
    struct pal_device dAttr;
    int status = 0;
    int txDevId = PAL_DEVICE_NONE;

    deviceList.push_back(deviceToDisconnect);
    rm->getBackEndNames(deviceList, rxAifBackEnds,txAifBackEnds);

    deviceToDisconnect->getDeviceAttributes(&dAttr);

    if (rxAifBackEnds.size() > 0) {
        status =  SessionAlsaUtils::disconnectSessionDevice(streamHandle,
                                                            streamType, rm,
                                                            dAttr, pcmDevRxIds,
                                                            rxAifBackEnds);
        if(0 != status) {
            PAL_ERR(LOG_TAG,"disconnectSessionDevice on RX Failed \n");
            return status;
        }
    } else if (txAifBackEnds.size() > 0) {
        /*if HW sidetone is enable disable it */
        status = getTXDeviceId(streamHandle, &txDevId);
        if (status){
            PAL_ERR(LOG_TAG, "could not find TX device associated with this stream cannot set sidetone");
        } else {
            status = setSidetone(txDevId,streamHandle,0);
            if(0 != status) {
                PAL_ERR(LOG_TAG,"disabling sidetone failed");
            }
        }
        status =  SessionAlsaUtils::disconnectSessionDevice(streamHandle,
                                                            streamType, rm,
                                                            dAttr, pcmDevTxIds,
                                                            txAifBackEnds);
        if(0 != status) {
            PAL_ERR(LOG_TAG,"disconnectSessionDevice on TX Failed");
        }
    }

    return status;
}

int SessionAlsaVoice::setupSessionDevice(Stream* streamHandle,
                                 pal_stream_type_t streamType,
                                 std::shared_ptr<Device> deviceToConnect)
{
    std::vector<std::shared_ptr<Device>> deviceList;
    std::vector<std::string> aifBackEndsToConnect;
    struct pal_device dAttr;
    int status = 0;
    int txDevId = PAL_DEVICE_NONE;

    deviceList.push_back(deviceToConnect);
    rm->getBackEndNames(deviceList, rxAifBackEnds, txAifBackEnds);
    deviceToConnect->getDeviceAttributes(&dAttr);

    if (rxAifBackEnds.size() > 0) {
        status =  SessionAlsaUtils::setupSessionDevice(streamHandle, streamType,
                                                       rm, dAttr, pcmDevRxIds,
                                                       rxAifBackEnds);
        if(0 != status) {
            PAL_ERR(LOG_TAG,"setupSessionDevice on RX Failed");
            return status;
        }
    } else if (txAifBackEnds.size() > 0) {
        /*set sidetone on new tx device*/
        if (deviceToConnect->getSndDeviceId() > PAL_DEVICE_IN_MIN &&
            deviceToConnect->getSndDeviceId() < PAL_DEVICE_IN_MAX) {
            txDevId = deviceToConnect->getSndDeviceId();
        }
        if(txDevId != PAL_DEVICE_NONE)
        {
            status = setSidetone(txDevId,streamHandle,1);
        }
        if(0 != status) {
            PAL_ERR(LOG_TAG,"enabling sidetone failed");
        }
        status =  SessionAlsaUtils::setupSessionDevice(streamHandle, streamType,
                                                       rm, dAttr, pcmDevTxIds,
                                                       txAifBackEnds);
        if(0 != status) {
            PAL_ERR(LOG_TAG,"setupSessionDevice on TX Failed");
        }
    }
    return status;
}

int SessionAlsaVoice::connectSessionDevice(Stream* streamHandle,
                                           pal_stream_type_t streamType,
                                           std::shared_ptr<Device> deviceToConnect)
{
    std::vector<std::shared_ptr<Device>> deviceList;
    std::vector<std::string> aifBackEndsToConnect;
    struct pal_device dAttr;
    int status = 0;

    deviceList.push_back(deviceToConnect);
    rm->getBackEndNames(deviceList, rxAifBackEnds, txAifBackEnds);
    deviceToConnect->getDeviceAttributes(&dAttr);

    if (rxAifBackEnds.size() > 0) {
        status =  SessionAlsaUtils::connectSessionDevice(this, streamHandle,
                                                         streamType, rm,
                                                         dAttr, pcmDevRxIds,
                                                         rxAifBackEnds);
        if(0 != status) {
            PAL_ERR(LOG_TAG,"connectSessionDevice on RX Failed");
            return status;
        }
    } else if (txAifBackEnds.size() > 0) {

        status =  SessionAlsaUtils::connectSessionDevice(this, streamHandle,
                                                         streamType, rm,
                                                         dAttr, pcmDevTxIds,
                                                         txAifBackEnds);
        if(0 != status) {
            PAL_ERR(LOG_TAG,"connectSessionDevice on TX Failed");
        }
    }
    return status;
}

int SessionAlsaVoice::setVoiceMixerParameter(Stream * s, struct mixer *mixer,
                                             void *payload, int size, int dir)
{
    char *control = (char*)"setParam";
    char *mixer_str;
    struct mixer_ctl *ctl;
    int ctl_len = 0,ret = 0;
    struct pal_stream_attributes sAttr;
    char *stream = SessionAlsaVoice::getMixerVoiceStream(s, dir);

    ret = s->getStreamAttributes(&sAttr);

    if (ret) {
         PAL_ERR(LOG_TAG, "could not get stream attributes\n");
        return ret;
    }

    ctl_len = strlen(stream) + 4 + strlen(control) + 1;
    mixer_str = (char *)calloc(1, ctl_len);
    if (!mixer_str) {
        free(payload);
        return -ENOMEM;
    }
    snprintf(mixer_str, ctl_len, "%s %s", stream, control);

    PAL_VERBOSE(LOG_TAG, "- mixer -%s-\n", mixer_str);
    ctl = mixer_get_ctl_by_name(mixer, mixer_str);
    if (!ctl) {
        PAL_ERR(LOG_TAG, "Invalid mixer control: %s\n", mixer_str);
        free(mixer_str);
        return ENOENT;
    }


    ret = mixer_ctl_set_array(ctl, payload, size);

    PAL_VERBOSE(LOG_TAG, "ret = %d, cnt = %d\n", ret, size);
    free(mixer_str);
    return ret;
}

char* SessionAlsaVoice::getMixerVoiceStream(Stream *s, int dir){
    char *stream = (char*)"VOICEMMODE1p";
    struct pal_stream_attributes sAttr;

    s->getStreamAttributes(&sAttr);
    if (sAttr.info.voice_call_info.VSID == VOICEMMODE1 ||
        sAttr.info.voice_call_info.VSID == VOICELBMMODE1) {
        if (dir == TXDIR) {
            stream = (char*)"VOICEMMODE1c";
        } else {
            stream = (char*)"VOICEMMODE1p";
        }
    } else {
        if (dir == TXDIR) {
            stream = (char*)"VOICEMMODE2c";
        } else {
            stream = (char*)"VOICEMMODE2p";
        }
    }

    return stream;
}

int SessionAlsaVoice::setECRef(Stream *s __unused, std::shared_ptr<Device> rx_dev __unused, bool is_enable __unused)
{
    return 0;
}

int SessionAlsaVoice::registerCallBack(session_callback cb, uint64_t cookie)
{
    sessionCb = cb;
    cbCookie = cookie;
    return 0;
}

int SessionAlsaVoice::getTXDeviceId(Stream *s, int *id)
{
    int status = 0;
    int i;
    std::vector<std::shared_ptr<Device>> associatedDevices;
    *id = PAL_DEVICE_NONE;

    status = s->getAssociatedDevices(associatedDevices);
    if(0 != status) {
        PAL_ERR(LOG_TAG,"getAssociatedDevices Failed");
        return status;
    }

    for (i =0; i < associatedDevices.size(); i++) {
        if (associatedDevices[i]->getSndDeviceId() > PAL_DEVICE_IN_MIN &&
            associatedDevices[i]->getSndDeviceId() < PAL_DEVICE_IN_MAX) {
            *id = associatedDevices[i]->getSndDeviceId();
            break;
        }
    }
    if(i >= PAL_DEVICE_IN_MAX){
        status = -EINVAL;
    }
    return status;
}

