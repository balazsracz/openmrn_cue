#include "mobilestation/TrainDb.hxx"

namespace mobilestation {

__attribute__((weak)) extern const struct const_loco_db_t const_lokdb[] = {
  // 0
  { 50, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 0xff} , { LIGHT, ENGINE, HONK, SPEECH, SPEECH, SPEECH, SPEECH, LIGHT1, FNP, ABV, HONK, SOUNDP, SOUNDP, SOUNDP, HONK, HONK, HONK, HONK, HONK, HONK, 0xff }, "ICN", DCC_28 | PUSHPULL },
  // 1
  { 51, { 0, 1, 3, 4,  0xff, }, { LIGHT, TELEX, FNT11, ABV,  0xff, },
    "BR 260417", DCC_28 },
  // 2
  { 12, { 0, 2, 3, 4,  0xff, }, { LIGHT, BEAMER, HONK, ABV,  0xff, },
    "RE 474 014-8", MFX }, // todo: check fnbits
  // 3
  { 2 , { 0, 2, 3, 4,  0xff, }, { LIGHT, HONK, FNT12, ABV,  0xff, },
    "ICE 2", MARKLIN_NEW | PUSHPULL }, // todo: check fnbits
  // 4
  { 22, { 0, 3, 4,  0xff, }, { LIGHT, FNT11, ABV,  0xff, },
    "RE 460 TSR", MARKLIN_NEW }, // todo: there is no beamer here
  // 5
  { 32, { 0, 4,  0xff, }, { LIGHT, ABV,  0xff, },
    "RTS RAILTR", MARKLIN_NEW },
  // 6
  { 61, { 0, 1, 2, 3, 4,  0xff, }, { LIGHT, ENGINE, LIGHT2, HONK, 7,  0xff, },
    "MAV M61", DCC_28 }, // todo: check fn definition and type mapping
  // 7
  { 46, { 0, 1, 2, 3, 4, 5, 0xff, }, { LIGHT, HONK, ENGINE, FNT11, ABV, BEAMER,  0xff, },
    "RE 460 118-2", MFX | PUSHPULL },  // todo: there is F5 with beamer that can't be switched.
  // 8
  { 15, { 0, 1, 3, 4,  0xff, }, { LIGHT, BEAMER, FNT11, ABV,  0xff, },
    "RE 4/4 II", DCC_14 }, // todo: snail mode
  // 9
  { 47, { 0,  0xff, }, { LIGHT,  0xff, },
    "RE 465", MARKLIN_OLD | PUSHPULL },
  // id 10
  { 72, { 0, 1, 4, 0xff, }, { LIGHT, LIGHT1, ABV, 0xff },
    "DHG 700", MARKLIN_NEW },
  // id 11
  { 29, { 0, 4,  0xff }, { LIGHT, ABV,  0xff, },
    "BR 290 022-3", MFX },
  // id 12
  { 43, { 0, 1, 3, 4,  0xff, }, { LIGHT, TELEX, FNT11, ABV,  0xff, },
    "Am 843 093-6", DCC_28 },
  // id 13
  { 27, { 0, 4,  0xff, }, { LIGHT, ABV,  0xff, },
    "WLE ER20", MARKLIN_NEW },
  // id 14
  { 58, { 0, 3, 4,  0xff, }, { LIGHT, FNT11, ABV,  0xff, },
    "185 595-6", DCC_28 }, // NOTE: hardware is programmed for addr 18
  // id 15
  { 26, { 0,  0xff, }, { LIGHT,  0xff, },
    "Re 460 HAG", MARKLIN_OLD | PUSHPULL },
  // id 16
  { 38, { 0,  0xff, }, { LIGHT,  0xff, },
    "BDe 4/4 1460", MARKLIN_OLD | PUSHPULL },
  // id 17
  { 48, { 0,  0xff, }, { LIGHT,  0xff, },
    "Re 4/4 II 11239", MARKLIN_NEW },
  // id 18
  { 66, { 0, 3, 4, 0xff, }, { LIGHT, FNT11, ABV, 0xff, },
    "Re 6/6 11665", DCC_128 },
  // id 19
  { 3, { 0, 3, 4, 0xff, }, { LIGHT, FNT11, ABV,  0xff, },
    "RBe 4/4 1423", DCC_28 | PUSHPULL },
  // id 20
  { 24, { 0, 3, 4, 0xff, }, { LIGHT, FNT11, ABV, 0xff, },
    "Taurus", MARKLIN_NEW },
  // id 21
  { 52, { 0, 3, 4, 0xff, }, { LIGHT, FNT11, ABV, 0xff, },
    "RBe 4/4 1451", MARKLIN_NEW },
  { 0, {0, }, {0,}, "", 0},
  { 0, {0, }, {0,}, "", 0},
  { 0, {0, }, {0,}, "", 0},
  { 0, {0, }, {0,}, "", 0},
  { 0, {0, }, {0,}, "", 0},
};

__attribute__((weak)) extern const size_t const_lokdb_size = sizeof(const_lokdb) / sizeof(const_lokdb[0]);

}  // namespace mobilestation
