#pragma once

namespace httplib {
class Server;
}

namespace native_server {

// Registers GET /subtitles - matches native-exp-server/server.py's
// _handle_subtitles contract exactly. Factored out of server.cpp since it
// pulls in youtube_utils, unlike everything else there.
void registerSubtitlesRoute(httplib::Server& svr);

}  // namespace native_server
