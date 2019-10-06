stm8flash
=========

This is a free and opensource software distributed under the terms of the GNU General Public License v2.

For years, it was the only program that's able to communicate through the SWIM interface of ST-LINKs under Linux.

Since 2018, OpenOCD also offers the basic functionality, and also has support for on-target debugging.
As of early 2018, stm8flash has wider device support, and better support for memory read/write operations.


Synopsis
--------

```
stm8flash -c <stlink|stlinkv2|espstlink> -p <partname> [-s flash|eeprom|0x8000] [-r|-w|-v] <filename>
```

The supported file types are Intel Hex, Motorola S-Record and Raw Binary. The type is detected by the file extension.

Flash examples:
```nohighlight
./stm8flash -c stlink -p stm8s003f3 -w blinky.bin
./stm8flash -c stlink -p stm8s003f3 -w blinky.ihx
./stm8flash -c stlinkv2 -p stm8s003f3 -w blinky.ihx
./stm8flash -c stlink -p stm8s105c6 -w blinky.bin
./stm8flash -c stlinkv2 -p stm8l150 -w blinky.bin
```

EEPROM examples:
```nohighlight
./stm8flash -c stlinkv2 -p stm8s003f3 -s eeprom -r ee.bin
./stm8flash -c stlinkv2 -p stm8s003f3 -s eeprom -w ee.bin
./stm8flash -c stlinkv2 -p stm8s003f3 -s eeprom -v ee.bin
```

Support table
-------------

  * ST-Link V1: flash/eeprom/opt
  * flash2/eeprom2/opt2: ST-LINK/V2, ST-LINK/V2-1 and STLINK-V3

| MCU         | flash | eeprom | opt  | flash2 | eeprom2 | opt2  |
|-------------|-------|--------|------|--------|---------|-------|
| stlux385    |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stlux???a   |  ?    |  ?     |  ?   |  ok    |  ok     |  ?    |
| stm8af526?  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8af528?  |  ?    |  ?     |  ?   |  ok    |  ?      |  ?    |
| stm8af52a?  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8af6213  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8af6223  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8af6223a |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8af6226  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8af624?  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8af6266  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8af6268  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8af6269  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8af628?  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8af62a?  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8al313?  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8al314?  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8al316?  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8al318?  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8al31e8? |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8al3l4?  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8al3l6?  |  ?    |  ?     |  ?   |  ok    |  ok     |  ok   |
| stm8al3l8?  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8al3le8? |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8l001j3  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8l050j3  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8l051f3  |  ok   |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8l052c6  |  ok   |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8l052r8  |  ok   |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8l101f1  |  ?    |  no    |  ?   |  ?     |  no     |  ?    |
| stm8l101?2  |  ?    |  no    |  ?   |  ?     |  no     |  ?    |
| stm8l101?3  |  ?    |  no    |  ?   |  ok    |  no     |  ?    |
| stm8l151?2  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8l151?3  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8l151?4  |  ok   |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8l151?6  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8l151?8  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8l152?4  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8l152?6  |  ok   |  FAIL  |  ?   |  ok    |  ok     |  ?    |
| stm8l152?8  |  ?    |  ?     |  ?   |  ok    |  ?      |  ?    |
| stm8l162?8  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8s001j3  |  ?    |  ?     |  ?   |  ok    |  ok     |  ?    |
| stm8s003?3  |  ok   |  FAIL  |  ?   |  ok    |  ok     |  ok   |
| stm8s005?6  |  ok   |  ?     |  ok  |  ok    |  ok     |  ok   |
| stm8s007c8  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8s103f2  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8s103?3  |  ok   |  ?     |  ?   |  ok    |  ?      |  ?    |
| stm8s105?4  |  ok   |  FAIL  |  ?   |  ok    |  ok     |  ?    |
| stm8s105?6  |  ok   |  ?     |  ?   |  ok    |  ?      |  ?    |
| stm8s207c8  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8s207cb  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8s207k8  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8s207m8  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8s207mb  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8s207r8  |  ?    |  ?     |  ?   |  ok    |  ?      |  ?    |
| stm8s207rb  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8s207s8  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8s207sb  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8s207?6  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8s208c6  |  ?    |  ?     |  ?   |  ok    |  ?      |  ?    |
| stm8s208r6  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8s208s6  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8s208?8  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8s208?b  |  ?    |  ?     |  ?   |  ok    |  ?      |  ok   |
| stm8s903?3  |  ?    |  ?     |  ?   |  ok    |  ok     |  ok   |
| stm8splnb1  |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8tl5??4  |  ?    |  no    |  ?   |  ?     |  no     |  ?    |
| stnrg???a   |  ?    |  ?     |  ?   |  ok    |  ok     |  ?    |

Legend:

  * `ok`   - Fully supported.
  * `no`   - Not supported.
  * `?`    - Not tested.
  * `FAIL` - Not working. Needs fix.

