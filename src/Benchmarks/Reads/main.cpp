#include "Commands/commands_client.h"
#include "Support/chrono.h"
#include "Support/googleraii.h"

#include <glog/logging.h>

#include <cstdlib>
#include <random>

namespace okcmds = okts::cmds;
namespace oksu   = okts::sup;

int main(int /*argc*/, char** argv) {

  std::random_device              rd;
  std::mt19937                    gen(rd());
  std::uniform_int_distribution<> distrib(1, 1000);

  oksu::GoogleRaii myShutdowner(argv[0], true, true);

  const std::string        myEndpoint("127.0.0.1:6666");
  const size_t             myValuesPerBatch(1);
  std::vector<std::string> myValues;
  myValues.reserve(myValuesPerBatch);

  for (size_t i = 0; i < myValuesPerBatch; ++i) {
    myValues.push_back("StringValue-" + std::to_string(i));
  }

  try {
    okcmds::CommandsClient myClient(myEndpoint);

    const std::size_t myListSize = 100000;

    const std::string myList("List-" + std::to_string(distrib(gen)));

    for (size_t i = 0; i < myListSize; ++i) {
      myClient.listPushFront(myList, myValues);
    }

    oksu::Chrono myChrono;
    for (size_t i = 0; i < myListSize * myValuesPerBatch; ++i) {
      myClient.listPopFront(myList);
    }
    std::cout << "Reads per seconds: "
              << myListSize * myValuesPerBatch / myChrono.stop() << "\n";

    std::cout << "Entries left: " << myClient.listLength(myList) << "\n";

  } catch (const std::exception& e) {
    LOG(ERROR) << "Error: " << e.what();
  } catch (...) {
    LOG(ERROR) << "Error: unknown";
  }

  return EXIT_FAILURE;
}
