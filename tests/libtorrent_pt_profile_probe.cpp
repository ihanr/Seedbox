#include <cstdlib>
#include <iostream>

#include <libtorrent/settings_pack.hpp>

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " EXPECTED_SEED_RATIO EXPECTED_BOOST\n";
        return 2;
    }

    namespace lt = libtorrent;
    const int expected_ratio = std::atoi(argv[1]);
    const int expected_boost = std::atoi(argv[2]);
    const auto defaults = lt::default_settings();
    const int ratio = defaults.get_int(lt::settings_pack::connect_seed_every_n_download);
    const int boost = defaults.get_int(lt::settings_pack::torrent_connect_boost);
    const int default_ratio = lt::default_int_value(lt::settings_pack::connect_seed_every_n_download);
    const int default_boost = lt::default_int_value(lt::settings_pack::torrent_connect_boost);

    std::cout << "default_settings: " << ratio << ',' << boost
              << "; default_int_value: " << default_ratio << ',' << default_boost << '\n';
    if (ratio != expected_ratio || boost != expected_boost
        || default_ratio != expected_ratio || default_boost != expected_boost)
    {
        std::cerr << "Expected both defaults to be " << expected_ratio << ',' << expected_boost << '\n';
        return 1;
    }
}
