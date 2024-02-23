set -xeu

out="$2"
from="$1"

# Generate gn desc.
pushd "${from}"
gn gen out/Default
gn gen out/Neon --args='target_cpu = "arm64" rtc_build_with_neon = true'
gn desc out/Default '*' --format=json > desc.json
gn desc out/Neon '*' --format=json > descarm.json
popd

rm -rf "${out}/"{api,audio,modules,rtc_base,system_wrappers,third_party,common_audio,experiments}
go run ./ -from "${from}" -to "${out}"

cd "${out}"
buildozer --quiet -f - \
    '//modules/audio_processing/aec3:aec3_avx2' \
    '//common_audio:common_audio_avx2' \
    '//modules/audio_processing/agc2/rnn_vad:vector_math_avx2' <<< 'copy target_compatible_with avx2_template|*
copy copts avx2_template|*
'
buildozer --quiet -f - <<< '
copy target_compatible_with neon_template|//common_audio:common_audio_neon_c|//common_audio:common_audio_neon
delete|//...:%buildozer_template
fix unusedLoads|//...:*
'
