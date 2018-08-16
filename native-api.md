# API header files

As a user of the WebRTC library, you may use headers and build files
in the following directories:

API directory | Including subdirectories?
--------------|-------------------------
`api`         | Yes

For now, you may also use headers and build files in the following
legacy API directories&mdash;but see the
[disclaimer](#legacy-disclaimer) below.

Legacy API directory                       | Including subdirectories?
-------------------------------------------|--------------------------
`common_audio/include`                     | No
`media/base`                               | No
`media/engine`                             | No
`modules/audio_coding/include`             | No
`modules/audio_device/include`             | No
`modules/audio_processing/include`         | No
`modules/bitrate_controller/include`       | No
`modules/congestion_controller/include`    | No
`modules/include`                          | No
`modules/remote_bitrate_estimator/include` | No
`modules/rtp_rtcp/include`                 | No
`modules/rtp_rtcp/source`                  | No
`modules/utility/include`                  | No
`modules/video_coding/codecs/h264/include` | No
`modules/video_coding/codecs/i420/include` | No
`modules/video_coding/codecs/vp8/include`  | No
`modules/video_coding/codecs/vp9/include`  | No
`modules/video_coding/include`             | No
`pc`                                       | No
`rtc_base`                                 | No
`system_wrappers/include`                  | No

While the files, types, functions, macros, build targets, etc. in the
API and legacy API directories will sometimes undergo incompatible
changes, such changes will be announced in advance to
[discuss-webrtc@googlegroups.com][discuss-webrtc], and a migration
path will be provided.

[discuss-webrtc]: https://groups.google.com/forum/#!forum/discuss-webrtc

In the directories not listed in the tables above, incompatible
changes may happen at any time, and are not announced.

## <a name="legacy-disclaimer"></a>The legacy API directories contain some things you shouldn&rsquo;t use

The legacy API directories, in addition to things that genuinely
should be part of the API, also contain things that should *not* be
part of the API. We are in the process of moving the good stuff to the
`api` directory tree, and will remove directories from the legacy list
once they no longer contain anything that should be in the API.

In other words, if you find things in the legacy API directories that
don&rsquo;t seem like they belong in the WebRTC native API,
don&rsquo;t grow too attached to them.

## All these worlds are yours&mdash;except Europa

In the API headers, or in files included by the API headers, there are
types, functions, namespaces, etc. that have `impl` or `internal` in
their names (in various styles, such as `CamelCaseImpl`,
`snake_case_impl`). They are not part of the API, and may change
incompatibly at any time; do not use them.

# Pre-processor macros

The following pre-processor macros are considered part of the WebRTC API since
they allow to enable or disable different parts of the library.

## WEBRTC_EXCLUDE_BUILT_IN_SSL_ROOT_CERTS
If you want to ship your own set of certificates and inject them into a WebRTC
PeerConnection, you will probably want to avoid to compile and ship the default
WebRTC set of certificates.

You can achieve this by definning the preprocessor macro
`WEBRTC_EXCLUDE_BUILT_IN_SSL_ROOT_CERTS`. If you use GN, you can just set the GN
argument rtc_builtin_ssl_root_certificates to false and GN will define the macro
for you.
