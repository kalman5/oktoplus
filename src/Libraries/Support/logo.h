#include <glog/logging.h>

namespace okts::sup {

inline void
logo(const std::string& aInfo, int cqs, int minPollers, int maxPollers) {
  // clang-format off
  LOG(INFO) << "                             ,╓▄▓█▀▀▀▀▀▀M▄µ";
  LOG(INFO) << "                          ,▄▀▀Ñ▓▓▀ ╓╖      `▀▄  Oktoplus service on: " << aInfo;
  LOG(INFO) << "                        ,█▀   ,,   `   ╨▀     ▀▄  cqs: " << cqs;                                
  LOG(INFO) << "                       ╓▀    ▓ÑÑM          @N   █  min pollers: " << minPollers;                               
  LOG(INFO) << "                      ▄▀      `                  █  max pollers: " << maxPollers;
  LOG(INFO) << "                     ,▌ #▓▄                       ▌";                             
  LOG(INFO) << "                     █  `                         ▀▄";                            
  LOG(INFO) << "               ╓▓▀``▀█             ,▒▒▒▒        ,, ▐▌,▄▓";
  LOG(INFO) << "              █`,▄   ▐           Å`      ªù  ,`    ²██ ▐▌";
  LOG(INFO) << "              ▀▀` ²▌  µ ░       É  ,▄╦┐    ╕ Γj█▄   j█  ▀▄          ,╓Æ▀▀`,█▌";  
  LOG(INFO) << "                   █  ▐▄ ░      ╡  └██▀    ▌ ░,    ,█`█,  ▀▓µ     ,█▀  ░#P█";
  LOG(INFO) << "        ,╓#KK#▄,  ,▌    V       `µ        /    ``▒`▐▌  ▀▄∞ µ █µ  J▀▓  `▄▀²";
  LOG(INFO) << "      ▄▀       `█ █    `█▀█,      ░W╥,,╥m`          ²▌  ▐▌H   █  ▌:  ╛▐▌";
  LOG(INFO) << "    ╓█Å   ,ñW   ▐██▄▄  ╗█  `▀▄               ,w╩*    █ ,█M▀▀``▀▄█    UW█";
  LOG(INFO) << "   ▐wå    █  ,A`      *╨▀█    ╙▄   ╒▀▄∞∞╨ª``   J     ██▀▒       ╨▓*µ ╡`█";
  LOG(INFO) << "   ²█J   J▌`█▌  ,▄ªy   ▐ N▀M▄æ#▀y    `█▄~--─ⁿ  j    , ▄╘    ▀`▄   h▐   ▐µ";
  LOG(INFO) << "    █    ²█ ▐H ▄▀█└j    ▌       ▐      `N╥,,,«m▒  ,A    `V▄,,▄█▄   µ╘ N⌂█";
  LOG(INFO) << "   ▐H`l    █▄█ █ ▌,     █ `░ ▄  `             -─*`    +▄, ▄█▀▄▀▌   ╘  Γ,█";
  LOG(INFO) << "    █, V    `▀██▄█       hª`      ,                ,▄  ` ``▀   ▌   JU h█";
  LOG(INFO) << "     ²▄ \\       Å- ]   J█ *╜`   ,▒H    ,     ,     ``*▄,╨┘ ,▄ª    ╛ ▄█";
  LOG(INFO) << "      █`¬ ≈     ▐ ΓL└     ▀▄ «m,A       `    ╒╫M▄ ª▓M    ╓ `      ╛ ▄┘";
  LOG(INFO) << "       ▀▄J  `¬≡. N⌐  Y   ª▓ ``` `                `ⁿ▄     `     ,ª╒,─█";
  LOG(INFO) << "          `▀▄--.  ╘   Y            ,═`ª∞,  ▄╗  'M ┌╗ÖYH═▒▒═∞ª`   ▄▄M";
  LOG(INFO) << "             ╙W▄▄▄▄▌ª- `╩       ▒º,, ¿û▐▀▀▄╫╜      ``  W ,⌐──╥▄╨`";
  LOG(INFO) << "                   ▀µ j   ````  ┌▒ ▓▄▄A`   █ñ   ╥╗▄     █▀≡≡∞²";
  LOG(INFO) << "                    ```*æ╖,/`~-]▌  ▀▄,     ,▌¿  `╙`     █";
  LOG(INFO) << "                           `²²²ª▌    `▀▀▀▀▀▀ƒ          J▌";
  LOG(INFO) << "                                `█,           ╓       ▄▀";
  LOG(INFO) << "                                  `▀█▄;       `  ,▄▄▀'";
  LOG(INFO) << "                                      ```╙╨╨╨╨ª``";
  // clang-format on
}

} // namespace okts::sup
