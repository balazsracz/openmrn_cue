#include "mobilestation/TrainDb.hxx"
#include "utils/constants.hxx"

namespace mobilestation {

__attribute__((weak)) extern const struct const_loco_db_t const_lokdb[] = {
  // 0
  { 50, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 0xff} , { LIGHT, ENGINE, HONK, SPEECH, SPEECH, SPEECH, SPEECH, LIGHT1, FNP, ABV, HONK, SOUNDP, SOUNDP, SOUNDP, HONK, HONK, HONK, HONK, HONK, HONK, 0xff }, "ICN", DCC_128 | PUSHPULL },  // ESU loksound V4.0 MM/DCC
  // 1
  { 51, { 0, 1, 3, 4,  0xff, }, { LIGHT, TELEX, SHUNT, ABV,  0xff, },
    "BR 260417", DCC_28 },  // ESU LokPilot 3.0
  // 2
  { 12, { 0, 2, 3, 4,  0xff, }, { LIGHT, BEAMER, HONK, ABV,  0xff, },
    "RE 474 014-8", MFX }, // todo: check fnbits
  // 3
  { 2 , { 0, 2, 3, 4,  0xff, }, { LIGHT, HONK, SPEECH, ABV,  0xff, },
    "ICE 2", MARKLIN_NEW | PUSHPULL }, // todo: check fnbits
  // 4
  { 22, { 0, 3, 4,  0xff, }, { LIGHT, FNT11, ABV,  0xff, },
    "RE 460 TSR", DCC_128 }, // todo: there is no beamer here // LD-32 decoder
  // 5
  { 32, { 0, 4,  0xff, }, { LIGHT, ABV,  0xff, },
    "RTS RAILTR", MARKLIN_NEW },  // Marklin Fx decoder
  // 6
  { 61, { 0, 1, 2, 3, 4,  0xff, }, { LIGHT, ENGINE, LIGHT2, HONK, 7,  0xff, },
    "MAV M61", DCC_128 }, // todo: check fn definition and type mapping // ESU LokSound V3.5
  // 7
  { 46, { 0, 1, 2, 3, 4, 5, 0xff, }, { LIGHT, HONK, ENGINE, FNT11, ABV, BEAMER,  0xff, },
    "RE 460 118-2", MFX | PUSHPULL },  // todo: there is F5 with beamer that can't be switched.
  // 8
  { 15, { 0, 1, 3, 4,  0xff, }, { LIGHT, BEAMER, FNT11, ABV,  0xff, },
    "RE 4/4 II", DCC_28 /* TODO: this should be dcc 14 */ }, // todo: snail mode
  // 9
  { 47, { 0,  0xff, }, { LIGHT,  0xff, },
    "RE 465", MARKLIN_OLD | PUSHPULL },  // Marklin 6090
  // id 10
  { 72, { 0, 1, 4, 0xff, }, { LIGHT, LIGHT1, ABV, 0xff },
    "DHG 700", MARKLIN_NEW },
  // id 11
  { 29, { 0, 4,  0xff }, { LIGHT, ABV,  0xff, },
    "BR 290 022-3", MFX }, // Marklin mfx
  // id 12
  { 43, { 0, 1, 3, 4,  0xff, }, { LIGHT, TELEX, FNT11, ABV,  0xff, },
    "Am 843 093-6", DCC_28 },
  // id 13
  { 27, { 0, 4,  0xff, }, { LIGHT, ABV,  0xff, },
    "WLE ER20", MARKLIN_NEW }, // Marklin fx
  // id 14
  { 58, { 0, 3, 4,  0xff, }, { LIGHT, FNT11, ABV,  0xff, },
    "185 595-6", DCC_28 }, // NOTE: hardware is programmed for addr 18 // I think this engine was returned to Acacio
  // id 15
  { 26, { 0,  0xff, }, { LIGHT,  0xff, },
    "Re 460 HAG", MARKLIN_OLD | PUSHPULL },  // Marklin 6090 (i think; best to check)
  // id 16
  { 38, { 0, 3, 4, 0xff, }, { LIGHT, FNT11, ABV, 0xff, },
    "BDe 4/4 1640", DCC_128 | PUSHPULL },  // Tams LD-G32, DC motor
  // id 17
  { 48, { 0,  0xff, }, { LIGHT,  0xff, },
    "Re 4/4 II 11239", MARKLIN_NEW }, // ESU pre-lokpilot decoder, no DCC support
  // id 18
  { 66, { 0, 3, 4, 0xff, }, { LIGHT, FNT11, ABV, 0xff, },
    "Re 6/6 11665", DCC_128 },
  // id 19
  { 52, { 0, 3, 4, 0xff, }, { LIGHT, FNT11, ABV, 0xff, },
    "RBe 4/4 1423 BL", DCC_128 }, // Viessmann basic decoder (te08 == LD32)
  // id 20
  { 24, { 0, 3, 4, 0xff, }, { LIGHT, FNT11, ABV, 0xff, },
    "Taurus", MARKLIN_NEW },
  // id 21
  { 33, { 0, 3, 4, 0xff, }, { LIGHT, FNT11, ABV,  0xff, },
    "RBe 4/4 1423", DCC_28 | PUSHPULL }, // Burned LokPilot 3.0
  // id 22
  { 68, {0, 1, 2, 3, 4, 0xff }, { LIGHT, LIGHT1, ENGINE, WHISTLE, ABV, 0xff   },
    "Ce6/8 Krokodil", MFX },
  // id 23
  { 3, {0, 1, 2, 3, 4, 0xff }, { LIGHT, HONK, ENGINE, SHUNT, ABV, 0xff },
    "Re 10/10", DCC_128 },
  // id 24
  { 19, {0, 1, 2, 3, 4, 0xff }, { LIGHT, SMOKE, ENGINE, WHISTLE, ABV, 0xff },
    "BR 18.3", MFX },
  // id 25
  { 4, {0, 1, 2, 3, 4, 0xff }, { LIGHT, SMOKE, ENGINE, WHISTLE, ABV, 0xff },
    "Re 420 cargo", DCC_128 },
  // id 26
  { 5, {0, 1, 2, 3, 4, 0xff }, { LIGHT, SMOKE, ENGINE, WHISTLE, ABV, 0xff },
    "Re 620 cargo", DCC_128 },
  { 0, {0, }, {0,}, "", 0},
  { 0, {0, }, {0,}, "", 0},
};

__attribute__((weak)) extern const size_t const_lokdb_size = sizeof(const_lokdb) / sizeof(const_lokdb[0]);

DEFAULT_CONST(mobile_station_train_count, 10);

}  // namespace mobilestation
