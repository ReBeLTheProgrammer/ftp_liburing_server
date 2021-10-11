#ifndef URING_TCP_SERVER_FTPCONNECTION_H
#define URING_TCP_SERVER_FTPCONNECTION_H

#include <string>
#include <vector>
#include <memory>
#include <arpa/inet.h>
#include <algorithm>
#include <mutex>
#include "FTPCommon.h"
#include "async_uring.h"
namespace mp {

    using namespace std::string_literals;

    class FTPConnection;

    /**
     * @brief
             ,▄Ñ▓▓▓▄
            ▄▀╫╫╫╫╣▓▓▄           L
           ▄▒╫╫╫╫╫╫╢▓╫▓⌐         ▌      Å           ,▄▄▄▓▓Φ▄
          ╣░╫╫╫╫╫╫╫╫║╫▓▓▄         ▄    ▄         ▄▓▓╫╫▓▀▒╫╫╫▀▄
         ╣░╫╫╫╫╫╫╫╫╫░█▓▓▓▄         Φ  Ä       ,▄▓▓▓▓▓▌░╫╫╫╫╫╫╢⌐
        ╓▌╫╫╫╫╫╫╫╫╫╫╫█╫▓▓╫▓µ╓▄▄▄Φ▓▓▓▓▓▓▓▓▓▓▄▄▓▓▓▓▓▓▓▀╫╫╫╫╫╫╫╫╫▌
        ▓░╫╫╫╫╫╫╫╫╫╫╫█╫▓▓▓▓╫▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓╫▓▓▓▓▓▓▀╫╫╫╫╫╫╫╫╫╫▌
       ▐▌╫╫╫╫╫╫╫╫╫╫╫╬█▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓█▌╫╫╫╫╫╫╫╫╫╫╫Γ
       ▐Ñ╫╫╫╫╢▒╫╫╫╫░█╫▓▓▓▓▓▓▓▓▓▓▓╫▓▓▓▓▓▓▓▓▓▓▓▓▓╫▓▓▓▓▓░╫╫╫╫╫╫╫╢
       ║Ñ╫╫╫╫╠▓░╫╫▄█╫▓▓▓▓▓▓▓▓▓▓▓██╫▓▓▓▓▓▓▓▓▓▓▓╫█▓▓▓▓╫▓▄╫╫╫╫╫░▌
       ║Ñ╫╫╫╫╫║▀▓▀█▓▓▓▓▓▓▓▓▓▓▓▓▓█▓▓▓▓▓▓▓▓▓▓▓▓╫▓██▓▓▌▓▓▓▌╬▒╫╫╢
       ▐Ñ╫╫╫╫╫╫╫╫╫▌▓▓▓▓▓▓▓▓▓▓▓╫█╫▓▓▓▓▓▓▓▓▓▓▓▓█╫▓▓▓█▓▓▓▓█▌╫╫╫╛
        ▌╫╫╫╫╫╫╫╫▓▓▓▓▓▓▓▓▓▓▓▓▓▓▌▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓█▓▓▓▓▓█╫╫╛
        ▓Ñ╫╫╫╫╫╫╫▌▓▓▓▓▓▓▓▓▓▓▓▓╫▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓█▌Γ
        ╙▌╫╫╫╫╫╫╫▌▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓╫█▌╫█▓▓▓▓▓▓╫▓█╫▓▌▓▓▓▓▓▓▓▌
         ╙▒╫╫╫╫╫╫▌▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓█▓▓▓╫█▓▓▓▓╫█▓█▓█▓▓▓▓▓▓▓▓▌
          ▀▒╫╫╫╫╫▌▓▓▓▓▓▓▓▓▓▓▓▓▓▓╫▀▄█▓╙█▓▓▓▓█░▄▄▀█▓▓▓▓▓▓▓▓M
           ▀▄╫╫╫║▌▓▓▓▓▓▓▓▓▓▓▓▓▓▓█▄▓██╦█▓▓▓╫▌║██▄█▓▓▓▓▓▓▓█
            ╙▓▒╫║▌▓▓▓▓▓▓▓▓▓▓▓▓▓▓╫█▓▓▀▀▀▓▓▓▓█╫▓╫█╫▓▓▓▓▓▓▓▌
              "▀▀▌▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓Ñ░░▄▓███▌░╠▀▀╠█╫▓▓▓▓▓▓█
                 ▀╫▓▓▓▓▓▓▓▓▓▓▓▓▓╫█░░███████▌░░░█╫▓▓▓▓▓▓╫▌
                  ▀▓╫▓▓▓▓▓▓▓▓▓╫▓█░░░▀█████▀░░░░█▓▓▓▓▓▓▓▀
                    ▀▓▓▓▓▓▓▓▓▀▀█▄░░░»»░¿░n░░░░╠█▓▓▓▓▓▓└
               ,╓▄▄Φ▓╫▓╫╫▓▓░░▄░╙████▓███▓▓▓▓▓█▄▓█▓▓▀`
         ,▄▄Φ▓╫╫▓▓▓▓▓▓▓▓▓▓▓▓▓▄▓▄░▀█▌╫╫▓▓▀▀▓█░▄█▓╫╫▓▓▓▄,
     ▄▄▓▓▓█▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓╫▓▓▄╠▀▓▓▓▓▓▀▀╠▓▓█▓▓▓▓▓▓▓▓▓▓▄
  ,▓▓▓▓▓╫▓▌▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓╫▓▓▄▄▄▄▄▓▓╫╫█▓▓▓▓▓▓▓▓▓▓▓╫▓▄
 ╓▓▓▓▓▓▓╫█╫▓▓▓▓▓▓▓▓▓▓╫╫▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓╫▓▓▓▓▓▓▌▓▓▓▓▓▓▓▓▓▓▓▓▓▓╫▓,
╓█▓▓▓▓▓╫▓█▓▓▓▓▀▀▀▀▀╙║█╫╫▓▓▓▓▓▓▓▓▓╫╫╫╫▓▓▓▓╫╫╫▓█▀▓▓▓╫▓▓▓▓▓▓▓▓▓▓▓▓▓▓▄,
█▓▓▓▓▓▓▓▓╨        ,▄▌▓▓▓▓▓▓▓▓▓▓▓╫╫╫╫╫╣▀╣╫╫╫╫╫█    ╙▀▀▓╫▓▓▓▓▓▓▓▓▓▓▓╫▓▄
██╫╫▓█▓█┘       ,▄▓▓▓▓▓▓▓▓▓▓▓▓╫╫╫╫╫╫╫╫╫╫╫╫╫╫╫▌        ╙▀▓▓▓▓▌▓▓▓▓▓▓▓╫█
█▓▓▓██╨       ,▄▓▓▓▓▓▓▓▓▓▓▓▓╫╫╫╫╫╫╫╫╫╫╫╫╫╫╫▓╫▌         ▐█╫▓▓▓▓▓▓▓▓▓▓▓╫▓
█▓█▓█⌐       ▄▓▓▓▓▓▓▓▓▓▓▓▓▓╫╫╫╫╫╫╫╫╫╫╫╫╫╫╫▓▓█H          '▀▀▀▓▓▓▓╫╫▓▓▓▓╫▌
  ██▄      ╓▓╫▓▓▓▓▓▓▓▓▓▓▓▓╫╫╫╫╫╫╫╫╫╫╫╫╫╫╫▓▓▓█               █▓▓▓╙█▓▓▓█▓▌
  ▐▌█     ╓█▓▓▓▓▓▓▓▓▓▓▓▓▓╫╫╫╫╫╫╫╫╫╫╫╫╫╫╫▓▓▓╫▌               █╫▓▌  █▓▓█
   █╫▌   ╓█▓▓▓▓▓▓▓▓▓▓▓▓▓╫╫╫╫╫╫╫╫╫╫╫╫╫╫╫▓▓▓╫█                 `
   █╫█  ,█▓▓▓▓▓▓▓▓▓▓▓▓█▓╬╫╫╫╫╫╫╫╫╫╫╫╫▓▓▓▓╫█`
   █╫╫█,█╫▓▓▓▓▓▓▓▓▓▓▓▓█╫▓▓╬╫╫╫╫╫╫╫╬▓▓▓▓╫▓█▌
   ╙█╫▓█▌▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓╫▓▓▓▓▓▓▓╫╫█
     ▀▓█▓▓▓▓▓▓▓▓▓▓▓▓▓▓█▓▀▀▀▀█▓▓╫╫▓▓▓▓▓▓▓▓█
     ╓█▓▓▓▓▓▓▓▓▓▓▓▓▓╫▓╨└   ▐█▓▓▓▓▓▓▓▓▓▓▓▓█
    ▄█▓▓▓▓▓▓▓▓▓▓▓▓▓▓▀       █╫▓▓▓▓▓▓▓▓▓▓▓╫▌
   ▄▓▓▓▓▓▓▓▓▓▓▓▓╫▓█└         █▓▓▓▓▓▓▓▓▓▓▓▓█▄
  ║▌▓▓▓▓▓▓▓▓▓▓▓▓█▄╓           ▀▓╫▓▓▓▓▓▓▓▓▓▓█▄
  ║█╫▓▓▓▓▓▓▓▓▓╫▓█╫╫█      ╓▄▄▄▓▀▓▓▓▓▓▓▓▓▓▓▓▓▌
   `▀▀▓▓▓▓▓▓▓▓▓▀▀╙╙      "█╫╫▓▓▓▓▓▓▓▓▓▓▓▓▓▓█
     */
    class FTPConnectionState {

    public:
        explicit FTPConnectionState(std::shared_ptr<FTPConnection>&& handledConnection): _handledConnection(handledConnection) {}

        virtual void user(const std::string& username) = 0;
        virtual void cwd(std::string path) = 0;
        virtual void cdup() = 0;
        virtual void quit() = 0;
        virtual void port(const std::string& hostPort) = 0;
        virtual void type(const std::string& typeCode) = 0;
        virtual void stru(const std::string& structureCode) = 0;
        virtual void mode(const std::string& modeCode) = 0;
        virtual void retr(std::filesystem::path path) = 0;
        virtual void stor(std::filesystem::path path) = 0;
        virtual void pwd() const = 0;
        virtual void list(std::filesystem::path path) const = 0;
        virtual void noop() const final;

        virtual ~FTPConnectionState() = default;

    protected:
        std::shared_ptr<FTPConnection> _handledConnection;
    };

    /**
     * @brief
░»╨""`,»»"░░░«═╩╨╫╫Ñ░═░░""»»»`╔ '.```/`` ````````»`^»»»»»»»»»»»»»»»»»»»»» ░»»»»»╚»»»»»»»░░░░░░▐▓▓███
,.  .  "``»~^`,≈«═╨░░░╥╦╦╦≈»ñ»û`»   ¿``  ``]`` '``Å;'`»`»»H»»»»»»»»»»»»»»»ù»»»»»»░»»»»»»»»░░░░ ▓▓███
````````. "```,«*^`»««═░░░░»Ñ".`   . `     H` '''].`»»`»»╬»»»»»»»»»»»»»»»»r»»»»»»░»»»»»»»»n»░░ ▓▓▓█▓
` `      `»»` .≈""░░/░░░░╩*`»   `  ╚!   ``» `'»H`╫;»»»»»╝▐»»å░»»»»»»»»»»»»:»»»»»»░»»»»»»»»»»░░ ▓▓▓▓▓
          `»»╦,.»╩hèô░░¿¡⌂«m"  `   Γ`''`»`╟`»`]`]j»¡░¡;" ]»╔ ░»|»»»»»»»»»`░»»H»»╔»»»»»'»»»»»░░H▓▓▓▓▓
»»n%ñ*░"╦╦».`»░╦╦¡╨╨╫░░░░^`  '```╓╫⌐»``»»»╫»╫∩╫`H²»╫») ``Ü╙Ü`░»H»»»»»»»»░φ»»»Ñ»»╠░»»»»,»»»»»»░╦╟▓▓▓▓
╨╨╨"""╨╫╦░░╦╦»»╨╫╫Ñ░░``''`»'``'╓╬╫╫░»░»»»j╫»╫▐ ░  ░Ñ»KN╦╦░╦ j»╬H»»»»»»Ü░╨╠%g╬╫»»░░»»»░ñ»»»»»»░░/▓▓▓█
,«≈]H╨░╫╦░╦░░╫N░░╫╫╫╦╙Ñ╦2╓░▄╦D╩╫╫╫╫M»H»»»╫╫¼╫@,╫#M▀▓╣╨MW▄╙╨╫Ü╬╫░»»»»x░»`,ì»╛╟Ñ╙M>w╔»»╔H»»»»»»░░░░▀██
╦╦╦╦╦╦µ░░░░╦╦░╨╫Ñ░╨╫╫H╫╫Ñ░]╫╫╫╫╫╫╫╫φ»½»»»╠Ñ ╣║▓" "╫▓▓╫▓"ÅH Å"╟░»»µA░H»╩╨╨W╙╨╢µ░ j╓Ñ»╔╫H»»»»nn░╙▄▄▄▄█
░╨╩Ñ╫╫╫╫╫╫N╦░░░╙╫╫Ñ░╙╫╫╫╫╫╫╫╫▓Ñ╠╫╫M╫U╟H\»»Ñ'▀▌` ▓▓▓██▓╫H`  ,ú»µ═"` ⁿ»╛Φ▀▀▄▓▓▄▀▓w╩╟U╬╫╫░»»»»░░░░╙████
╫╫ÑN╦╦░╨╨╫╫╫╫╫Ñ╦░╙╨╬Ñ╦░╫▓╫╫╫▓▓╫]╫▌ ╫╫m╫NX»╙⌐ ╚  ▌╢█▓▓▌║   "``       ░░',,╫▓▓╫▌╙█╬╨╫╫╫Ñ»╔Hn»░╔░░░╙▀██
╫╫╫╫╫╫╫╫Ñ╗░░╫╫╫╫╫░╫N╫Ñ╫╫╫▓╫╫▓▓╫╩░M ╫╫╫╫╫╫Mw░,   `Φ▄▀▄Å               \ ▌╣▓██▓▓ ╫█▄╫╫╫U╬Ñ»»»»▓▓▄░ññ╢█
╫╫░╫╫╫╫╫╫╫▓▓╬╬╫╫╫╫N╨╫Ñ╫╫Ñ╫▓▓▓╫╩╔╫╬⌂╫╫╫╫╫╫Ñ»└*∞⌂ »hn≈n»»:        ¡    . ▌╣▓█▓╣▀ ╟▀╣╫╫╫╫Ü»╔Ü╓▓████████
╫╫╫╫╫╫╬╬╬╬Ñ░╩╫╣╫░╫╫╫╬╫╫Ñ░╫▌╫╫Mû╬╫╫╫╫╫╫╫╫╫╫░~H»»»»»»»»»»»»»:    ╔╫    .»▐╩ÖÜ╝- └╓╬╫╫╫▀▄╬▓Ü▄██████████
▓▀▀╫╫╫╫╫╫╫╫╫╫╫╫╫╫╫╫╬╫╣▓╫╫Ñ╫╫╫H»╫╫╫╫╫╫╫╫╫╫╫H░!"»░░░░░░░░»»»»`   ╫╫H'»¿»»»»┴┴»;»╙Φ╣╫╫╫╫▓▓╨║███████████
╫╫╫╫╫╫╬╫╫╫╫╬╬╬╬╫╫╫╫╫╫▓╫╫╫╫╫╫╫░░╫╫╫╫╫╫╫╫╫╫╫Ñ» ``"»»»»»»»»»```` ,Ñ╨'»»»░»û░░»»»` ╓╫╫╫╫╫╫Ñ░▓███████▓▓█▓
▓▓▓╫╫╫╫╫╫╫╫╫╫╬╬╫╫╫╫╫╫╫╫╫╫╫╬╫╫:░╫╫╫╫╫╫╫╫╣╫╫╫░>r````````````    "  ```»»»»»░░░»»╔╫╫╫╫╫╫╫░`░╙╙░░░░░░░╦╦
╫╫╫╬╬▓▓▓▓▓▓▓▓╫╣╫╫╫╫╫╬╬╫╫╫▓▓╬╫]░╫╫╫╫▓╫╫╫░╣╫╫H»└                    ````»»»»»``╟╫╫╫╫╫╫╫╫░ Ñ╫╫╫╫╫╫╫╫╫╫╫
▓▓▓▓▓▓▓▓▓▓▓▓▓▀╫╫╫╫╫╫╫▓▓▓▓▓▓╫▌░░╫╫╫╫▓╫╫╫m╙╣╫Ñ» ⌐         .»«««w       ```````╬╫╫╫╫╫╫╫╫Ñ░▐╫╫╫╫╫╫╫╫╫╫▓▓
▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▀▓▓▓▓▓▓▓╫▓▓▌░░╫╫╫╫▓▓╫╫Ü░░╣╫░╦╔           «╦╥ ` `      `  ╓╣╫╫╫╫╫╫╫▓╫░░╟╫╫╫╫╫╫╫▓▓▓▓▓
▓▓▓▓▓▓█▓╫╫▓▓▓▓▓▓▓▓▓▓▓▓▓▀M╣▀╙Ü░░╫╫╫╫╫▓╫╫Ü░░░╫░░▐╫N⌂»»»»»»»»»»»»»»»»»»»»»,╔╬╫▓╫╫╫╫▓╫╫╫Ü░░╣╫▓▓▓▓▓▓▓▓▓▓▓
▓▓▓▓▓▓▓▓▓▓▓▓▓▓▀▀╨╨░░░░░░╦╦╦R░░╟╫▓▓▓▓▓▓▓Ü░░░ñÑ░H╫╫╫Ñ▄»»»»»»»»»»»»»»»╓▄Φ╣▓▓▓▓▓▓▓▓▓▓▓▓▓░░H╫▓▓▓▓▓▓▓▓▓▓▓▓
▓▓▓▓▓▓▀▀╣╩░░░░╦╦╦╬╫╫╫╫╫╫╫╫╣░░░╣▓▓▓▓▓▓▓▓K░╬░░╟░H╫╫╫╫╫▓╗µ»»»»»»»¡▄▄╬▓╫▓╫╫▓▓▓▓▓▓▓▓▓▓▓▓M░░▐▓▓▓▓▓▓▓▓▓▓▓▓▓
▀╫░╫╦╦╫╫╫╫╫╫╫╫╫╫╫╫╫╫╫╫╫╫╫╫░░░╣╫╫╫╫╫▓╫▓╫K░╟╬░░░░╟╫╫╫╫╫╫╫▓▄▄▄╗╬▓▓╫╫╫╫▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓╫░░░╟╫▓▓▓▓╣▀▀▀╬╫╠╫
╫╫╫╫╫╫╫╫╫╫╫╫╫╫╫╫╫╫╫╫╣╩╩╝╩½╩░╬╣╫╫╫╫╫╫╫╫╫K░╟╫Ñ░░░ ╫╫╫╫╫╫╫╫╫▓▓▓╫╫╫╫╫Å╣╫╫╫╫╫╫╫╫▓╫╫╫╫╫╫╫░░░╦╦NNÑ╬╫╫╫╫╫╫╫╫
▓▓▓▓╫╫╫╫╫╫╫╫╫╫╫▓╨╙````````````` ````▐╫╫Ü░░╫╫Ñ░░ ░╙╫╫╫╫╫╫╫╫╫╫╫╫╫╫╫`` ╙*═╬╣╫╫╫╫╫╫╫╫╫Ñ░░C╫╫╫╫╫╫╫╫╫╫╫╫╫╫
▓▓▓▓▓▓▓╫╫╫╫╫╫╫╨ `` `  ``` `  `  `  ``╫╫Ñ░░╟╫╫Ñ» r` ╙Ñ╫╫╫╫╫╫╫╫╫╫╫H ` `` '` `╙╫╫╫╫╫╫Ñ░░▐╙╙╙╙╙╙╙╨╨╩╫╫╫╫
▓▓▓▓▓▓▓▓▓▓╫╫▓` '``                   ╫╫Ñ»»░╫╫╫░ !`  ╘`╨╫╫╫╫╫╫Ñ╨ U           ╫╫╫╬╫╫░»»|    ``»»»»»»╨╫
▓▓▓▓▓▓▓▓▓▓╫╫─                        ╫╫╫░»n╙╫╫ÜU   ``¼  ╙╨╨`    └       `  `╟╫╫╫╫╫░»»ñ  `'````»»»»»»
▓▓╣▀▀╬╣╠╠Ñ╥╛      ` ``   ` ,         ╫╫╫Ñ░░░╢╫╫░ w   `N         `  ```  .-─^`╫╫╫╫Ñ░░░░H `   ``»»»»»»
╬╬╫╫╫╫╫╫╫╫╫M    `           \        ╫╫╫╫╫m░░╣╫M | `"*╩╫⌂      ,-─*^``       ╟╫╫╫╬U░░░': ` `  '»»»»»
╫╫╫╫╫╫╫╫╫╫╫H                 ╙╦╥    ▐╫▓╫╫╫╫N░»╣Ü½   `  `╙]H` .^    `          ▓╫╫╫╫╬U░░░"⌂```:»»»»»»
╫╫╫╫╫╫╫╫╫╫╫N                   ╙╫N⌂ ▐╫M╣╫╫╫╫╬m░╢░░`      `                    ╟╫╫╫╫╫╫Ñ░░░»\  ``»»»»»
╫╫╫╫╫╫╫╫╫╫╫╫                     ╬╫  ╫H╙╣╫╫╫╫╫Ñ░½n░'   `                      ╫╫╫╫╫╫╫╫╫N░»░/»``»»»»»
╫╫╫╫╫╫╫╫╫╫╫╫H`                    ╫Ñ ╙╫N╙╫╫╫╫╫╫Ñ░\░░`    `                   `╫╫╫╫╫╫╫╫╫╫╬U░░/⌐`»;>n░
╫╫╫╫╫╫╫╫╫╫╫╫╫    `     ` ` ( `»   ╙╫N`╙╩╬╬╣╫╫╫╫╫╬m\░░/   "]╦,                `╚╫╫╫╫╫╫╫╫╫╫╫H╚U╘¿░»»»»
"""""""""""""``````````````````````""````"""""""""``````````"```````````````````"""""""""""`````````
*/
    class FTPConnection: public FTPConnectionBase {

    public:
        FTPConnection(std::shared_ptr<FTPConnectionBase>&& parent,
                      const std::filesystem::path&& root,
                      const std::shared_ptr<FTPFileSystem>&& fileSystem,
                      std::function<void(void)>&& waitingForConnectionCallback = [](){}
                      ):
                      FTPConnectionBase(std::move(parent),
                                        std::move(waitingForConnectionCallback)),
                                        _root(root), _fileSystem(fileSystem) {}

        //Starts an asynchronous FTP Connection
//        void start();

        void processCommand(std::size_t commandLen);

        const int fd() const noexcept { return _fd; }
        [[nodiscard]]const std::shared_ptr<uring_wrapper>& ring() const { return _ring; }
        const std::filesystem::path& pwd() const noexcept { return _pwd; }
        std::filesystem::path& pwd() noexcept { return _pwd; }
        bool hasChildConnections() noexcept { return !_childConnections.empty(); }
        const std::filesystem::path& root() const noexcept { return _root; }
        void switchState(std::unique_ptr<FTPConnectionState>&& state) noexcept {
            _state = std::move(state);
        }
        std::shared_ptr<FTPFileSystem> fileSystem() { return _fileSystem; }

        const int port() const noexcept {
            return _port;
        }

        void setPort(int port) noexcept {
            _port = port;
        }

        void setStructure(FTPFileStructure structure) noexcept { _structure = structure; }

        Callback defaultAsyncOpHandler = [this](std::size_t res){
            if(res == -1) stop();
            _ring->async_read_until(
                    _fd,
                    _command,
                    "\r\n",
                    [this](std::size_t commandLen){ processCommand(commandLen); }
            );
        };

        ~FTPConnection(){
            close(_fd);
        }

    protected:

        void startActing() override;

    private:
        int _fd;
        std::unique_ptr<FTPConnectionState> _state;
        std::filesystem::path _pwd;
        std::filesystem::path _root;
        std::string _command;
        int _port;
        FTPRepresentationType _type;
        FTPFileStructure _structure;
        FTPTransferMode _mode;
        std::shared_ptr<mp::FTPFileSystem> _fileSystem;
    };

    class FTPConnectionStateNotLoggedIn: public FTPConnectionState{
    public:
        explicit FTPConnectionStateNotLoggedIn(std::shared_ptr<FTPConnection>&& handledConnection): FTPConnectionState(std::move(handledConnection)) {}

        void user(const std::string& username) final;
        void cwd(std::string path) final { defaultBehavior(); }
        void cdup() final { defaultBehavior(); }
        void quit() final { defaultBehavior(); }
        void port(const std::string& hostPort) final { defaultBehavior(); }
        void type(const std::string& typeCode) final { defaultBehavior(); }
        void stru(const std::string& structureCode) final { defaultBehavior(); }
        void mode(const std::string& modeCode) final { defaultBehavior(); }
        void retr(std::filesystem::path path) final { defaultBehavior(); }
        void stor(std::filesystem::path path) final { defaultBehavior(); }
        void pwd() const final { defaultBehavior(); }
        void list(std::filesystem::path path) const final { defaultBehavior(); }

        void defaultBehavior() const {
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    "530 Not Logged In\r\n"s,
                    22,
                    _handledConnection->defaultAsyncOpHandler);
        }

    };

    class FTPConnectionStateLoggedIn: public FTPConnectionState{
    public:
        explicit FTPConnectionStateLoggedIn(std::shared_ptr<FTPConnection>&& handledConnection): FTPConnectionState(std::move(handledConnection)) {}

        void user(const std::string& username) final;
        void cwd(std::string path) final;
        void cdup() final;
        void quit() final;
        void port(const std::string& hostPort) final;
        void type(const std::string& typeCode) final;
        void stru(const std::string& structureCode) final;
        void mode(const std::string& modeCode) final;
        void retr(std::filesystem::path path) final;
        void stor(std::filesystem::path path) final;
        void pwd() const final;
        void list(std::filesystem::path path) const final;

    private:
        std::string _systemCommandResult;
    };

    class FTPConnectionDataSender: public FTPConnectionBase{
    public:
        enum class ConnectionMode{
            sender,
            receiver
        };
        FTPConnectionDataSender(std::shared_ptr<FTPConnectionBase>&& parent,
                                std::shared_ptr<FTPFileSystem>&& fileSystem,
                                std::filesystem::path&& pathToFile,
                                ConnectionMode mode,
                                std::function<void(void)>&& dataTransmissionEndCallback,
                                std::function<void(void)>&& waitingForConnectionCallback = [](){}
                                ):
                                FTPConnectionBase(std::move(parent),
                                                  std::move(waitingForConnectionCallback)
                                                  ),
                                                  _pathToFile(pathToFile),
                                                  _fileSystem(fileSystem),
                                                  _mode(mode),
                                                  _dataTransmissionEndCallback(dataTransmissionEndCallback) {};

    protected:
        void startActing() override{
            _fileFd = _fileSystem->open(_pathToFile,
                                        _mode==ConnectionMode::sender ?
                                        FTPFileSystem::OpenMode::readonly :
                                        FTPFileSystem::OpenMode::writeonly);
            //Now we have opened the requested file and need to start the connection session
            continue_transmission(0);
        }

        Callback continue_transmission = [this](std::int64_t res){
            _buffer.clear();
            _buffer.resize(500);
            if(res < 0){
                //previous socket/file operation failed - assume it is closed.
                _fileSystem->close(_fileFd);
                _dataTransmissionEndCallback();
                stop();
            } else {
                if(_mode == ConnectionMode::sender){
                    _ring->async_read_some(_fileFd, _buffer, [this](int res){
                        if(res > 0){
                            //read from file successful
                            _buffer.resize(_buffer.find_last_not_of(static_cast<char>(0)) + 1);
                            _ring->async_write_some(_fd, _buffer, continue_transmission);
                        } else {
                            //read from file failed - eof reached
                            _fileSystem->close(_fileFd);
                            _dataTransmissionEndCallback();
                            stop();
                        }
                    });
                } else {
                    //mode: receiver
                    _ring->async_read_some(_fd, _buffer, [this](int res){
                        if(res > 0){
                            //read from socket successful
                            _buffer.resize(_buffer.find_last_not_of(static_cast<char>(0)) + 1);
                            _ring->async_write_some(_fileFd, _buffer, continue_transmission);
                        } else {
                            //read from socket failed - connection closed
                            _fileSystem->close(_fileFd);
                            _dataTransmissionEndCallback();
                            stop();
                        }
                    });
                }
            }
        };

    private:
        std::filesystem::path _pathToFile;
        std::shared_ptr<FTPFileSystem> _fileSystem;
        ConnectionMode _mode;
        std::function<void(void)> _dataTransmissionEndCallback;
        int _fileFd;
        std::string _buffer;
    };

}

#endif //URING_TCP_SERVER_FTPCONNECTION_H