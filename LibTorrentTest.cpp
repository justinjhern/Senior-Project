#include <iostream>
#include <libtorrent/session.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/torrent_status.hpp>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <magnet-link OR path-to-torrent-file>\n";
        return 1;
    }

    try {
        lt::settings_pack pack;
        pack.set_int(lt::settings_pack::alert_mask, lt::alert_category::error | lt::alert_category::storage | lt::alert_category::status);

        lt::session ses(pack);

        lt::add_torrent_params atp;
        if (strstr(argv[1], "magnet:?") == argv[1]) {
            atp = lt::parse_magnet_uri(argv[1]);
        } else {
            atp.ti = std::make_shared<lt::torrent_info>(argv[1]);
        }

        atp.save_path = "."; // Save path for downloaded files
        ses.async_add_torrent(std::move(atp));

        std::cout << "Downloading... Press Ctrl+C to abort.\n";
        int count = 0;
        // Main download loop
        for (;;) {
            std::vector<lt::alert*> alerts;
            ses.pop_alerts(&alerts);

            if(count % 5 == 0) {
                std::cout << "Count is: " << count << std::endl;
            }
           
            for (lt::alert const* a : alerts) {

                if (auto st = lt::alert_cast<lt::state_update_alert>(a)) {
                    count++;

                    for (auto const& s : st->status) {
                        if (!s.handle.is_valid()) {
                            continue;
                        }

                        std::cout << "\rProgress: " << (s.progress_ppm / 10000) << "%";
                        std::cout.flush();
                    }
                } else if (auto pt = lt::alert_cast<lt::torrent_finished_alert>(a)) {
                    std::cout << "\nDownload complete: " << pt->message() << std::endl;
                    return 0;
                } else if (lt::alert_cast<lt::torrent_error_alert>(a)) {
                    std::cerr << "\nError: " << a->message() << std::endl;
                    return 1;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
