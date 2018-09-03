#include "Commands/commands_server.h"
#include "Configurations/jsonconfiguration.h"
#include "Support/googleraii.h"

#include <glog/logging.h>

namespace okco = okts::commands;
namespace okcfg = okts::cfg;
namespace oksu = okts::sup;

int main(int /*argc*/, char** argv) {

  oksu::GoogleRaii myShutdowner(argv[0], true, true);

  okcfg::JsonConfiguration myConfiguration("oktoplus.cfg");

  try {
    okco::CommandsServer myServer(myConfiguration.endpoint());

    myServer.wait();

    return EXIT_SUCCESS;

  } catch (const std::exception& e) {
    LOG(ERROR) << "Error: " << e.what();

  } catch (...) {
    LOG(ERROR) << "Error: unknown";
  }

  return EXIT_FAILURE;
}
