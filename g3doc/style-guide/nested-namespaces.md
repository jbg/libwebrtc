# `.h` and `.cc` files come in pairs

<?% config.freshness.owner = 'mbonadei' %?>
<?% config.freshness.reviewed = '2021-08-16' %?>

### ::webrtc::flat_containers_internal

This namespace is used to hide implementation details from headers that can
be leaked by `rtc_base/containers/flat_set.h` and
`rtc_base/containers/flat_map.h`. These implementation details are not common
to other `//rtc_base` constructs, hence they are hidden by this logical
namespace.
