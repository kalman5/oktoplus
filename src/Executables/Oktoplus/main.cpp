#include "Commands/commands_server.h"
#include "Support/googleraii.h"

#include <glog/logging.h>

namespace okco = okts::commands;
namespace oksu = okts::sup;

int main(int /*argc*/, char** argv) {

  oksu::GoogleRaii myShutdowner(argv[0], true, true);

  try {
    okco::CommandsServer myServer("127.0.0.1:6666");

    myServer.wait();

    return EXIT_SUCCESS;

  } catch (const std::exception& e) {
    LOG(ERROR) << "Error: " << e.what();

  } catch (...) {
    LOG(ERROR) << "Error: unknown";
  }

  return EXIT_FAILURE;
}
