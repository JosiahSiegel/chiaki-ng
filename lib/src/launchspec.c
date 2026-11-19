// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <chiaki/launchspec.h>
#include <chiaki/session.h>
#include <chiaki/base64.h>

#include <stdio.h>

static const char launchspec_fmt[] =
	"{"
		"\"sessionId\":\"sessionId4321\","
		"\"streamResolutions\":["
			"{"
				"\"resolution\":"
				"{"
					"\"width\":%u," // 0
					"\"height\":%u" // 1
				"},"
				"\"maxFps\":%u," // 2
				"\"score\":10"
			"}"
		"],"
		"\"network\":{"
			"\"bwKbpsSent\":%u," // 3
			"\"bwLoss\":0.001000,"
			"\"mtu\":%u," // 4
			"\"rtt\":%u," // 5
			"\"ports\":[53,2053]"
		"},"
		"\"slotId\":1,"
		"\"appSpecification\":{"
			"\"minFps\":30,"
			"\"minBandwidth\":0,"
			"\"extTitleId\":\"ps3\","
			"\"version\":1,"
			"\"timeLimit\":1,"
			"\"startTimeout\":100,"
			"\"afkTimeout\":100,"
			"\"afkTimeoutDisconnect\":100"
		"},"
		"\"konan\":{"
			"\"ps3AccessToken\":\"accessToken\","
			"\"ps3RefreshToken\":\"refreshToken\""
		"},\"requestGameSpecification\":{"
			"\"model\":\"%s\"," // 6
			"\"platform\":\"%s\"," // 7
			"\"audioChannels\":\"5.1\","
			"\"language\":\"sp\","
			"\"acceptButton\":\"X\","
			"\"connectedControllers\":[\"xinput\",\"ds3\",\"ds4\"],"
			"\"yuvCoefficient\":\"bt601\","
			"\"videoEncoderProfile\":\"%s\"," // 8
			"\"audioEncoderProfile\":\"audio1\""
			"%s" // 9
			"%s" // 10
		"},"
		"\"userProfile\":{"
			"\"onlineId\":\"psnId\","
			"\"npId\":\"npId\","
			"\"region\":\"US\","
			"\"languagesUsed\":[\"en\",\"jp\"]"
		"},"
		"%s" // 11
		"%s" // 12
		"\"handshakeKey\":\"%s\"" // 13
	"}";

// Mirrors the videoStreamSettings block PS Plus cloud clients send when they ask for 1440p/2160p
// while identifying as a PlayStation Portal. Whether a PS5 reads it for Remote Play is unknown.
static const char video_stream_settings_fmt[] =
	",\"videoStreamSettings\":{"
		"\"clientHeight\":%u,"
		"\"supportedMaxResolution\":%u,"
		"\"supportedVideoEncoderProfiles\":[\"hevc_hw4\"],"
		"\"supportedDynamicRange\":\"%s\","
		"\"preferredMaxResolution\":%u,"
		"\"preferredDynamicRange\":\"%s\","
		"\"hqMode\":1"
	"}";

CHIAKI_EXPORT int chiaki_launchspec_format(char *buf, size_t buf_size, ChiakiLaunchSpec *launch_spec)
{
	char handshake_key_b64[CHIAKI_HANDSHAKE_KEY_SIZE * 2];
	ChiakiErrorCode err = chiaki_base64_encode(launch_spec->handshake_key, CHIAKI_HANDSHAKE_KEY_SIZE, handshake_key_b64, sizeof(handshake_key_b64));
	if(err != CHIAKI_ERR_SUCCESS)
		return -1;

	char *extras[3];
	if(chiaki_target_is_ps5(launch_spec->target)) // TODO: probably also for ps4, but only 12
	{
		extras[0] = ",\"adaptiveStreamMode\": \"resize\"";
		extras[1] = chiaki_codec_is_h265(launch_spec->codec)
			? "\"videoCodec\":\"hevc\","
			: "\"videoCodec\":\"avc\",";
		extras[2] = chiaki_codec_is_hdr(launch_spec->codec)
			? "\"dynamicRange\":\"HDR\","
			: "\"dynamicRange\":\"SDR\",";
	}
	else
		extras[0] = extras[1] = extras[2] = "";

	const char *video_encoder_profile = launch_spec->video_encoder_profile && launch_spec->video_encoder_profile[0]
		? launch_spec->video_encoder_profile
		: "hw4.1";

	const bool portal = launch_spec->portal_identity && chiaki_target_is_ps5(launch_spec->target);
	char video_stream_settings[320] = "";
	if(portal)
	{
		const char *range = chiaki_codec_is_hdr(launch_spec->codec) ? "hdr" : "sdr";
		int n = snprintf(video_stream_settings, sizeof(video_stream_settings), video_stream_settings_fmt,
				launch_spec->height, launch_spec->height, range, launch_spec->height, range);
		if(n < 0 || (size_t)n >= sizeof(video_stream_settings))
			return -1;
	}

	int written = snprintf(buf, buf_size, launchspec_fmt,
			launch_spec->width, launch_spec->height, launch_spec->max_fps,
			launch_spec->bw_kbps_sent, launch_spec->mtu, launch_spec->rtt,
			portal ? "portal" : "bravia_tv", portal ? "qlite" : "android",
			video_encoder_profile, extras[0], video_stream_settings,
			extras[1], extras[2], handshake_key_b64);
	if(written < 0 || written >= buf_size)
		return -1;
	return written;
}
