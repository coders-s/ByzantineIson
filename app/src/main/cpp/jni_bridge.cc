/*
 * Copyright 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <jni.h>
#include <SLES/OpenSLES.h>
#include <cassert>
#include "audio_player.h"
#include "synthesizer.h"
#include "load_stabilizer.h"
#include "android_log.h"

// OpenSL ES interfaces
static SLObjectItf sl_engine_object_itf = nullptr;
static SLEngineItf sl_engine_engine_itf = nullptr;
static SLObjectItf sl_output_mix_object_itf = nullptr;

static LoadStabilizer *load_stabilizer;
static Synthesizer *synth;
static AudioPlayer *player;

static int api_level;

#define NUM_AUDIO_CHANNELS 2 // 1 = mono, 2 = stereo

extern "C" {

// create the engine and output mix objects
JNIEXPORT void JNICALL Java_com_coderss_ison_utility_Player_native_1createEngine(
        JNIEnv *env,
        jclass clazz,
        jint j_api_level) {

    LOGV("Creating audio engine");

    api_level = (int) j_api_level;

    // create the OpenSL ES engine and output mix objects
    SLresult result;

    result = slCreateEngine(&sl_engine_object_itf,
                            0, /* numOptions */
                            nullptr, /* pEngineOptions */
                            0, /* numInterfaces */
                            nullptr, /* pInterfaceIds */
                            nullptr /* pInterfaceRequired */);
    SLASSERT(result);

    // realize the engine
    result = (*sl_engine_object_itf)->Realize(sl_engine_object_itf, SL_BOOLEAN_FALSE /* async */);
    SLASSERT(result);

    // get the engine interface, which is needed to create other objects
    result = (*sl_engine_object_itf)->GetInterface(sl_engine_object_itf,
                                                   SL_IID_ENGINE,
                                                   &sl_engine_engine_itf);
    SLASSERT(result);
    // create the output mix
    result = (*sl_engine_engine_itf)->CreateOutputMix(sl_engine_engine_itf,
                                                      &sl_output_mix_object_itf,
                                                      0,
                                                      nullptr,
                                                      nullptr);
    SLASSERT(result);

    // realize the output mix
    result = (*sl_output_mix_object_itf)->Realize(sl_output_mix_object_itf,
                                                  SL_BOOLEAN_FALSE /* async */);
    SLASSERT(result);
}

JNIEXPORT void JNICALL Java_com_coderss_ison_utility_Player_native_1destroyEngine(
        JNIEnv *env,
        jclass type) {

    if (sl_output_mix_object_itf != nullptr) {
        (*sl_output_mix_object_itf)->Destroy(sl_output_mix_object_itf);
        sl_output_mix_object_itf = nullptr;
    }
    if (sl_engine_object_itf != nullptr) {
        (*sl_engine_object_itf)->Destroy(sl_engine_object_itf);
        sl_engine_object_itf = nullptr;
    }
}

JNIEXPORT jobject JNICALL Java_com_coderss_ison_utility_Player_native_1createAudioPlayer(
        JNIEnv *env,
        jclass clazz,
        jint j_frame_rate,
        jint j_frames_per_buffer,
        jint j_num_buffers) {

    AudioStreamFormat format;
    format.frame_rate = (uint32_t) j_frame_rate;
    format.frames_per_buffer = (uint32_t) j_frames_per_buffer;
    format.num_audio_channels = NUM_AUDIO_CHANNELS;
    format.num_buffers = (uint16_t) j_num_buffers;

    synth = new Synthesizer(format.num_audio_channels, format.frame_rate);

    int64_t callback_period_ns =
            ((int64_t) format.frames_per_buffer * NANOS_IN_SECOND) / format.frame_rate;
    load_stabilizer = new LoadStabilizer(synth, callback_period_ns);

    player = new AudioPlayer(sl_engine_engine_itf,
                             sl_output_mix_object_itf,
                             load_stabilizer,
                             format,
                             api_level);

    Trace::initialize();

    player->play();
    return player->getAudioTrack();
}

JNIEXPORT void JNICALL Java_com_coderss_ison_utility_Player_native_1destroyAudioPlayer(
        JNIEnv *env,
        jclass type) {

    LOGV("Destroy checkpoint A");
    player->stop();
    LOGV("Destroy checkpoint B");
    delete player;
    LOGV("Destroy checkpoint C");
    delete load_stabilizer;
    LOGV("Destroy checkpoint D");
    delete synth;
    LOGV("Destroy checkpoint E");
}

JNIEXPORT void JNICALL Java_com_coderss_ison_utility_Player_native_1setFrequency(
        JNIEnv *env,
        jclass clazz,
        jfloat frequency) {
    synth->setFrequency(frequency);
}

JNIEXPORT void JNICALL Java_com_coderss_ison_utility_Player_native_1setVolume(
        JNIEnv *env,
        jclass clazz,
        jfloat volume) {
    synth->setVolume(volume);
}

JNIEXPORT void JNICALL Java_com_coderss_ison_utility_Player_native_1setPlayerPreferences(
        JNIEnv *env,
        jclass clazz,
        jfloat frequency_change_time,
        jfloat volume_change_time,
        jint blend_mode) {
    synth->setPreferences(frequency_change_time, volume_change_time, blend_mode);
}

JNIEXPORT void JNICALL Java_com_coderss_ison_utility_Player_native_1setSounds(
        JNIEnv *env,
        jclass clazz,
        jobjectArray sound_data_array,
        jfloatArray frequency_array) {
    synth->setSounds(env, sound_data_array, frequency_array);
}

JNIEXPORT void JNICALL Java_com_coderss_ison_utility_Player_native_1setWorkCycles(
        JNIEnv *env,
        jclass clazz,
        jint workCycles) {
    synth->setWorkCycles((int) workCycles);
}

JNIEXPORT void JNICALL Java_com_coderss_ison_utility_Player_native_1setLoadStabilizationEnabled(
        JNIEnv *env,
        jclass clazz,
        jboolean is_enabled) {
    load_stabilizer->setStabilizationEnabled((bool) is_enabled);
}

} // end extern "C"
