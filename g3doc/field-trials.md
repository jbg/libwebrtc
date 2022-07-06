# Field trials

<?% config.freshness.owner = 'lndmrk' %?>
<?% config.freshness.reviewed = '2022-06-23' %?>

WebRTC provides some means to alter its default behavior during run-time,
colloquially known as *field trials*. This is foremost used for A/B testing new
features and are related to
[Chromium field trials](https://chromium.googlesource.com/chromium/src/+/main/testing/variations/README.md)
to facilitate interoperability.

A field trial consist of a key-value pair of strings. By convention, the field
trial key is prefixed with `WebRTC-` and each word is capitalized without
spaces. Sometimes the key is further subdivided into a category, for example,
`WebRTC-MyCategory-MyExperiment`. The field trial value is an opaque string and
it is up to the author to define what it represents. There are
[helper functions](https://webrtc.googlesource.com/src/+/refs/heads/main/api/field_trials_view.h)
to use a field trial as a boolean, with the string `Enabled` representing true
and `Disabled` representing false. You can also use
[field trial parameters](https://webrtc.googlesource.com/src/+/refs/heads/main/rtc_base/experiments/field_trial_parser.h)
if you wish to encode more elaborate data types.

The set of field trials can be instantiated from a single string with the format
`<key-1>/<value-1>/<key-2>/<value-2>/`. Note the final `/` at the end! In
Chromium you can launch with the `--force-fieldtrials` flag to instantiate field
trials this way, for example:

```
--force-fieldtrials="WebRTC-Foo/Enabled/WebRTC-Bar/Disabled/"
```

## Policy

The policy for field trials is:

-   A field trial should only be used to test out new code or parameters for a
    limited time period. It should not be used for configuring persistent
    behavior.
-   The field trial must have an end date. The end date may be pushed back if
    necessary, but should not be pushed back indefinitely.
-   A field trial must be associated with a bug that
    -   reserves the field trial key,
    -   is open,
    -   is assigned to an owner, and
    -   has the end date specified.

## Killswitches

Field trials can be used as killswitches for new features which have a moderate amount of breaking
in unforeseen circumstances. In such cases, the killswitch can be used to deactivate the feature
without the need for a new release.

This will typically use the
[IsDisabled](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/api/field_trials_view.h;l=35;drc=e62c2f2c77bca9af93b1f8cc5a37bb9978a18844)
method.

## Standing field trials

Some field trials are used to guard test-only features or features that should not be enabled
by default in Chrome. Examples of this include

* [WebRTC-Debugging-RtpDump](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/pc/srtp_session.cc;l=123;drc=2d6c4d07129f88d599fc41965c22c7faaf09e793) which dumps decrypted RTP packets to the logs.
* [WebRTCFakeNetwork...](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/call/call_factory.cc;l=45;drc=80b7c6befd39c83241ad1bdfb3902be45d0a0ccf) which allows configuring a virtual network with custom delays and packet loss.

