#include "Commands/commands_client.h"
#include "Support/chrono.h"
#include "Support/googleraii.h"

#include <glog/logging.h>

#include <cstdlib>
#include <random>

namespace okcmds = okts::cmds;
namespace oksu   = okts::sup;

int main(int /*argc*/, char** argv) {

  std::random_device              myRd;
  std::mt19937                    myGen(myRd());
  std::uniform_int_distribution<> myDistrib(1, 1000);

  oksu::GoogleRaii myShutdowner(argv[0], true, true);

  const std::string        myEndpoint("127.0.0.1:6666");
  const size_t             myValuesPerBatch(10);
  std::vector<std::string> myValues;
  myValues.reserve(myValuesPerBatch);

  for (size_t i = 0; i < myValuesPerBatch; ++i) {
    myValues.push_back("StringValue-" + std::to_string(i));
  }

  try {
    okcmds::CommandsClient myClient(myEndpoint);

    const std::size_t myListSize = 100000;

    const std::string myList("List-" + std::to_string(myDistrib(myGen)));

    oksu::Chrono myChrono;
    for (size_t i = 0; i < myListSize; ++i) {
      myClient.dequePushFront(myList, myValues);
    }
    std::cout << "Inserts per seconds: "
              << myListSize * myValuesPerBatch / myChrono.stop() << "\n";

    myClient.dequeTrim(myList, 1, 0);

  } catch (const std::exception& e) {
    LOG(ERROR) << "Error: " << e.what();
    return EXIT_FAILURE;
  } catch (...) {
    LOG(ERROR) << "Error: unknown";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
